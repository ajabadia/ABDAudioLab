/**
 * @file MeasurementComparisonReportGenerator.h
 * @brief Standalone generator for self-contained multi-container FAIR/LNL comparison HTML reports.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../gui/measurement/MeasurementComparisonSession.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::measurement
{

class MeasurementComparisonReportGenerator
{
public:
    /**
     * @brief Generates a comprehensive, self-contained HTML comparison report.
     * 
     * @param session The comparison session containing verified, excluded, or corrupt containers.
     * @param destinationFile The target HTML file to write.
     * @param outError Diagnostic error string if generation fails.
     * @return true if written successfully.
     */
    static bool generateReport(const abdaudiolab::gui::measurement::MeasurementComparisonSession& session,
                               const juce::File& destinationFile,
                               juce::String& outError);

    /**
     * @brief Generates the full HTML string in memory.
     */
    static juce::String generateReportHtml(const abdaudiolab::gui::measurement::MeasurementComparisonSession& session);
};

} // namespace abdaudiolab::measurement
