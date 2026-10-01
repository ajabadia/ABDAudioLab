/**
 * @file MainContentComponentPersistence.cpp
 * @brief Session persistence and file I/O delegation.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"

namespace abdaudiolab
{

// ==============================================================================
// POR QUE ESTO ESTA EN UN FICHERO PROPIO Y NO EN MainContentComponent.cpp.
//
// Porque el fichero eran 3925 lineas con 80 metodos. En un fichero asi el numero
// de un metodo no dice nada: hay que recorrerlo entero para saber si esta en esta
// seccion o en la siguiente. Leer no es el problema, modificar si: cambiar diez
// lineas de un metodo obliga a recorrer 4000 lineas, y el metodo que toca acaba
// en el sitio menos probable de donde estabas mirando.
//
// No se ha movido ni una linea de codigo: los mismos cuerpos de funcion, en la
// misma clase, enlazados igual. Lo unico que cambia es el fichero donde viven, y
// eso se comprueba compilando.
// ==============================================================================

// ==============================================================================
// SECTION 10: SESSION PERSISTENCE & FILE I/O DELEGATION
// Owns session file chooser dialogs (Save, Save As, Open, New) and session reset confirmations.
// JSON serialization/deserialization delegated to SessionSerializer and SessionIoController.
// ==============================================================================
void MainContentComponent::promptNewSession()
{
    sessionIoController.promptNewSession(this, [this] {
        performNewSessionReset();
    });
}

void MainContentComponent::performNewSessionReset()
{
    suiteList.clearQueue();
    curvePlotter.clear();
    sessionManager.resetSession();
    sessionManager.cleanupTempSession();
    totalPointsMeasured = 0;
    drawer.setHardwareLocked(false);
    drawer.clearSelectedHardware();
    catalogSelector.setHardwareLocked(false);
    catalogSelector.resetSelection();
    mainHeader.clearHardware();
    suiteList.setStandardTestAvailable(false);

    // Disconnect and release active plugin instance
    pluginUiCoordinator.unloadPlugin();

    // Reset session summary in sidebar stepper
    gui::SoundIdSidebarStepper::SessionSummaryInfo emptySummary;
    sidebarStepper.setSessionSummary(emptySummary);

    // Reset target in profilingSessionController and targetView
    gui::session::TargetSelectionState emptyTarget;
    profilingSessionController.selectTarget(emptyTarget);
    targetView.updateFromSnapshot(profilingSessionController.getCurrentSnapshot());

    // Reset workflow stepper navigation via WorkflowNavigationController
    workflowNavController.resetToNewSession();

    manualPromptLabel.setText(juce::String::fromUTF8(u8"Nueva sesi\u00f3n inicializada. Seleccione el dispositivo y objetivo a medir."), juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
    resized();
}

void MainContentComponent::handleOpenSession()
{
    sessionIoController.handleOpenSession(this);
}

void MainContentComponent::performOpenSessionFileChooser()
{
    sessionIoController.handleOpenSession(this);
}

void MainContentComponent::promptDeleteTest(int index, const gui::QueueItem& item)
{
    sessionIoController.promptDeleteTest(this, index, item,
        [this](int idx) {
            suiteList.removeTestDirectly(idx);
            sessionManager.setDirty(true);
        },
        [this](int idx) {
            suiteList.invalidateTest(idx);
            sessionManager.setDirty(true);
        }
    );
}

void MainContentComponent::confirmAndExit()
{
    sessionIoController.confirmAndExit(this, [] {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });
}

void MainContentComponent::initializeAutoUpdater()
{
    autoUpdater = std::make_unique<ABDShared::AutoUpdater>(config::getAutoUpdaterConfig());
    autoUpdater->setUpdateCallback([this](const ABDShared::AutoUpdater::UpdateInfo& info, bool isManualCheck) {
        juce::MessageManager::callAsync([this, info, isManualCheck]() {
            if (aboutSplashWindow != nullptr)
            {
                if (autoUpdater->isUpdateAvailable())
                    aboutSplashWindow->setStatus("New update available: v" + info.version);
                else
                    aboutSplashWindow->setStatus("Up to date: v" + autoUpdater->getCurrentVersion());
            }

            if (autoUpdater->isUpdateAvailable())
            {
                juce::String msg = "A new version of ABDAudioLab is available!\n\n"
                                   "Latest Version: " + info.version + "\n"
                                   "Current Version: " + autoUpdater->getCurrentVersion() + "\n\n"
                                   "Would you like to open the GitHub releases download page?";

                confirmationModal.show(
                    this,
                    "New Update Available",
                    msg,
                    "Download Update",
                    "",
                    "Later",
                    [info](gui::ConfirmationModalDialog::Result result) {
                        if (result == gui::ConfirmationModalDialog::Result::Primary)
                        {
                            if (info.downloadUrl.isNotEmpty())
                                juce::URL(info.downloadUrl).launchInDefaultBrowser();
                            else
                                juce::URL("https://github.com/ajabadia/ABDAudioLab/releases").launchInDefaultBrowser();
                        }
                    }
                );
            }
            else
            {
                // Si ya está actualizado, no se pregunta ni interrumpe al usuario con un diálogo modal.
                if (aboutSplashWindow != nullptr)
                    aboutSplashWindow->setStatus("Up to date: v" + autoUpdater->getCurrentVersion());
            }
        });
    });

    // Run background check on launch
    autoUpdater->checkForUpdates(false);
}

void MainContentComponent::checkForAppUpdates(bool isManual)
{
    if (autoUpdater != nullptr)
    {
        autoUpdater->checkForUpdates(isManual);
    }
}

void MainContentComponent::openAudioABVerificationModal()
{
    const auto& pts = sessionManager.getMeasuredPoints();
    if (!pts.empty())
    {
        const auto& lastPt = pts.back();
        if (!lastPt.irSamples.empty())
        {
            juce::AudioBuffer<float> refBuf(1, static_cast<int>(lastPt.irSamples.size()));
            std::copy(lastPt.irSamples.begin(), lastPt.irSamples.end(), refBuf.getWritePointer(0));
            double sRate = audioEngine.getCurrentSampleRate();
            if (sRate <= 0.0) sRate = 44100.0;
            abVerificationModal.setReferenceSignal(refBuf, sRate, "Hardware Capture (Point #" + juce::String(pts.size()) + ")");
        }
    }
    abVerificationModal.showDialog(this);
}

} // namespace abdaudiolab
