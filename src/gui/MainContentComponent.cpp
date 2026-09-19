/**
 * @file MainContentComponent.cpp
 * @brief Implementation of MainContentComponent layout, sequencing and UI orchestration.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"
#include "hardware/AudioMidiInterfaceDetector.h"
#include "core/LabDataDirectories.h"
#include "gui/measurement/MeasurementViewerPanel.h"
#include "gui/measurement/MeasurementComparisonPanel.h"
#include "core/ProfilingSessionBuilder.h"
#include "synth/Sha256.h"
#include <cmath>

namespace abdaudiolab
{

// ==============================================================================
// SECTION 1: AUDIO ENGINE, TELEMETRY & FFT BRIDGE
// Owns UI-side audio device initialization, telemetry stream subscriptions, and auxiliary floating window host.
// Real-time audio processing delegated to AudioEngine; FFT computation to AudioBridge/SpectrumAnalyzer.
// ==============================================================================

namespace
{
class MeasurementFloatingWindow : public juce::DocumentWindow
{
public:
    MeasurementFloatingWindow(const juce::String& title,
                              juce::Component* contentComponent,
                              int defaultWidth,
                              int defaultHeight,
                              int minWidth,
                              int minHeight)
        : DocumentWindow(title,
                         gui::AppTheme::BackgroundApp,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(minWidth, minHeight, 2560, 1440);
        setContentOwned(contentComponent, true);
        centreWithSize(defaultWidth, defaultHeight);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};
} // namespace

hardware::AiraModel mapHardwareIdToAiraModel(const juce::String& hwId)
{
    if (hwId == "roland_aira_bitrazer") return hardware::AiraModel::Bitrazer;
    if (hwId == "roland_aira_demora")   return hardware::AiraModel::Demora;
    if (hwId == "roland_aira_torcido")  return hardware::AiraModel::Torcido;
    if (hwId == "roland_aira_scooper")  return hardware::AiraModel::Scooper;
    return hardware::AiraModel::GenericModular;
}

// ==============================================================================
// SECTION 4: CONSTRUCTOR, UI WIRING & DRAWER BINDINGS
// Owns component tree instantiation, child component hierarchy assembly, sidebar stepper wireup, and drawer slide-in binding.
// Individual component internals delegated to their respective GUI classes (MainHeader, SoundIdSuiteList, SetupDrawer).
// ==============================================================================
MainContentComponent::MainContentComponent(StartupProgressCallback onProgress)
: sequencer(audioEngine, *hardwareManager.getMockController()),
  mainHeader(audioEngine)
{
    auto report = [&](const juce::String& msg, float prog) {
        if (onProgress)
            onProgress(msg, prog);
    };

    report("Iniciando Motor de Audio & Controladores ASIO...", 0.15f);

    setLookAndFeel(&soundIdTheme);
    juce::LookAndFeel::setDefaultLookAndFeel(&soundIdTheme);

    // 1. Initialize Audio Engine & Restore State
    juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
    settingsFile = appData.getChildFile("AudioSettings.xml");
    audioEngine.initializeAudioDevices(settingsFile);
    audioEngine.setMockHardware(hardwareManager.getMockController());

    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    juce::String devName = (dev != nullptr) ? dev->getName() : "Drivers de Audio WASAPI/ASIO OK";
    report("Audio verificado: " + devName, 0.32f);

    hardwareManager.getContractRegistry().onProfileWarning = [this](const juce::String& warning) {
        juce::MessageManager::callAsync([this, warning]() {
            manualPromptLabel.setText(warning, juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            hidePromptAfterDelay(6000);
        });
    };

    report("Escaneando contratos de hardware en ABDSharedAssets...", 0.45f);

    // Load Contract Specifications: Prioritize ABDSharedAssets/contracts as the single source of truth
    std::vector<juce::File> roots = {
        juce::File("D:/desarrollos/ABDSynths/ABDSharedAssets/contracts"),
        juce::File::getCurrentWorkingDirectory(),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory()
    };

    for (auto root : roots)
    {
        // Direct ABDSharedAssets/contracts directory
        if (root.isDirectory() && root.getFileName() == "contracts" && hardwareManager.getContractRegistry().loadContractsFromDirectory(root))
            break;

        for (int i = 0; i < 6; ++i)
        {
            // 1. Check ABDSharedAssets/contracts (sibling or child)
            auto shared = root.getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (shared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(shared))
                break;

            auto siblingShared = root.getParentDirectory().getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (siblingShared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(siblingShared))
                break;

            // 2. Fallback to local contracts/hardware
            auto direct = root.getChildFile("contracts").getChildFile("hardware");
            if (direct.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(direct))
                break;

            root = root.getParentDirectory();
        }
        if (hardwareManager.getContractRegistry().hasContracts())
            break;
    }

    if (!hardwareManager.getContractRegistry().hasContracts())
    {
        juce::Logger::writeToLog("[HardwareContractRegistry ERROR] " + juce::String(hardwareManager.getContractRegistry().getLastError()));
        report("Sin contratos de hardware detectados", 0.62f);
    }
    else
    {
        hardwareManager.getHotplugMonitor().setContracts(hardwareManager.getContractRegistry().getContracts());
        size_t nContracts = hardwareManager.getContractRegistry().getContracts().size();
        report("Contratos cargados: " + juce::String(nContracts) + " modelos de sintetizador", 0.62f);
    }

    report("Configurando Motores DSP (Farina, Wiener-Hammerstein & RTNeural)...", 0.76f);

    // Default export directory via deterministic LabDataDirectories
    auto labDirs = core::resolveLabDataDirectories();
    exportDirectory = labDirs.exports;
    exportDirectory.createDirectory();

    // Auto-generate Casio CZ Automated Live Scan Session Manifest in assets/presets
    {
        auto presetsDir = juce::File::getCurrentWorkingDirectory().getChildFile("assets").getChildFile("presets");
        presetsDir.createDirectory();
        auto czSessionFile = presetsDir.getChildFile("casio_cz101_mame_ves_session.json");
        if (!czSessionFile.existsAsFile())
        {
            auto czSuite = core::ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 100, 8);
            czSuite.exportSessionToJsonFile(czSessionFile.getFullPathName().toStdString());
        }
    }

    // 2. Setup Manual Controller Callback (Only active in Manual Analogue mode)
    hardwareManager.setManualPromptCallback([this](const juce::String& paramName, float norm, int raw) {
        juce::MessageManager::callAsync([this, paramName, norm, raw]() {
            manualPromptLabel.setText("MANUAL ACTION: Adjust [" + paramName + "] to " + 
                                      juce::String(norm, 2) + " (Raw: " + juce::String(raw) + ") and press SPACEBAR", 
                                      juce::dontSendNotification);
            confirmManualButton.setEnabled(true);
            confirmManualButton.setVisible(true);
            manualPromptLabel.setVisible(true);
            resized();
        });
    });

    // Populate Hardware Selector from Contract Registry
    std::vector<gui::HardwareItem> hwItems;
    for (const auto& c : hardwareManager.getContractRegistry().getContracts())
    {
        gui::HardwareItem item;
        item.id = juce::String(c.id);
        item.displayName = juce::String(c.displayName);
        item.description = juce::String(c.description);
        item.category = juce::String(c.deviceType);
        item.brand = juce::String(c.brand);
        item.brandLogo = juce::String(c.brandLogo);
        item.modelImage = juce::String(c.modelImage);

        for (const auto& f : c.functions)
        {
            gui::FunctionItem fItem;
            fItem.id = juce::String(f.id);
            fItem.name = juce::String(f.name);
            fItem.blockType = juce::String(f.blockType);
            fItem.stimulusOutput = juce::String(f.routingGuide.stimulusOutput);
            fItem.responseInput = juce::String(f.routingGuide.responseInput);
            fItem.notes = juce::String(f.routingGuide.notes);
            fItem.captureMode = juce::String(f.captureMode);
            fItem.defaultBurstDurationSec = f.defaultBurstDurationSec;
            for (const auto& ctrl : f.controls)
            {
                gui::ControlItem cItem;
                cItem.name = juce::String(ctrl.name);
                cItem.type = juce::String(ctrl.type);
                fItem.controls.push_back(cItem);
            }
            item.functions.push_back(fItem);
        }
        hwItems.push_back(item);
    }
    drawer.setHardwareList(hwItems);
    drawer.setContracts(hardwareManager.getContractRegistry().getContracts());
    drawer.setHardwareLocked(false);

    // Wire SessionIoController Callbacks
    sessionIoController.setExportDirectory(exportDirectory);
    sessionIoController.setSessionContextProvider([this]() -> gui::SessionSaveContext {
        gui::SessionSaveContext ctx;
        ctx.manifest = buildCurrentSessionManifest();
        ctx.metadata.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
        ctx.metadata.targetModule = drawer.getSelectedFunctionId().toStdString();
        ctx.metadata.sampleRate = audioEngine.getCurrentSampleRate();
        ctx.metadata.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
        ctx.metadata.operatorNotes = drawer.getOperatorNotes().toStdString();
        ctx.metadata.ambientTemperatureC = drawer.getAmbientTemperature();
        ctx.metadata.warmupTimeMinutes = drawer.getWarmupTimeMinutes();
        ctx.suggestedFileName = drawer.getActiveHardwareDisplayName().replaceCharacter(' ', '_') + ".abdlabtest";
        return ctx;
    });
    sessionIoController.onSessionLoaded = [this](const core::SessionManifest& manifest, const std::vector<exporting::MeasuredPoint>& points) {
        applyLoadedSession(manifest, points);
    };
    sessionIoController.onSessionSaved = [this](const juce::File& /*savedFile*/) {
        drawer.openFileDrawer(exportDirectory.getFullPathName());
    };
    sessionIoController.onStatusNotification = [this](const juce::String& msg, bool isError) {
        manualPromptLabel.setText(msg, juce::dontSendNotification);
        manualPromptLabel.setColour(juce::Label::textColourId, isError ? juce::Colours::coral : gui::SoundIdTheme::accentGreen);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(5000);
    };

    // Main Header Controller Integration (Barra Superior Modular)
    mainHeader.onNewSession = [this] { promptNewSession(); };
    mainHeader.onOpenSession = [this] { handleOpenSession(); };
    mainHeader.onSaveSession = [this] { handleSaveSession(); };
    mainHeader.onSaveSessionAs = [this] { handleSaveSessionAs(); };
    mainHeader.onReanalyzeOffline = [this] { performOfflineReanalysis(); };
    mainHeader.onExportCertificationReport = [this] { reportExportController.requestExportCertificationReport(); };
    mainHeader.onOpenExportFolder = [this] {
        reportExportController.openExportFolderInExplorer();
    };
    mainHeader.onOpenExperimentsFolder = [this] {
        auto dirs = core::resolveLabDataDirectories();
        if (!dirs.experiments.exists())
            dirs.experiments.createDirectory();
        dirs.experiments.revealToUser();
    };
    mainHeader.onExitApp = [this] { confirmAndExit(); };
    mainHeader.onOpenMeasurementViewer = [this] { openMeasurementViewerWindow(); };
    mainHeader.onOpenMeasurementComparison = [this] { openMeasurementComparisonWindow(); };

    // Plugin Scan Directories modal wiring
    mainHeader.onScanPluginDirectories = [this] {
        juce::Logger::writeToLog("[MainComponent] Opening Plugin Scan Directories modal...");
        pluginScanModal.showModal(this);
    };
    pluginScanModal.onScanRequested = [this](const juce::FileSearchPath& paths,
                                              std::function<void(const juce::String&, float)> progressCb) {
        juce::Logger::writeToLog("[MainComponent] Plugin scan requested. Search paths: " + paths.toString());
        // Run scan in background thread
        auto safePaths = paths;
        auto safeProgressCb = std::move(progressCb);
        juce::Thread::launch([this, safePaths, safeProgressCb]() {
            juce::Logger::writeToLog("[MainComponent] Background scan thread started.");
            pluginHostManager.scanPlugins(safePaths, true, safeProgressCb);
            juce::Logger::writeToLog("[MainComponent] Background scan finished, saving cache...");
            pluginHostManager.saveCache(core::PluginHostManager::getDefaultCacheFile());

            juce::MessageManager::callAsync([this]() {
                auto plugins = pluginHostManager.getAvailablePlugins();
                juce::Logger::writeToLog("[MainComponent] Updating catalogSelector with "
                    + juce::String(static_cast<int>(plugins.size())) + " plugins.");
                catalogSelector.setAvailablePlugins(plugins);

                if (pluginScanModal.onScanComplete)
                    pluginScanModal.onScanComplete(static_cast<int>(plugins.size()));

                juce::Logger::writeToLog("[PluginHost] Scan complete: "
                    + juce::String(static_cast<int>(plugins.size())) + " plugins found");
            });
        });
    };

    // Startup: load persisted plugin directories and scan if any exist
    {
        auto dirs = pluginScanModal.loadPersistedDirectories();
        juce::Logger::writeToLog("[MainComponent] Loaded " + juce::String(dirs.size()) + " persisted plugin directories.");
        if (!dirs.empty())
        {
            juce::FileSearchPath startupPaths;
            for (const auto& dir : dirs)
            {
                if (dir.isDirectory())
                {
                    juce::Logger::writeToLog("[MainComponent]   Persisted dir: " + dir.getFullPathName());
                    startupPaths.add(dir);
                }
                else
                {
                    juce::Logger::writeToLog("[MainComponent WARNING] Persisted path is not a valid directory: " + dir.getFullPathName());
                }
            }
            if (startupPaths.getNumPaths() > 0)
            {
                juce::Logger::writeToLog("[MainComponent] Launching background startup plugin scan...");
                juce::Thread::launch([this, startupPaths]() {
                    juce::Logger::writeToLog("[MainComponent] Startup background scan thread executing...");
                    pluginHostManager.scanPlugins(startupPaths, true, nullptr);
                    pluginHostManager.saveCache(core::PluginHostManager::getDefaultCacheFile());

                    juce::MessageManager::callAsync([this]() {
                        auto plugins = pluginHostManager.getAvailablePlugins();
                        catalogSelector.setAvailablePlugins(plugins);
                        juce::Logger::writeToLog("[PluginHost] Startup scan complete: "
                            + juce::String(static_cast<int>(plugins.size()))
                            + " plugins loaded into catalog.");
                    });
                });
            }
        }
    }

    mainHeader.onScopeToggle = [this] { toggleScopeWebWindow(); };
    mainHeader.onVirtualKeyboardToggle = [this] { toggleVirtualKeyboardWindow(); };
    mainHeader.onConfigureAudioMidi = [this] { openAudioMidiSettings(); };
    mainHeader.onCalibrateClicked = [this] {
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::CalibrateLoopback);
    };
    mainHeader.onHardwareSelectorClicked = [this] {
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::HardwareRouting);
    };
    mainHeader.onThemeToggled = [this] {
        auto newMode = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Light)
                           ? gui::AppTheme::ThemeMode::Dark
                           : gui::AppTheme::ThemeMode::Light;
        gui::SoundIdTheme::applyThemeMode(newMode, &soundIdTheme);
        sendLookAndFeelChange();

        mainHeader.updateTheme();
        healthPanel.repaint();
        meterStrip.repaint();
        curvePlotter.updateTheme();
        suiteList.updateTheme();
        stepperBar.repaint();

        if (scopeWebWindow != nullptr)
            scopeWebWindow->updateTheme();

        if (virtualKeyboardWindow != nullptr)
        {
            auto kbdTheme = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark) ? "audiolab" : "audiolab-light";
            virtualKeyboardWindow->setTheme(kbdTheme, gui::AppTheme::BackgroundApp);
            virtualKeyboardWindow->repaint();
        }
        
        pluginWindowController.updateTheme();

        auto themeStr = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark) ? "audiolab" : "audiolab-light";
        topologyController.updateTheme(themeStr, gui::AppTheme::BackgroundApp);

        if (measurementViewerWindow != nullptr)
            measurementViewerWindow->setBackgroundColour(gui::AppTheme::BackgroundApp);

        if (measurementComparisonWindow != nullptr)
            measurementComparisonWindow->setBackgroundColour(gui::AppTheme::BackgroundApp);

        drawer.updateTheme();
        operatorStepModal.updateTheme();
        setupInfoTab.updateTheme();
        catalogSelector.updateTheme();
        repaint();
    };
    addAndMakeVisible(mainHeader);

    // Step 0: Setup & Telemetry Info Tab (Paso 0: Información)
    setupInfoTab.onOpenAudioSettingsClicked = [this] {
        openAudioMidiSettings();
    };
    setupInfoTab.onOpenTopologyModalClicked = [this] {
        toggleStudioTopologyWindow();
    };
    setupInfoTab.onAboutClicked = [this] {
        showAboutDialog();
    };
    setupInfoTab.onRefreshRequested = [this] {
        updateSetupDrawerInfo();
        manualPromptLabel.setText("✓ Audio/MIDI connections and telemetry refreshed.", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(3000);
        if (topologyController.isWindowVisible())
            toggleStudioTopologyWindow();
    };
    addChildComponent(setupInfoTab);
    setupInfoTab.setVisible(false);
    updateSetupDrawerInfo();

    // 4. Workflow Stepper Bar & Export Report Panel (Paso 4: Certificación SoundID)
    addChildComponent(exportReportPanel);
    exportReportPanel.setVisible(false);

    exportReportPanel.onExportRequested = [this] {
        reportExportController.requestExportProductionPackage();
    };
    exportReportPanel.onOpenFolderRequested = [this] {
        reportExportController.openExportFolderInExplorer();
    };
    exportReportPanel.onViewHtmlRequested = [this] {
        reportExportController.openCertificationReportHtml();
    };
    exportReportPanel.onPublishCloudRequested = [this] {
        publishCertificationToCloud();
    };
    exportReportPanel.onAuditionToggled = [this](bool active) {
        if (active)
        {
            prepareAuditionLut();
            audioEngine.enableAuditionMode(true);
            exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"Audición DSP activada. Ajuste Cutoff y Resonancia para escuchar el modelo."));
        }
        else
        {
            audioEngine.enableAuditionMode(false);
            exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"Audición DSP detenida."));
        }
    };
    exportReportPanel.onAuditionParamsChanged = [this](float p1, float p2) {
        audioEngine.setAuditionParameters(p1, p2);
    };
    exportReportPanel.onAuditionWaveformChanged = [this](int wf) {
        audioEngine.setAuditionWaveform(wf);
    };
    exportReportPanel.onVerifyAbRequested = [this] {
        openAudioABVerificationModal();
    };

    hardwareRoutingPanel.setContracts(hardwareManager.getContractRegistry().getContracts());
    hardwareRoutingPanel.onHardwareSelected = [this](const juce::String& hwId, const juce::String& funcId) {
        onHardwareSelected(hwId, funcId);
        drawer.setSelectedHardwareId(hwId);
    };
    hardwareRoutingPanel.onContinueToCalibration = [this] {
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::HardwareRouting,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
        if (sidebarStepper.getStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback) == gui::SoundIdSidebarStepper::StepStatus::Completed)
        {
            workflowNavController.setStep(gui::WorkflowNavigationController::Step::RunSession);
        }
        else
        {
            workflowNavController.setStep(gui::WorkflowNavigationController::Step::CalibrateLoopback);
        }
    };
    hardwareRoutingPanel.onOpenAdvancedSettings = [this] {
        drawer.openHardwareDrawer();
    };
    hardwareRoutingPanel.onOpenTopologyModal = [this] {
        toggleStudioTopologyWindow();
    };
    hardwareRoutingPanel.onAutoDetectRequested = [this] {
        drawer.triggerAutoDetect();
    };
    hardwareRoutingPanel.onNewFlowRequested = [this] {
        if (drawer.onNewFlowRequested != nullptr)
            drawer.onNewFlowRequested();
    };
    addChildComponent(hardwareRoutingPanel);

    nativeCalibrationPanel.onCalibrationApplied = [this](const math::LoopbackCalibrationData& cal) {
        float gainDb = 20.0f * std::log10(std::max(cal.recommendedTrimGain, 1e-4f));
        juce::String sign = (gainDb >= 0.0f) ? "+" : "";
        juce::String msg = "Calibración completada. Auto-trim aplicado: " + sign + juce::String(gainDb, 1) + " dB";
        manualPromptLabel.setText(msg, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);

        mainHeader.updateCalibrationStatus(true, cal.sampleRate, false);

        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Completed);
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Completed);

        auto summary = sidebarStepper.getSessionSummary();
        summary.loopbackCalibrated = true;
        summary.loopbackSnrDb = cal.snrDb > 0.0f ? cal.snrDb : 90.0f;
        sidebarStepper.setSessionSummary(summary);
        resized();
    };
    nativeCalibrationPanel.onCalibrationSkipped = [this] {
        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Skipped);
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Skipped);
        mainHeader.updateCalibrationStatus(false, 0.0, true);

        manualPromptLabel.setText("Step 1 Bypassed: Operating with nominal gain (0 dB). Step 2 enabled!", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        resized();
    };
    nativeCalibrationPanel.onContinueToSession = [this] {
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::CalibrateLoopback,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::HardwareRouting);
    };
    addChildComponent(nativeCalibrationPanel);

    stepperBar.onStepSelected = [this](gui::WorkflowStepperBar::Step targetStep) {
        switch (targetStep)
        {
            case gui::WorkflowStepperBar::Step::HardwareRouting:
                break;

            case gui::WorkflowStepperBar::Step::CalibrateLoopback:
                nativeCalibrationPanel.resetToInitialState();
                break;

            case gui::WorkflowStepperBar::Step::RunSession:
                manualPromptLabel.setText("Step 3: Profiling session ready. Click Play on the right panel to begin.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                hidePromptAfterDelay(4000);
                break;

            case gui::WorkflowStepperBar::Step::ExportReport:
                updateExportReportMetrics();
                break;
        }
        stepperBar.setCurrentStep(targetStep);
        workflowNavController.setStep(static_cast<gui::WorkflowNavigationController::Step>(targetStep));
        resized();
    };
    // Hide horizontal stepperBar in favor of collapsible sidebarStepper, maintaining full logic
    stepperBar.setVisible(false);
    addChildComponent(stepperBar);

    // Coordinate WorkflowNavigationController as the Single Source of Truth
    workflowNavController.onStepChanged = [this](gui::WorkflowNavigationController::Step targetStep) {
        stepperBar.setCurrentStep(static_cast<gui::WorkflowStepperBar::Step>(targetStep));
        switch (targetStep)
        {
            case gui::WorkflowNavigationController::Step::SystemInfo:
                showInfoDrawer();
                break;

            case gui::WorkflowNavigationController::Step::HardwareRouting:
                break;

            case gui::WorkflowNavigationController::Step::CalibrateLoopback:
                nativeCalibrationPanel.resetToInitialState();
                break;

            case gui::WorkflowNavigationController::Step::RunSession:
                manualPromptLabel.setText("Step 3: Profiling session ready. Click Play on the right panel to begin.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                hidePromptAfterDelay(4000);
                break;

            case gui::WorkflowNavigationController::Step::ExportReport:
                updateExportReportMetrics();
                break;
        }
        updateGovernanceUi();
        resized();
    };

    sidebarStepper.onCollapseToggled = [this](bool /*collapsed*/) {
        resized();
    };
    addAndMakeVisible(sidebarStepper);

    // ==============================================================================
    // SECTION 2: VST3 HOSTING & CONTRACT BINDING
    // Owns UI-facing plugin selection, host scanner integration, and editor window lifecycle.
    // Dynamic parameter contract creation delegated to PluginUiCoordinator; processing to AudioEngine.
    // ==============================================================================

    // Wire cascading catalog selector
    catalogSelector.setContracts(hardwareManager.getContractRegistry().getContracts());
    catalogSelector.onSelectionChanged = [this](const juce::String& hwId, const juce::String& funcId) {
        if (catalogSelector.isPluginVirtualMode())
        {
            // Clear physical hardware selection to prevent residues
            drawer.clearSelectedHardware();
            sessionCoordinator.setHardwareContext(&hardwareManager, {});
            hardwareRoutingPanel.setHardwareLocked(false);
            auto desc = pluginUiCoordinator.getActivePluginDescription();
            hardwareRoutingPanel.setPluginVirtualRouting(
                desc.name.isNotEmpty() ? desc.name : "Plugin Virtual",
                desc.pluginFormatName.isNotEmpty() ? desc.pluginFormatName : "VST3",
                desc.isInstrument
            );

            // Update standard test button state
            suiteList.setStandardTestAvailable(pluginUiCoordinator.hasActivePlugin());
            return;
        }

        // Switching to physical hardware: safely disconnect virtual plugin if any
        pluginUiCoordinator.unloadPlugin();

        drawer.setSelectedHardwareId(hwId);
        onHardwareSelected(hwId, funcId);
        hardwareRoutingPanel.setSelectedHardware(hwId, funcId);
        gui::SoundIdSidebarStepper::SessionSummaryInfo summary = sidebarStepper.getSessionSummary();
        summary.hardwareName = hwId;
        sidebarStepper.setSessionSummary(summary);

        const auto* c = hardwareManager.findContractById(hwId.toStdString());
        suiteList.setStandardTestAvailable(c != nullptr && !c->functions.empty());
    };
    catalogSelector.onContinueRequested = [this] {
        if (catalogSelector.isPluginVirtualMode())
        {
            catalogSelector.setHardwareLocked(true);
            drawer.setHardwareLocked(true);
            hardwareRoutingPanel.setHardwareLocked(true);

            if (pluginUiCoordinator.hasActivePlugin())
            {
                auto desc = pluginUiCoordinator.getActivePluginDescription();
                std::string contractId = "plugin_" + juce::File::createLegalFileName(desc.fileOrIdentifier).toStdString();
                const auto* contract = hardwareManager.findContractById(contractId);
                std::string funcId = (contract != nullptr && !contract->functions.empty()) ? contract->functions[0].id : "";
                onHardwareSelected(juce::String(contractId), juce::String(funcId));
                drawer.setSelectedHardwareId(juce::String(contractId));
            }
        }
        else
        {
            juce::String hwId = catalogSelector.getSelectedHardwareId();
            juce::String funcId = catalogSelector.getSelectedFunctionId();
            if (hwId.isNotEmpty())
            {
                onHardwareSelected(hwId, funcId);
                drawer.setSelectedHardwareId(hwId);
                catalogSelector.setHardwareLocked(true);
                drawer.setHardwareLocked(true);
                hardwareRoutingPanel.setHardwareLocked(true);
            }
        }

        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::HardwareRouting,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::RunSession);
    };
    catalogSelector.onAutoDetectRequested = [this] {
        drawer.triggerAutoDetect();
    };
    catalogSelector.onResetOrUnlockRequested = [this] {
        if (drawer.onNewFlowRequested != nullptr)
            drawer.onNewFlowRequested();
    };
    catalogSelector.onLoadPluginFromFileRequested = [this] {
        juce::Logger::writeToLog("[MainComponent] onLoadPluginFromFileRequested triggered.");
        auto chooser = std::make_shared<juce::FileChooser>(
            juce::String::fromUTF8(u8"Seleccionar Plugin VST3 / AU / LV2"),
            juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory),
            "*.vst3;*.component;*.lv2");

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result == juce::File{})
                    return;

                pluginUiCoordinator.loadPluginFromFile(result, [this](bool ok) {
                    if (ok)
                        catalogSelector.setAvailablePlugins(pluginHostManager.getAvailablePlugins());
                });
            });
    };
    catalogSelector.onPluginSelected = [this](const juce::PluginDescription& desc) {
        juce::Logger::writeToLog("[MainComponent] onPluginSelected: '" + desc.name + "' [" + desc.pluginFormatName + "]");
        pluginUiCoordinator.loadPlugin(desc);
    };
    catalogSelector.onShowPluginGuiRequested = [this] {
        if (pluginUiCoordinator.hasActivePlugin())
        {
            pluginUiCoordinator.showEditor();
        }
        else if (auto* desc = catalogSelector.getSelectedPluginDescription())
        {
            pluginUiCoordinator.loadPlugin(*desc, [this](bool success) {
                if (success)
                    pluginUiCoordinator.showEditor();
            });
        }
    };
    catalogSelector.onOpenKeyboardRequested = [this] {
        toggleVirtualKeyboardWindow();
    };
    pluginWindowController.onOpenKeyboardRequested = [this] {
        toggleVirtualKeyboardWindow();
    };

    // Initialize plugin host cache
    pluginHostManager.loadCache(core::PluginHostManager::getDefaultCacheFile());

    addChildComponent(catalogSelector);

    // ==============================================================================
    // SECTION 3: TEST SUITE QUEUE & EVENT DELEGATION
    // Owns suite list UI event wireup, table selection dispatch, and real-time curve display bridging.
    // Queue item state mutations delegated to SuiteQueueModelManager; event business logic to SuiteListEventHandler.
    // ==============================================================================

    // 5. Center Curve Plotter & Real-Time Visualization
    addAndMakeVisible(healthPanel);
    addAndMakeVisible(curvePlotter);

    curvePlotter.onToggleCollapse = [this] {
        if (centerSplitMode == CenterSplitMode::GraphMaximized)
            centerSplitMode = CenterSplitMode::Balanced;
        else if (centerSplitMode == CenterSplitMode::QueueMaximized)
            centerSplitMode = CenterSplitMode::Balanced;
        else
            centerSplitMode = CenterSplitMode::GraphMaximized;
        updateSplitLayout();
    };

    suiteList.onToggleCollapse = [this] {
        if (centerSplitMode == CenterSplitMode::QueueMaximized)
            centerSplitMode = CenterSplitMode::Balanced;
        else if (centerSplitMode == CenterSplitMode::GraphMaximized)
            centerSplitMode = CenterSplitMode::Balanced;
        else
            centerSplitMode = CenterSplitMode::QueueMaximized;
        updateSplitLayout();
    };

    centerSplitterBar.onDragged = [this](int deltaY) {
        if (centerSplitMode == CenterSplitMode::Balanced)
        {
            auto bounds = getLocalBounds().reduced(20);
            int minGraphAreaH = 180 + 32 + 12; // 180px curvePlotter + 32px healthPanel + 12px splitter
            int maxBottomH = bounds.getHeight() - minGraphAreaH;
            balancedBottomH = juce::jlimit(50.0f, static_cast<float>(std::max(50, maxBottomH)), balancedBottomH - static_cast<float>(deltaY));
            targetBottomH = balancedBottomH;
            currentBottomH = balancedBottomH;
            resized();
        }
    };
    centerSplitterBar.onResetToDefault = [this] {
        if (centerSplitMode == CenterSplitMode::Balanced)
        {
            balancedBottomH = 220.0f;
            targetBottomH = balancedBottomH;
        }
    };
    addChildComponent(centerSplitterBar);

    // 5. Bottom Test Queue (Session Test Plan & CRUD)
    suiteList.onAddStandardClicked = [this] {
        // Determine contract: plugin virtual mode uses the dynamic contract; physical mode uses drawer selection
        juce::String targetHwId;
        if (catalogSelector.isPluginVirtualMode() || pluginUiCoordinator.hasActivePlugin())
        {
            // Use the dynamic contract registered for this plugin
            auto desc = pluginUiCoordinator.getActivePluginDescription();
            targetHwId = juce::String("plugin_") + juce::File::createLegalFileName(desc.fileOrIdentifier);
            // Fallback: search by name if not found
            if (hardwareManager.findContractById(targetHwId.toStdString()) == nullptr)
            {
                const auto& allContracts = hardwareManager.getContractRegistry().getContracts();
                for (const auto& c : allContracts)
                {
                    if (juce::String(c.id).startsWith("plugin_") &&
                        (desc.name.isEmpty() ||
                         juce::String(c.displayName).containsIgnoreCase(desc.name)))
                    {
                        targetHwId = juce::String(c.id);
                        break;
                    }
                }
            }
        }
        else
        {
            targetHwId = drawer.getSelectedHardwareId();
        }

        juce::String selectedFuncId = drawer.getSelectedFunctionId();
        const auto* contract = hardwareManager.findContractById(targetHwId.toStdString());
        if (contract == nullptr || contract->functions.empty()) return;

        const auto* targetFunc = &contract->functions[0];
        for (const auto& func : contract->functions)
        {
            if (func.id == selectedFuncId.toStdString())
            {
                targetFunc = &func;
                break;
            }
        }

        const auto& f = *targetFunc;
        gui::TestConfiguration stdConf;
        stdConf.testName = juce::String(contract->displayName) + " (" + juce::String(f.name) + ")";
        if (f.blockType == "TimeDynamic") stdConf.stimulusType = audio::StimulusType::SyncPulses3;
        else if (f.blockType == "WaveShaper") stdConf.stimulusType = audio::StimulusType::AmplitudeRamp;
        else if (f.blockType == "CyclicModulator") stdConf.stimulusType = audio::StimulusType::SineWave1kHz;
        else stdConf.stimulusType = audio::StimulusType::LogFarinaSweep;

        stdConf.burstDurationSec = f.defaultBurstDurationSec > 0.05f ? f.defaultBurstDurationSec : 1.0f;
        stdConf.captureMode = f.captureMode;

        stdConf.controls.clear();
        for (size_t k = 0; k < f.controls.size(); ++k)
        {
            gui::ControlStepConfig cs;
            cs.name = f.controls[k].name;
            cs.type = f.controls[k].type;
            cs.steps = (k == 0) ? 8 : ((k == 1) ? 4 : 1);
            stdConf.controls.push_back(cs);
        }

        bool isManual = (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL");
        drawer.openTestEditorDrawer(stdConf, -1, isManual);
    };

    suiteList.onAddCustomClicked = [this] {
        gui::TestConfiguration customConf;
        customConf.testName = "Custom Profile";
        customConf.stimulusType = audio::StimulusType::LogFarinaSweep;
        customConf.burstDurationSec = 1.0f;
        customConf.captureMode = "FIXED_TIME";

        // Plugin Virtual mode: expose all plugin parameters for the user to pick
        if (pluginUiCoordinator.hasActivePlugin())
        {
            auto desc = pluginUiCoordinator.getActivePluginDescription();
            customConf.testName = juce::String(desc.name) + " \u2013 Custom Profile";
            std::vector<gui::ControlStepConfig> availableParams;
            if (auto* instance = pluginUiCoordinator.getActivePluginInstance())
            {
                const auto& params = instance->getParameters();
                for (int i = 0; i < params.size(); ++i)
                {
                    auto* p = params[i];
                    if (p == nullptr) continue;
                    gui::ControlStepConfig cs;
                    cs.id = juce::String(i);
                    cs.name = p->getName(64);
                    cs.type = "Normalized";
                    cs.steps = 1;
                    cs.minPct = 0.0f;
                    cs.maxPct = 100.0f;
                    cs.sortOrder = i;
                    availableParams.push_back(cs);
                }
            }
            // Open the drawer first, then set the available params (plugin is automated)
            drawer.openTestEditorDrawer(customConf, -1, false);
            drawer.setAvailablePluginParams(availableParams);
            return;
        }

        // Hardware mode: pre-fill controls from contract (existing behaviour)
        juce::String selectedHwId = drawer.getSelectedHardwareId();
        juce::String selectedFuncId = drawer.getSelectedFunctionId();
        const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());
        drawer.clearAvailablePluginParams();
        bool isManual = false;
        if (contract != nullptr && !contract->functions.empty())
        {
            isManual = (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL");
            const auto* targetFunc = &contract->functions[0];
            for (const auto& func : contract->functions)
            {
                if (func.id == selectedFuncId.toStdString())
                {
                    targetFunc = &func;
                    break;
                }
            }
            const auto& f = *targetFunc;
            for (const auto& c : f.controls)
            {
                gui::ControlStepConfig cs;
                cs.name = c.name;
                cs.type = c.type;
                cs.steps = 4;
                customConf.controls.push_back(cs);
            }
        }
        drawer.openTestEditorDrawer(customConf, -1, isManual);
    };

    suiteList.onEditTestClicked = [this](int index, const gui::QueueItem& item) {
        const auto* contract = item.hwId.isNotEmpty() ? hardwareManager.findContractById(item.hwId.toStdString()) : nullptr;
        bool isManual = (contract != nullptr) && (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL");

        if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
        {
            confirmationModal.show(
                this,
                "Edit Completed Test",
                "Modifying the parameters of '" + item.title + "' will invalidate its recorded measurements.\n\nDo you want to proceed and re-queue this test?",
                "Edit & Invalidate",
                "",
                "Cancel",
                [this, index, item, isManual](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        gui::TestConfiguration conf;
                        conf.testName = item.title;
                        conf.stimulusType = item.stimulusType;
                        conf.burstDurationSec = item.burstDurationSec;
                        conf.captureMode = item.captureMode;
                        conf.controls = item.controls;
                        drawer.openTestEditorDrawer(conf, index, isManual);
                    }
                }
            );
        }
        else
        {
            gui::TestConfiguration conf;
            conf.testName = item.title;
            conf.stimulusType = item.stimulusType;
            conf.burstDurationSec = item.burstDurationSec;
            conf.captureMode = item.captureMode;
            conf.controls = item.controls;
            drawer.openTestEditorDrawer(conf, index, isManual);
        }
    };

    suiteList.onRequestDeleteTest = [this](int index, const gui::QueueItem& item) {
        promptDeleteTest(index, item);
    };

    drawer.onTestConfigConfirmed = [this](const gui::TestConfiguration& conf, int editingIndex) {
        gui::QueueItem item;
        item.title = conf.testName;
        item.stimulusType = conf.stimulusType;
        item.burstDurationSec = conf.burstDurationSec;
        item.captureMode = conf.captureMode;
        item.controls = conf.controls;
        item.totalPoints = conf.getTotalMeasurementPoints();

        applyBadgeForStimulus(item, conf.stimulusType);

        juce::String formulaStr;
        for (const auto& c : item.controls) {
            if (c.steps > 1) {
                if (formulaStr.isNotEmpty()) formulaStr += " x ";
                formulaStr += juce::String(c.steps);
            }
        }
        item.description = juce::String::fromUTF8(u8"Sweep \u2022 ") + formulaStr + " = " + juce::String(item.totalPoints) + " points";
        item.status = gui::QueueItemStatus::Queued;

        if (editingIndex >= 0 && editingIndex < suiteList.getQueueSize())
        {
            item.id = suiteList.getQueue()[static_cast<size_t>(editingIndex)].id;
            item.hwId = suiteList.getQueue()[static_cast<size_t>(editingIndex)].hwId;
            item.funcId = suiteList.getQueue()[static_cast<size_t>(editingIndex)].funcId;
            suiteList.updateTestInQueue(editingIndex, item);
        }
        else
        {
            item.id = "test_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
            suiteList.addTestToQueue(item);
        }
    };

    suiteList.onRestartTestClicked = [this](int index) {
        suiteList.updateItemStatus(index, gui::QueueItemStatus::Queued, 0);
        startProfilingSession(false);
    };

    suiteList.onContinueTestClicked = [this](int index) {
        juce::ignoreUnused(index);
        startProfilingSession(true);
    };

    suiteList.onSelectPointClicked = [this](int queueIdx, int pointIdx) {
        curvePlotter.setHighlightedPointIndex(pointIdx);

        if (queueIdx >= 0 && queueIdx < static_cast<int>(suiteList.getQueue().size()))
        {
            const auto& item = suiteList.getQueue()[static_cast<size_t>(queueIdx)];
            float stepPct = (item.totalPoints > 1) ? (static_cast<float>(pointIdx) / static_cast<float>(item.totalPoints - 1) * 100.0f) : 0.0f;
            
            juce::String statusStr = (item.status == gui::QueueItemStatus::Completed) ? "Measured" : "Queued";
            juce::String prompt = "Current State: Point #" + juce::String(pointIdx + 1) + "/" + juce::String(item.totalPoints) 
                                + " (" + juce::String(stepPct, 1) + "% Position) \u2014 " + item.title;

            std::vector<core::ParameterStep> pSteps;

            // Check if this point has already been measured in sessionPoints
            int sessionOffset = 0;
            for (int i = 0; i < queueIdx; ++i)
            {
                if (!suiteList.getQueue()[static_cast<size_t>(i)].isSkipped)
                    sessionOffset += suiteList.getQueue()[static_cast<size_t>(i)].totalPoints;
            }
            size_t globalPointIdx = static_cast<size_t>(sessionOffset + pointIdx);
            if (globalPointIdx < sessionManager.getPointCount())
            {
                const auto& pt = *sessionManager.getPoint(globalPointIdx);
                prompt += " | Gain: " + juce::String(pt.secondaryValue.mean, 2) + " dB, THD: " + juce::String(pt.thdPercent, 2) + "%, SNR: " + juce::String(pt.snrDb, 1) + " dB";
                pSteps = pt.controlSteps;

                if (!pt.irSamples.empty())
                {
                    double sRate = audioEngine.getCurrentSampleRate();
                    juce::String label = "Point #" + juce::String(pointIdx + 1) + " (" + juce::String(stepPct, 1) + "%)";
                    curvePlotter.getSpectrumAnalyzer().setCapturedSignal(pt.irSamples, sRate, label);
                }
            }

            if (pSteps.empty())
            {
                size_t numControls = item.controls.size();
                if (numControls > 0)
                {
                    std::vector<int> stepsPerControl(numControls);
                    for (size_t k = 0; k < numControls; ++k)
                        stepsPerControl[k] = std::max(1, item.controls[k].steps);

                    int temp = pointIdx;
                    std::vector<int> stepIndices(numControls);
                    for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
                    {
                        stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
                        temp /= stepsPerControl[static_cast<size_t>(k)];
                    }

                    for (size_t k = 0; k < numControls; ++k)
                    {
                        const auto& c = item.controls[k];
                        int stepIdx = stepIndices[k];
                        int sCount = stepsPerControl[k];
                        float minN = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
                        float maxN = std::clamp(c.maxPct / 100.0f, minN, 1.0f);
                        float normVal = (sCount > 1)
                            ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                            : (minN + maxN) * 0.5f;

                        core::ParameterStep ps;
                        ps.paramIndex = static_cast<int>(k) + 1;
                        ps.paramName = c.name.toStdString();
                        ps.controlType = c.type.isEmpty() ? "Knob" : c.type.toStdString();
                        ps.minNormalized = minN;
                        ps.maxNormalized = maxN;
                        ps.normalizedValue = normVal;
                        ps.rawValue = static_cast<int>(std::round(normVal * 127.0f));
                        ps.id = c.id.isNotEmpty() ? c.id.toStdString() : ("ctrl_" + std::to_string(k + 1));
                        ps.sortOrder = c.sortOrder;
                        pSteps.push_back(ps);
                    }
                }
                else
                {
                    float norm = (item.totalPoints > 1) ? (static_cast<float>(pointIdx) / static_cast<float>(item.totalPoints - 1)) : 0.0f;
                    core::ParameterStep ps;
                    ps.paramName = "Parameter 1";
                    ps.normalizedValue = norm;
                    ps.controlType = "Knob";
                    pSteps.push_back(ps);
                }
            }

            suiteList.setVisible(false);
            operatorStepModal.showInspector(item.title, pointIdx + 1, item.totalPoints, pSteps, prompt);
            resized();
        }
    };

    suiteList.onClearPointClicked = [this](int queueIdx, int pointIdx) {
        handleClearPoint(queueIdx, pointIdx);
    };

    suiteList.onDeletePointClicked = [this](int queueIdx, int pointIdx) {
        handleClearPoint(queueIdx, pointIdx);
    };

    suiteList.onRerunSelectedClicked = [this](const std::vector<std::pair<int, int>>& points) {
        startTargetedPatchSession(points);
    };

    suiteList.onToggleSessionRunClicked = [this](bool start) {
        if (start)
        {
            if (!mainHeader.hasHardwareSelected())
            {
                suiteList.setSessionRunning(false);
                drawer.openHardwareDrawer();
                manualPromptLabel.setText("Please select a Target Hardware device before starting the session.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                hidePromptAfterDelay(5000);
                return;
            }
            startProfilingSession(false);
        }
        else stopProfilingSession();
    };

    suiteList.onDuplicateWarning = [this](const juce::String& msg) {
        manualPromptLabel.setText(msg, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
    };
    addAndMakeVisible(suiteList);

    // 6. Right Master Level & Meter Strip
    meterStrip.onProfilingToggled = [this](bool start) {
        if (start)
        {
            if (!mainHeader.hasHardwareSelected())
            {
                suiteList.setSessionRunning(false);
                meterStrip.setProfilingActive(false);
                drawer.openHardwareDrawer();
                manualPromptLabel.setText("Please select a Target Hardware device before starting the session.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                hidePromptAfterDelay(5000);
                return;
            }
            startProfilingSession();
        }
        else stopProfilingSession();
    };
    meterStrip.onAutoTrimClicked = [this] {
        audioEngine.performAutoGainTrim();
    };
    addAndMakeVisible(meterStrip);

    // 6. Manual Adjustment Prompt & Operator Correction Controls (Hidden by default)
    manualPromptLabel.setText("", juce::dontSendNotification);
    manualPromptLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
    manualPromptLabel.setColour(juce::Label::backgroundColourId, gui::SoundIdTheme::bgCard);
    manualPromptLabel.setVisible(false);
    addChildComponent(manualPromptLabel);

    btnStepBack.setButtonText("<- STEP BACK");
    btnStepBack.setTooltip("Return to previous measurement step to redo or adjust physical knob");
    btnStepBack.onClick = [this] { sessionCoordinator.stepBack(); };
    btnStepBack.setEnabled(false);
    btnStepBack.setVisible(false);
    addChildComponent(btnStepBack);

    btnRepeatStep.setButtonText("REPEAT STEP");
    btnRepeatStep.setTooltip("Re-measure current knob position in case of audio glitch or misadjustment");
    btnRepeatStep.onClick = [this] { sessionCoordinator.repeatCurrentStep(); };
    btnRepeatStep.setEnabled(false);
    btnRepeatStep.setVisible(false);
    addChildComponent(btnRepeatStep);

    confirmManualButton.setButtonText("Confirm Step (Space)");
    confirmManualButton.setTooltip("Confirm Step - Signal the sequencer that hardware control is positioned and proceed with stimulus");
    confirmManualButton.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::pillBlackBg);
    confirmManualButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    confirmManualButton.setEnabled(false);
    confirmManualButton.setVisible(false);
    confirmManualButton.onClick = [this] { confirmManualStep(); };
    addChildComponent(confirmManualButton);

    // Measurement Workspace Governance & Dual Mode UI
    btnModeToggle.setButtonText("Modo: Guiado");
    btnModeToggle.setTooltip("Alternar entre Modo Guiado (asistente paso a paso) y Modo Libre (captura ad-hoc)");
    btnModeToggle.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnModeToggle.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentBlue);
    btnModeToggle.onClick = [this] {
        auto currentMode = sessionCoordinator.getWorkspaceInteractionMode();
        auto targetMode = (currentMode == measurement::WorkspaceInteractionMode::Guided)
                              ? measurement::WorkspaceInteractionMode::Free
                              : measurement::WorkspaceInteractionMode::Guided;
        if (!sessionCoordinator.switchWorkspaceInteractionMode(targetMode))
        {
            lblActionReasonBanner.setText(sessionCoordinator.getRejectionReasonForAction("change_mode"), juce::dontSendNotification);
            lblActionReasonBanner.setVisible(true);
            return;
        }
        updateGovernanceUi();
    };
    addAndMakeVisible(btnModeToggle);

    lblHeaderStatusBadge.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    lblHeaderStatusBadge.setJustificationType(juce::Justification::centred);
    lblHeaderStatusBadge.setColour(juce::Label::backgroundColourId, gui::SoundIdTheme::bgCard);
    addAndMakeVisible(lblHeaderStatusBadge);

    lblActionReasonBanner.setFont(juce::FontOptions(12.0f));
    lblActionReasonBanner.setJustificationType(juce::Justification::centredLeft);
    lblActionReasonBanner.setVisible(false);
    addChildComponent(lblActionReasonBanner);

    btnFreeCapture.setButtonText(juce::String::fromUTF8(u8"Capturar Toma Libre"));
    btnFreeCapture.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentGreen.withAlpha(0.2f));
    btnFreeCapture.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentGreen);
    btnFreeCapture.onClick = [this] {
        if (!sessionCoordinator.isDirectCaptureAllowed())
        {
            lblActionReasonBanner.setText(sessionCoordinator.getRejectionReasonForAction("capture"), juce::dontSendNotification);
            lblActionReasonBanner.setVisible(true);
            return;
        }
        juce::String hid = drawer.getSelectedHardwareId();
        juce::String fid = drawer.getSelectedFunctionId();
        if (hardwareManager.isAutonomousSynth(hid, fid) || pluginUiCoordinator.hasActivePlugin())
        {
            audioEngine.postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.8f));
            juce::Timer::callAfterDelay(1200, [this] {
                audioEngine.postLiveMidiMessage(juce::MidiMessage::noteOff(1, 60, 0.0f));
            });
        }
        sessionCoordinator.triggerFreeCapture();
    };
    addChildComponent(btnFreeCapture);

    btnFreeStop.setButtonText("Detener");
    btnFreeStop.setColour(juce::TextButton::buttonColourId, juce::Colours::coral.withAlpha(0.2f));
    btnFreeStop.setColour(juce::TextButton::textColourOffId, juce::Colours::coral);
    btnFreeStop.onClick = [this] {
        if (!sessionCoordinator.isCancellationAllowed())
        {
            lblActionReasonBanner.setText(sessionCoordinator.getRejectionReasonForAction("cancel"), juce::dontSendNotification);
            lblActionReasonBanner.setVisible(true);
            return;
        }
        sessionCoordinator.triggerStopSession();
    };
    addChildComponent(btnFreeStop);

    btnPrimaryAction.setButtonText(juce::String::fromUTF8(u8"▶  INICIAR MEDICIÓN"));
    btnPrimaryAction.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentGreen);
    btnPrimaryAction.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnPrimaryAction.onClick = [this] {
        auto state = sessionCoordinator.getCoordinatorState();
        bool isRunning = sessionCoordinator.isRunningSession();

        if (state == measurement::CoordinatorState::SessionCompleted)
        {
            workflowNavController.setStep(gui::WorkflowNavigationController::Step::ExportReport);
            return;
        }

        if (isRunning)
        {
            sessionCoordinator.togglePauseSession();
        }
        else
        {
            startProfilingSession(false);
        }
    };
    addChildComponent(btnPrimaryAction);

    btnCancelAction.setButtonText("CANCELAR");
    btnCancelAction.setColour(juce::TextButton::buttonColourId, juce::Colours::coral.withAlpha(0.25f));
    btnCancelAction.setColour(juce::TextButton::textColourOffId, juce::Colours::coral);
    btnCancelAction.onClick = [this] {
        stopProfilingSession();
    };
    addChildComponent(btnCancelAction);

    // 7. Slide-In Drawer & Modals (Overlays on top)
    drawer.onHardwareSelected = [this](const juce::String& hwId, const juce::String& funcId) {
        onHardwareSelected(hwId, funcId);
        hardwareRoutingPanel.setSelectedHardware(hwId, funcId);
    };
    drawer.onDeviceDetected = [this](const juce::String& displayName) {
        hardwareRoutingPanel.setAutoDetectButtonText("✓ " + displayName);
        manualPromptLabel.setText("Dispositivo detectado vía MIDI: " + displayName, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
    };
    drawer.onChangeExportFolderClicked = [this] {
        chooseExportFolder();
    };
    drawer.onOpenAudioSettingsClicked = [this] {
        openAudioMidiSettings();
    };
    drawer.onAboutClicked = [this] {
        showAboutDialog();
    };
    drawer.onRefreshSetupRequested = [this] {
        updateSetupDrawerInfo();
        manualPromptLabel.setText(juce::String::fromUTF8(u8"✓ Telemetría y conexiones actualizadas."), juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(3000);
        if (topologyController.isWindowVisible())
            toggleStudioTopologyWindow();
    };
    drawer.onNewSessionClicked = [this] {
        promptNewSession();
    };
    drawer.onOpenSessionClicked = [this] {
        handleOpenSession();
    };
    drawer.onSaveSessionClicked = [this] {
        handleSaveSession();
    };
    drawer.onSaveSessionAsClicked = [this] {
        handleSaveSessionAs();
    };
    drawer.onReanalyzeSessionClicked = [this] {
        performOfflineReanalysis();
    };
    drawer.onRevealExportFolderClicked = [this] {
        reportExportController.openExportFolderInExplorer();
    };
    drawer.onExportReportClicked = [this] {
        reportExportController.requestExportCertificationReport();
    };
    drawer.onExitAppClicked = [this] {
        confirmAndExit();
    };
    drawer.onCheckUpdatesClicked = [this] {
        checkForAppUpdates(true);
    };
    drawer.onNewFlowRequested = [this] {
        if (sessionManager.isDirty() && sessionManager.hasPoints())
        {
            confirmationModal.show(
                this,
                "Iniciar Nuevo Flujo",
                juce::String::fromUTF8(u8"¿Desea cambiar de hardware e iniciar un nuevo flujo?\nSe reiniciarán los puntos medidos de la sesión actual."),
                "Iniciar Nuevo Flujo",
                "",
                "Cancelar",
                [this](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        performNewSessionReset();
                        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::HardwareRouting);
                        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Current);
                        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Pending);
                        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, gui::WorkflowStepperBar::StepStatus::Pending);
                        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::ExportReport, gui::WorkflowStepperBar::StepStatus::Pending);
                    }
                }
            );
        }
        else
        {
            performNewSessionReset();
            stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::HardwareRouting);
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Current);
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Pending);
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, gui::WorkflowStepperBar::StepStatus::Pending);
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::ExportReport, gui::WorkflowStepperBar::StepStatus::Pending);
        }
    };
    addChildComponent(drawer);

    initializeAutoUpdater();

    loopbackModal.onCalibrationApplied = [this](const math::LoopbackCalibrationData& cal) {
        float gainDb = 20.0f * std::log10(std::max(cal.recommendedTrimGain, 1e-4f));
        juce::String sign = (gainDb >= 0.0f) ? "+" : "";
        juce::String msg = "Loopback Calibration complete! Auto-trim applied: " + 
                           sign + juce::String(gainDb, 1) + " dB (Target: -3.0 dBfs)";
        manualPromptLabel.setText(msg, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);

        if (cal.isCalibrated)
        {
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Completed);
            stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::RunSession);
        }
    };

    loopbackModal.onCalibrationSkipped = [this] {
        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Skipped);
        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::RunSession);
        if (stepperBar.onStepSelected != nullptr)
            stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::RunSession);

        mainHeader.updateCalibrationStatus(false, 0.0, true);

        manualPromptLabel.setText("Paso 2 Omitido: Operando con ganancia nominal (0 dB). ¡Paso 3 y 4 habilitados!", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(5000);
        resized();
    };
    addChildComponent(loopbackModal);
    addChildComponent(aboutModal);
    addChildComponent(operatorStepModal);
    addChildComponent(confirmationModal);
    addChildComponent(abVerificationModal);

    operatorStepModal.onAccept = [this] { confirmManualStep(); };
    operatorStepModal.onRepeat = [this] { sessionCoordinator.repeatCurrentStep(); };
    operatorStepModal.onStepBack = [this] { sessionCoordinator.stepBack(); };
    operatorStepModal.onCancel = [this] { stopProfilingSession(); };
    operatorStepModal.onCollapseToggled = [this](bool isCollapsed) {
        juce::ignoreUnused(isCollapsed);
        resized();
    };
    operatorStepModal.onCloseInspector = [this] {
        suiteList.setVisible(true);
        resized();
    };
    operatorStepModal.onMetronomeTick = [this](int /*sec*/) {
        audioEngine.triggerMetronomeTick();
    };

    // 8. Session Execution Coordinator (Frente 8.5-C: Thread-Safe Mediator)
    sessionCoordinator.setCoordinatedViews(&suiteList,
                                           &healthPanel,
                                           &operatorStepModal,
                                           &manualPromptLabel,
                                           &btnStepBack,
                                           &btnRepeatStep,
                                           &confirmManualButton);
    sessionCoordinator.setHardwareContext(&hardwareManager, drawer.getSelectedHardwareId());

    sessionCoordinator.onExecutionStateChanged = [this](bool isRunning) {
        meterStrip.setProfilingActive(isRunning);
        suiteList.setSessionRunning(isRunning);
        if (!isRunning)
            resized();
    };

    sessionCoordinator.onSessionFinished = [this](bool isPatching) {
        if (!isPatching)
        {
            stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, gui::WorkflowStepperBar::StepStatus::Completed);
            stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::ExportReport);
        }
        updateExportReportMetrics();
        sessionManager.triggerAutoSave(buildCurrentSessionManifest());
        resized();
    };

    sessionCoordinator.onSessionAutoSaveRequested = [this] {
        sessionManager.triggerAutoSave(buildCurrentSessionManifest());
    };

    sessionCoordinator.onCoordinatorStateChanged = [this](measurement::CoordinatorState /*oldSt*/,
                                                         measurement::CoordinatorState /*newSt*/,
                                                         const juce::String& /*reason*/) {
        juce::MessageManager::callAsync([this]() {
            updateGovernanceUi();
        });
    };
    updateGovernanceUi();

    // Phase 14: Pause/Resume — master button cycles; LED state synced back to strip
    meterStrip.onPauseResumeClicked = [this] {
        sessionCoordinator.togglePauseSession();
    };
    sessionCoordinator.onSessionPauseStateChanged = [this](bool isPaused) {
        meterStrip.setSessionPaused(isPaused);
    };

    // Phase 14: Single-point live re-run from context menu
    suiteList.onRerunPointRequested = [this](int queueIndex, int pointIndex) {
        if (!sessionCoordinator.isRunningSession()) return;
        // Compute the flat globalPointIndex by walking the queue up to (queueIndex, pointIndex)
        int globalIdx = 0;
        const auto& queue = suiteList.getQueue();
        for (int qi = 0; qi < static_cast<int>(queue.size()); ++qi)
        {
            const auto& item = queue[static_cast<size_t>(qi)];
            if (item.isSkipped) continue;
            if (qi == queueIndex)
            {
                globalIdx += pointIndex;
                break;
            }
            globalIdx += static_cast<int>(item.pointStatuses.size());
        }
        sessionCoordinator.rerunSelectedPoint(globalIdx);
        // Reset the cell status so the user sees the re-run starting
        suiteList.setPointStatus(queueIndex, pointIndex, gui::PointStatus::Running);
    };

    sessionCoordinator.wireSequencerCallbacks();

    addKeyListener(this);
    setWantsKeyboardFocus(true);
    audioEngine.getDeviceManager().addChangeListener(this);
    startTimerHz(60);

    // Ensure initial state starts completely clean with no hardware selected
    drawer.clearSelectedHardware();
    hardwareRoutingPanel.resetSelection();
    catalogSelector.resetSelection();
    mainHeader.clearHardware();
    suiteList.setStandardTestAvailable(false);
    suiteList.clearQueue();

    setSize(1240, 780);

    report("Precalentando Monitor Reactivo de Hardware MIDI...", 0.88f);
    // Pre-warm MIDI Hardware Hotplug Monitor & Detector in background
    hardwareManager.getHotplugMonitor().preWarmAsync();

    report("Precalentando Motores Web Chromium (Scope & Detector)...", 0.96f);
    // Pre-warm WebView2 for ABDScope & Hardware Detector in background to eliminate first-click cold-start lag
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainContentComponent>(this)]() {
        if (safeThis != nullptr)
        {
            safeThis->preWarmScopeWindow();
            safeThis->preWarmHardwareDetector();
        }
    });

    // Inicializar Contenedor del Flujo Guiado SoundID y Toggle de Modo (Fase 16 / 20.7)
    guidedWorkflowContainer = std::make_unique<gui::soundid::SoundIdGuidedWorkflowContainer>(profilingSessionController);
    addChildComponent(guidedWorkflowContainer.get());

    profilingRunView = std::make_unique<gui::soundid::SoundIdProfilingRunView>(profilingSessionController);
    profilingRunView->onStartClicked = [this] {
        startProfilingSession(false);
    };
    profilingRunView->onPauseClicked = [this] {
        sessionCoordinator.togglePauseSession();
    };
    profilingRunView->onCancelClicked = [this] {
        stopProfilingSession();
    };
    addChildComponent(profilingRunView.get());

    btnWorkflowModeToggle.setVisible(false);

    setupGuidedWorkflowInitialData();

    report("Listo.", 1.0f);
}

MainContentComponent::~MainContentComponent()
{
    juce::Logger::writeToLog("[MainComponent] Destructor: closing plugin window and resetting active plugin.");
    pluginUiCoordinator.unloadPlugin();

    audioEngine.getDeviceManager().removeChangeListener(this);

    if (scopeWebWindow != nullptr)
    {
        scopeWebWindow->setVisible(false);
        scopeWebWindow = nullptr;
    }

    if (virtualKeyboardWindow != nullptr)
    {
        virtualKeyboardWindow->setVisible(false);
        virtualKeyboardWindow = nullptr;
    }

    if (measurementViewerWindow != nullptr)
    {
        measurementViewerWindow->setVisible(false);
        measurementViewerWindow = nullptr;
    }

    if (measurementComparisonWindow != nullptr)
    {
        measurementComparisonWindow->setVisible(false);
        measurementComparisonWindow = nullptr;
    }

    topologyController.closeWindow();

    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
    removeKeyListener(this);
    stopTimer();
    sessionCoordinator.triggerStopSession();
    audioEngine.saveAudioSettings(settingsFile);
}


void MainContentComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioEngine.getDeviceManager())
    {
        mainHeader.updateAudioMidiStatus();
        updateSetupDrawerInfo();

        if (topologyController.isWindowVisible())
        {
            toggleStudioTopologyWindow();
        }
    }
}

bool MainContentComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    bool isCmdOrCtrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    bool isShift = key.getModifiers().isShiftDown();

    if (isCmdOrCtrl && isShift && key.getKeyCode() == 'S')
    {
        handleSaveSessionAs();
        return true;
    }
    else if (isCmdOrCtrl && key.getKeyCode() == 'S')
    {
        handleSaveSession();
        return true;
    }
    else if (isCmdOrCtrl && key.getKeyCode() == 'N')
    {
        promptNewSession();
        return true;
    }
    else if (isCmdOrCtrl && key.getKeyCode() == 'O')
    {
        handleOpenSession();
        return true;
    }
    else if (key.isKeyCode(juce::KeyPress::spaceKey))
    {
        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided &&
            sessionCoordinator.isManualConfirmationAllowed())
        {
            confirmManualStep();
            return true;
        }
    }
    else if (key.isKeyCode(juce::KeyPress::escapeKey))
    {
        if (sessionCoordinator.isCancellationAllowed())
        {
            sessionCoordinator.triggerStopSession();
            return true;
        }
    }
    return false;
}

// ==============================================================================
// SECTION 5: UI GOVERNANCE & INTERACTION MODES (GUIDED / LAB BENCH)
// Owns studio step state machine UI representation, view mode toggling (Guided vs Lab Bench), and action guards.
// Session execution state delegated to SessionExecutionCoordinator; hardware capability validation to ContractRegistry.
// ==============================================================================
void MainContentComponent::updateGovernanceUi()
{
    bool isRunSessionStep = (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession);
    if (!isRunSessionStep)
    {
        btnModeToggle.setVisible(false);
        lblHeaderStatusBadge.setVisible(false);
        btnFreeCapture.setVisible(false);
        btnFreeStop.setVisible(false);
        btnPrimaryAction.setVisible(false);
        btnCancelAction.setVisible(false);
        lblActionReasonBanner.setVisible(false);
        return;
    }

    btnModeToggle.setVisible(true);
    lblHeaderStatusBadge.setVisible(true);

    // Hide suiteList run button to avoid duplicate conflicting execution buttons
    suiteList.setRunButtonVisible(false);

    auto mode = sessionCoordinator.getWorkspaceInteractionMode();
    auto state = sessionCoordinator.getCoordinatorState();
    int currentPt = sessionCoordinator.getTotalPointsMeasured();
    int totalPts = suiteList.getQueueSize();

    auto sessState = sessionCoordinator.getSessionState();
    unsigned progressPct = totalPts > 0 ? static_cast<unsigned>(currentPt * 100 / totalPts) : 0;

    juce::String errMessage;
    if (sessState == gui::SessionState::Failed || state == measurement::CoordinatorState::Error)
    {
        const auto& hist = sessionCoordinator.getTransitionHistory();
        errMessage = hist.empty() ? juce::String("Error en el flujo de medicion") : juce::String(hist.back().reason);
    }
    const auto sessionStatus = gui::presentation::SessionStatusPresenter::present(sessState, progressPct, errMessage);

    // 1. Mode Text
    juce::String modeStr = (mode == measurement::WorkspaceInteractionMode::Guided) ? gui::strings::MODE_GUIDED : gui::strings::MODE_LAB;
    btnModeToggle.setButtonText(modeStr);
    btnModeToggle.setEnabled(sessionCoordinator.isModeChangeAllowed());
    if (!sessionCoordinator.isModeChangeAllowed())
        btnModeToggle.setTooltip(sessionCoordinator.getRejectionReasonForAction("change_mode"));
    else
        btnModeToggle.setTooltip(gui::strings::TOOLTIP_MODE_TOGGLE);

    // 2. Lifecycle State Text and Colors (from pure SessionStatusPresenter)
    juce::String stateStr = sessionStatus.statusText;
    juce::Colour stateCol = sessionStatus.badgeColour;

    // 3. Point Progress & Live Telemetry Text
    juce::String ptStr = (mode == measurement::WorkspaceInteractionMode::Guided)
                             ? ("Point: " + juce::String(currentPt) + " of " + juce::String(std::max(currentPt, totalPts)))
                             : ("Recorded takes: " + juce::String(currentPt));

    float liveRms = audioEngine.getLastPluginOutputRms();
    int lastNote = audioEngine.getLastNoteOnNumber();
    juce::String telemetrySuffix;
    if (liveRms > 0.00001f)
    {
        float rmsDb = juce::Decibels::gainToDecibels(liveRms);
        telemetrySuffix = "  |  RMS: " + juce::String(rmsDb, 1) + " dBFS";
        if (lastNote >= 0)
            telemetrySuffix += " (" + juce::MidiMessage::getMidiNoteName(lastNote, true, true, 3) + ")";
    }

    lblHeaderStatusBadge.setText("  " + modeStr + "  |  " + stateStr + "  |  " + ptStr + telemetrySuffix + "  ", juce::dontSendNotification);
    lblHeaderStatusBadge.setColour(juce::Label::textColourId, stateCol);
    lblHeaderStatusBadge.setColour(juce::Label::outlineColourId, stateCol.withAlpha(0.6f));

    // 4. Button Enablement & Explanatory Tooltips
    bool canConfirm = sessionCoordinator.isManualConfirmationAllowed();
    confirmManualButton.setEnabled(canConfirm);
    if (!canConfirm)
        confirmManualButton.setTooltip(sessionCoordinator.getRejectionReasonForAction("confirm"));
    else
        confirmManualButton.setTooltip(gui::strings::TOOLTIP_CONFIRM_MANUAL);

    bool isFreeMode = (mode == measurement::WorkspaceInteractionMode::Free);
    bool isRunning = sessionCoordinator.isRunningSession();
    bool isPaused = sessionCoordinator.isSessionPaused();
    bool isCompleted = (sessState == gui::SessionState::Completed || state == measurement::CoordinatorState::SessionCompleted);

    if (isFreeMode)
    {
        btnPrimaryAction.setVisible(false);
        btnCancelAction.setVisible(false);

        btnFreeCapture.setButtonText(gui::strings::FREE_CAPTURE);
        btnFreeCapture.setVisible(true);
        bool canCapture = sessionCoordinator.isDirectCaptureAllowed();
        btnFreeCapture.setEnabled(canCapture);
        if (!canCapture)
            btnFreeCapture.setTooltip(sessionCoordinator.getRejectionReasonForAction("capture"));
        else
            btnFreeCapture.setTooltip(gui::strings::TOOLTIP_FREE_CAPTURE);

        btnFreeStop.setButtonText(gui::strings::STOP);
        btnFreeStop.setVisible(true);
        bool canCancel = sessionCoordinator.isCancellationAllowed();
        btnFreeStop.setEnabled(canCancel);
        if (!canCancel)
            btnFreeStop.setTooltip(sessionCoordinator.getRejectionReasonForAction("cancel"));
        else
            btnFreeStop.setTooltip(gui::strings::TOOLTIP_FREE_STOP);
    }
    else
    {
        btnFreeCapture.setVisible(false);
        btnFreeStop.setVisible(false);
        btnPrimaryAction.setVisible(true);

        if (isCompleted)
        {
            btnPrimaryAction.setButtonText(gui::strings::VIEW_RESULTS_EXPORT);
            btnPrimaryAction.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentPurple);
            btnPrimaryAction.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btnPrimaryAction.setEnabled(true);
            btnPrimaryAction.setTooltip(gui::strings::TOOLTIP_VIEW_RESULTS);
            btnCancelAction.setVisible(false);
        }
        else if (isRunning)
        {
            if (isPaused)
            {
                btnPrimaryAction.setButtonText(gui::strings::RESUME);
                btnPrimaryAction.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentGreen);
                btnPrimaryAction.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                btnPrimaryAction.setTooltip(gui::strings::TOOLTIP_RESUME);
            }
            else
            {
                btnPrimaryAction.setButtonText(gui::strings::PAUSE);
                btnPrimaryAction.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentAmber.withAlpha(0.35f));
                btnPrimaryAction.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentAmber);
                btnPrimaryAction.setTooltip(gui::strings::TOOLTIP_PAUSE);
            }
            btnPrimaryAction.setEnabled(sessionStatus.primaryEnabled);

            btnCancelAction.setButtonText(gui::strings::CANCEL);
            btnCancelAction.setVisible(sessionStatus.cancelVisible);
            btnCancelAction.setEnabled(true);
            btnCancelAction.setTooltip(gui::strings::TOOLTIP_CANCEL);
        }
        else
        {
            btnPrimaryAction.setButtonText(gui::strings::START_MEASUREMENT);
            btnPrimaryAction.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentGreen);
            btnPrimaryAction.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

            bool canStart = (state == measurement::CoordinatorState::SessionReady
                             || state == measurement::CoordinatorState::ProfileSelected
                             || (state != measurement::CoordinatorState::NoSession && suiteList.getQueueSize() > 0));
            btnPrimaryAction.setEnabled(canStart && sessionStatus.primaryEnabled);
            if (!canStart)
                btnPrimaryAction.setTooltip(gui::strings::TOOLTIP_START_BLOCKED);
            else
                btnPrimaryAction.setTooltip(gui::strings::TOOLTIP_START_READY);

            btnCancelAction.setVisible(sessionStatus.cancelVisible);
        }
    }

    // 6. Persistent Operator Instructions & Error Banners
    if (sessionStatus.bannerVisible)
    {
        lblActionReasonBanner.setText(sessionStatus.bannerText, juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, sessionStatus.bannerColour);
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::AwaitingManualConfirmation)
    {
        lblActionReasonBanner.setText("Paso de alineacion manual pendiente. Ajuste el control fisico y pulse Confirmar o la barra espaciadora.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::Capturing)
    {
        lblActionReasonBanner.setText("Capturando audio inmutable: Cambio de perfil y controles bloqueados durante la grabacion.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, juce::Colour(0xff3498db));
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::Aborted)
    {
        lblActionReasonBanner.setText("Sesion cancelada. Las tomas previas selladas se han preservado.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, juce::Colours::grey);
        lblActionReasonBanner.setVisible(true);
    }
    else
    {
        lblActionReasonBanner.setVisible(false);
    }

    resized();
    repaint();
}

// ==============================================================================
// SECTION 6: COMPONENT SIZING, SPLITTER & STEP LAYOUT
// Owns component bounds calculation, responsive splitter positioning, paint dispatch, and UI refresh timer.
// Child element internal layout delegated to individual view components.
// ==============================================================================
void MainContentComponent::updateSplitLayout()
{
    switch (centerSplitMode)
    {
        case CenterSplitMode::Balanced:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (pointing down to expand downwards)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (pointing up to expand upwards)
            curvePlotter.setCollapsed(false);
            suiteList.setCollapsed(false);
            targetBottomH = balancedBottomH;
            break;

        case CenterSplitMode::GraphMaximized:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
            targetBottomH = 36.0f;
            break;

        case CenterSplitMode::QueueMaximized:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
            {
                auto totalArea = getLocalBounds().reduced(20);
                // Reserve at least 180px graph + 32px health + 12px splitter + 48px top header
                int minTopAndGraphH = 48 + 180 + 32 + 12;
                int maxH = totalArea.getHeight() - minTopAndGraphH;
                targetBottomH = static_cast<float>(std::max(140, maxH));
            }
            break;
    }
}

void MainContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(gui::SoundIdTheme::bgLight);
}

void MainContentComponent::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // 1. Top Header Area (Single Coordinated Component)
    auto headerRow = bounds.removeFromTop(36);
    mainHeader.setBounds(headerRow);

    bounds.removeFromTop(10);

    // En modo guiado, el contenedor ocupa todo el canvas central
    if (currentWorkflowMode == gui::session::UiWorkflowMode::Guided)
    {
        if (guidedWorkflowContainer != nullptr)
        {
            guidedWorkflowContainer->setVisible(true);
            guidedWorkflowContainer->setBounds(bounds);
        }

        // Slide-in Drawer & Modals fill full window bounds
        drawer.setBounds(getLocalBounds());
        aboutModal.setBounds(getLocalBounds());
        confirmationModal.setBounds(getLocalBounds());
        abVerificationModal.setBounds(getLocalBounds());
        return;
    }

    if (guidedWorkflowContainer != nullptr)
        guidedWorkflowContainer->setVisible(false);

    // 2. Left Collapsible Sidebar Stepper (SoundID Vertical Workflow Rail)
    int sidebarW = sidebarStepper.getDesiredWidth();
    sidebarStepper.setBounds(bounds.removeFromLeft(sidebarW));
    bounds.removeFromLeft(12);

    // 3. Right Meter Strip
    auto rightArea = bounds.removeFromRight(120);
    meterStrip.setBounds(rightArea);
    bounds.removeFromRight(12);

    // 4. In Step::RunSession, show Governance Bar at the top of the central measurement area
    if (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession)
    {
        auto govRow = bounds.removeFromTop(32);
        btnModeToggle.setBounds(govRow.removeFromLeft(110));
        govRow.removeFromLeft(10);
        lblHeaderStatusBadge.setBounds(govRow.removeFromLeft(380));
        govRow.removeFromLeft(10);

        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Free)
        {
            btnFreeCapture.setBounds(govRow.removeFromLeft(160));
            govRow.removeFromLeft(8);
            btnFreeStop.setBounds(govRow.removeFromLeft(80));
            govRow.removeFromLeft(10);
        }
        else
        {
            int btnW = (sessionCoordinator.getCoordinatorState() == measurement::CoordinatorState::SessionCompleted) ? 240 : 180;
            btnPrimaryAction.setBounds(govRow.removeFromLeft(btnW));
            govRow.removeFromLeft(8);
            if (btnCancelAction.isVisible())
            {
                btnCancelAction.setBounds(govRow.removeFromLeft(110));
                govRow.removeFromLeft(10);
            }
        }
        if (lblActionReasonBanner.isVisible())
        {
            lblActionReasonBanner.setBounds(govRow);
        }
        bounds.removeFromTop(10);
    }

    // Manual Prompt Banner & Error Correction Controls
    if (confirmManualButton.isVisible() && !operatorStepModal.isVisible())
    {
        auto manualRow = bounds.removeFromBottom(36);
        confirmManualButton.setBounds(manualRow.removeFromRight(150));
        manualRow.removeFromRight(8);
        btnRepeatStep.setBounds(manualRow.removeFromRight(100));
        manualRow.removeFromRight(8);
        btnStepBack.setBounds(manualRow.removeFromRight(100));
        manualRow.removeFromRight(8);
        manualPromptLabel.setBounds(manualRow);
        bounds.removeFromBottom(8);
    }

    // Delegate SoundID canvas switching and geometry to WorkflowNavigationController
    bool isSplittingBalanced = (centerSplitMode == CenterSplitMode::Balanced) &&
                               (std::abs(targetBottomH - currentBottomH) < 2.0f);
    workflowNavController.layoutStepViews(bounds, currentBottomH, isSplittingBalanced);

    if (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession)
    {
        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided)
        {
            if (profilingRunView != nullptr)
            {
                profilingRunView->setVisible(true);
                profilingRunView->setBounds(bounds);
            }
            curvePlotter.setVisible(false);
            suiteList.setVisible(false);
            centerSplitterBar.setVisible(false);
            healthPanel.setVisible(false);
        }
        else
        {
            if (profilingRunView != nullptr)
                profilingRunView->setVisible(false);
        }
    }
    else
    {
        if (profilingRunView != nullptr)
            profilingRunView->setVisible(false);
    }

    // 6. Slide-in Drawer & Modals fill full window bounds
    drawer.setBounds(getLocalBounds());
    aboutModal.setBounds(getLocalBounds());
    confirmationModal.setBounds(getLocalBounds());
    abVerificationModal.setBounds(getLocalBounds());
}

void MainContentComponent::timerCallback()
{
    diagnosticsTelemetryPoller.pollNow();
    animateSplitter();
}

void MainContentComponent::animateSplitter()
{
    // Slower, smooth and relaxed chevron/split animation
    if (std::abs(targetBottomH - currentBottomH) > 0.5f)
    {
        float diff = targetBottomH - currentBottomH;
        float step = diff * 0.07f;
        if (std::abs(step) < 0.35f)
            step = (diff > 0.0f ? 0.35f : -0.35f);

        if (std::abs(diff) <= std::abs(step))
            currentBottomH = targetBottomH;
        else
            currentBottomH += step;

        resized();
    }
    else if (currentBottomH != targetBottomH)
    {
        currentBottomH = targetBottomH;
        if (centerSplitMode == CenterSplitMode::GraphMaximized)
            suiteList.setCollapsed(true);
        else if (centerSplitMode == CenterSplitMode::QueueMaximized)
            curvePlotter.setCollapsed(true);
        resized();
    }
}

void MainContentComponent::applyTelemetrySnapshot(const gui::TelemetrySnapshot& snap)
{
    meterStrip.setLevels(snap.inputPeakL, snap.inputPeakR, snap.inputRmsL,
                         snap.outputPeakL, snap.outputPeakR, snap.outputRmsL);

    if (snap.spectrumReady)
    {
        curvePlotter.getSpectrumAnalyzer().pushSpectrumData(snap.fftMagnitudes, snap.sampleRate);
    }

    if (snap.calibrationTickDue)
    {
        mainHeader.updateCalibrationStatus(snap.isCalibrated, snap.calibrationSampleRate, snap.isCalibrationSkipped);
    }

    if (profilingRunView != nullptr && profilingRunView->isVisible())
    {
        auto profileSnap = profilingSessionController.getCurrentSnapshot();
        profileSnap.progress.currentTrial = snap.currentTrial;
        profileSnap.progress.totalTrials = snap.totalTrials;
        profileSnap.progress.progressPercent = snap.progressPercent;
        profileSnap.observation.lastRmsDb = snap.lastPluginOutputRmsDb;
        profileSnap.progress.currentStimulusDescription = snap.stimulusDescription;

        switch (snap.sessionStateCode)
        {
            case 1: profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Profiling; break;
            case 2: profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Paused; break;
            case 3: profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Completed; break;
            case 4: profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Cancelled; break;
            default: profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::ReadyToProfile; break;
        }

        profilingRunView->updateFromSnapshot(profileSnap);
    }

    if (guidedWorkflowContainer != nullptr && currentWorkflowMode == gui::session::UiWorkflowMode::Guided)
    {
        guidedWorkflowContainer->updateTelemetry(snap.sampleRate, snap.bufferSizeSamples, snap.cpuUsagePercent);
    }
}

// ==============================================================================
// SECTION 7: TARGET HARDWARE SELECTION & WORKSPACE NAVIGATION
// Owns hardware target selection UI dispatch, setup drawer interactions, and auxiliary tool window triggers.
// Hardware discovery delegated to AudioMidiInterfaceDetector; contract resolution to HardwareProfileManager.
// ==============================================================================
void MainContentComponent::preWarmScopeWindow()
{
    if (scopeWebWindow == nullptr)
    {
        scopeWebWindow = std::make_unique<gui::ScopeWebFloatingWindow>(
            audioEngine,
            [this] {
                audioEngine.getScopeCollector().deactivateAll();
            }
        );
        scopeWebWindow->setVisible(false);
    }
}

void MainContentComponent::preWarmHardwareDetector()
{
    drawer.preWarmHardwareDetector();
}

void MainContentComponent::performOfflineReanalysis()
{
    if (!sessionManager.hasPoints())
    {
        manualPromptLabel.setText("No points in session to re-analyze.", juce::dontSendNotification);
        manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(3000);
        return;
    }

    manualPromptLabel.setText("Re-analyzing session offline from raw audio...", juce::dontSendNotification);
    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
    manualPromptLabel.setVisible(true);

    int count = sessionManager.reanalyzeSessionOffline([this](float progress, const core::SessionManager::ReanalysisProgress& info) {
        juce::ignoreUnused(progress, info);
    });

    // Refresh visualizer with updated metrics
    curvePlotter.clear();
    for (size_t i = 0; i < sessionManager.getPointCount(); ++i)
    {
        if (const auto* pt = sessionManager.getPoint(i))
            curvePlotter.addMeasuredPoint(*pt);
    }

    // Auto-save reanalyzed results into session container
    sessionManager.triggerAutoSave(buildCurrentSessionManifest());

    manualPromptLabel.setText("Offline Re-Analysis complete: " + juce::String(count) + " points recomputed.", juce::dontSendNotification);
    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4500);
}

void MainContentComponent::toggleScopeWebWindow()
{
    if (scopeWebWindow == nullptr)
    {
        scopeWebWindow = std::make_unique<gui::ScopeWebFloatingWindow>(
            audioEngine,
            [this] {
                audioEngine.getScopeCollector().deactivateAll();
            }
        );
    }

    scopeWebWindow->updateTheme();

    if (scopeWebWindow->isVisible())
    {
        scopeWebWindow->toFront(true);
    }
    else
    {
        scopeWebWindow->setVisible(true);
        scopeWebWindow->toFront(true);
        scopeWebWindow->onWindowShown();
    }
}

void MainContentComponent::toggleVirtualKeyboardWindow()
{
    auto themeStr = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark) ? "audiolab" : "audiolab-light";

    if (virtualKeyboardWindow == nullptr)
    {
        virtualKeyboardWindow = std::make_unique<abd::keyboard::MidiKeyboardFloatingWindow>(
            themeStr,
            [this](const juce::MidiMessage& msg) {
                audioEngine.postLiveMidiMessage(msg);
            }
        );
        virtualKeyboardWindow->setUsingNativeTitleBar(false);
    }

    virtualKeyboardWindow->setTheme(themeStr, gui::AppTheme::BackgroundApp);

    // Ensure direct monitoring is enabled so audio flows to speakers
    pluginUiCoordinator.setMonitoringEnabled(true);

    if (virtualKeyboardWindow->isVisible())
    {
        virtualKeyboardWindow->toFront(true);
    }
    else
    {
        virtualKeyboardWindow->setVisible(true);
        virtualKeyboardWindow->toFront(true);
    }
}

void MainContentComponent::openMeasurementViewerWindow()
{
    if (measurementViewerWindow == nullptr)
    {
        auto* panel = new gui::measurement::MeasurementViewerPanel();
        measurementViewerWindow = std::make_unique<MeasurementFloatingWindow>(
            juce::String::fromUTF8(u8"ABDAudioLab — Visor de Mediciones FAIR / LNL"),
            panel,
            1050, 720, 800, 550
        );
    }

    measurementViewerWindow->setBackgroundColour(gui::AppTheme::BackgroundApp);
    measurementViewerWindow->setVisible(true);
    measurementViewerWindow->toFront(true);
}

void MainContentComponent::openMeasurementComparisonWindow()
{
    if (measurementComparisonWindow == nullptr)
    {
        auto* panel = new gui::measurement::MeasurementComparisonPanel();
        measurementComparisonWindow = std::make_unique<MeasurementFloatingWindow>(
            juce::String::fromUTF8(u8"ABDAudioLab — Comparador Multicontenedor FAIR / LNL"),
            panel,
            1150, 750, 900, 600
        );
    }

    measurementComparisonWindow->setBackgroundColour(gui::AppTheme::BackgroundApp);
    measurementComparisonWindow->setVisible(true);
    measurementComparisonWindow->toFront(true);
}

void MainContentComponent::toggleStudioTopologyWindow()
{
    juce::String hwId = drawer.getSelectedHardwareId();
    if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();

    abd::topology::TopologyTargetInfo target;

    if (pluginUiCoordinator.hasActivePlugin())
    {
        auto desc = pluginUiCoordinator.getActivePluginDescription();
        juce::String pluginName = desc.name.isNotEmpty() ? desc.name : "Active VST3 Plugin";
        target.name = pluginName;
        target.category = desc.isInstrument ? "VST3 Virtual Instrument" : "VST3 Virtual Effect";
        target.details = "Virtual VST3 | Internal Direct Bus (ITB)";
        target.imageRelPath = desc.isInstrument ? "models/generic-digital-keyboard.png" : "models/generic-audio-rack.png";
        target.hasMidi = true;
        target.isVirtualPlugin = true;
    }
    else
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
        {
            target.name = contract->displayName;
            target.category = contract->deviceType;
            auto fnName = drawer.getActiveFunctionDisplayName().trim();
            if (fnName.isNotEmpty())
                target.details = "Profile: " + fnName;
            else
                target.details = "Profile: Default Factory Setup";

            if (contract->deviceType != "ANALOGUE_PEDAL" && 
                contract->deviceType != "MANUAL_EURORACK" && 
                contract->deviceType != "VIRTUAL_LOOPBACK_ASIO")
            {
                if (!contract->midiIdentity.model.empty() || 
                    !contract->midiIdentity.manufacturer.empty() ||
                    !contract->midiIdentity.portNameMatches.empty() ||
                    contract->deviceType == "AUTOMATED_SYSEX" || 
                    contract->deviceType == "AUTOMATED_MIDI_CC")
                {
                    target.hasMidi = true;
                }
            }

            if (!contract->modelImage.empty())
            {
                auto f = gui::locateAssetFile(contract->modelImage);
                if (f.existsAsFile())
                    target.imageRelPath = contract->modelImage;
            }
            else
            {
                std::string catLower = target.category.toStdString();
                std::transform(catLower.begin(), catLower.end(), catLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (catLower.find("pedal") != std::string::npos || catLower.find("guitar") != std::string::npos)
                    target.imageRelPath = "models/generic-guitar-pedal.png";
                else if (catLower.find("eurorack") != std::string::npos || catLower.find("modular") != std::string::npos)
                    target.imageRelPath = "models/generic-eurorack.png";
                else if (catLower.find("rack") != std::string::npos || catLower.find("studio") != std::string::npos)
                    target.imageRelPath = "models/generic-audio-rack.png";
                else if (catLower.find("anal") != std::string::npos)
                    target.imageRelPath = "models/generic-analog-keyboard.png";
                else if (catLower.find("drum") != std::string::npos)
                    target.imageRelPath = "models/generic-drum-machine.png";
                else if (catLower.find("preamp") != std::string::npos || catLower.find("mic") != std::string::npos)
                    target.imageRelPath = "models/generic-mic-preamp.png";
                else if (catLower.find("desktop") != std::string::npos)
                    target.imageRelPath = "models/generic-desktop-module.png";
                else
                    target.imageRelPath = "models/generic-digital-keyboard.png";
            }
        }
    }

    abd::topology::TopologyContext ctx {
        audioEngine.getDeviceManager(),
        target,
        (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark) ? "audiolab" : "audiolab-light",
        gui::AppTheme::BackgroundApp
    };

    topologyController.toggleWindow(ctx);
}



void MainContentComponent::updateSetupDrawerInfo()
{
    gui::TelemetryInfo info;
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    if (device != nullptr)
    {
        info.audioDeviceName = device->getName();
        info.sampleRate = device->getCurrentSampleRate();
        info.bufferSize = device->getCurrentBufferSizeSamples();
        info.latencyMs = (info.sampleRate > 0) ? (static_cast<double>(info.bufferSize) * 1000.0 / info.sampleRate) : 0.0;
    }
    else
    {
        info.audioDeviceName = "Windows Audio (Default)";
        info.sampleRate = 96000.0;
        info.bufferSize = 256;
        info.latencyMs = 2.67;
    }

    // MIDI Input real activo (solo los puertos habilitados en AudioDeviceManager)
    juce::StringArray activeMidiInNames;
    for (const auto& mIn : juce::MidiInput::getAvailableDevices())
    {
        if (audioEngine.getDeviceManager().isMidiInputDeviceEnabled(mIn.identifier))
            activeMidiInNames.add(mIn.name);
    }
    if (!activeMidiInNames.isEmpty())
        info.midiInputName = activeMidiInNames.joinIntoString(", ");
    else
        info.midiInputName = "None";

    // MIDI Output real activo (solo el dispositivo asignado como default output en AudioDeviceManager)
    if (auto* defOut = audioEngine.getDeviceManager().getDefaultMidiOutput())
        info.midiOutputName = defOut->getName();
    else
        info.midiOutputName = "None";

    info.exportDirectoryPath = exportDirectory.getFullPathName();
    info.autoTrimGainDb = (audioEngine.getInputAutoTrim() > 1e-4f) ? (20.0f * std::log10(audioEngine.getInputAutoTrim())) : 0.0f;
    info.totalMeasuredPoints = totalPointsMeasured;
    info.appVersion = version::kAppVersion;
    info.buildNumber = version::kBuildNumber;

    setupInfoTab.setTelemetryInfo(info);
    drawer.getSetupTab().setTelemetryInfo(info);

    // Keep target hardware info synchronized from selected contract or active plugin
    if (pluginUiCoordinator.hasActivePlugin())
    {
        auto desc = pluginUiCoordinator.getActivePluginDescription();
        auto fullImgFile = gui::locateAssetFile(desc.isInstrument
            ? "models/generic-digital-keyboard.png"
            : "models/generic-audio-rack.png");
        juce::Image pluginFullImg;
        if (fullImgFile.existsAsFile())
            pluginFullImg = juce::ImageFileFormat::loadFrom(fullImgFile);

        juce::String plugTitle = desc.name + (desc.isInstrument ? gui::strings::BADGE_INSTRUMENT : gui::strings::BADGE_EFFECT);
        setupInfoTab.setTargetHardwareInfo(
            plugTitle,
            desc.pluginFormatName + " Virtual Bus",
            "Internal Digital Bus (Zero Converter Coloration)",
            pluginFullImg,
            nullptr,
            "PLUGIN_VIRTUAL"
        );
        drawer.getSetupTab().setTargetHardwareInfo(
            plugTitle,
            desc.pluginFormatName + " Virtual Bus",
            "Internal Digital Bus (Zero Converter Coloration)",
            pluginFullImg,
            nullptr,
            "PLUGIN_VIRTUAL"
        );
    }
    else
    {
        juce::String hwId = drawer.getSelectedHardwareId();
        if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
        {
            // Hardware is considered "connected" when an active controller has been assigned
            // (MidiDeviceHotplugMonitor does not expose a synchronous discovered-device list).
            const bool isConnected = (hardwareManager.getActiveController() != nullptr);

            juce::String status = isConnected ? "CONNECTED HARDWARE" : "SELECTED PROFILE";
            juce::Colour col = isConnected ? gui::SoundIdTheme::accentGreen : gui::SoundIdTheme::accentAmber;

            setupInfoTab.setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType),
                status,
                col
            );
            drawer.getSetupTab().setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType),
                status,
                col
            );
        }
    }

    // Detect known host Audio/MIDI hardware interfaces (e.g., PreSonus AudioBox, Roland AIRA MX-1, Generic)
    auto detectedInterfaces = hardware::AudioMidiInterfaceDetector::detectInterfaces(audioEngine.getDeviceManager());
    if (!detectedInterfaces.empty())
    {
        // Encontrar el interface activo en el software (o el primero si ninguno está activo)
        const auto* activeIface = &detectedInterfaces.front();
        for (const auto& candidate : detectedInterfaces)
        {
            if (candidate.isAnyActiveInSoftware())
            {
                activeIface = &candidate;
                break;
            }
        }

        const auto& iface = *activeIface;
        bool isConnectedToSoftware = iface.isAnyActiveInSoftware();

        // Determinar conectividad real del software
        bool hasAudioActive = (device != nullptr && device->isOpen());
        bool hasMidiInActive = false;
        for (const auto& mIn : juce::MidiInput::getAvailableDevices())
        {
            if (audioEngine.getDeviceManager().isMidiInputDeviceEnabled(mIn.identifier))
            {
                hasMidiInActive = true;
                break;
            }
        }
        bool hasMidiOutActive = (audioEngine.getDeviceManager().getDefaultMidiOutput() != nullptr);

        juce::String details;
        if (hasAudioActive && hasMidiInActive && hasMidiOutActive)
            details = "Audio & MIDI I/O Asignados";
        else if (hasAudioActive && (hasMidiInActive || hasMidiOutActive))
            details = "Audio y MIDI Parcial";
        else if (hasAudioActive)
            details = "Audio Asignado";
        else if (hasMidiInActive || hasMidiOutActive)
            details = "Solo MIDI Activo";
        else
            details = "Detectada en Windows (No Asignada)";

        juce::Image ifaceImg;
        auto imgFile = gui::locateAssetFile(iface.imageRelPath);
        if (imgFile.existsAsFile())
            ifaceImg = juce::ImageFileFormat::loadFrom(imgFile);

        setupInfoTab.setDetectedInterfaceInfo(iface.displayName,
                                              details,
                                              ifaceImg,
                                              isConnectedToSoftware,
                                              hasAudioActive, // Audio In real
                                              hasAudioActive, // Audio Out real
                                              hasMidiInActive, // MIDI In real
                                              hasMidiOutActive); // MIDI Out real

        drawer.getSetupTab().setDetectedInterfaceInfo(iface.displayName,
                                                      details,
                                                      ifaceImg,
                                                      isConnectedToSoftware,
                                                      hasAudioActive, // Audio In real
                                                      hasAudioActive, // Audio Out real
                                                      hasMidiInActive, // MIDI In real
                                                      hasMidiOutActive); // MIDI Out real
    }
    else
    {
        setupInfoTab.setDetectedInterfaceInfo({}, {}, {}, false, false, false, false, false);
        drawer.getSetupTab().setDetectedInterfaceInfo({}, {}, {}, false, false, false, false, false);
    }
}

void MainContentComponent::showInfoDrawer()
{
    updateSetupDrawerInfo();

    if (drawer.isDrawerOpen())
        drawer.closeDrawer();
}

void MainContentComponent::chooseExportFolder()
{
    sessionReportManager.triggerSelectExportFolderAsync(exportDirectory, [this](const juce::File& chosen) {
        if (chosen.isDirectory())
        {
            exportDirectory = chosen;
            drawer.openFileDrawer(exportDirectory.getFullPathName());
            manualPromptLabel.setText("Target export folder updated to: " + exportDirectory.getFullPathName(), juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            hidePromptAfterDelay(5000);
        }
    });
}

void MainContentComponent::onHardwareSelected(const juce::String& hwId, const juce::String& funcId)
{
    const auto* contract = hardwareManager.findContractById(hwId.toStdString());
    if (contract == nullptr) return;

    gui::HardwareConnectionStatus connStatus = hardwareManager.selectHardware(hwId, funcId, audioEngine);

    juce::String funcName;
    for (const auto& fn : contract->functions)
    {
        if (juce::String(fn.id) == funcId)
        {
            funcName = juce::String(fn.name);
            break;
        }
    }
    if (funcName.isEmpty() && !contract->functions.empty())
        funcName = juce::String(contract->functions.front().name);
    if (funcName.isEmpty())
        funcName = drawer.getActiveFunctionDisplayName();

    juce::Image hwImg;
    if (!contract->modelImage.empty())
    {
        auto imgFile = gui::locateAssetFile(juce::String(contract->modelImage));
        if (imgFile.existsAsFile())
            hwImg = juce::ImageFileFormat::loadFrom(imgFile);
    }
    if (!hwImg.isValid())
        hwImg = drawer.getActiveModelRasterImage();

    mainHeader.setHardwareInfo(
        juce::String(contract->displayName),
        funcName,
        hwImg,
        connStatus
    );

    setupInfoTab.setTargetHardwareInfo(
        juce::String(contract->displayName),
        funcName,
        "Direct Loopback / Audio Routing",
        hwImg,
        nullptr,
        juce::String(contract->deviceType)
    );

    drawer.getSetupTab().setTargetHardwareInfo(
        juce::String(contract->displayName),
        funcName,
        "Direct Loopback / Audio Routing",
        hwImg,
        nullptr,
        juce::String(contract->deviceType)
    );

    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting, gui::SoundIdSidebarStepper::StepStatus::Completed);

    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName = juce::String(contract->displayName);
    summary.hardwareCategory = juce::String(contract->deviceType);
    sidebarStepper.setSessionSummary(summary);

    auto calStatus = stepperBar.getStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback);
    if (calStatus != gui::WorkflowStepperBar::StepStatus::Completed && calStatus != gui::WorkflowStepperBar::StepStatus::Skipped)
    {
        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::CalibrateLoopback);
        sidebarStepper.setCurrentStep(gui::SoundIdSidebarStepper::Step::CalibrateLoopback);
    }

    // Auto-populate measurement recipe for the selected function (preserving pinned noise baseline)
    if (!contract->functions.empty())
    {
        // 1. Remove previous non-pinned / non-baseline items
        for (int i = suiteList.getQueueSize() - 1; i >= 0; --i)
        {
            const auto& qItem = suiteList.getQueue()[static_cast<size_t>(i)];
            if (!qItem.isPinned && qItem.stimulusType != audio::StimulusType::Silence)
            {
                suiteList.removeTestDirectly(i);
            }
        }

        const auto* targetFunc = &contract->functions[0];
        for (const auto& f : contract->functions)
        {
            if (f.id == funcId.toStdString())
            {
                targetFunc = &f;
                break;
            }
        }
        const auto& f = *targetFunc;
        gui::QueueItem item;
        item.title = juce::String(contract->displayName) + " (" + juce::String(f.name) + ")";
        item.hwId = hwId;
        item.funcId = funcId;
        item.burstDurationSec = f.defaultBurstDurationSec > 0.05f ? f.defaultBurstDurationSec : 1.0f;
        item.captureMode = f.captureMode;

        if (f.excitationMode == core::ExcitationMode::MidiNotes)
        {
            item.stimulusType = audio::StimulusType::Silence;
            item.badgeText = "SYN";
        }
        else if (f.blockType == "TimeDynamic") item.stimulusType = audio::StimulusType::SyncPulses3;
        else if (f.blockType == "WaveShaper") item.stimulusType = audio::StimulusType::AmplitudeRamp;
        else if (f.blockType == "CyclicModulator") item.stimulusType = audio::StimulusType::SineWave1kHz;
        else item.stimulusType = audio::StimulusType::LogFarinaSweep;

        if (f.excitationMode != core::ExcitationMode::MidiNotes)
            applyBadgeForStimulus(item, item.stimulusType);

        int totalPts = 1;
        if (!f.controls.empty())
        {
            for (size_t k = 0; k < f.controls.size(); ++k)
            {
                gui::ControlStepConfig cs;
                cs.id = juce::String(f.controls[k].name);
                cs.name = f.controls[k].name;
                cs.type = f.controls[k].type;
                cs.steps = (k == 0) ? 8 : ((k == 1) ? 4 : 1);
                totalPts *= cs.steps;
                item.controls.push_back(cs);
            }
        }
        else if (!f.measurementRecipe.excitationNotes.empty())
        {
            totalPts = std::max(1, static_cast<int>(f.measurementRecipe.excitationNotes.size()));
        }
        item.totalPoints = totalPts;
        item.description = juce::String::fromUTF8(u8"Standard Recipe • ") + juce::String(item.totalPoints) + " points";
        item.status = gui::QueueItemStatus::Queued;
        item.id = "test_standard_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
        suiteList.addTestToQueue(item);

        summary.totalPointsPlanned = totalPts;
        sidebarStepper.setSessionSummary(summary);
    }

    // Inicializar formalmente la MeasurementSession en el coordinador
    std::string profSha = synth::Sha256::computeHex(contract->id + ":" + contract->displayName);
    sessionCoordinator.initializeMeasurementSession(*contract, funcId, juce::String(profSha));
}

void MainContentComponent::hidePromptAfterDelay(int delayMs)
{
    juce::Component::SafePointer<MainContentComponent> safeThis(this);
    juce::Timer::callAfterDelay(delayMs, [safeThis] {
        if (safeThis != nullptr && safeThis->sessionCoordinator.getSequencerState() != core::SequencerState::WaitingForOperator)
            safeThis->manualPromptLabel.setVisible(false);
    });
}

void MainContentComponent::handleClearPoint(int queueIdx, int pointIdx)
{
    curvePlotter.removePoint(pointIdx);
    sessionManager.removeMeasuredPoint(static_cast<size_t>(pointIdx));

    suiteList.setPointStatus(queueIdx, pointIdx, gui::PointStatus::Annulled);
    manualPromptLabel.setText("Point #" + juce::String(pointIdx + 1) + " marked as ANNULLED.", juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(3500);
}

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

// ==============================================================================
// SECTION 9: REPORT GENERATION & DATASET EXPORT DELEGATION
// Owns UI export action triggers, report format selection modals, and export progress display.
// Report compilation delegated to SessionReportManager; certification rendering to CertificationReportExporter.
// ==============================================================================
core::SessionManifest MainContentComponent::buildCurrentSessionManifest()
{
    core::SessionManifest sm;
    sm.appVersion = version::kAppVersion;
    sm.buildNumber = version::kBuildNumber;
    sm.formatVersion = "1.0";
    sm.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    sm.hardwareId = drawer.getSelectedHardwareId().toStdString();
    sm.hardwareDisplayName = drawer.getActiveHardwareDisplayName().toStdString();
    sm.activeFunctionId = drawer.getSelectedFunctionId().toStdString();
    sm.activeFunctionName = drawer.getActiveFunctionDisplayName().toStdString();
    sm.sampleRate = audioEngine.getSampleRate();
    sm.lineCalibrationGainDb = -3.0f;
    sm.noiseFloorThresholdDb = -85.0f;
    sm.totalMeasuredPoints = totalPointsMeasured;
    sm.operatorNotes = drawer.getOperatorNotes().toStdString();
    sm.ambientTemperatureC = drawer.getAmbientTemperature();
    sm.warmupTimeMinutes = drawer.getWarmupTimeMinutes();

    for (const auto& item : suiteList.getQueue())
    {
        gui::TestConfiguration tc;
        tc.testName = item.title;
        tc.stimulusType = item.stimulusType;
        tc.burstDurationSec = item.burstDurationSec;
        tc.captureMode = item.captureMode;
        tc.controls = item.controls;
        sm.tests.push_back(tc);
    }
    return sm;
}

void MainContentComponent::applyLoadedSession(const core::SessionManifest& manifest,
                                              const std::vector<exporting::MeasuredPoint>& points)
{
    const bool hasContract = (hardwareManager.findContractById(manifest.hardwareId) != nullptr);
    auto result = gui::LoadedSessionApplier::apply(manifest, points, hasContract, *this);

    if (!result.succeeded())
    {
        manualPromptLabel.setText("Failed to load session: " + result.message, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        return;
    }

    resized();
    manualPromptLabel.setText(result.message, juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
}

// =============================================================================
// ILoadedSessionTarget implementation
// =============================================================================

void MainContentComponent::setSessionData(const core::SessionManifest& manifest,
                                           const std::vector<exporting::MeasuredPoint>& points)
{
    totalPointsMeasured = 0;
    sessionManager.setManifest(manifest);
    sessionManager.setMeasuredPoints(points);
    sessionManager.setDirty(false);
}

void MainContentComponent::clearPlotterAndAddPoints(const std::vector<exporting::MeasuredPoint>& points)
{
    curvePlotter.clear();
    for (const auto& pt : points)
    {
        curvePlotter.addMeasuredPoint(pt);
        ++totalPointsMeasured;
    }
}

void MainContentComponent::updateDrawerAndEnvironment(const gui::SessionUiPresentationData& data)
{
    drawer.setOperatorNotes(data.operatorNotes);
    drawer.setAmbientTemperature(data.ambientTemperatureC);
    drawer.setWarmupTimeMinutes(data.warmupTimeMinutes);

    // Synchronize Session Summary card
    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName       = data.hardwareDisplayName;
    summary.hardwareCategory   = data.targetModule;
    summary.loopbackCalibrated = true;
    sidebarStepper.setSessionSummary(summary);
}

void MainContentComponent::updateHardwarePanels(const gui::SessionUiPresentationData& data)
{
    drawer.setSelectedHardwareId(data.hardwareId);
    drawer.setHardwareLocked(true);
    hardwareRoutingPanel.setSelectedHardware(data.hardwareId, data.activeFunctionId);
    hardwareRoutingPanel.setHardwareLocked(true);
    catalogSelector.setSelectedHardware(data.hardwareId, data.activeFunctionId);
    catalogSelector.setHardwareLocked(true);

    if (data.hasValidHardwareContract)
    {
        onHardwareSelected(data.hardwareId, data.activeFunctionId);
    }
    else
    {
        mainHeader.setHardwareInfo(data.hardwareDisplayName,
                                   data.activeFunctionId,
                                   juce::Image(),
                                   gui::HardwareConnectionStatus::NotApplicable);
    }
}

void MainContentComponent::rebuildTestSuiteQueue(const std::vector<core::SessionManifest>& /*manifests*/,
                                                  const std::vector<gui::QueueItem>& items)
{
    suiteList.clearQueue();
    for (const auto& item : items)
        suiteList.addTestToQueue(item);
}

void MainContentComponent::updateWorkflowAndNavigation(const gui::WorkflowStepState& workflowState)
{
    // Unlock prerequisites so loaded sessions can re-enter from step 1
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::SystemInfo,        false);
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::HardwareRouting,   false);
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::CalibrateLoopback, false);
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::SystemInfo,        false);
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::HardwareRouting,   false);
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, false);

    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::SystemInfo,        gui::WorkflowStepperBar::StepStatus::Completed);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting,   gui::WorkflowStepperBar::StepStatus::Completed);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Completed);

    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::SystemInfo,        gui::SoundIdSidebarStepper::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting,   gui::SoundIdSidebarStepper::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Completed);

    stepperBar.setCurrentStep(workflowState.targetStepperStep);
    sidebarStepper.setCurrentStep(workflowState.targetSidebarStep);

    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, workflowState.runSessionStatus);
    sidebarStepper.setStepStatus(
        gui::SoundIdSidebarStepper::Step::RunSession,
        workflowState.runSessionStatus == gui::WorkflowStepperBar::StepStatus::Completed
            ? gui::SoundIdSidebarStepper::StepStatus::Completed
            : gui::SoundIdSidebarStepper::StepStatus::Current);

    if (workflowState.isSessionComplete)
    {
        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::ExportReport,    gui::WorkflowStepperBar::StepStatus::Current);
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::ExportReport, gui::SoundIdSidebarStepper::StepStatus::Current);
    }

    workflowNavController.setStep(workflowState.targetSidebarStep);
}

void MainContentComponent::handleSaveSession()
{
    sessionIoController.handleSaveSession();
}

void MainContentComponent::handleSaveSessionAs()
{
    sessionIoController.handleSaveSessionAs();
}

void MainContentComponent::saveSessionToFile(const juce::File& file)
{
    sessionIoController.saveSessionToFile(file);
}

void MainContentComponent::exportCertificationReport()
{
    reportExportController.requestExportCertificationReport();
}

void MainContentComponent::updateExportReportMetrics()
{
    reportExportController.updateMetricsPreview();
}

void MainContentComponent::exportProductionPackage()
{
    reportExportController.requestExportProductionPackage();
}

void MainContentComponent::openCertificationReportHtml()
{
    reportExportController.openCertificationReportHtml();
}

// ==============================================================================
// IReportExportHost Implementation
// ==============================================================================
gui::ReportExportSnapshot MainContentComponent::createReportExportSnapshot() const
{
    gui::ReportExportSnapshot snapshot;

    juce::String hwId = drawer.getSelectedHardwareId();
    juce::String funcId = drawer.getSelectedFunctionId();
    if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();
    if (funcId.isEmpty()) funcId = hardwareRoutingPanel.getSelectedFunctionId();

    snapshot.manifest.hardwareId = hwId.toStdString();
    juce::String hwName = drawer.getActiveHardwareDisplayName();
    if (hwName.isEmpty())
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr) hwName = contract->displayName;
        else hwName = hwId;
    }
    snapshot.manifest.hardwareDisplayName = hwName.toStdString();
    snapshot.manifest.hardwareName = hwName.toStdString();

    snapshot.manifest.activeFunctionId = funcId.toStdString();
    juce::String funcName = drawer.getActiveFunctionDisplayName();
    if (funcName.isEmpty()) funcName = funcId;
    snapshot.manifest.activeFunctionName = funcName.toStdString();

    snapshot.manifest.targetModule = hwId.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL_EURORACK";

    snapshot.measuredPoints = sessionManager.getMeasuredPoints();
    snapshot.exportDirectory = sessionIoController.getExportDirectory().getFullPathName().toStdString();

    juce::String base = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();
    snapshot.baseFileName = base.toStdString();

    snapshot.sampleRate = audioEngine.getCurrentSampleRate();
    snapshot.inputTrimDb = audioEngine.getInputAutoTrim();
    snapshot.operatorNotes = drawer.getOperatorNotes().toStdString();
    snapshot.ambientTemperatureC = drawer.getAmbientTemperature();
    snapshot.warmupMinutes = drawer.getWarmupTimeMinutes();

    return snapshot;
}

void MainContentComponent::showStatusBanner(const juce::String& message, bool /*isError*/)
{
    manualPromptLabel.setText(message, juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
}

void MainContentComponent::showMessageBox(const juce::String& title, const juce::String& message, bool isError)
{
    juce::AlertWindow::showMessageBoxAsync(
        isError ? juce::AlertWindow::WarningIcon : juce::AlertWindow::InfoIcon,
        title,
        message,
        "OK"
    );
}

void MainContentComponent::updateExportReportMetrics(const exporting::CalculatedSessionMetrics& metrics)
{
    exportReportPanel.updateMetrics(
        metrics.avgSnrDb,
        metrics.noiseFloorDb,
        metrics.avgThdPercent,
        std::max(1, metrics.validPointCount),
        metrics.totalDurationSec
    );
}

void MainContentComponent::notifyExportSuccess(const juce::File& destinationDir, const juce::String& baseName)
{
    exportReportPanel.showExportSuccess(destinationDir.getFullPathName(), baseName);
}

void MainContentComponent::showPanelStatus(const juce::String& statusMessage, bool isWarning)
{
    exportReportPanel.showStatusMessage(statusMessage, isWarning);
}

void MainContentComponent::launchProcess(const juce::File& file)
{
    file.startAsProcess();
}

void MainContentComponent::revealInFolder(const juce::File& folder)
{
    if (!folder.startAsProcess())
        folder.revealToUser();
}

void MainContentComponent::prepareAuditionLut()
{
    const int gridSize = 8;
    auto lut = gui::SessionReportManager::buildAuditionLutGrid(sessionManager.getMeasuredPoints(), gridSize);
    audioEngine.loadAuditionLut(lut, gridSize);
}

void MainContentComponent::publishCertificationToCloud()
{
    exportProductionPackage();
    sessionIoController.publishCertificationToCloud();
}

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
    hardwareRoutingPanel.setHardwareLocked(false);
    hardwareRoutingPanel.resetSelection();
    catalogSelector.setHardwareLocked(false);
    catalogSelector.resetSelection();
    mainHeader.clearHardware();
    suiteList.setStandardTestAvailable(false);

    // Disconnect and release active plugin instance
    pluginUiCoordinator.unloadPlugin();

    // Reset session summary in sidebar stepper
    gui::SoundIdSidebarStepper::SessionSummaryInfo emptySummary;
    sidebarStepper.setSessionSummary(emptySummary);

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

// ==============================================================================
// SECTION 2: IPluginUiHost IMPLEMENTATION (VST3 HOSTING & PRESENTATION PORT)
// ==============================================================================

void MainContentComponent::updatePluginLoadingState(bool isSuccess, const juce::String& message)
{
    juce::Logger::writeToLog("[PluginUiHost] LoadingState: " + juce::String(isSuccess ? "Success" : "Failed") + " - " + message);
    if (!isSuccess && message.isNotEmpty())
    {
        manualPromptLabel.setText(message, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(5000);
    }
}

void MainContentComponent::updatePluginIdentity(const gui::PluginIdentityPresentation& identity,
                                                const juce::PluginDescription& description)
{
    juce::ignoreUnused(description);

    drawer.setContracts(hardwareManager.getContractRegistry().getContracts());
    drawer.setSelectedHardwareId(identity.legalTargetId);

    hardwareRoutingPanel.setContracts(hardwareManager.getContractRegistry().getContracts());
    hardwareRoutingPanel.setPluginVirtualRouting(identity.pluginName, identity.formatName, identity.isInstrument);

    suiteList.setStandardTestAvailable(true);

    auto imgFile = gui::locateAssetFile(identity.modelAssetPath);
    juce::Image pluginImg;
    if (imgFile.existsAsFile())
        pluginImg = juce::ImageFileFormat::loadFrom(imgFile);

    mainHeader.setHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        pluginImg,
        gui::HardwareConnectionStatus::Connected
    );

    setupInfoTab.setTargetHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        "Internal Digital Bus (Zero Converter Coloration)",
        pluginImg,
        nullptr,
        identity.category
    );
    drawer.getSetupTab().setTargetHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        "Internal Digital Bus (Zero Converter Coloration)",
        pluginImg,
        nullptr,
        identity.category
    );
    updateSetupDrawerInfo();

    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName = identity.titleBadge;
    summary.hardwareCategory = identity.category;
    sidebarStepper.setSessionSummary(summary);
}

void MainContentComponent::showPluginError(const juce::String& title, const juce::String& message)
{
    juce::Logger::writeToLog("[PluginUiHost ERROR] " + title + ": " + message);
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}

void MainContentComponent::notifyPluginUnloaded()
{
    suiteList.setStandardTestAvailable(false);
    mainHeader.setHardwareInfo(
        "Ninguno",
        "Sin hardware seleccionado",
        {},
        gui::HardwareConnectionStatus::Disconnected
    );
    auto summary = sidebarStepper.getSessionSummary();
    if (summary.hardwareCategory == "PLUGIN_VIRTUAL")
    {
        summary.hardwareName = "";
        summary.hardwareCategory = "";
        sidebarStepper.setSessionSummary(summary);
    }
}

// ==============================================================================
// SECTION 5 (AUXILIARY IMPLEMENTATION): WORKFLOW MODE SWITCHING & FIXTURE PRELOAD
// Owns switching visibility between 3-step Guided container and Lab Bench surfaces.
// Guided workflow state machine delegated to ProfilingSessionController.
// ==============================================================================
void MainContentComponent::setWorkflowMode(gui::session::UiWorkflowMode mode)
{
    if (currentWorkflowMode == mode)
        return;

    currentWorkflowMode = mode;
    profilingSessionController.setWorkflowMode(mode);

    if (mode == gui::session::UiWorkflowMode::Guided)
    {
        btnWorkflowModeToggle.setButtonText(juce::String::fromUTF8(u8"Modo: Guiado (Cambiar a Cl\u00e1sico)"));
        btnWorkflowModeToggle.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentBlue.withAlpha(0.15f));
        btnWorkflowModeToggle.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentBlue);

        // Sincronizar target real con el controlador si existe plugin o hardware activo
        if (pluginUiCoordinator.hasActivePlugin())
        {
            auto desc = pluginUiCoordinator.getActivePluginDescription();
            auto* instance = pluginUiCoordinator.getActivePluginInstance();
            gui::session::TargetSelectionState target;
            target.targetId = "plugin_" + desc.fileOrIdentifier.toStdString();
            target.targetName = desc.name.toStdString();
            target.manufacturer = desc.manufacturerName.toStdString();
            target.version = desc.version.toStdString();
            target.kind = gui::session::TargetKind::PluginVST3;
            target.isConnected = true;
            target.isDeterministic = true;
            target.availableDomainDescription = "MIDI C1-C6, Vel 1-127, Automatable Parameters";
            target.parameterCount = (instance != nullptr) ? instance->getParameters().size() : 0;
            profilingSessionController.selectTarget(target);
        }
        else if (mainHeader.hasHardwareSelected())
        {
            juce::String hwId = drawer.getSelectedHardwareId();
            if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();
            const auto* c = hardwareManager.findContractById(hwId.toStdString());
            if (c != nullptr)
            {
                gui::session::TargetSelectionState target;
                target.targetId = c->id;
                target.targetName = c->displayName;
                target.manufacturer = c->manufacturer;
                target.version = c->schemaVersion;
                target.kind = (c->deviceType == "MANUAL_EURORACK" || c->deviceType == "ANALOGUE_PEDAL")
                    ? gui::session::TargetKind::HardwareAnalogue
                    : gui::session::TargetKind::HardwareDigital;
                target.isConnected = true;
                target.isDeterministic = (c->deviceType != "MANUAL_EURORACK" && c->deviceType != "ANALOGUE_PEDAL");
                target.availableDomainDescription = "MIDI CC / SysEx Profiles";
                target.parameterCount = static_cast<int>(c->functions.size());
                profilingSessionController.selectTarget(target);
            }
        }

        // Ocultar superficies clasicas para evitar solapamientos
        sidebarStepper.setVisible(false);
        meterStrip.setVisible(false);
        setupInfoTab.setVisible(false);
        catalogSelector.setVisible(false);
        nativeCalibrationPanel.setVisible(false);
        exportReportPanel.setVisible(false);
        curvePlotter.setVisible(false);
        healthPanel.setVisible(false);
        suiteList.setVisible(false);
        centerSplitterBar.setVisible(false);

        if (guidedWorkflowContainer != nullptr)
            guidedWorkflowContainer->setVisible(true);
    }
    else
    {
        btnWorkflowModeToggle.setButtonText(juce::String::fromUTF8(u8"Modo: Cl\u00e1sico (Cambiar a Guiado 3 Pasos)"));
        btnWorkflowModeToggle.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnWorkflowModeToggle.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentBlue);

        if (guidedWorkflowContainer != nullptr)
            guidedWorkflowContainer->setVisible(false);

        // Restaurar superficies clasicas
        sidebarStepper.setVisible(true);
        meterStrip.setVisible(true);
        centerSplitterBar.setVisible(true);
        workflowNavController.setStep(sidebarStepper.getCurrentStep());
    }

    resized();
}

void MainContentComponent::setupGuidedWorkflowInitialData()
{
    gui::session::TargetSelectionState target;
    target.targetId = "synthetic_fixture_demo";
    target.targetName = "Sintetizador Virtual de Prueba (Demo Snapshot)";
    target.manufacturer = "ABDAudioLab";
    target.version = "1.0.0";
    target.kind = gui::session::TargetKind::SyntheticFixture;
    target.isConnected = true;
    target.isDeterministic = true;
    target.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127, Controles de Filtro y Modulación";
    target.parameterCount = 8;

    profilingSessionController.selectTarget(target);
    profilingSessionController.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (Fixture Digital)",
        "Reset de fase instantaneo (0 ms)",
        0.0,
        false,
        {},
        "Target de prueba sintetico precalificado para validacion acustica");

    // Precargar evaluacion de demostracion para que el usuario siempre tenga datos listos
    profilingSessionController.loadPredefinedFixture("fixture_approved.json");
    profilingSessionController.navigateToStage(gui::session::ProfilingWorkflowStage::ConfigureAndStart);

    profilingSessionController.setWorkflowMode(currentWorkflowMode);
}

} // namespace abdaudiolab
