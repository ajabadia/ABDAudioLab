/**
 * @file DynamicsAnalytics.cpp
 * @brief Implementation of DynamicsAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "DynamicsAnalytics.h"
#include "../LabAnalyticEngine.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math::analytics
{

float DynamicsAnalytics::calculateSignalToNoiseRatioDb(const std::vector<float>& signalBuffer,
                                                       float baselineNoiseRmsDb)
{
    if (signalBuffer.empty())
        return 0.0f;

    double sumSq = 0.0;
    for (float s : signalBuffer)
        sumSq += static_cast<double>(s * s);

    double signalRms = std::sqrt(sumSq / static_cast<double>(signalBuffer.size()));
    float signalDb = (signalRms > 1e-6) ? static_cast<float>(20.0 * std::log10(signalRms)) : -120.0f;

    return std::max(0.0f, signalDb - baselineNoiseRmsDb);
}

WaveShaperAnalysisData DynamicsAnalytics::analyzeWaveShaperRamps(const std::vector<std::vector<float>>& recordedPasses,
                                                                 double sampleRate)
{
    WaveShaperAnalysisData result;
    if (recordedPasses.empty())
        return result;

    const auto& pass = recordedPasses[0];
    const size_t numPoints = 128;
    result.transferCurveInput.resize(numPoints);
    result.transferCurveOutput.resize(numPoints);

    for (size_t i = 0; i < numPoints; ++i)
    {
        float inVal = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        result.transferCurveInput[i] = inVal;

        size_t sampleIdx = std::min(pass.size() - 1, static_cast<size_t>(inVal * static_cast<float>(pass.size() - 1)));
        result.transferCurveOutput[i] = std::abs(pass[sampleIdx]);
    }

    // Single-cycle Goertzel DFT to compute THD (H1..H5)
    std::vector<float> thds;
    for (const auto& p : recordedPasses)
    {
        if (p.size() < 256 || sampleRate <= 0.0)
            continue;

        // Extract steady-state portion
        size_t N = std::min(p.size(), static_cast<size_t>(2048));
        size_t start = (p.size() > N) ? (p.size() - N) / 2 : 0;

        auto computeHarmonicMag = [&](float targetFreqHz) -> float {
            double w = 2.0 * std::numbers::pi * (targetFreqHz / sampleRate);
            double coeff = 2.0 * std::cos(w);
            double q0 = 0.0, q1 = 0.0, q2 = 0.0;

            for (size_t k = 0; k < N; ++k)
            {
                q0 = coeff * q1 - q2 + static_cast<double>(p[start + k]);
                q2 = q1;
                q1 = q0;
            }
            double realPart = q1 - q2 * std::cos(w);
            double imagPart = q2 * std::sin(w);
            return static_cast<float>(std::sqrt(realPart * realPart + imagPart * imagPart) / static_cast<double>(N));
        };

        float f1 = 1000.0f; // 1 kHz test tone
        float h1 = computeHarmonicMag(f1);
        float h2 = computeHarmonicMag(f1 * 2.0f);
        float h3 = computeHarmonicMag(f1 * 3.0f);
        float h4 = computeHarmonicMag(f1 * 4.0f);
        float h5 = computeHarmonicMag(f1 * 5.0f);

        if (h1 > 1e-5f)
        {
            float harmonicSumSq = h2 * h2 + h3 * h3 + h4 * h4 + h5 * h5;
            float thd = (std::sqrt(harmonicSumSq) / h1) * 100.0f;
            thds.push_back(std::clamp(thd, 0.01f, 100.0f));
        }
        else
        {
            thds.push_back(0.05f);
        }
    }

    result.thdPercent = FilterAnalytics::calculateStatistics(thds);
    return result;
}

GainAnalysisData DynamicsAnalytics::analyzeGainTones(const std::vector<std::vector<float>>& recordedPasses,
                                                    double /*sampleRate*/)
{
    GainAnalysisData result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> gains;
    std::vector<float> snrs;

    for (const auto& pass : recordedPasses)
    {
        if (pass.empty())
            continue;

        double sumSq = 0.0;
        for (float v : pass)
            sumSq += v * v;
        double rms = std::sqrt(sumSq / static_cast<double>(pass.size()));
        float gainDb = static_cast<float>(20.0 * std::log10(std::max(rms, 1e-6)));
        gains.push_back(gainDb);

        float snr = calculateSignalToNoiseRatioDb(pass, -85.0f);
        snrs.push_back(snr);
    }

    result.gainDb = FilterAnalytics::calculateStatistics(gains);
    result.snrDb = FilterAnalytics::calculateStatistics(snrs);
    return result;
}

WienerHammersteinAnalysisData DynamicsAnalytics::analyzeWienerHammerstein(
    const std::vector<std::vector<float>>& recordedPasses,
    const std::vector<float>& inputStimulus,
    double sampleRate)
{
    WienerHammersteinAnalysisData result;
    if (recordedPasses.empty() || inputStimulus.empty() || sampleRate <= 0.0)
        return result;

    WienerHammersteinFitter fitter;
    std::vector<float> coeffsA;
    std::vector<float> h1Centroids;
    std::vector<float> h2Centroids;
    std::vector<float> r2Fits;

    for (const auto& pass : recordedPasses)
    {
        if (pass.empty()) continue;
        auto model = fitter.fit(inputStimulus, pass, sampleRate);
        coeffsA.push_back(model.nonLinearCoeffA);
        h1Centroids.push_back(model.preFilterCentroidHz);
        h2Centroids.push_back(model.postFilterCentroidHz);
        r2Fits.push_back(model.goodnessOfFitR2);

        if (result.representativeH1.empty())
        {
            result.representativeH1 = model.h1Taps;
            result.representativeH2 = model.h2Taps;
        }
    }

    result.nonLinearCoeffA = FilterAnalytics::calculateStatistics(coeffsA);
    result.preFilterCentroidHz = FilterAnalytics::calculateStatistics(h1Centroids);
    result.postFilterCentroidHz = FilterAnalytics::calculateStatistics(h2Centroids);
    result.goodnessOfFitR2 = FilterAnalytics::calculateStatistics(r2Fits);

    return result;
}

math::PreScanResult DynamicsAnalytics::evaluateLinearBypass(const std::vector<float>& recordedBuffer,
                                                           const std::vector<float>& stimulusBuffer,
                                                           float noiseFloorDb)
{
    math::PreScanResult result;
    const size_t numSamples = std::min(recordedBuffer.size(), stimulusBuffer.size());

    if (numSamples == 0) return result;

    double dotProduct_YX = 0.0;
    double dotProduct_XX = 0.0;

    for (size_t n = 0; n < numSamples; ++n)
    {
        dotProduct_YX += static_cast<double>(recordedBuffer[n] * stimulusBuffer[n]);
        dotProduct_XX += static_cast<double>(stimulusBuffer[n] * stimulusBuffer[n]);
    }

    float g = (dotProduct_XX > 1e-9) ? static_cast<float>(dotProduct_YX / dotProduct_XX) : 0.0f;

    double sumErrorSq = 0.0;
    for (size_t n = 0; n < numSamples; ++n)
    {
        float error = recordedBuffer[n] - (g * stimulusBuffer[n]);
        sumErrorSq += static_cast<double>(error * error);
    }

    float rmsError = std::sqrt(static_cast<float>(sumErrorSq / static_cast<double>(numSamples)));
    result.residualRmsDb = (rmsError > 1e-7f) ? 20.0f * std::log10(rmsError) : -140.0f;

    float strictLinearThreshold = std::max(noiseFloorDb + 3.0f, -75.0f);
    if (result.residualRmsDb <= strictLinearThreshold)
    {
        result.isLinear = true;
        result.recommendedSteps = { 0, 127 };
    }

    return result;
}

void DynamicsAnalytics::computeAdaptiveRoadmap(math::PreScanResult& result, float gradientThreshold)
{
    if (result.isLinear || result.trajectory.size() < 3) return;

    result.recommendedSteps.clear();
    result.recommendedSteps.push_back(0);

    const size_t numPoints = result.trajectory.size();
    std::vector<float> derivatives(numPoints - 1, 0.0f);
    result.maxDerivative = 0.0f;

    for (size_t k = 0; k < numPoints - 1; ++k)
    {
        float deltaControl = result.trajectory[k+1].controlValue - result.trajectory[k].controlValue;
        if (deltaControl > 0.0f)
        {
            derivatives[k] = std::abs(result.trajectory[k+1].primaryMetric - result.trajectory[k].primaryMetric) / deltaControl;
            result.maxDerivative = std::max(result.maxDerivative, derivatives[k]);
        }
    }

    int lastAddedMidiValue = 0;
    for (size_t k = 1; k < numPoints - 1; ++k)
    {
        int currentMidiValue = static_cast<int>(std::round(result.trajectory[k].controlValue));
        if (currentMidiValue == lastAddedMidiValue) continue;

        float normalizedGradient = (result.maxDerivative > 0.0f) ? (derivatives[k] / result.maxDerivative) : 0.0f;

        if (normalizedGradient >= gradientThreshold)
        {
            if (currentMidiValue >= lastAddedMidiValue + 2)
            {
                result.recommendedSteps.push_back(currentMidiValue);
                lastAddedMidiValue = currentMidiValue;
            }
        }
        else
        {
            if (currentMidiValue >= lastAddedMidiValue + 16)
            {
                result.recommendedSteps.push_back(currentMidiValue);
                lastAddedMidiValue = currentMidiValue;
            }
        }
    }

    if (result.recommendedSteps.back() != 127)
    {
        result.recommendedSteps.push_back(127);
    }
}

math::SingleTakeAnalysisResult DynamicsAnalytics::analyzeSingleTakeMultiplexed(const std::vector<float>& alignedAudio,
                                                                              double sampleRate,
                                                                              float transientDurationSec)
{
    math::SingleTakeAnalysisResult result;
    if (alignedAudio.empty() || sampleRate <= 0.0)
        return result;

    size_t totalSamples = alignedAudio.size();
    size_t transientSamples = std::min(totalSamples, static_cast<size_t>(std::max(0.01f, transientDurationSec) * sampleRate));

    float transientPeak = 0.0f;
    double transientSumSq = 0.0;

    for (size_t i = 0; i < transientSamples; ++i)
    {
        float absVal = std::abs(alignedAudio[i]);
        if (absVal > transientPeak)
            transientPeak = absVal;
        transientSumSq += static_cast<double>(absVal) * static_cast<double>(absVal);
    }

    result.transientPeakLevel = transientPeak;
    float transientRms = static_cast<float>(std::sqrt(transientSumSq / std::max<size_t>(1, transientSamples)));

    if (transientRms > 1e-6f)
    {
        float crest = transientPeak / transientRms;
        result.transientThd = std::max(0.0f, (1.414f - crest) / 1.414f);
    }

    if (totalSamples > transientSamples)
    {
        size_t sustainedCount = totalSamples - transientSamples;
        double sustainedSumSq = 0.0;
        int zeroCrossings = 0;

        for (size_t i = transientSamples; i < totalSamples; ++i)
        {
            float s = alignedAudio[i];
            sustainedSumSq += static_cast<double>(s) * static_cast<double>(s);
            if (i > transientSamples && ((alignedAudio[i - 1] < 0.0f && s >= 0.0f) || (alignedAudio[i - 1] >= 0.0f && s < 0.0f)))
            {
                zeroCrossings++;
            }
        }

        float sustainedRms = static_cast<float>(std::sqrt(sustainedSumSq / sustainedCount));
        result.sustainedRmsDb = (sustainedRms > 1e-7f) ? (20.0f * std::log10(sustainedRms)) : -120.0f;

        double durationSec = static_cast<double>(sustainedCount) / sampleRate;
        if (durationSec > 0.0)
        {
            result.peakResonanceHz = static_cast<float>((zeroCrossings * 0.5) / durationSec);
        }

        float threshold60Db = transientPeak * 0.001f;
        size_t decayIdx = totalSamples;
        for (size_t i = transientSamples; i < totalSamples; ++i)
        {
            if (std::abs(alignedAudio[i]) < threshold60Db)
            {
                decayIdx = i;
                break;
            }
        }
        result.decayTimeMs = static_cast<float>((static_cast<double>(decayIdx) / sampleRate) * 1000.0);
    }
    else
    {
        result.sustainedRmsDb = (transientRms > 1e-7f) ? (20.0f * std::log10(transientRms)) : -120.0f;
    }

    return result;
}

math::PreScanResult DynamicsAnalytics::applyPlateauCollapseFilter(const std::vector<math::PreScanPoint>& rawTrajectory,
                                                                 float noiseFloorVariance) noexcept
{
    math::PreScanResult result;
    if (rawTrajectory.size() < 3)
    {
        result.trajectory = rawTrajectory;
        return result;
    }

    const float epsilon = std::max(3.0f * noiseFloorVariance, 0.005f);
    std::vector<math::PreScanPoint> condensedPoints;
    condensedPoints.reserve(rawTrajectory.size());
    condensedPoints.push_back(rawTrajectory.front());

    for (size_t i = 1; i < rawTrajectory.size() - 1; ++i)
    {
        const auto& prev = rawTrajectory[i - 1];
        const auto& curr = rawTrajectory[i];
        const auto& next = rawTrajectory[i + 1];

        float deltaPrev = std::abs(curr.primaryMetric - prev.primaryMetric);
        float deltaNext = std::abs(next.primaryMetric - curr.primaryMetric);

        if (deltaPrev < epsilon && deltaNext < epsilon)
        {
            continue;
        }

        condensedPoints.push_back(curr);
    }

    condensedPoints.push_back(rawTrajectory.back());

    if (condensedPoints.size() < 2)
    {
        result.trajectory = condensedPoints;
        return result;
    }

    result.trajectory.resize(condensedPoints.size());
    float totalPoints = static_cast<float>(condensedPoints.size() - 1);

    for (size_t i = 0; i < condensedPoints.size(); ++i)
    {
        result.trajectory[i] = condensedPoints[i];
        float progress = static_cast<float>(i) / totalPoints;
        result.trajectory[i].controlValue = progress * 127.0f;
        result.trajectory[i].timeSec = progress * 10.0f;
    }

    return result;
}

} // namespace abdaudiolab::math::analytics
