/**
 * @file MeasurementReportGenerator.h
 * @brief Standalone HTML report generator for response measurements.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class MeasurementReportGenerator
 * @brief Generates self-contained, publication-grade HTML reports with vector curves and audio controls.
 */
class MeasurementReportGenerator
{
public:
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
};

} // namespace abdaudiolab::measurement
