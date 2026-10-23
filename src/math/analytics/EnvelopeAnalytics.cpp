/**
 * @file EnvelopeAnalytics.cpp
 * @brief Implementation of EnvelopeAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "EnvelopeAnalytics.h"
#include "../LabEnvelopeCurveAnalyzer.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math::analytics
{

EnvelopeAnalysisData EnvelopeAnalytics::analyzeAdsrEnvelopes(const std::vector<std::vector<float>>& recordedPasses,
                                                             double sampleRate)
{
    EnvelopeAnalysisData result;
    if (recordedPasses.empty() || sampleRate <= 0.0)
        return result;

    std::vector<float> attacks;
    std::vector<float> decays;
    std::vector<float> sustains;
    std::vector<float> releases;

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 128)
            continue;

        // 1. Calculate smoothed Hilbert / RMS envelope follower
        std::vector<float> env(pass.size());
        float lp = 0.0f;
        float alpha = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 100.0 / sampleRate)); // 100Hz LPF
        for (size_t i = 0; i < pass.size(); ++i)
        {
            float absVal = std::abs(pass[i]);
            lp = (1.0f - alpha) * absVal + alpha * lp;
            env[i] = lp;
        }

        // 2. Find Peak Amplitude V_peak and Peak Index
        float maxVal = 0.0f;
        size_t peakIdx = 0;
        for (size_t i = 0; i < env.size(); ++i)
        {
            if (env[i] > maxVal)
            {
                maxVal = env[i];
                peakIdx = i;
            }
        }

        if (maxVal < 1e-5f)
            continue;

        // 3. Attack Time: from note start (or threshold -60dB = 0.001 * maxVal) to peak
        size_t startIdx = 0;
        float noiseThreshold = maxVal * 0.001f; // -60 dBFS relative to peak
        for (size_t i = 0; i < peakIdx; ++i)
        {
            if (env[i] >= noiseThreshold)
            {
                startIdx = i;
                break;
            }
        }
        float attackMs = static_cast<float>((static_cast<double>(peakIdx - startIdx) / sampleRate) * 1000.0);
        attacks.push_back(attackMs);

        // 4. Sustain Level & Decay Time: find plateau after peak
        size_t sustainSearchEnd = std::min(env.size(), peakIdx + static_cast<size_t>(sampleRate * 0.5)); // search next 500ms
        float sustainSum = 0.0f;
        int sustainCount = 0;
        size_t decayEndIdx = peakIdx;

        for (size_t i = peakIdx + static_cast<size_t>(sampleRate * 0.1); i < sustainSearchEnd; ++i)
        {
            sustainSum += env[i];
            sustainCount++;
        }

        float sustainVal = (sustainCount > 0) ? (sustainSum / static_cast<float>(sustainCount)) : maxVal;
        float normalizedSustain = std::clamp(sustainVal / maxVal, 0.0f, 1.0f);
        sustains.push_back(normalizedSustain);

        // Decay Time: from peak to within 10% of sustain plateau
        float decayTarget = sustainVal + (maxVal - sustainVal) * 0.1f;
        for (size_t i = peakIdx; i < sustainSearchEnd; ++i)
        {
            if (env[i] <= decayTarget)
            {
                decayEndIdx = i;
                break;
            }
        }
        float decayMs = static_cast<float>((static_cast<double>(decayEndIdx - peakIdx) / sampleRate) * 1000.0);
        decays.push_back(decayMs);

        // 5. Release Time: decay from sustain down to -60dB cutoff, with Schroeder backward integration
        //    fallback when stationary analog noise prevents reaching -60 dB.
        size_t releaseStartIdx = sustainSearchEnd;
        size_t releaseEndIdx = env.size();
        bool crossedThreshold = false;
        for (size_t i = releaseStartIdx; i < env.size(); ++i)
        {
            if (env[i] <= noiseThreshold)
            {
                releaseEndIdx = i;
                crossedThreshold = true;
                break;
            }
        }

        float releaseMs = static_cast<float>((static_cast<double>(releaseEndIdx - releaseStartIdx) / sampleRate) * 1000.0);
        if (!crossedThreshold && releaseStartIdx + 64 < pass.size())
        {
            std::vector<float> releaseSlice(pass.begin() + static_cast<std::ptrdiff_t>(releaseStartIdx), pass.end());
            auto schroeder = LabEnvelopeCurveAnalyzer::analyzeSchroederDecay(releaseSlice, sampleRate, 0.90f);
            if (schroeder.valid && schroeder.rt60Seconds > 0.0f)
                releaseMs = schroeder.rt60Seconds * 1000.0f;
        }
        releases.push_back(releaseMs);
    }

    result.attackTimeMs = FilterAnalytics::calculateStatistics(attacks);
    result.decayTimeMs = FilterAnalytics::calculateStatistics(decays);
    result.sustainLevel = FilterAnalytics::calculateStatistics(sustains);
    result.releaseTimeMs = FilterAnalytics::calculateStatistics(releases);
    result.delayTimeMs = { 0.0f, 0.0f };

    return result;
}

EnvelopeAnalysisData EnvelopeAnalytics::analyzeDelayImpulses(const std::vector<std::vector<float>>& recordedPasses,
                                                             double sampleRate)
{
    EnvelopeAnalysisData result = analyzeAdsrEnvelopes(recordedPasses, sampleRate);
    if (recordedPasses.empty() || sampleRate <= 0.0)
        return result;

    std::vector<float> delayTimesMs;
    std::vector<float> rt60ReleasesMs;

    const size_t minEchoSeparationSamples = std::max<size_t>(16, static_cast<size_t>(std::lround(0.005 * sampleRate))); // 5 ms

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 128)
            continue;

        // 1. Primary direct impulse peak
        float primaryPeak = 0.0f;
        size_t primaryIdx = 0;
        for (size_t i = 0; i < pass.size(); ++i)
        {
            const float a = std::abs(pass[i]);
            if (a > primaryPeak)
            {
                primaryPeak = a;
                primaryIdx = i;
            }
        }

        if (primaryPeak < 1e-5f)
            continue;

        // 2. Secondary echo tap peak after dead-zone
        const size_t searchStart = primaryIdx + minEchoSeparationSamples;
        if (searchStart < pass.size())
        {
            float secondaryPeak = 0.0f;
            size_t secondaryIdx = searchStart;
            for (size_t i = searchStart; i < pass.size(); ++i)
            {
                const float a = std::abs(pass[i]);
                if (a > secondaryPeak)
                {
                    secondaryPeak = a;
                    secondaryIdx = i;
                }
            }

            if (secondaryPeak >= primaryPeak * 0.02f) // >= -34 dB relative echo tap
            {
                const float echoMs = static_cast<float>(
                    (static_cast<double>(secondaryIdx - primaryIdx) / sampleRate) * 1000.0);
                delayTimesMs.push_back(echoMs);
            }
        }

        // 3. Reverberant / feedback tail decay via Schroeder backward integration (T30/T20/T10)
        if (primaryIdx + 64 < pass.size())
        {
            std::vector<float> tailSlice(pass.begin() + static_cast<std::ptrdiff_t>(primaryIdx), pass.end());
            auto schroeder = LabEnvelopeCurveAnalyzer::analyzeSchroederDecay(tailSlice, sampleRate, 0.90f);
            if (schroeder.valid && schroeder.rt60Seconds > 0.0f)
                rt60ReleasesMs.push_back(schroeder.rt60Seconds * 1000.0f);
        }
    }

    if (!delayTimesMs.empty())
        result.delayTimeMs = FilterAnalytics::calculateStatistics(delayTimesMs);

    if (!rt60ReleasesMs.empty())
        result.releaseTimeMs = FilterAnalytics::calculateStatistics(rt60ReleasesMs);

    return result;
}

} // namespace abdaudiolab::math::analytics
