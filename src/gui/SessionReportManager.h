/**
 * @file SessionReportManager.h
 * @brief Manages asynchronous file dialogs, background checkpoint auto-save,
 *        and multi-target report export (C++ LUT, JSON, HTML Certification) for ABDAudioLab.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <functional>
#include <vector>

#include "../core/ProfilingSession.h"
#include "../core/SessionManager.h"
#include "../export/LutExporter.h"
#include "../export/CertificationReportExporter.h"
#include "../dsp/LutEvaluatorSimd.h"

namespace abdaudiolab {
namespace gui {

/**
 * @brief Output formats supported by SessionReportManager.
 */
enum class ReportFormat
{
    HtmlCertification,
    CppLutHeader,
    JsonTelemetry,
    ProductionPackage,   // Atomic multi-target export (C++, JSON, HTML, Manifest with SHA-256)
    AuditionLutGrid      // 8x8 evaluation LUT
};

/**
 * @brief Explicit status outcomes for report export operations.
 */
enum class ExportStatus
{
    Succeeded,
    Cancelled,
    ValidationFailed,
    WriteFailed,
    UnsupportedFormat
};

/**
 * @brief Structured request describing an export target.
 */
struct ReportExportRequest
{
    ReportFormat format{ ReportFormat::HtmlCertification };
    juce::File destination;
    juce::String baseName;
    juce::String hardwareId;
    juce::String hardwareName;
    juce::String functionId;
    juce::String functionName;
    juce::String deviceType;
    double sampleRate{ 44100.0 };
    juce::String operatorNotes;
    double ambientTemperatureC{ 21.0 };
    double warmupTimeMinutes{ 15.0 };
    bool includeRawAudio{ false };
    bool includeDerivedData{ true };
};

/**
 * @brief Rich result describing export outcome, artifacts produced, and SHA-256 fixity.
 */
struct ReportExportResult
{
    ExportStatus status{ ExportStatus::ValidationFailed };
    bool succeeded{ false };
    juce::File outputPath;
    std::string manifestSha256;
    std::vector<juce::File> artifactPaths;
    juce::String errorCode;
    juce::String userMessage;
};

/**
 * @class SessionReportManager
 * @brief Encapsulates asynchronous file chooser dialogues and report generation,
 *        decoupling file I/O and export orchestration from MainContentComponent.
 */
class SessionReportManager
{
public:
    SessionReportManager() = default;
    ~SessionReportManager() = default;

    /**
     * @brief Exports report artifacts synchronously according to the request.
     * Uses atomic staging for multi-file exports (ProductionPackage).
     */
    ReportExportResult exportReport(const ReportExportRequest& request,
                                    const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Asynchronously launches export on a worker thread with progress/completion callback.
     */
    void exportReportAsync(const ReportExportRequest& request,
                           const std::vector<exporting::MeasuredPoint>& points,
                           std::function<void(const ReportExportResult&)> onComplete);

    /**
     * @brief Computes 8x8 audition LUT table from measured points.
     */
    static std::vector<dsp::AbdBatchedPoint> buildAuditionLutGrid(
        const std::vector<exporting::MeasuredPoint>& sessionPoints,
        int gridSize = 8);

    /**
     * @brief Computes SNR, THD, noise floor and duration statistics for export display.
     */
    static void calculateSessionMetrics(
        const std::vector<exporting::MeasuredPoint>& points,
        float inputTrim,
        float& outAvgSnr,
        float& outNoiseFloor,
        float& outAvgThd,
        int& outCount,
        float& outDurationSec);

    /**
     * @brief Asynchronously launches native FileChooser to save session package (.abdlabtest).
     */
    void triggerSaveSessionAsync(const juce::File& initialLocation,
                                 const juce::String& defaultName,
                                 std::function<void(const juce::File& chosenFile)> onFileChosen);

    /**
     * @brief Asynchronously launches native FileChooser to load existing session package.
     */
    void triggerLoadSessionAsync(const juce::File& initialLocation,
                                 std::function<void(const juce::File& chosenFile)> onFileChosen);

    /**
     * @brief Asynchronously prompts operator for export folder destination.
     */
    void triggerSelectExportFolderAsync(const juce::File& initialFolder,
                                        std::function<void(const juce::File& chosenDir)> onDirectoryChosen);

    /**
     * @brief Exports synchronized C++ LUT header and JSON diagnostic report.
     */
    bool exportLutAndJsonReports(const juce::File& destinationDir,
                                 const juce::String& baseName,
                                 const core::ProfilingMetadata& metadata,
                                 const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Exports HTML certification audit report.
     */
    bool exportCertificationHtmlReport(const juce::File& destinationDir,
                                       const juce::String& baseName,
                                       const exporting::SessionManifestData& manifest,
                                       const std::vector<exporting::MeasuredPoint>& points,
                                       juce::File& outHtmlFile);

    /**
     * @brief Abre de forma robusta un informe HTML en el visor predeterminado del sistema operativo (DRY).
     * Utiliza startAsProcess() con fallback a launchInDefaultBrowser(), sin borrar el archivo temporal.
     */
    static bool launchHtmlReportInDefaultViewer(const juce::File& reportFile, juce::String& outError);

    /**
     * @brief Saves a crash-recovery checkpoint via SessionManager.
     */
    void triggerPeriodicAutoSaveCheckpoint(core::SessionManager& sessionManager,
                                           const core::SessionManifest& currentManifest);

    /**
     * @brief Checks if a recoverable auto-saved session exists on disk.
     */
    [[nodiscard]] juce::File checkRecoverableSession(const core::SessionManager& sessionManager) const;

private:
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastExportDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionReportManager)
};

} // namespace gui
} // namespace abdaudiolab
