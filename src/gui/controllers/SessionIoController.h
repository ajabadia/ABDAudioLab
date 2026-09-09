/**
 * @file SessionIoController.h
 * @brief Autonomous controller orchestrating session loading, saving, package export and publication.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

#include "../../core/SessionManager.h"
#include "../../core/ProfilingSession.h"
#include "../../export/LutExporter.h"
#include "../../export/CertificationReportExporter.h"
#include "../SessionReportManager.h"
#include "../ConfirmationModalDialog.h"
#include "../ExportReportPanel.h"

namespace abdaudiolab::gui
{

/**
 * @class SessionIoController
 * @brief Handles all disk I/O, package serialization (.abdlabtest), production export, and cloud publication.
 */
class SessionIoController
{
public:
    SessionIoController(core::SessionManager& sessionMgr,
                        SessionReportManager& reportMgr,
                        ExportReportPanel& exportPanel,
                        ConfirmationModalDialog& confirmModal);
    ~SessionIoController() = default;

    void setExportDirectory(const juce::File& dir) { exportDirectory = dir; }
    [[nodiscard]] const juce::File& getExportDirectory() const noexcept { return exportDirectory; }

    // Session Operations
    void handleOpenSession(juce::Component* modalParent);
    void handleSaveSession(const core::SessionManifest& manifest,
                           const core::ProfilingMetadata& meta);
    void handleSaveSessionAs(const core::SessionManifest& manifest,
                             const core::ProfilingMetadata& meta,
                             const juce::String& defaultName);

    void saveSessionToFile(const juce::File& file,
                           const core::SessionManifest& manifest,
                           const core::ProfilingMetadata& meta);

    // Production & Report Export
    void exportProductionPackage(const juce::String& hwId,
                                 const juce::String& funcId,
                                 const core::ProfilingMetadata& meta,
                                 const exporting::SessionManifestData& manifestData);

    void exportCertificationReport(const juce::String& hwId,
                                   const juce::String& funcId,
                                   const exporting::SessionManifestData& manifestData);

    void openCertificationReportHtml(const juce::String& hwId, const juce::String& funcId);
    void publishCertificationToCloud();

    // Callbacks
    std::function<void(const core::SessionManifest& manifest, const std::vector<exporting::MeasuredPoint>& points)> onSessionLoaded;
    std::function<void(const juce::File& savedFile)> onSessionSaved;
    std::function<void(const juce::String& message, bool isError)> onStatusNotification;

private:
    void performOpenSessionFileChooser();

    core::SessionManager& sessionManager;
    SessionReportManager& sessionReportManager;
    ExportReportPanel& exportReportPanel;
    ConfirmationModalDialog& confirmationModal;

    juce::File exportDirectory;
};

} // namespace abdaudiolab::gui
