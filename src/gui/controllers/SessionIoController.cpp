/**
 * @file SessionIoController.cpp
 * @brief Implementation of autonomous session I/O, package serialization and production export.
 * @author ABDSynths
 * @date 2026
 */

#include "SessionIoController.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

SessionIoController::SessionIoController(core::SessionManager& sessionMgr,
                                         SessionReportManager& reportMgr,
                                         ExportReportPanel& exportPanel,
                                         ConfirmationModalDialog& confirmModal)
    : sessionManager(sessionMgr),
      sessionReportManager(reportMgr),
      exportReportPanel(exportPanel),
      confirmationModal(confirmModal)
{
    exportDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("exported_luts");
    exportDirectory.createDirectory();
}

void SessionIoController::handleOpenSession(juce::Component* modalParent)
{
    if (sessionManager.isDirty())
    {
        confirmationModal.show(
            modalParent,
            "Unsaved Changes",
            "The current session contains unsaved changes.\nDo you want to save before opening another session?",
            "Save",
            "Don't Save",
            "Cancel",
            [this](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    if (sessionManager.getActiveSessionFile().existsAsFile())
                    {
                        if (saveSessionToFile(sessionManager.getActiveSessionFile()))
                            performOpenSessionFileChooser();
                    }
                    else
                    {
                        juce::String suggested = (sessionContextProvider ? sessionContextProvider().suggestedFileName : "session.abdlabtest");
                        sessionReportManager.triggerSaveSessionAsync(exportDirectory, suggested, [this](const juce::File& file) {
                            if (file != juce::File())
                            {
                                if (saveSessionToFile(file))
                                    performOpenSessionFileChooser();
                            }
                        });
                    }
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    performOpenSessionFileChooser();
                }
                // Cancel: do nothing
            }
        );
    }
    else
    {
        performOpenSessionFileChooser();
    }
}

void SessionIoController::performOpenSessionFileChooser()
{
    sessionReportManager.triggerLoadSessionAsync(exportDirectory, [this](const juce::File& file) {
        if (file.existsAsFile())
        {
            juce::String err;
            if (sessionManager.loadSessionFromPackage(file, err))
            {
                sessionManager.setActiveSessionFile(file);
                sessionManager.setDirty(false);
                if (onSessionLoaded)
                    onSessionLoaded(sessionManager.getManifest(), sessionManager.getMeasuredPoints());
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Failed to Load Session",
                    "Could not load session package:\n" + file.getFullPathName() + "\n\nError: " + err,
                    "OK"
                );
            }
        }
    });
}

void SessionIoController::handleSaveSession()
{
    if (sessionManager.getActiveSessionFile().existsAsFile())
    {
        saveSessionToFile(sessionManager.getActiveSessionFile());
        return;
    }

    juce::String defaultName;
    if (sessionContextProvider)
    {
        auto ctx = sessionContextProvider();
        defaultName = ctx.suggestedFileName;
    }
    if (defaultName.isEmpty())
        defaultName = "session.abdlabtest";

    handleSaveSessionAs(defaultName);
}

void SessionIoController::handleSaveSessionAs(const juce::String& defaultName)
{
    juce::String suggested = defaultName;
    if (suggested.isEmpty() && sessionContextProvider)
    {
        auto ctx = sessionContextProvider();
        suggested = ctx.suggestedFileName;
    }
    if (suggested.isEmpty())
        suggested = "session.abdlabtest";

    sessionReportManager.triggerSaveSessionAsync(exportDirectory, suggested, [this](const juce::File& file) {
        if (file != juce::File())
            saveSessionToFile(file);
    });
}

bool SessionIoController::saveSessionToFile(const juce::File& file)
{
    if (!sessionContextProvider)
    {
        if (onStatusNotification)
            onStatusNotification("Error: No session context provider configured for save.", true);
        return false;
    }

    auto ctx = sessionContextProvider();
    return saveSessionToFile(file, ctx.manifest, ctx.metadata);
}

void SessionIoController::handleSaveSession(const core::SessionManifest& manifest,
                                            const core::ProfilingMetadata& meta)
{
    if (sessionManager.getActiveSessionFile().existsAsFile())
    {
        saveSessionToFile(sessionManager.getActiveSessionFile(), manifest, meta);
        return;
    }
    handleSaveSessionAs(manifest, meta, juce::String(meta.hardwareName).replaceCharacter(' ', '_') + ".abdlabtest");
}

void SessionIoController::handleSaveSessionAs(const core::SessionManifest& manifest,
                                              const core::ProfilingMetadata& meta,
                                              const juce::String& defaultName)
{
    sessionReportManager.triggerSaveSessionAsync(exportDirectory, defaultName, [this, manifest, meta](const juce::File& file) {
        if (file != juce::File())
            saveSessionToFile(file, manifest, meta);
    });
}

bool SessionIoController::saveSessionToFile(const juce::File& file,
                                            const core::SessionManifest& manifest,
                                            const core::ProfilingMetadata& meta)
{
    bool ok = sessionManager.saveSessionToPackage(file, manifest);
    if (ok)
    {
        sessionManager.setActiveSessionFile(file);
        sessionManager.setDirty(false);

        const auto& sessionPoints = sessionManager.getMeasuredPoints();
        juce::String baseName = file.getFileNameWithoutExtension();

        sessionReportManager.exportLutAndJsonReports(exportDirectory, baseName, meta, sessionPoints);

        if (onStatusNotification)
            onStatusNotification("Session package saved successfully: " + file.getFileName(), false);

        if (onSessionSaved)
            onSessionSaved(file);

        return true;
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Save Failed",
            "Could not write session package to:\n" + file.getFullPathName() + "\n\nPlease check disk space and folder permissions.",
            "OK"
        );
        return false;
    }
}

void SessionIoController::promptNewSession(juce::Component* modalParent, std::function<void()> onPerformReset)
{
    if (sessionManager.isDirty())
    {
        confirmationModal.show(
            modalParent,
            "Unsaved Changes",
            "The current session contains unsaved changes.\nDo you want to save before creating a new session?",
            "Save",
            "Don't Save",
            "Cancel",
            [this, onPerformReset](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    if (sessionManager.getActiveSessionFile().existsAsFile())
                    {
                        if (saveSessionToFile(sessionManager.getActiveSessionFile()))
                        {
                            if (onPerformReset) onPerformReset();
                        }
                    }
                    else
                    {
                        juce::String suggested = (sessionContextProvider ? sessionContextProvider().suggestedFileName : "session.abdlabtest");
                        sessionReportManager.triggerSaveSessionAsync(exportDirectory, suggested, [this, onPerformReset](const juce::File& file) {
                            if (file != juce::File())
                            {
                                if (saveSessionToFile(file))
                                {
                                    if (onPerformReset) onPerformReset();
                                }
                            }
                        });
                    }
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    if (onPerformReset) onPerformReset();
                }
                // Cancel: do nothing
            }
        );
    }
    else
    {
        if (onPerformReset) onPerformReset();
    }
}

void SessionIoController::confirmAndExit(juce::Component* modalParent, std::function<void()> onProceedExit)
{
    if (sessionManager.isDirty())
    {
        confirmationModal.show(
            modalParent,
            "Exit ABDAudioLab",
            "You have unsaved changes in this session. Do you want to save before exiting?",
            "Save and Exit",
            "Exit Without Saving",
            "Cancel",
            [this, onProceedExit](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    if (sessionManager.getActiveSessionFile().existsAsFile())
                    {
                        if (saveSessionToFile(sessionManager.getActiveSessionFile()))
                        {
                            if (onProceedExit) onProceedExit();
                        }
                    }
                    else
                    {
                        juce::String suggested = (sessionContextProvider ? sessionContextProvider().suggestedFileName : "session.abdlabtest");
                        sessionReportManager.triggerSaveSessionAsync(exportDirectory, suggested, [this, onProceedExit](const juce::File& file) {
                            if (file != juce::File())
                            {
                                if (saveSessionToFile(file))
                                {
                                    if (onProceedExit) onProceedExit();
                                }
                            }
                        });
                    }
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    if (onProceedExit) onProceedExit();
                }
                // Cancel: do nothing
            }
        );
    }
    else
    {
        if (onProceedExit) onProceedExit();
    }
}

void SessionIoController::promptDeleteTest(juce::Component* modalParent,
                                           int index,
                                           const gui::QueueItem& item,
                                           std::function<void(int)> onDiscard,
                                           std::function<void(int)> onInvalidate)
{
    if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
    {
        confirmationModal.show(
            modalParent,
            "Delete Measured Test",
            "Test '" + item.title + "' contains recorded measurement data.\n\nWhat would you like to do?",
            "Discard & Delete",
            "Mark as Invalid (Keep)",
            "Cancel",
            [index, onDiscard, onInvalidate](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    if (onDiscard) onDiscard(index);
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    if (onInvalidate) onInvalidate(index);
                }
                // Cancel: do nothing
            }
        );
    }
    else
    {
        if (onDiscard) onDiscard(index);
    }
}


void SessionIoController::publishCertificationToCloud()
{
    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"☁ Conectando con Servidor Cloud Comunitario..."));

    juce::Thread::launch([dirPath = exportDirectory.getFullPathName(), this]() {
        juce::ChildProcess proc;
        juce::StringArray args;
        args.add("python");
        args.add("-m");
        args.add("abdaudiolab.scripts.publish_to_cloud");
        args.add(dirPath);

        if (proc.start(args))
        {
            proc.waitForProcessToFinish(10000);
            int exitCode = proc.getExitCode();

            juce::MessageManager::callAsync([this, exitCode]() {
                if (exitCode == 0)
                {
                    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"✓ ¡Certificación publicada con éxito en la Nube Comunitaria! (CERTIFIED_GOLD)"));
                    if (onStatusNotification)
                        onStatusNotification(juce::String::fromUTF8(u8"✓ Publicación Cloud completada: Bundle .tar.gz verificado e indexado."), false);
                }
                else
                {
                    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"⚠️ API Cloud: Servidor no disponible o bundle rechazado. Revisa la consola."), true);
                }
            });
        }
    });
}

} // namespace abdaudiolab::gui
