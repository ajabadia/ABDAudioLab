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
     * - measurement_result
     * - envelope_curve
     * - measurement_baseline_audio
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
};

} // namespace abdaudiolab::measurement
