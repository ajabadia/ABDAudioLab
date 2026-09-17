/**
 * @file FilterMeasurementAdapter.h
 * @brief Adapter coordinating FarinaDeconvolver and FilterAnalytics into canonical response-measurement-1.0 contracts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../MeasurementContracts.h"
#include "../../math/analytics/FilterAnalytics.h"
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class FilterMeasurementAdapter
 * @brief Adapter translating Farina log-sine sweep deconvolution and FilterAnalytics into canonical response-measurement-1.0 contracts.
 * 
 * Strict non-duplication:
 * - Delegates all sweep generation, inverse filtering, and harmonic impulse extraction to FarinaDeconvolver.
 * - Delegates all metrological analysis (cutoff, resonance, Q, asymptotic slope) to FilterAnalytics.
 * - Separates direct transfer functions (audio-in) from observed spectral responses under declared MIDI conditions.
 * - Enforces explicit observability without generating false PASS verdicts.
 */
class FilterMeasurementAdapter
{
public:
    static constexpr const char* kAnalyzerName = "FarinaDeconvolver/FilterAnalytics";
    static constexpr const char* kAnalyzerVersion = "1.0.0";

    /**
     * @brief Measures filter response from captured audio using Farina deconvolution.
     * 
     * @param spec Measurement specification.
     * @param capturedAudio Recorded audio buffer from device under test.
     * @param sampleRate Operating sample rate in Hz.
     * @param audioArtifactPath Relative path to persisted WAV artifact in container.
     * @param audioSha256 SHA-256 hash of the audio WAV artifact.
     * @return MeasurementResult Canonical result record.
     */
    static MeasurementResult measure(const MeasurementSpec& spec,
                                     const std::vector<float>& capturedAudio,
                                     double sampleRate,
                                     const std::string& audioArtifactPath = "",
                                     const std::string& audioSha256 = "");

    /**
     * @brief Measures filter response directly from pre-computed deconvolution result.
     */
    static MeasurementResult measureFromDeconvolution(
        const MeasurementSpec& spec,
        const math::DeconvolutionResult& deco,
        double sampleRate,
        const std::string& audioArtifactPath = "",
        const std::string& audioSha256 = "");
};

} // namespace abdaudiolab::measurement
