/**
 * @file MeasurementContainerExporter.h
 * @brief Transactional FAIR experiment packager and HTML report generator for response measurements.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <juce_core/juce_core.h>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @struct FilterExportArtifacts
 * @brief Acoustic and derived artifacts for filter response measurement FAIR packaging.
 */
struct FilterExportArtifacts
{
    std::vector<float> capturedAudio;
    std::vector<float> stimulusAudio;     /**< Input audio sweep (for directTransferFunction auditing) */
    std::vector<float> impulseResponse;   /**< Deconvolved IR audio */
    double sampleRateHz { 48000.0 };

    juce::File capturedAudioWav;
    juce::File stimulusAudioWav;
    juce::File impulseResponseWav;
};

/**
 * @class MeasurementContainerExporter
 * @brief Exports measurement records into immutable FAIR containers with manifest.json and vector HTML reports.
 */
class MeasurementContainerExporter
{
public:
    /**
     * @brief Persists a complete measurement session into an immutable container with FAIR roles and SHA-256 hashes.
     * 
     * Registered Roles:
     * - measurement_spec
     * - measurement_stimulus
     * - envelope_curve
     * - measurement_baseline_audio
     * - measurement_result
     * - measurement_report
     * 
     * @param containerDir Target directory for the experiment container.
     * @param spec Canonical measurement specification.
     * @param result Canonical measurement outcome.
     * @param sourceAudioWav Optional source WAV file to copy into audio/ directory.
     * @param outError Diagnostic error string if operation fails.
     * @return true on success, false on error.
     */
    static bool exportMeasurement(const juce::File& containerDir,
                                  const MeasurementSpec& spec,
                                  const MeasurementResult& result,
                                  const juce::File& sourceAudioWav,
                                  juce::String& outError);

    /**
     * @brief Persists a complete filter measurement into an immutable FAIR container with manifest and vector reports.
     * 
     * Registered Roles:
     * - measurement_spec
     * - measurement_stimulus
     * - filter_response_curve
     * - measurement_captured_audio
     * - measurement_stimulus_audio (if present)
     * - measurement_impulse_response (if present)
     * - measurement_result
     * - measurement_report
     * 
     * @param containerDir Target directory for the experiment container.
     * @param spec Canonical measurement specification.
     * @param result Canonical measurement outcome.
     * @param artifacts Audio buffers or files for captured audio, stimulus and impulse response.
     * @param outError Diagnostic error string if operation fails.
     * @return true on success, false on error.
     */
    static bool exportFilterMeasurement(const juce::File& containerDir,
                                        const MeasurementSpec& spec,
                                        const MeasurementResult& result,
                                        const FilterExportArtifacts& artifacts,
                                        juce::String& outError);

    /**
     * @brief Persists a complete MIDI dynamics measurement into an immutable FAIR container with manifest.
     */
    static bool exportDynamicsMeasurement(const juce::File& containerDir,
                                          const MeasurementSpec& spec,
                                          const MeasurementResult& result,
                                          juce::String& outError);

    /**
     * @brief Persists a complete LFO modulation measurement into an immutable FAIR container with manifest.
     */
    static bool exportModulationMeasurement(const juce::File& containerDir,
                                            const MeasurementSpec& spec,
                                            const MeasurementResult& result,
                                            const juce::File& sourceAudioWav,
                                            juce::String& outError);

    /**
     * @brief Writes a single-channel or stereo PCM 16-bit WAV file from float samples.
     */
    static bool writeWavFile(const juce::File& file,
                             const std::vector<float>& samples,
                             double sampleRateHz,
                             int numChannels = 1);

    /**
     * @brief Generates self-contained HTML publication-grade report with vector curves and audio controls.
     * 
     * Adheres strictly to the rule: 'completed' status indicates observable measurement,
     * without generating false PASS/FAIL verdicts.
     * 
     * @param spec Source specification.
     * @param result Measurement result.
     * @param relativeAudioPath Relative URL path from HTML report to audio WAV file.
     * @return std::string Self-contained HTML string.
     */
    static std::string generateReportHtml(const MeasurementSpec& spec,
                                          const MeasurementResult& result,
                                          const std::string& relativeAudioPath = "");

    /**
     * @brief Generates self-contained HTML publication-grade report for filter measurements.
     */
    static std::string generateFilterReportHtml(const MeasurementSpec& spec,
                                                const MeasurementResult& result,
                                                const std::string& relCapturedAudio = "",
                                                const std::string& relStimulusAudio = "",
                                                const std::string& relImpulseResponse = "");

    /**
     * @brief Renders inline vector SVG line chart for temporal envelope response (time ms vs amplitude dBFS).
     * 
     * @param timeMs Vector of time stamps in ms.
     * @param amplitudeDbfs Vector of amplitude values in dBFS.
     * @param width SVG width in pixels.
     * @param height SVG height in pixels.
     * @return std::string Inline SVG string.
     */
    static std::string generateTemporalCurveSvg(const std::vector<double>& timeMs,
                                                const std::vector<double>& amplitudeDbfs,
                                                int width = 760,
                                                int height = 280);

    /**
     * @brief Renders inline vector SVG chart for frequency response (log frequency Hz vs magnitude dB).
     * 
     * @param frequenciesHz Vector of frequency points in Hz.
     * @param magnitudesDb Vector of magnitude values in dB.
     * @param slopeFit Optional metadata describing the asymptotic slope fit region and R^2.
     * @param cutoffHz Optional cutoff frequency marker in Hz.
     * @param width SVG width in pixels.
     * @param height SVG height in pixels.
     * @return std::string Inline SVG string.
     */
    static std::string generateFilterCurveSvg(const std::vector<double>& frequenciesHz,
                                              const std::vector<double>& magnitudesDb,
                                              const std::optional<SlopeFitMetadata>& slopeFit = std::nullopt,
                                              double cutoffHz = -1.0,
                                              int width = 760,
                                              int height = 280);
};

} // namespace abdaudiolab::measurement
