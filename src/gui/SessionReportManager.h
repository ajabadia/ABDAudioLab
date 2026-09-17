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

namespace abdaudiolab {
namespace gui {

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
