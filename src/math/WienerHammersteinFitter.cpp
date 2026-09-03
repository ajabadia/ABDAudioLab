/**
 * @file WienerHammersteinFitter.cpp
 * @brief Implementation of Wiener-Hammerstein system identification with Adam backpropagation.
 * @author ABDSynths
 * @date 2026
 */

#include "WienerHammersteinFitter.h"
#include <algorithm>
#include <numeric>
#include <numbers>

namespace abdaudiolab::math
{

WienerHammersteinFitter::WienerHammersteinFitter(WienerHammersteinConfig cfg)
    : config(cfg)
{
    if (config.numTapsH1 % 2 == 0) config.numTapsH1 += 1;
    if (config.numTapsH2 % 2 == 0) config.numTapsH2 += 1;
}

void WienerHammersteinFitter::convolveSame(const std::vector<float>& x,
                                          const std::vector<float>& h,
                                          std::vector<float>& y)
{
    const int nSamples = static_cast<int>(x.size());
    const int numTaps = static_cast<int>(h.size());
    const int mid = numTaps / 2;

    y.resize(static_cast<size_t>(nSamples), 0.0f);

    for (int n = 0; n < nSamples; ++n)
    {
        float acc = 0.0f;
        for (int k = 0; k < numTaps; ++k)
        {
            int srcIdx = n - k + mid;
            if (srcIdx >= 0 && srcIdx < nSamples)
            {
                acc += h[static_cast<size_t>(k)] * x[static_cast<size_t>(srcIdx)];
            }
        }
        y[static_cast<size_t>(n)] = acc;
    }
}

void WienerHammersteinFitter::process(const WienerHammersteinModel& model,
                                     const std::vector<float>& input,
                                     std::vector<float>& output)
{
    if (input.empty())
    {
        output.clear();
        return;
    }

    std::vector<float> y1;
    convolveSame(input, model.h1Taps, y1);

    std::vector<float> x2(input.size());
    const float a = model.nonLinearCoeffA;
    for (size_t n = 0; n < input.size(); ++n)
    {
        float val = y1[n];
        x2[n] = val + a * (val * val * val);
    }

    convolveSame(x2, model.h2Taps, output);
}

float WienerHammersteinFitter::calculateCentroidHz(const std::vector<float>& firTaps, double sampleRate)
{
    if (firTaps.empty() || sampleRate <= 0.0) return 0.0f;

    const int numBins = 128;
    const double nyquist = sampleRate * 0.5;
    const int mid = static_cast<int>(firTaps.size()) / 2;

    double weightedFreqSum = 0.0;
    double magSum = 0.0;

    for (int b = 1; b <= numBins; ++b)
    {
        double freqHz = (static_cast<double>(b) / numBins) * nyquist;
        double omega = (2.0 * std::numbers::pi * freqHz) / sampleRate;

        double re = 0.0;
        double im = 0.0;
        for (int k = 0; k < static_cast<int>(firTaps.size()); ++k)
        {
            double phase = -omega * (k - mid);
            re += firTaps[static_cast<size_t>(k)] * std::cos(phase);
            im += firTaps[static_cast<size_t>(k)] * std::sin(phase);
        }
        double mag = std::sqrt(re * re + im * im);
        weightedFreqSum += freqHz * mag;
        magSum += mag;
    }

    return (magSum > 1e-9) ? static_cast<float>(weightedFreqSum / magSum) : static_cast<float>(nyquist * 0.5);
}

WienerHammersteinModel WienerHammersteinFitter::fit(const std::vector<float>& inputSignal,
                                                  const std::vector<float>& targetSignal,
                                                  double sampleRate)
{
    WienerHammersteinModel model;
    const size_t N = std::min(inputSignal.size(), targetSignal.size());
    if (N == 0) return model;

    const int K1 = config.numTapsH1;
    const int K2 = config.numTapsH2;
    const int mid1 = K1 / 2;
    const int mid2 = K2 / 2;

    // Initialize filters as unit impulses at center, a = 0
    std::vector<float> h1(static_cast<size_t>(K1), 0.0f);
    std::vector<float> h2(static_cast<size_t>(K2), 0.0f);
    h1[static_cast<size_t>(mid1)] = 1.0f;
    h2[static_cast<size_t>(mid2)] = 1.0f;
    float a = 0.0f;

    // Adam optimizer state
    std::vector<float> m_h1(static_cast<size_t>(K1), 0.0f), v_h1(static_cast<size_t>(K1), 0.0f);
    std::vector<float> m_h2(static_cast<size_t>(K2), 0.0f), v_h2(static_cast<size_t>(K2), 0.0f);
    float m_a = 0.0f, v_a = 0.0f;

    std::vector<float> grad_h1(static_cast<size_t>(K1), 0.0f);
    std::vector<float> grad_h2(static_cast<size_t>(K2), 0.0f);

    std::vector<float> y1(N, 0.0f);
    std::vector<float> x2(N, 0.0f);
    std::vector<float> y2(N, 0.0f);
    std::vector<float> e(N, 0.0f);
    std::vector<float> dE_dx2(N, 0.0f);
    std::vector<float> dE_dy1(N, 0.0f);

    float prevLoss = 1e9f;
    int epoch = 0;
    const float invN = 1.0f / static_cast<float>(N);

    for (epoch = 1; epoch <= config.maxEpochs; ++epoch)
    {
        // 1. Forward Pass
        convolveSame(inputSignal, h1, y1);

        for (size_t n = 0; n < N; ++n)
        {
            float val = y1[n];
            x2[n] = val + a * (val * val * val);
        }

        convolveSame(x2, h2, y2);

        // 2. Compute Loss & Error
        float loss = 0.0f;
        for (size_t n = 0; n < N; ++n)
        {
            e[n] = y2[n] - targetSignal[n];
            loss += e[n] * e[n];
        }
        loss *= (0.5f * invN);

        if (std::abs(prevLoss - loss) < config.tolerance)
            break;
        prevLoss = loss;

        // 3. Backward Pass (Gradients)
        // dE/dh2[k] = sum_n (e[n] * x2[n - k + mid2])
        for (int k = 0; k < K2; ++k)
        {
            float grad = 0.0f;
            for (int n = 0; n < static_cast<int>(N); ++n)
            {
                int src = n - k + mid2;
                if (src >= 0 && src < static_cast<int>(N))
                {
                    grad += e[static_cast<size_t>(n)] * x2[static_cast<size_t>(src)];
                }
            }
            grad_h2[static_cast<size_t>(k)] = grad * invN;
        }

        // dE/dx2[n] = sum_k (e[n + k - mid2] * h2[k])
        for (int n = 0; n < static_cast<int>(N); ++n)
        {
            float dE = 0.0f;
            for (int k = 0; k < K2; ++k)
            {
                int eIdx = n + k - mid2;
                if (eIdx >= 0 && eIdx < static_cast<int>(N))
                {
                    dE += e[static_cast<size_t>(eIdx)] * h2[static_cast<size_t>(k)];
                }
            }
            dE_dx2[static_cast<size_t>(n)] = dE;
        }

        // dE/da = sum_n (dE/dx2[n] * (y1[n])^3)
        float grad_a = 0.0f;
        for (size_t n = 0; n < N; ++n)
        {
            float y1_val = y1[n];
            grad_a += dE_dx2[n] * (y1_val * y1_val * y1_val);
            // dE/dy1[n] = dE/dx2[n] * (1 + 3 * a * y1[n]^2)
            dE_dy1[n] = dE_dx2[n] * (1.0f + 3.0f * a * y1_val * y1_val);
        }
        grad_a *= invN;

        // dE/dh1[k] = sum_n (dE/dy1[n] * input[n - k + mid1])
        for (int k = 0; k < K1; ++k)
        {
            float grad = 0.0f;
            for (int n = 0; n < static_cast<int>(N); ++n)
            {
                int src = n - k + mid1;
                if (src >= 0 && src < static_cast<int>(N))
                {
                    grad += dE_dy1[static_cast<size_t>(n)] * inputSignal[static_cast<size_t>(src)];
                }
            }
            grad_h1[static_cast<size_t>(k)] = grad * invN;
        }

        // 4. Adam Optimizer Parameter Updates
        const float beta1_t = std::pow(config.beta1, static_cast<float>(epoch));
        const float beta2_t = std::pow(config.beta2, static_cast<float>(epoch));
        const float lr_t = config.learningRate * std::sqrt(1.0f - beta2_t) / (1.0f - beta1_t);

        // Update h1
        for (int k = 0; k < K1; ++k)
        {
            size_t idx = static_cast<size_t>(k);
            m_h1[idx] = config.beta1 * m_h1[idx] + (1.0f - config.beta1) * grad_h1[idx];
            v_h1[idx] = config.beta2 * v_h1[idx] + (1.0f - config.beta2) * (grad_h1[idx] * grad_h1[idx]);
            h1[idx] -= lr_t * m_h1[idx] / (std::sqrt(v_h1[idx]) + config.epsilon);
        }

        // Update a
        m_a = config.beta1 * m_a + (1.0f - config.beta1) * grad_a;
        v_a = config.beta2 * v_a + (1.0f - config.beta2) * (grad_a * grad_a);
        a -= lr_t * m_a / (std::sqrt(v_a) + config.epsilon);

        // Update h2
        for (int k = 0; k < K2; ++k)
        {
            size_t idx = static_cast<size_t>(k);
            m_h2[idx] = config.beta1 * m_h2[idx] + (1.0f - config.beta1) * grad_h2[idx];
            v_h2[idx] = config.beta2 * v_h2[idx] + (1.0f - config.beta2) * (grad_h2[idx] * grad_h2[idx]);
            h2[idx] -= lr_t * m_h2[idx] / (std::sqrt(v_h2[idx]) + config.epsilon);
        }
    }

    // Evaluate final model performance (R^2, RMSE)
    model.h1Taps = std::move(h1);
    model.nonLinearCoeffA = a;
    model.h2Taps = std::move(h2);
    model.iterationsRun = epoch;

    std::vector<float> finalPrediction;
    process(model, inputSignal, finalPrediction);

    double targetMean = 0.0;
    for (size_t n = 0; n < N; ++n) targetMean += targetSignal[n];
    targetMean *= invN;

    double ssTot = 0.0;
    double ssRes = 0.0;
    for (size_t n = 0; n < N; ++n)
    {
        double diffTarget = targetSignal[n] - targetMean;
        double diffModel = targetSignal[n] - finalPrediction[n];
        ssTot += diffTarget * diffTarget;
        ssRes += diffModel * diffModel;
    }

    model.residualErrorRms = static_cast<float>(std::sqrt(ssRes * invN));
    model.goodnessOfFitR2 = (ssTot > 1e-12) ? static_cast<float>(std::max(0.0, 1.0 - (ssRes / ssTot))) : 0.0f;
    model.preFilterCentroidHz = calculateCentroidHz(model.h1Taps, sampleRate);
    model.postFilterCentroidHz = calculateCentroidHz(model.h2Taps, sampleRate);

    return model;
}

} // namespace abdaudiolab::math
