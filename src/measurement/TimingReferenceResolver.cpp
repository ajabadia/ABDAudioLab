/**
 * @file TimingReferenceResolver.cpp
 * @brief Implementation of timing resolution with correlation ambiguity detection and confidence scoring.
 * @author ABDSynths
 * @date 2026
 */

#include "TimingReferenceResolver.h"
#include "ComplexEnvelopeAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::measurement
{

TimingResolution TimingReferenceResolver::resolveFromProvidedEvent(
    size_t noteOnSample,
    double /*sampleRate*/) noexcept
{
    TimingResolution res;
    res.type = TimingReferenceType::ProvidedEvent;
    res.offsetSamples = static_cast<int>(noteOnSample);
    res.offsetFractionalSamples = static_cast<double>(noteOnSample);
    res.confidence = 1.0;
    res.ambiguityMargin = 1.0;
    res.peakRatio = 0.0;
    res.status = "resolved";
    res.resolutionDetails = "Direct timestamp from MIDI/host event";
    return res;
}

TimingResolution TimingReferenceResolver::resolveFromAudioOnset(
    std::span<const float> audio,
    double sampleRate,
    const Config& config) noexcept
{
    TimingResolution res;
    res.type = TimingReferenceType::AudioOnset;

    if (audio.empty())
    {
        res.status = "insufficient_signal";
        res.resolutionDetails = "Empty audio buffer";
        return res;
    }

    std::vector<float> audioVec(audio.begin(), audio.end());
    auto onsetOpt = ComplexEnvelopeAnalyzer::detectEnergyOnset(
        audioVec,
        sampleRate,
        config.noiseFloorDbfs,
        config.energyOnsetRatio);

    if (!onsetOpt.has_value())
    {
        res.status = "insufficient_signal";
        res.resolutionDetails = "Audio energy remained below onset detection threshold";
        return res;
    }

    res.offsetSamples = static_cast<int>(*onsetOpt);
    res.offsetFractionalSamples = static_cast<double>(*onsetOpt);
    res.confidence = 0.85;
    res.ambiguityMargin = 0.50;
    res.peakRatio = 0.50;
    res.status = "resolved";
    res.resolutionDetails = "Energy onset detected via acoustic threshold";
    return res;
}

TimingResolution TimingReferenceResolver::resolveFromLoopbackCorrelation(
    std::span<const float> stimulus,
    std::span<const float> captured,
    double /*sampleRate*/,
    int maxLagSamples,
    const Config& config) noexcept
{
    TimingResolution res;
    res.type = TimingReferenceType::LoopbackCorrelation;

    if (stimulus.empty() || captured.empty())
    {
        res.status = "not_available";
        res.resolutionDetails = "Stimulus or captured buffer is empty";
        return res;
    }

    const size_t stimLen = std::min<size_t>(stimulus.size(), 4096);
    if (captured.size() < stimLen)
    {
        res.status = "insufficient_signal";
        res.resolutionDetails = "Captured buffer shorter than stimulus correlation window";
        return res;
    }

    // Precompute stimulus energy
    double stimEnergy = 0.0;
    for (size_t i = 0; i < stimLen; ++i)
    {
        stimEnergy += static_cast<double>(stimulus[i]) * static_cast<double>(stimulus[i]);
    }

    if (stimEnergy < 1e-12)
    {
        res.status = "insufficient_signal";
        res.resolutionDetails = "Stimulus energy is near zero (silent stimulus)";
        return res;
    }

    const int searchRange = std::min<int>(
        maxLagSamples,
        static_cast<int>(captured.size() - stimLen));

    if (searchRange <= 0)
    {
        res.status = "insufficient_signal";
        res.resolutionDetails = "Search range is empty";
        return res;
    }

    std::vector<double> corr(static_cast<size_t>(searchRange), 0.0);

    for (int lag = 0; lag < searchRange; ++lag)
    {
        double cross = 0.0;
        double capEnergy = 0.0;
        for (size_t i = 0; i < stimLen; ++i)
        {
            double c = static_cast<double>(captured[static_cast<size_t>(lag) + i]);
            double s = static_cast<double>(stimulus[i]);
            cross += s * c;
            capEnergy += c * c;
        }

        if (capEnergy > 1e-12)
        {
            corr[static_cast<size_t>(lag)] = cross / std::sqrt(stimEnergy * capEnergy);
        }
        else
        {
            corr[static_cast<size_t>(lag)] = 0.0;
        }
    }

    // Find global peak R1
    int bestLag = 0;
    double bestR = -1e9;
    for (int lag = 0; lag < searchRange; ++lag)
    {
        if (corr[static_cast<size_t>(lag)] > bestR)
        {
            bestR = corr[static_cast<size_t>(lag)];
            bestLag = lag;
        }
    }

    if (bestR < config.minCorrelationConfidence)
    {
        res.confidence = std::max(0.0, bestR);
        res.status = "insufficient_signal";
        res.resolutionDetails = "Peak correlation (" + std::to_string(bestR) +
                                ") is below minimum confidence threshold (" +
                                std::to_string(config.minCorrelationConfidence) + ")";
        return res;
    }

    // Find second local peak R2 (exclude immediate neighborhood of bestLag: +/- 10 samples)
    double secondR = 0.0;
    const int exclusionRadius = 10;
    for (int lag = 0; lag < searchRange; ++lag)
    {
        if (std::abs(lag - bestLag) > exclusionRadius)
        {
            // Check if local peak
            bool isLocalPeak = true;
            if (lag > 0 && corr[static_cast<size_t>(lag - 1)] > corr[static_cast<size_t>(lag)]) isLocalPeak = false;
            if (lag + 1 < searchRange && corr[static_cast<size_t>(lag + 1)] > corr[static_cast<size_t>(lag)]) isLocalPeak = false;

            if (isLocalPeak && corr[static_cast<size_t>(lag)] > secondR)
            {
                secondR = corr[static_cast<size_t>(lag)];
            }
        }
    }

    double peakRatio = 0.0;
    double ambiguityMargin = 1.0;
    if (bestR > 0.0)
    {
        if (secondR > 0.0)
        {
            peakRatio = std::clamp(secondR / bestR, 0.0, 1.0);
            ambiguityMargin = std::clamp((bestR - secondR) / bestR, 0.0, 1.0);
        }
        else
        {
            peakRatio = 0.0;
            ambiguityMargin = 1.0;
        }
    }

    res.confidence = std::clamp(bestR, 0.0, 1.0);
    res.ambiguityMargin = ambiguityMargin;
    res.peakRatio = peakRatio;
    res.offsetSamples = bestLag;

    // Fractional peak interpolation if interior
    double fractionalOffset = static_cast<double>(bestLag);
    if (bestLag > 0 && bestLag + 1 < searchRange)
    {
        double alpha = corr[static_cast<size_t>(bestLag - 1)];
        double beta  = corr[static_cast<size_t>(bestLag)];
        double gamma = corr[static_cast<size_t>(bestLag + 1)];
        double denom = alpha - 2.0 * beta + gamma;
        if (std::abs(denom) > 1e-9)
        {
            double delta = 0.5 * (alpha - gamma) / denom;
            fractionalOffset += std::clamp(delta, -0.5, 0.5);
        }
    }
    res.offsetFractionalSamples = fractionalOffset;

    // Ambiguous if peakRatio >= maxPeakRatio (e.g. 0.85) or equivalently ambiguityMargin <= ambiguityThreshold (e.g. 0.15)
    if (peakRatio >= config.maxPeakRatio || ambiguityMargin <= config.ambiguityThreshold)
    {
        res.status = "ambiguous";
        res.resolutionDetails = "Multiple competing correlation peaks detected; peakRatio " +
                                std::to_string(peakRatio) + " >= " +
                                std::to_string(config.maxPeakRatio) +
                                " (ambiguityMargin " + std::to_string(ambiguityMargin) + " <= " +
                                std::to_string(config.ambiguityThreshold) + ")";
        return res;
    }

    res.status = "resolved";
    res.resolutionDetails = "Cross-correlation peak unambiguously identified at lag " +
                            std::to_string(bestLag);
    return res;
}

} // namespace abdaudiolab::measurement
