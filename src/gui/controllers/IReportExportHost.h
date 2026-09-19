#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <filesystem>
#include <string>

#include "../../core/ProfilingSession.h"
#include "../../export/ReportExportService.h"

namespace abdaudiolab::gui {

/**
 * @struct ReportExportSnapshot
 * @brief Immutable value snapshot captured on the JUCE Message Thread prior to export.
 * Strictly free of any GUI widgets, managers, or mutable references.
 */
struct ReportExportSnapshot
{
    core::SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> measuredPoints;
    std::filesystem::path exportDirectory;
    std::string baseFileName;
    double sampleRate { 44100.0 };
    float inputTrimDb { 0.0f };
    std::string operatorNotes;
    double ambientTemperatureC { 21.0 };
    double warmupMinutes { 15.0 };
};

/**
 * @class IReportExportHost
 * @brief Abstract presentation port decoupling ReportExportUiController from MainContentComponent.
 * All presentation mutators and notifications must be executed strictly on the JUCE Message Thread.
 */
class IReportExportHost
{
public:
    virtual ~IReportExportHost() = default;

    // Snapshot capture (executed strictly on the Message Thread)
    virtual ReportExportSnapshot createReportExportSnapshot() const = 0;

    // Presentation actions & feedback (executed strictly on the Message Thread)
    virtual void showStatusBanner(const juce::String& message, bool isError = false) = 0;
    virtual void showMessageBox(const juce::String& title, const juce::String& message, bool isError) = 0;
    virtual void updateExportReportMetrics(const exporting::CalculatedSessionMetrics& metrics) = 0;
    virtual void notifyExportSuccess(const juce::File& destinationDir, const juce::String& baseName) = 0;
    virtual void showPanelStatus(const juce::String& statusMessage, bool isWarning = false) = 0;
    virtual void launchProcess(const juce::File& file) = 0;
    virtual void revealInFolder(const juce::File& folder) = 0;
};

} // namespace abdaudiolab::gui
