/**
 * @file TimingReferenceResolver.h
 * @brief Multi-mode temporal alignment resolver with confidence scoring and ambiguity detection.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeOrchestratorContracts.h"
#include <span>
#include <vector>

namespace abdaudiolab::measurement
{

class TimingReferenceResolver
{
public:
    struct Config
    {
        double ambiguityThreshold { 0.15 };      /**< Minimum relative margin (R1 - R2)/R1 required to avoid ambiguity */
        double maxPeakRatio { 0.85 };            /**< Maximum allowable peak ratio R2/R1 before flagging ambiguity (peakRatio >= 0.85 <=> ambiguityMargin <= 0.15) */
        double minCorrelationConfidence { 0.30 }; /**< Minimum normalized correlation required for a valid match */
        double noiseFloorDbfs { -90.0 };
        double energyOnsetRatio { 0.05 };
    };

    /**
     * @brief Resolves timing reference from provided MIDI/host events.
     */
    [[nodiscard]] static TimingResolution resolveFromProvidedEvent(
        size_t noteOnSample,
        double sampleRate) noexcept;

    /**
     * @brief Resolves timing reference from acoustic waveform energy onset.
     */
    [[nodiscard]] static TimingResolution resolveFromAudioOnset(
        std::span<const float> audio,
        double sampleRate,
        const Config& config = {}) noexcept;

    /**
     * @brief Resolves timing reference via measured cross-correlation with reference/stimulus.
     * @param stimulus Reference stimulus audio emitted by the system.
     * @param captured Captured audio (loopback reference channel or DUT response).
     * @param sampleRate Sample rate in Hz.
     * @param maxLagSamples Maximum search lag in samples (window around expected arrival).
     * @param config Detection parameters.
     */
    [[nodiscard]] static TimingResolution resolveFromLoopbackCorrelation(
        std::span<const float> stimulus,
        std::span<const float> captured,
        double sampleRate,
        int maxLagSamples = 48000,
        const Config& config = {}) noexcept;
};

} // namespace abdaudiolab::measurement
