/**
 * @file MainContentComponentProfilingSession.cpp
 * @brief Profiling session orchestration: triggers, manual steps and session building.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"
#include "core/ProfilingSessionBuilder.h"

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
// SECTION 8: PROFILING SESSION ORCHESTRATION (EXECUTION & GUARDS)
// Owns UI profiling triggers (Start/Pause/Cancel), manual step prompts, and session progress dispatch.
// Execution state machine, cancellation, and re-arming delegated to SessionExecutionCoordinator; test stimuli to Sequencer.
// ==============================================================================
void MainContentComponent::startProfilingSession(bool resumeFromExisting)
{
    if (sessionCoordinator.isRunningSession())
        return;

    if (suiteList.getQueueSize() <= 0)
    {
        manualPromptLabel.setText("Please add at least one test to the Session Plan before starting.", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        meterStrip.setProfilingActive(false);
        return;
    }

    juce::String selectedHwId = drawer.getSelectedHardwareId();
    juce::String selectedFuncId = drawer.getSelectedFunctionId();
    const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());

    std::string modeStr = "MOCK_DSP";
    std::string hwName = "MOCK_VA_SYNTH";

    if (contract != nullptr)
    {
        modeStr = contract->deviceType;
        hwName = contract->id;
    }
    else
    {
        audioEngine.setMockHardware(nullptr);
    }

    sessionCoordinator.setHardwareContext(&hardwareManager, selectedHwId);

    if (!resumeFromExisting)
    {
        // Silence any lingering active notes from previous session
        sessionCoordinator.silenceAllNotes();
        for (int ch = 1; ch <= 16; ++ch)
        {
            audioEngine.postLiveMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }

        suiteList.resetAllStatuses();
        suiteList.updateItemStatus(0, gui::QueueItemStatus::Running, 0);
        curvePlotter.clear();
        curvePlotter.clearPreScanData();
        sessionManager.clearMeasuredPoints();
        totalPointsMeasured = 0;

        sessionCoordinator.rearmSession();
    }

    suiteList.setSessionRunning(true);

    core::ProfilingSession currentProfilingSession = buildProfilingSessionFromQueue(hwName, modeStr);
    juce::String baseName = juce::String(hwName) + "_" + selectedFuncId;

    if (resumeFromExisting && sessionManager.hasPoints())
    {
        auto allTestCases = currentProfilingSession.getTestCases();
        size_t alreadyMeasured = sessionManager.getPointCount();
        if (alreadyMeasured < allTestCases.size())
        {
            std::vector<core::TestCase> remainingTestCases(allTestCases.begin() + alreadyMeasured, allTestCases.end());
            currentProfilingSession.setTestCases(remainingTestCases);
        }
    }

    meterStrip.setProfilingActive(true);
    sessionCoordinator.triggerStartSession(currentProfilingSession, exportDirectory, baseName, resumeFromExisting);
    updateGovernanceUi();
    resized();
}

void MainContentComponent::stopProfilingSession()
{
    sessionCoordinator.triggerStopSession();

    // Silence any active live notes
    for (int ch = 1; ch <= 16; ++ch)
    {
        audioEngine.postLiveMidiMessage(juce::MidiMessage::allNotesOff(ch));
    }

    meterStrip.setProfilingActive(false);
    updateGovernanceUi();
    resized();
}

void MainContentComponent::confirmManualStep()
{
    sessionCoordinator.confirmOperatorStep();
}

void MainContentComponent::openAudioMidiSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(
        audioEngine.getDeviceManager(),
        0, 2,
        0, 2,
        true,
        true,
        false,
        false
    );
    selector->setLookAndFeel(&soundIdTheme);
    selector->setSize(520, 520);

    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(selector);
    opt.dialogTitle = "Audio & MIDI Configuration";
    opt.dialogBackgroundColour = gui::SoundIdTheme::bgLight;
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
}

void MainContentComponent::showAboutDialog()
{
    if (aboutSplashWindow == nullptr)
    {
        aboutSplashWindow = std::make_unique<gui::SoundIdSplashWindow>(true);
        aboutSplashWindow->setStatus("All Systems Nominal", 1.0f);
        aboutSplashWindow->onCloseRequest = [this] {
            aboutSplashWindow.reset();
        };
        aboutSplashWindow->onCheckUpdates = [this] {
            if (aboutSplashWindow != nullptr)
                aboutSplashWindow->setStatus("Checking for updates...");
            checkForAppUpdates(true);

            juce::Timer::callAfterDelay(3500, [this, safeWindow = juce::Component::SafePointer<juce::Component>(aboutSplashWindow.get())]() {
                if (safeWindow != nullptr && aboutSplashWindow != nullptr)
                {
                    if (!autoUpdater || !autoUpdater->isUpdateAvailable())
                    {
                        juce::String ver = autoUpdater ? autoUpdater->getCurrentVersion() : juce::String(version::kAppVersion);
                        aboutSplashWindow->setStatus("Up to date (v" + ver + ")");
                    }
                }
            });
        };
    }
    else
    {
        aboutSplashWindow->toFront(true);
    }
}

core::ProfilingSession MainContentComponent::buildProfilingSessionFromQueue(const std::string& hwName, const std::string& modeStr)
{
    core::SessionMetadataConfig metaConfig;
    metaConfig.hardwareName = hwName;
    metaConfig.targetModule = drawer.getSelectedFunctionId().toStdString();
    metaConfig.operatorMode = modeStr;
    metaConfig.sampleRate = audioEngine.getSampleRate();
    metaConfig.bitDepth = 24;
    metaConfig.timestampIso8601 = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    metaConfig.operatorNotes = drawer.getOperatorNotes().toStdString();
    metaConfig.ambientTemperatureC = drawer.getAmbientTemperature();
    metaConfig.warmupTimeMinutes = drawer.getWarmupTimeMinutes();

    core::HardwareContractSnapshot hwSnapshot;
    hwSnapshot.selectedHardwareId = drawer.getSelectedHardwareId().toStdString();
    hwSnapshot.selectedFunctionId = drawer.getSelectedFunctionId().toStdString();
    hwSnapshot.isAutonomousSynth = hardwareManager.isAutonomousSynth(drawer.getSelectedHardwareId(), drawer.getSelectedFunctionId());
    hwSnapshot.contracts = hardwareManager.getContractRegistry().getContracts();

    const auto& queue = suiteList.getQueue();
    auto result = core::ProfilingSessionBuilder::buildFromQueue(queue, hwSnapshot, metaConfig);
    return result.session;
}

void MainContentComponent::startTargetedPatchSession(const std::vector<std::pair<int, int>>& pointsToPatch)
{
    if (sessionCoordinator.isRunningSession())
    {
        manualPromptLabel.setText("A profiling session is already running. Please wait or stop it first.", juce::dontSendNotification);
        manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        return;
    }

    if (pointsToPatch.empty())
    {
        manualPromptLabel.setText("Please select at least one point to re-measure.", juce::dontSendNotification);
        manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        return;
    }

    if (!mainHeader.hasHardwareSelected())
    {
        drawer.openHardwareDrawer();
        manualPromptLabel.setText("Please select a Target Hardware device before starting the patch session.", juce::dontSendNotification);
        manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(5000);
        return;
    }

    isPatchingSession = true;
    suiteList.setSessionRunning(true);

    juce::String selectedHwId = drawer.getSelectedHardwareId();
    juce::String selectedFuncId = drawer.getSelectedFunctionId();
    const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());

    std::string modeStr = "MOCK_DSP";
    std::string hwName = "MOCK_VA_SYNTH";

    if (contract != nullptr)
    {
        modeStr = contract->deviceType;
        hwName = contract->id;
    }
    else
    {
        audioEngine.setMockHardware(nullptr);
    }

    sessionCoordinator.setHardwareContext(&hardwareManager, selectedHwId);

    core::ProfilingSession patchSession = buildPatchProfilingSession(pointsToPatch, hwName, modeStr);
    patchSession.setIsPatchSession(true);

    juce::String baseName = juce::String(hwName) + "_" + selectedFuncId;

    meterStrip.setProfilingActive(true);
    sessionCoordinator.triggerStartSession(patchSession, exportDirectory, baseName, true);
}

core::ProfilingSession MainContentComponent::buildPatchProfilingSession(const std::vector<std::pair<int, int>>& pointsToPatch,
                                                                        const std::string& hwName,
                                                                        const std::string& modeStr)
{
    core::SessionMetadataConfig metaConfig;
    metaConfig.hardwareName = hwName;
    metaConfig.targetModule = drawer.getSelectedFunctionId().toStdString();
    metaConfig.operatorMode = modeStr;
    metaConfig.sampleRate = audioEngine.getSampleRate();
    metaConfig.bitDepth = 24;
    metaConfig.timestampIso8601 = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    metaConfig.operatorNotes = drawer.getOperatorNotes().toStdString();
    metaConfig.ambientTemperatureC = drawer.getAmbientTemperature();
    metaConfig.warmupTimeMinutes = drawer.getWarmupTimeMinutes();

    core::HardwareContractSnapshot hwSnapshot;
    hwSnapshot.selectedHardwareId = drawer.getSelectedHardwareId().toStdString();
    hwSnapshot.selectedFunctionId = drawer.getSelectedFunctionId().toStdString();
    hwSnapshot.isAutonomousSynth = hardwareManager.isAutonomousSynth(drawer.getSelectedHardwareId(), drawer.getSelectedFunctionId());
    hwSnapshot.contracts = hardwareManager.getContractRegistry().getContracts();

    std::vector<core::PatchPoint> patchPoints;
    patchPoints.reserve(pointsToPatch.size());
    for (const auto& pt : pointsToPatch)
    {
        patchPoints.push_back({ pt.first, pt.second });
    }

    const auto& queue = suiteList.getQueue();
    auto result = core::ProfilingSessionBuilder::buildPatch(patchPoints, queue, hwSnapshot, metaConfig);
    return result.session;
}

} // namespace abdaudiolab
