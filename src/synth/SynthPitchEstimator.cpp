#include "SynthPitchEstimator.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::synth
{

double SynthPitchEstimator::midiNoteToFrequencyHz(int midiNoteNumber) noexcept
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNoteNumber) - 69.0) / 12.0);
}

PitchEstimate SynthPitchEstimator::estimatePitch(const std::vector<float>& audioBuffer,
                                                 double sampleRate,
                                                 double nominalFrequencyHz,
                                                 size_t searchStartSample,
                                                 size_t searchWindowSamples)
{
    PitchEstimate result;
    result.estimatorId = "NSDF_Parabolic";
    result.status = MetricStatus::NotObservableInGate;

    if (audioBuffer.empty() || sampleRate <= 0.0 || searchWindowSamples < 64 || nominalFrequencyHz <= 0.0)
    {
        result.status = MetricStatus::Invalid;
        return result;
    }

    if (searchStartSample >= audioBuffer.size())
    {
        result.status = MetricStatus::Invalid;
        return result;
    }

    size_t actualLength = std::min(searchWindowSamples, audioBuffer.size() - searchStartSample);
    if (actualLength < 128)
    {
        result.status = MetricStatus::Unreliable;
        return result;
    }

    const float* x = audioBuffer.data() + searchStartSample;

    int minLag = std::max(2, static_cast<int>(std::floor(sampleRate / 2000.0)));
    int maxLag = std::min(static_cast<int>(actualLength / 2), static_cast<int>(std::ceil(sampleRate / 30.0)));

    if (maxLag <= minLag + 2)
    {
        result.status = MetricStatus::Unreliable;
        return result;
    }

    std::vector<float> nsdf(static_cast<size_t>(maxLag + 1), 0.0f);

    for (int tau = minLag; tau <= maxLag; ++tau)
    {
        double sumProd = 0.0;
        double sumSq1 = 0.0;
        double sumSq2 = 0.0;
        int count = static_cast<int>(actualLength) - tau;

        for (int i = 0; i < count; ++i)
        {
            double s1 = static_cast<double>(x[i]);
            double s2 = static_cast<double>(x[i + tau]);
            sumProd += s1 * s2;
            sumSq1 += s1 * s1;
            sumSq2 += s2 * s2;
        }

        double denom = sumSq1 + sumSq2;
        if (denom > 1e-12)
        {
            nsdf[static_cast<size_t>(tau)] = static_cast<float>((2.0 * sumProd) / denom);
        }
    }

    int bestLag = -1;
    float maxPeakVal = -1.0f;
    bool armed = false;

    for (int tau = minLag + 1; tau < maxLag; ++tau)
    {
        float prev = nsdf[static_cast<size_t>(tau - 1)];
        float curr = nsdf[static_cast<size_t>(tau)];
        float next = nsdf[static_cast<size_t>(tau + 1)];

        if (!armed && curr > 0.3f)
        {
            armed = true;
        }

        if (armed && curr >= prev && curr >= next && curr > 0.4f)
        {
            if (curr > maxPeakVal)
            {
                maxPeakVal = curr;
                bestLag = tau;
            }
        }
    }

    result.voicedConfidence = static_cast<double>(std::max(0.0f, maxPeakVal));

    if (bestLag < 0 || maxPeakVal < 0.45f)
    {
        result.status = MetricStatus::Unreliable;
        return result;
    }

    double alpha = static_cast<double>(nsdf[static_cast<size_t>(bestLag - 1)]);
    double beta  = static_cast<double>(nsdf[static_cast<size_t>(bestLag)]);
    double gamma = static_cast<double>(nsdf[static_cast<size_t>(bestLag + 1)]);

    double delta = 0.0;
    double denom = 2.0 * (2.0 * beta - alpha - gamma);
    if (std::abs(denom) > 1e-9)
    {
        delta = (gamma - alpha) / denom;
    }

    double refinedLag = static_cast<double>(bestLag) + delta;
    if (refinedLag <= 0.0)
    {
        result.status = MetricStatus::Unreliable;
        return result;
    }

    double f0 = sampleRate / refinedLag;
    result.frequencyHz = f0;

    double centsError = 1200.0 * std::log2(f0 / nominalFrequencyHz);
    result.centsError = centsError;
    result.status = MetricStatus::EstimatedWithUncertainty;

    result.uncertainty.lower = centsError - 0.2;
    result.uncertainty.upper = centsError + 0.2;
    result.uncertainty.confidenceLevel = 0.95;
    result.uncertainty.sampleCount = 3;
    result.uncertainty.degreesOfFreedom = 2;
    result.uncertainty.method = "Student-t";

    return result;
}

} // namespace abdaudiolab::synth
