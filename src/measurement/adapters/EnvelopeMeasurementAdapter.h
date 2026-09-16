/**
 * @file EnvelopeMeasurementAdapter.h
 * @brief Adapter coordinating SynthEnvelopeAnalyzer into canonical response-measurement-1.0 contracts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../MeasurementContracts.h"
#include <vector>

namespace abdaudiolab::measurement
{

/**
 * @class EnvelopeMeasurementAdapter
 * @brief Translates SynthEnvelopeAnalyzer outputs into strongly-typed MeasurementResult.
 * 
 * Strict non-duplication: delegates all ADSR calculations to SynthEnvelopeAnalyzer.
 * Enforces metrological observability: distinguishes observed from unobservable gate states,
 * extracts temporal envelope curves, and prevents synthetic metric invention.
 */
class EnvelopeMeasurementAdapter
{
public:
    static constexpr const char* kAnalyzerName = "SynthEnvelopeAnalyzer";
    static constexpr const char* kAnalyzerVersion = "1.0.0";

    /**
     * @brief Measures ADSR envelope on captured audio.
     * 
     * @param spec Source measurement specification.
     * @param audioBuffer Recorded audio samples (mono or downmixed).
     * @param sampleRate Audio sampling rate in Hz.
     * @param noteOnSample Sample index where Note-On was dispatched.
     * @param noteOffSample Sample index where Note-Off was dispatched.
     * @param detectedOnsetSample Optional sample index of acoustic onset. If 0, auto-detected.
     * @param audioArtifactPath Relative path to persisted WAV artifact in container.
     * @param audioSha256 SHA-256 hash of the audio WAV artifact.
     * @return MeasurementResult Fully populated canonical measurement result.
     */
    static MeasurementResult measure(const MeasurementSpec& spec,
                                     const std::vector<float>& audioBuffer,
                                     double sampleRate,
                                     size_t noteOnSample,
                                     size_t noteOffSample,
                                     size_t detectedOnsetSample = 0,
                                     const std::string& audioArtifactPath = "",
                                     const std::string& audioSha256 = "");

    /**
     * @brief Extracts downsampled temporal envelope curve for plotting and persistence.
     * 
     * @param audioBuffer Audio samples.
     * @param sampleRate Audio sampling rate.
     * @param hopSamples Downsampling hop size (e.g. 128 samples).
     * @return MeasurementCurve Populated curve (time ms vs amplitude dBFS).
     */
    static MeasurementCurve extractTemporalCurve(const std::vector<float>& audioBuffer,
                                                 double sampleRate,
                                                 size_t hopSamples = 128);
};

} // namespace abdaudiolab::measurement
