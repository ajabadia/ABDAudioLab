#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include <optional>

#include "IReportExportHost.h"
#include "../../export/ReportExportService.h"

namespace abdaudiolab::gui {

/**
 * @struct ExportExecutionToken
 * @brief Opaque token uniquely identifying an asynchronous export execution run.
 */
struct ExportExecutionToken
{
    std::uint64_t runId { 0 };
};

/**
 * @class ReportExportUiController
 * @brief UI presentation controller for report and production package export.
 *
 * Enforces strict thread isolation:
 * - Message Thread: builds immutable snapshot, handles UI feedback, dialogs and process launching.
 * - Background Worker: receives snapshot by value, invokes pure ReportExportService, stores results.
 * - AsyncUpdater: dispatches completion back to the Message Thread.
 *
 * Guarantees zero worker leaks and safe destruction via explicit thread join and token invalidation.
 */
class ReportExportUiController : private juce::AsyncUpdater
{
public:
    explicit ReportExportUiController(IReportExportHost& host);
    ~ReportExportUiController() override;

    ReportExportUiController(const ReportExportUiController&) = delete;
    ReportExportUiController& operator=(const ReportExportUiController&) = delete;

    /**
     * @brief Initiates an asynchronous ProductionPackage export.
     * @return true if export started, false if an export was already in progress or snapshot failed.
     */
    bool requestExportProductionPackage();

    /**
     * @brief Initiates an asynchronous CertificationReport HTML export.
     * @return true if export started, false if an export was already in progress or snapshot failed.
     */
    bool requestExportCertificationReport();

    /**
     * @brief Synchronously calculates metrics preview from current host snapshot on message thread.
     */
    void updateMetricsPreview();

    /**
     * @brief Flushes any pending async update callbacks synchronously on the message thread.
     */
    void flushAsyncUpdates();

    /**
     * @brief Opens the published export directory in system file explorer.
     */
    void openExportFolderInExplorer();

    /**
     * @brief Opens the published HTML Certification Report in default viewer/browser.
     */
    void openCertificationReportHtml();

    /**
     * @brief Returns true if a background export operation is currently running.
     */
    [[nodiscard]] bool isExportInProgress() const noexcept;

private:
    void handleAsyncUpdate() override;

    struct CompletedResult {
        ExportExecutionToken token;
        exporting::ReportExportResult result;
        exporting::ReportExportOptions options;
        juce::String baseFileName;
        juce::File destinationDirectory;
    };

    struct SharedWorkerState {
        std::atomic<bool> cancelled { false };
        std::atomic<std::uint64_t> currentRunId { 0 };
        std::mutex queueMutex;
        std::vector<CompletedResult> completedResults;
    };

    bool launchExportAsync(exporting::ReportExportOptions options);

    IReportExportHost& host;
    std::atomic<bool> inProgress { false };
    std::shared_ptr<SharedWorkerState> sharedState;
    std::thread workerThread;
};

} // namespace abdaudiolab::gui
