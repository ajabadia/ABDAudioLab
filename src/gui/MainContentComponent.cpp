/**
 * @file MainContentComponent.cpp
 * @brief Implementation of MainContentComponent layout, sequencing and UI orchestration.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"
#include <cmath>

namespace abdaudiolab
{

std::string mapBadgeToBlockType(const juce::String& badgeText)
{
    if (badgeText == "FLT") return "SpectrumFilter";
    if (badgeText == "ENV") return "TimeDynamic";
    if (badgeText == "SAT") return "WaveShaper";
    if (badgeText == "MOD") return "CyclicModulator";
    if (badgeText == "WNH") return "WienerHammerstein";
    if (badgeText == "NAM") return "NeuralCalibration";
    return "AmplitudeGain";
}

hardware::AiraModel mapHardwareIdToAiraModel(const juce::String& hwId)
{
    if (hwId == "roland_aira_bitrazer") return hardware::AiraModel::Bitrazer;
    if (hwId == "roland_aira_demora")   return hardware::AiraModel::Demora;
    if (hwId == "roland_aira_torcido")  return hardware::AiraModel::Torcido;
    if (hwId == "roland_aira_scooper")  return hardware::AiraModel::Scooper;
    return hardware::AiraModel::GenericModular;
}

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

    // Load Contract Specifications Dynamically from contracts/hardware/ via relative traversal
    std::vector<juce::File> roots = {
        juce::File::getCurrentWorkingDirectory(),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory()
    };

    for (auto root : roots)
    {
        for (int i = 0; i < 6; ++i)
        {
            auto direct = root.getChildFile("contracts").getChildFile("hardware");
            if (direct.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(direct))
                break;

            auto shared = root.getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (shared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(shared))
                break;

            auto siblingShared = root.getParentDirectory().getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (siblingShared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(siblingShared))
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

    // Default export directory
    exportDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("exported_luts");
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

    // Main Header Controller Integration (Barra Superior Modular)
    mainHeader.onNewSession = [this] { promptNewSession(); };
    mainHeader.onOpenSession = [this] { handleOpenSession(); };
    mainHeader.onSaveSession = [this] { handleSaveSession(); };
    mainHeader.onSaveSessionAs = [this] { handleSaveSessionAs(); };
    mainHeader.onReanalyzeOffline = [this] { performOfflineReanalysis(); };
    mainHeader.onExportCertificationReport = [this] { exportCertificationReport(); };
    mainHeader.onOpenExportFolder = [this] {
        if (!exportDirectory.exists())
            exportDirectory.createDirectory();
        exportDirectory.revealToUser();
    };
    mainHeader.onExitApp = [this] { confirmAndExit(); };

    mainHeader.onScopeToggle = [this] { toggleScopeWebWindow(); };
    mainHeader.onConfigureAudioMidi = [this] { openAudioMidiSettings(); };
    mainHeader.onCalibrateClicked = [this] {
        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::CalibrateLoopback);
        sidebarStepper.setCurrentStep(gui::SoundIdSidebarStepper::Step::CalibrateLoopback);
        if (stepperBar.onStepSelected != nullptr)
            stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::CalibrateLoopback);
        resized();
    };
    mainHeader.onHardwareSelectorClicked = [this] {
        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::HardwareRouting);
        sidebarStepper.setCurrentStep(gui::SoundIdSidebarStepper::Step::HardwareRouting);
        if (stepperBar.onStepSelected != nullptr)
            stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::HardwareRouting);
        resized();
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

        drawer.updateTheme();
        operatorStepModal.updateTheme();
        repaint();
    };
    mainHeader.onInfoClicked = [this] { showInfoDrawer(); };
    addAndMakeVisible(mainHeader);

    // 4. Workflow Stepper Bar & Export Report Panel (Paso 4: Certificación SoundID)
    addChildComponent(exportReportPanel);
    exportReportPanel.setVisible(false);

    exportReportPanel.onExportRequested = [this] {
        exportProductionPackage();
    };
    exportReportPanel.onOpenFolderRequested = [this] {
        if (!exportDirectory.exists())
            exportDirectory.createDirectory();
        if (!exportDirectory.startAsProcess())
            exportDirectory.revealToUser();
        exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"✓ Carpeta de exportación abierta en el Explorador."));
    };
    exportReportPanel.onViewHtmlRequested = [this] {
        openCertificationReportHtml();
    };
    exportReportPanel.onPublishCloudRequested = [this] {
        publishCertificationToCloud();
    };
    exportReportPanel.onAuditionToggled = [this](bool active) {
        if (active)
        {
            prepareAuditionLut();
            audioEngine.enableAuditionMode(true);
            exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"\U0001f3a7 Audición DSP activada. Ajuste Cutoff y Resonancia para escuchar el modelo."));
        }
        else
        {
            audioEngine.enableAuditionMode(false);
            exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"⏹ Audición DSP detenida."));
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
        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Completed);
        if (stepperBar.getStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback) == gui::WorkflowStepperBar::StepStatus::Completed)
        {
            stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::RunSession);
            if (stepperBar.onStepSelected != nullptr)
                stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::RunSession);
        }
        else
        {
            stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::CalibrateLoopback);
            if (stepperBar.onStepSelected != nullptr)
                stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::CalibrateLoopback);
        }
        resized();
    };
    hardwareRoutingPanel.onOpenAdvancedSettings = [this] {
        drawer.openHardwareDrawer();
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

        manualPromptLabel.setText("Paso 2 Omitido: Operando con ganancia nominal (0 dB). ¡Paso 3 habilitado!", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        resized();
    };
    nativeCalibrationPanel.onContinueToSession = [this] {
        stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::RunSession);
        if (stepperBar.onStepSelected != nullptr)
            stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::RunSession);
        resized();
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
                manualPromptLabel.setText("Paso 3: Sesión de Profiling lista. Pulse Play en el medidor derecho para comenzar.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                hidePromptAfterDelay(4000);
                break;

            case gui::WorkflowStepperBar::Step::ExportReport:
                updateExportReportMetrics();
                break;
        }
        sidebarStepper.setCurrentStep(static_cast<gui::SoundIdSidebarStepper::Step>(targetStep));
        resized();
    };
    // Hide horizontal stepperBar in favor of collapsible sidebarStepper, maintaining full logic
    stepperBar.setVisible(false);
    addChildComponent(stepperBar);

    // Wire collapsible vertical sidebarStepper
    sidebarStepper.onStepSelected = [this](gui::SoundIdSidebarStepper::Step targetStep) {
        stepperBar.setCurrentStep(static_cast<gui::WorkflowStepperBar::Step>(targetStep));
        if (stepperBar.onStepSelected != nullptr)
            stepperBar.onStepSelected(static_cast<gui::WorkflowStepperBar::Step>(targetStep));
    };
    sidebarStepper.onCollapseToggled = [this](bool /*collapsed*/) {
        resized();
    };
    addAndMakeVisible(sidebarStepper);

    // Wire cascading catalog selector
    catalogSelector.setContracts(hardwareManager.getContractRegistry().getContracts());
    catalogSelector.onSelectionChanged = [this](const juce::String& hwId, const juce::String& funcId) {
        onHardwareSelected(hwId, funcId);
        drawer.setSelectedHardwareId(hwId);
        gui::SoundIdSidebarStepper::SessionSummaryInfo summary = sidebarStepper.getSessionSummary();
        summary.hardwareName = hwId;
        sidebarStepper.setSessionSummary(summary);
    };
    catalogSelector.onContinueRequested = [this] {
        if (hardwareRoutingPanel.onContinueToCalibration != nullptr)
            hardwareRoutingPanel.onContinueToCalibration();
    };
    catalogSelector.onAutoDetectRequested = [this] {
        drawer.triggerAutoDetect();
    };
    catalogSelector.onResetOrUnlockRequested = [this] {
        if (drawer.onNewFlowRequested != nullptr)
            drawer.onNewFlowRequested();
    };
    addChildComponent(catalogSelector);

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
        juce::String selectedHwId = drawer.getSelectedHardwareId();
        juce::String selectedFuncId = drawer.getSelectedFunctionId();
        const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());
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

        drawer.openTestEditorDrawer(stdConf, -1);
    };

    suiteList.onAddCustomClicked = [this] {
        gui::TestConfiguration customConf;
        customConf.testName = "Custom Profile";
        customConf.stimulusType = audio::StimulusType::LogFarinaSweep;
        customConf.burstDurationSec = 1.0f;
        customConf.captureMode = "FIXED_TIME";
        
        juce::String selectedHwId = drawer.getSelectedHardwareId();
        juce::String selectedFuncId = drawer.getSelectedFunctionId();
        const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());
        if (contract != nullptr && !contract->functions.empty())
        {
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
        drawer.openTestEditorDrawer(customConf, -1);
    };

    suiteList.onEditTestClicked = [this](int index, const gui::QueueItem& item) {
        if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
        {
            confirmationModal.show(
                this,
                "Edit Completed Test",
                "Modifying the parameters of '" + item.title + "' will invalidate its recorded measurements.\n\nDo you want to proceed and re-queue this test?",
                "Edit & Invalidate",
                "",
                "Cancel",
                [this, index, item](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        gui::TestConfiguration conf;
                        conf.testName = item.title;
                        conf.stimulusType = item.stimulusType;
                        conf.burstDurationSec = item.burstDurationSec;
                        conf.captureMode = item.captureMode;
                        conf.controls = item.controls;
                        drawer.openTestEditorDrawer(conf, index);
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
            drawer.openTestEditorDrawer(conf, index);
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
            juce::String prompt = "Point #" + juce::String(pointIdx + 1) + " / " + juce::String(item.totalPoints) 
                                + " (" + juce::String(stepPct, 1) + "% Pos) — " + item.title;

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
    btnStepBack.onClick = [this] { sequencer.stepBack(); };
    btnStepBack.setEnabled(false);
    btnStepBack.setVisible(false);
    addChildComponent(btnStepBack);

    btnRepeatStep.setButtonText("REPEAT STEP");
    btnRepeatStep.setTooltip("Re-measure current knob position in case of audio glitch or misadjustment");
    btnRepeatStep.onClick = [this] { sequencer.repeatCurrentStep(); };
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
        if (!exportDirectory.exists())
            exportDirectory.createDirectory();
        exportDirectory.revealToUser();
    };
    drawer.onExportReportClicked = [this] {
        exportCertificationReport();
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
    operatorStepModal.onRepeat = [this] { sequencer.repeatCurrentStep(); };
    operatorStepModal.onStepBack = [this] { sequencer.stepBack(); };
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

    sessionCoordinator.wireSequencerCallbacks();

    addKeyListener(this);
    setWantsKeyboardFocus(true);
    audioEngine.getDeviceManager().addChangeListener(this);
    startTimerHz(60);
    setSize(1040, 720);

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

    report("Listo.", 1.0f);
}

MainContentComponent::~MainContentComponent()
{
    audioEngine.getDeviceManager().removeChangeListener(this);

    if (scopeWebWindow != nullptr)
    {
        scopeWebWindow->setVisible(false);
        scopeWebWindow = nullptr;
    }

    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
    removeKeyListener(this);
    stopTimer();
    sequencer.stopSession();
    audioEngine.saveAudioSettings(settingsFile);
}


void MainContentComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioEngine.getDeviceManager())
    {
        mainHeader.updateAudioMidiStatus();
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
        if ((confirmManualButton.isEnabled() && confirmManualButton.isVisible())
            || (operatorStepModal.isVisible() && !operatorStepModal.isMeasuring))
        {
            confirmManualStep();
            return true;
        }
    }
    return false;
}
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
    mainHeader.setBounds(bounds.removeFromTop(36));

    bounds.removeFromTop(10);

    // 2. Left Collapsible Sidebar Stepper (SoundID Vertical Workflow Rail)
    int sidebarW = sidebarStepper.getDesiredWidth();
    sidebarStepper.setBounds(bounds.removeFromLeft(sidebarW));
    bounds.removeFromLeft(12);

    // 3. Right Meter Strip
    auto rightArea = bounds.removeFromRight(120);
    meterStrip.setBounds(rightArea);
    bounds.removeFromRight(12);

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

    // 6. Slide-in Drawer & Modals fill full window bounds
    drawer.setBounds(getLocalBounds());
    aboutModal.setBounds(getLocalBounds());
    confirmationModal.setBounds(getLocalBounds());
    abVerificationModal.setBounds(getLocalBounds());
}

void MainContentComponent::timerCallback()
{
    meterStrip.setLevels(audioEngine.getInputPeakL(), audioEngine.getInputPeakR(), audioEngine.getInputRmsL(),
                         audioEngine.getOutputPeakL(), audioEngine.getOutputPeakR(), audioEngine.getOutputRmsL());

    if (audioEngine.isSpectrumReady())
    {
        std::array<float, audio::LabAudioEngine::kSpectrumBins> fftData;
        audioEngine.getSpectrumMagnitudes(fftData);
        curvePlotter.getSpectrumAnalyzer().pushSpectrumData(fftData, audioEngine.getCurrentSampleRate());
    }

    if ((statusUpdateCounter++ % 15) == 0)
    {
        bool isCalibrated = loopbackModal.getCalibrationData().isCalibrated;
        double calSr = loopbackModal.getCalibrationData().sampleRate;
        bool isSkipped = (stepperBar.getStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback) == gui::WorkflowStepperBar::StepStatus::Skipped);

        mainHeader.updateCalibrationStatus(isCalibrated, calSr, isSkipped);
    }

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

    // Status updates
}

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



void MainContentComponent::showInfoDrawer()
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

    auto midiInputs = juce::MidiInput::getAvailableDevices();
    if (!midiInputs.isEmpty())
        info.midiInputName = midiInputs[0].name;

    auto midiOutputs = juce::MidiOutput::getAvailableDevices();
    if (!midiOutputs.isEmpty())
        info.midiOutputName = midiOutputs[0].name;

    info.exportDirectoryPath = exportDirectory.getFullPathName();
    info.autoTrimGainDb = (audioEngine.getInputAutoTrim() > 1e-4f) ? (20.0f * std::log10(audioEngine.getInputAutoTrim())) : 0.0f;
    info.totalMeasuredPoints = totalPointsMeasured;
    info.appVersion = version::kAppVersion;
    info.buildNumber = version::kBuildNumber;

    drawer.openSetupDrawer(info);
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

    mainHeader.setHardwareInfo(
        juce::String(contract->displayName),
        drawer.getActiveFunctionDisplayName(),
        drawer.getActiveModelRasterImage(),
        connStatus
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

    // Auto-populate default test plan for the selected function if queue is currently empty
    if (suiteList.getQueueSize() == 0 && !contract->functions.empty())
    {
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

        if (f.blockType == "TimeDynamic") item.stimulusType = audio::StimulusType::SyncPulses3;
        else if (f.blockType == "WaveShaper") item.stimulusType = audio::StimulusType::AmplitudeRamp;
        else if (f.blockType == "CyclicModulator") item.stimulusType = audio::StimulusType::SineWave1kHz;
        else item.stimulusType = audio::StimulusType::LogFarinaSweep;

        applyBadgeForStimulus(item, item.stimulusType);

        int totalPts = 1;
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
        item.totalPoints = totalPts;
        item.description = juce::String::fromUTF8(u8"Standard Recipe • ") + juce::String(item.totalPoints) + " points";
        item.status = gui::QueueItemStatus::Queued;
        item.id = "test_standard_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
        suiteList.addTestToQueue(item);
    }
}

void MainContentComponent::hidePromptAfterDelay(int delayMs)
{
    juce::Component::SafePointer<MainContentComponent> safeThis(this);
    juce::Timer::callAfterDelay(delayMs, [safeThis] {
        if (safeThis != nullptr && safeThis->sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
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

void MainContentComponent::startProfilingSession(bool resumeFromExisting)
{
    if (sequencer.isRunningSession())
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
        suiteList.resetAllStatuses();
        suiteList.updateItemStatus(0, gui::QueueItemStatus::Running, 0);
        curvePlotter.clear();
        curvePlotter.clearPreScanData();
        sessionManager.clearMeasuredPoints();
        totalPointsMeasured = 0;
    }

    suiteList.setSessionRunning(true);

    juce::String selectedHwId = drawer.getSelectedHardwareId();
    juce::String selectedFuncId = drawer.getSelectedFunctionId();
    const auto* contract = hardwareManager.findContractById(selectedHwId.toStdString());

    hardware::IHardwareController* activeHw = hardwareManager.getActiveController();
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

    sequencer.setHardwareController(activeHw);

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
    sequencer.startSession(currentProfilingSession, exportDirectory, baseName);
}

void MainContentComponent::stopProfilingSession()
{
    sessionCoordinator.triggerStopSession();
    meterStrip.setProfilingActive(false);
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
    core::ProfilingSession profSession;
    core::ProfilingMetadata meta;
    meta.hardwareName = hwName;
    meta.targetModule = drawer.getSelectedFunctionId().toStdString();
    meta.operatorMode = modeStr;
    meta.sampleRate = audioEngine.getSampleRate();
    meta.bitDepth = 24;
    meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    meta.operatorNotes = drawer.getOperatorNotes().toStdString();
    meta.ambientTemperatureC = drawer.getAmbientTemperature();
    meta.warmupTimeMinutes = drawer.getWarmupTimeMinutes();
    profSession.setMetadata(meta);

    const auto& queue = suiteList.getQueue();
    bool isAutonomousSynth = hardwareManager.isAutonomousSynth(drawer.getSelectedHardwareId(), drawer.getSelectedFunctionId());
    int globalPointCounter = 0;

    for (int qIdx = 0; qIdx < static_cast<int>(queue.size()); ++qIdx)
    {
        const auto& item = queue[static_cast<size_t>(qIdx)];
        if (item.isSkipped) continue;

        if (item.stimulusType == audio::StimulusType::Silence)
        {
            core::TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = 1;
            tc.totalPointsInTest = 1;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = "NoiseFloor";
            tc.stimulusType = audio::StimulusType::Silence;
            tc.stimulusDurationSec = (item.burstDurationSec > 0.1f) ? item.burstDurationSec : 0.8;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            profSession.addTestCase(tc);
            continue;
        }

        size_t numControls = item.controls.size();

        // 1.7.16 Resolve measurement recipe from active hardware contract
        core::MeasurementPresetRecipe itemRecipe;
        const auto* itemContract = item.hwId.isNotEmpty() ? hardwareManager.getContractRegistry().findContractById(item.hwId.toStdString()) : nullptr;
        if (itemContract == nullptr && !hardwareManager.getContractRegistry().getContracts().empty())
        {
            itemContract = &hardwareManager.getContractRegistry().getContracts().front();
        }
        if (itemContract != nullptr)
        {
            for (const auto& fn : itemContract->functions)
            {
                if (fn.id == item.funcId.toStdString() || fn.name == item.title.toStdString())
                {
                    itemRecipe = fn.measurementRecipe;
                    break;
                }
            }
        }

        if (numControls == 0)
        {
            core::TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = 1;
            tc.totalPointsInTest = 1;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            tc.stimulusType = isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
            tc.isAutonomousSynth = isAutonomousSynth;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            profSession.addTestCase(tc);
            continue;
        }

        std::vector<int> stepsPerControl(numControls);
        std::vector<std::string> controlNames(numControls);
        std::vector<std::string> controlTypes(numControls);
        std::vector<float> minNorms(numControls);
        std::vector<float> maxNorms(numControls);

        int totalTestPoints = 1;
        for (size_t k = 0; k < numControls; ++k)
        {
            const auto& c = item.controls[k];
            stepsPerControl[k] = std::max(1, c.steps);
            controlNames[k] = c.name.toStdString();
            controlTypes[k] = c.type.isEmpty() ? "Knob" : c.type.toStdString();
            minNorms[k] = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
            maxNorms[k] = std::clamp(c.maxPct / 100.0f, minNorms[k], 1.0f);
            totalTestPoints *= stepsPerControl[k];
        }

        for (int p = 0; p < totalTestPoints; ++p)
        {
            int temp = p;
            std::vector<int> stepIndices(numControls);
            for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
            {
                stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
                temp /= stepsPerControl[static_cast<size_t>(k)];
            }

            core::TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = p + 1;
            tc.totalPointsInTest = totalTestPoints;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();

            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            tc.stimulusType = isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
            tc.isAutonomousSynth = isAutonomousSynth;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;

            for (size_t k = 0; k < numControls; ++k)
            {
                int stepIdx = stepIndices[k];
                int sCount = stepsPerControl[k];
                float minN = minNorms[k];
                float maxN = maxNorms[k];

                float normVal = (sCount > 1)
                    ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                    : (minN + maxN) * 0.5f;

                int rawVal = static_cast<int>(std::round(normVal * 127.0f));

                core::ParameterStep ps;
                ps.paramIndex = static_cast<int>(k) + 1;
                ps.paramName = controlNames[k];
                ps.controlType = controlTypes[k];
                ps.minNormalized = minN;
                ps.maxNormalized = maxN;
                ps.normalizedValue = normVal;
                ps.rawValue = rawVal;
                ps.id = item.controls[k].id.isNotEmpty() ? item.controls[k].id.toStdString() : ("ctrl_" + std::to_string(k + 1));
                ps.sortOrder = item.controls[k].sortOrder;
                tc.parameterSteps.push_back(ps);
            }

            profSession.addTestCase(tc);
        }
    }
    return profSession;
}

void MainContentComponent::startTargetedPatchSession(const std::vector<std::pair<int, int>>& pointsToPatch)
{
    if (sequencer.isRunningSession())
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

    hardware::IHardwareController* activeHw = hardwareManager.getActiveController();
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

    sequencer.setHardwareController(activeHw);

    core::ProfilingSession patchSession = buildPatchProfilingSession(pointsToPatch, hwName, modeStr);
    patchSession.setIsPatchSession(true);

    juce::String baseName = juce::String(hwName) + "_" + selectedFuncId;

    meterStrip.setProfilingActive(true);
    sequencer.startSession(patchSession, exportDirectory, baseName);
}

core::ProfilingSession MainContentComponent::buildPatchProfilingSession(const std::vector<std::pair<int, int>>& pointsToPatch,
                                                                        const std::string& hwName,
                                                                        const std::string& modeStr)
{
    core::ProfilingSession profSession;
    core::ProfilingMetadata meta;
    meta.hardwareName = hwName;
    meta.targetModule = drawer.getSelectedFunctionId().toStdString();
    meta.operatorMode = modeStr;
    meta.sampleRate = audioEngine.getSampleRate();
    meta.bitDepth = 24;
    meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    meta.operatorNotes = drawer.getOperatorNotes().toStdString();
    meta.ambientTemperatureC = drawer.getAmbientTemperature();
    meta.warmupTimeMinutes = drawer.getWarmupTimeMinutes();
    profSession.setMetadata(meta);

    const auto& queue = suiteList.getQueue();
    bool isAutonomousSynth = hardwareManager.isAutonomousSynth(drawer.getSelectedHardwareId(), drawer.getSelectedFunctionId());

    for (const auto& target : pointsToPatch)
    {
        int qIdx = target.first;
        int pIdx = target.second;

        if (qIdx < 0 || qIdx >= static_cast<int>(queue.size()))
            continue;

        const auto& item = queue[static_cast<size_t>(qIdx)];
        if (pIdx < 0 || pIdx >= item.totalPoints)
            continue;

        // Calculate global point index in full session
        int sessionOffset = 0;
        for (int k = 0; k < qIdx; ++k)
            sessionOffset += queue[static_cast<size_t>(k)].totalPoints;
        int globalPointIdx = sessionOffset + pIdx;

        if (item.stimulusType == audio::StimulusType::Silence)
        {
            core::TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = pIdx + 1;
            tc.totalPointsInTest = item.totalPoints;
            tc.globalPointIndex = globalPointIdx;
            tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = "NoiseFloor";
            tc.stimulusType = audio::StimulusType::Silence;
            tc.stimulusDurationSec = (item.burstDurationSec > 0.1f) ? item.burstDurationSec : 0.8;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            profSession.addTestCase(tc);
            continue;
        }

        size_t numControls = item.controls.size();

        // 1.7.16 Resolve measurement recipe from active hardware contract
        core::MeasurementPresetRecipe itemRecipe;
        const auto* itemContract = item.hwId.isNotEmpty() ? hardwareManager.getContractRegistry().findContractById(item.hwId.toStdString()) : nullptr;
        if (itemContract == nullptr && !hardwareManager.getContractRegistry().getContracts().empty())
        {
            itemContract = &hardwareManager.getContractRegistry().getContracts().front();
        }
        if (itemContract != nullptr)
        {
            for (const auto& fn : itemContract->functions)
            {
                if (fn.id == item.funcId.toStdString() || fn.name == item.title.toStdString())
                {
                    itemRecipe = fn.measurementRecipe;
                    break;
                }
            }
        }

        if (numControls == 0)
        {
            core::TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = pIdx + 1;
            tc.totalPointsInTest = item.totalPoints;
            tc.globalPointIndex = globalPointIdx;
            tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            tc.stimulusType = isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
            tc.isAutonomousSynth = isAutonomousSynth;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            profSession.addTestCase(tc);
            continue;
        }

        std::vector<int> stepsPerControl(numControls);
        std::vector<std::string> controlNames(numControls);
        std::vector<std::string> controlTypes(numControls);
        std::vector<float> minNorms(numControls);
        std::vector<float> maxNorms(numControls);

        int totalTestPoints = 1;
        for (size_t k = 0; k < numControls; ++k)
        {
            const auto& c = item.controls[k];
            stepsPerControl[k] = std::max(1, c.steps);
            controlNames[k] = c.name.toStdString();
            controlTypes[k] = c.type.isEmpty() ? "Knob" : c.type.toStdString();
            minNorms[k] = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
            maxNorms[k] = std::clamp(c.maxPct / 100.0f, minNorms[k], 1.0f);
            totalTestPoints *= stepsPerControl[k];
        }

        int temp = pIdx;
        std::vector<int> stepIndices(numControls);
        for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
        {
            stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
            temp /= stepsPerControl[static_cast<size_t>(k)];
        }

        core::TestCase tc;
        tc.queueItemIndex = qIdx;
        tc.pointIndexInTest = pIdx + 1;
        tc.totalPointsInTest = totalTestPoints;
        tc.globalPointIndex = globalPointIdx;
        tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
        tc.testId = item.title.toStdString();
        tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
        tc.presetRecipe = itemRecipe;
        tc.stimulusType = isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
        tc.isAutonomousSynth = isAutonomousSynth;
        tc.midiNoteNumber = 60;
        tc.midiVelocity = 0.8f;
        tc.noteGateDurationSec = item.burstDurationSec;
        tc.stimulusDurationSec = item.burstDurationSec;
        tc.startFreqHz = 20.0f;
        tc.endFreqHz = 20000.0f;
        tc.numPasses = 1;
        tc.stabilizationWaitMs = 50.0;

        for (size_t k = 0; k < numControls; ++k)
        {
            int stepIdx = stepIndices[k];
            int sCount = stepsPerControl[k];
            float minN = minNorms[k];
            float maxN = maxNorms[k];

            float normVal = (sCount > 1)
                ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                : (minN + maxN) * 0.5f;

            int rawVal = static_cast<int>(std::round(normVal * 127.0f));

            core::ParameterStep ps;
            ps.paramIndex = static_cast<int>(k) + 1;
            ps.paramName = controlNames[k];
            ps.controlType = controlTypes[k];
            ps.minNormalized = minN;
            ps.maxNormalized = maxN;
            ps.normalizedValue = normVal;
            ps.rawValue = rawVal;
            ps.id = item.controls[k].id.isNotEmpty() ? item.controls[k].id.toStdString() : ("ctrl_" + std::to_string(k + 1));
            ps.sortOrder = item.controls[k].sortOrder;
            tc.parameterSteps.push_back(ps);
        }

        profSession.addTestCase(tc);
    }

    return profSession;
}

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

void MainContentComponent::applyLoadedSession(const core::SessionManifest& manifest, const std::vector<exporting::MeasuredPoint>& points)
{
    totalPointsMeasured = 0;
    sessionManager.setManifest(manifest);
    sessionManager.setMeasuredPoints(points);
    sessionManager.setDirty(false);
    curvePlotter.clear();
    for (const auto& pt : points)
    {
        curvePlotter.addMeasuredPoint(pt);
        totalPointsMeasured++;
    }

    // Apply Laboratory Conditions & Notes (1.7.12)
    drawer.setOperatorNotes(juce::String(manifest.operatorNotes));
    drawer.setAmbientTemperature(manifest.ambientTemperatureC);
    drawer.setWarmupTimeMinutes(manifest.warmupTimeMinutes);

    // Apply Hardware & Function
    drawer.setSelectedHardwareId(juce::String(manifest.hardwareId));
    drawer.setHardwareLocked(true);
    hardwareRoutingPanel.setSelectedHardware(juce::String(manifest.hardwareId), juce::String(manifest.activeFunctionId));
    hardwareRoutingPanel.setHardwareLocked(true);
    catalogSelector.setSelectedHardware(juce::String(manifest.hardwareId), juce::String(manifest.activeFunctionId));
    catalogSelector.setHardwareLocked(true);

    const auto* contract = hardwareManager.findContractById(manifest.hardwareId);
    if (contract != nullptr)
    {
        onHardwareSelected(juce::String(manifest.hardwareId), juce::String(manifest.activeFunctionId));
    }
    else
    {
        mainHeader.setHardwareInfo(juce::String(manifest.hardwareDisplayName),
                                   juce::String(manifest.activeFunctionId),
                                   juce::Image(),
                                   gui::HardwareConnectionStatus::NotApplicable);
    }

    // Synchronize Session Summary card
    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName = juce::String(manifest.hardwareDisplayName);
    summary.hardwareCategory = juce::String(manifest.targetModule);
    summary.totalPointsPlanned = static_cast<int>(points.size());
    summary.pointsMeasured = static_cast<int>(points.size());
    summary.loopbackCalibrated = true;
    sidebarStepper.setSessionSummary(summary);

    // Apply Test Queue
    suiteList.clearQueue();
    for (const auto& tc : manifest.tests)
    {
        gui::QueueItem item;
        item.title = tc.testName;
        item.stimulusType = tc.stimulusType;
        item.burstDurationSec = tc.burstDurationSec;
        item.captureMode = tc.captureMode;
        item.controls = tc.controls;
        item.totalPoints = tc.getTotalMeasurementPoints();
        item.id = "test_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
        item.status = (!points.empty()) ? gui::QueueItemStatus::Completed : gui::QueueItemStatus::Queued;

        applyBadgeForStimulus(item, tc.stimulusType);

        suiteList.addTestToQueue(item);
    }

    // Bloquear Pasos 1 y 2 para proteger el ruteo del hardware en sesiones cargadas
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::HardwareRouting, true);
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::CalibrateLoopback, true);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Completed);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Completed);

    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting, gui::SoundIdSidebarStepper::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Completed);

    bool sessionComplete = (!points.empty() && points.size() >= manifest.totalMeasuredPoints && manifest.totalMeasuredPoints > 0);
    auto targetStep = sessionComplete ? gui::WorkflowStepperBar::Step::ExportReport : gui::WorkflowStepperBar::Step::RunSession;
    auto targetSidebarStep = sessionComplete ? gui::SoundIdSidebarStepper::Step::ExportReport : gui::SoundIdSidebarStepper::Step::RunSession;

    stepperBar.setCurrentStep(targetStep);
    sidebarStepper.setCurrentStep(targetSidebarStep);

    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, (!points.empty()) ? gui::WorkflowStepperBar::StepStatus::Completed : gui::WorkflowStepperBar::StepStatus::Current);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::RunSession, (!points.empty()) ? gui::SoundIdSidebarStepper::StepStatus::Completed : gui::SoundIdSidebarStepper::StepStatus::Current);

    if (sessionComplete)
    {
        stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::ExportReport, gui::WorkflowStepperBar::StepStatus::Current);
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::ExportReport, gui::SoundIdSidebarStepper::StepStatus::Current);
    }

    if (stepperBar.onStepSelected != nullptr)
        stepperBar.onStepSelected(targetStep);
    resized();

    manualPromptLabel.setText("Session loaded: " + juce::String(manifest.hardwareDisplayName) + " (" + juce::String(points.size()) + " points)", juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
}

void MainContentComponent::handleSaveSession()
{
    if (sessionManager.getActiveSessionFile().existsAsFile())
    {
        saveSessionToFile(sessionManager.getActiveSessionFile());
        return;
    }
    handleSaveSessionAs();
}

void MainContentComponent::handleSaveSessionAs()
{
    juce::String defaultName = drawer.getActiveHardwareDisplayName().replaceCharacter(' ', '_') + ".abdlabtest";
    sessionReportManager.triggerSaveSessionAsync(exportDirectory, defaultName, [this](const juce::File& file) {
        if (file != juce::File())
            saveSessionToFile(file);
    });
}

void MainContentComponent::saveSessionToFile(const juce::File& file)
{
    core::SessionManifest manifest = buildCurrentSessionManifest();
    bool ok = sessionManager.saveSessionToPackage(file, manifest);
    if (ok)
    {
        const auto& sessionPoints = sessionManager.getMeasuredPoints();
        juce::String baseName = file.getFileNameWithoutExtension();

        core::ProfilingMetadata meta;
        meta.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
        meta.targetModule = drawer.getSelectedFunctionId().toStdString();
        meta.sampleRate = audioEngine.getCurrentSampleRate();
        meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
        meta.operatorNotes = drawer.getOperatorNotes().toStdString();
        meta.ambientTemperatureC = drawer.getAmbientTemperature();
        meta.warmupTimeMinutes = drawer.getWarmupTimeMinutes();

        sessionReportManager.exportLutAndJsonReports(exportDirectory, baseName, meta, sessionPoints);

        manualPromptLabel.setText("Session package saved successfully: " + file.getFileName(), juce::dontSendNotification);
        manualPromptLabel.setVisible(true);

        // Update file drawer preview
        drawer.openFileDrawer(exportDirectory.getFullPathName());
        hidePromptAfterDelay(4000);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Save Failed",
            "Could not write session package to:\n" + file.getFullPathName() + "\n\nPlease check disk space and folder permissions.",
            "OK"
        );
    }
}

void MainContentComponent::exportCertificationReport()
{
    if (!exportDirectory.exists())
        exportDirectory.createDirectory();

    juce::String hwId = drawer.getSelectedHardwareId();
    juce::String funcId = drawer.getSelectedFunctionId();
    juce::String baseName = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();

    exporting::SessionManifestData manifest;
    manifest.hardwareId = hwId.toStdString();
    manifest.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
    manifest.functionId = funcId.toStdString();
    manifest.functionName = drawer.getActiveFunctionDisplayName().toStdString();
    manifest.deviceType = hwId.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL_EURORACK";
    manifest.sampleRate = audioEngine.getCurrentSampleRate();

    juce::File htmlFile;
    bool success = sessionReportManager.exportCertificationHtmlReport(
        exportDirectory,
        baseName,
        manifest,
        sessionManager.getMeasuredPoints(),
        htmlFile
    );

    if (success)
    {
        manualPromptLabel.setText("Certification Report exported: " + htmlFile.getFileName(), juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        drawer.openFileDrawer(exportDirectory.getFullPathName());
        htmlFile.startAsProcess();
        hidePromptAfterDelay(5000);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Failed",
            "Could not generate HTML Certification Report at:\n" + htmlFile.getFullPathName(),
            "OK"
        );
    }
}

void MainContentComponent::updateExportReportMetrics()
{
    float avgSnr = 0.0f;
    float avgThd = 0.0f;
    int count = static_cast<int>(sessionManager.getPointCount());

    if (count > 0)
    {
        float sumSnr = 0.0f;
        float sumThd = 0.0f;
        for (const auto& p : sessionManager.getMeasuredPoints())
        {
            sumSnr += p.snrDb;
            sumThd += p.thdPercent;
        }
        avgSnr = sumSnr / static_cast<float>(count);
        avgThd = sumThd / static_cast<float>(count);
    }
    else
    {
        avgSnr = 38.5f;
        avgThd = 0.015f;
    }

    float noiseFloor = (audioEngine.getInputAutoTrim() > 1e-4f) ? -84.2f : -90.0f;
    float durationSec = static_cast<float>(count) * 2.5f;
    if (durationSec < 1.0f) durationSec = 10.0f;

    exportReportPanel.updateMetrics(avgSnr, noiseFloor, avgThd, std::max(1, count), durationSec);
}

void MainContentComponent::exportProductionPackage()
{
    const auto& sessionPoints = sessionManager.getMeasuredPoints();
    if (!exportDirectory.exists())
        exportDirectory.createDirectory();

    juce::String hwId = drawer.getSelectedHardwareId();
    juce::String funcId = drawer.getSelectedFunctionId();
    if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();
    if (funcId.isEmpty()) funcId = hardwareRoutingPanel.getSelectedFunctionId();
    juce::String baseName = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();

    // 1. C++ Header with alignas(16)
    juce::File headerFile = exportDirectory.getChildFile(baseName + "_lut.h");
    core::ProfilingMetadata meta;
    meta.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
    if (meta.hardwareName.empty())
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr) meta.hardwareName = contract->displayName;
        else meta.hardwareName = hwId.toStdString();
    }
    meta.targetModule = funcId.toStdString();
    meta.operatorMode = hwId.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL";
    meta.sampleRate = audioEngine.getCurrentSampleRate();
    meta.operatorNotes = drawer.getOperatorNotes().toStdString();
    meta.ambientTemperatureC = drawer.getAmbientTemperature();
    meta.warmupTimeMinutes = drawer.getWarmupTimeMinutes();
    exporting::LutExporter::exportToCppHeader(headerFile.getFullPathName().toStdString(), meta, (baseName + "_table").toStdString(), sessionPoints);

    // 2. JSON Telemetry Report
    juce::File jsonFile = exportDirectory.getChildFile(baseName + "_telemetry.json");
    exporting::LutExporter::exportToJsonReport(jsonFile.getFullPathName().toStdString(), meta, sessionPoints);

    // 3. HTML Certification Report with auto-print
    juce::File htmlFile = exportDirectory.getChildFile(baseName + "_Certification_Report.html");
    exporting::SessionManifestData manifest;
    manifest.hardwareId = hwId.toStdString();
    manifest.hardwareName = meta.hardwareName;
    manifest.functionId = funcId.toStdString();
    manifest.functionName = drawer.getActiveFunctionDisplayName().toStdString();
    if (manifest.functionName.empty()) manifest.functionName = funcId.toStdString();
    manifest.deviceType = hwId.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL_EURORACK";
    manifest.sampleRate = audioEngine.getCurrentSampleRate();
    manifest.operatorNotes = drawer.getOperatorNotes().toStdString();
    manifest.ambientTemperatureC = drawer.getAmbientTemperature();
    manifest.warmupTimeMinutes = drawer.getWarmupTimeMinutes();
    manifest.cppHeaderFilename = headerFile.getFileName().toStdString();
    manifest.jsonReportFilename = jsonFile.getFileName().toStdString();
    exporting::CertificationReportExporter::exportReportToHtml(htmlFile.getFullPathName().toStdString(), manifest, sessionPoints);

    // 4. Session Manifest JSON
    juce::File manifestFile = exportDirectory.getChildFile(baseName + "_manifest.json");
    exporting::LutExporter::exportSessionManifest(manifestFile.getFullPathName().toStdString(), manifest, sessionPoints);

    // Immediate visual confirmation on Step 4 certification card
    exportReportPanel.showExportSuccess(exportDirectory.getFullPathName(), baseName);

    manualPromptLabel.setText(juce::String::fromUTF8(u8"⚡ Paquete de producción exportado con éxito (C++ alignas(16), JSON, HTML)"), juce::dontSendNotification);
    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(5000);

    // 5. Asynchronous Pre-Flight Sanitizer & Local Staging Sealing via Python
    juce::Thread::launch([dirPath = exportDirectory.getFullPathName(), this]() {
        juce::ChildProcess proc;
        juce::String cmd = "python tools/sync_certification_cloud.py --dir \"" + dirPath + "\" --target local";
        if (proc.start(cmd))
        {
            proc.waitForProcessToFinish(6000);
            if (proc.getExitCode() == 0)
            {
                juce::MessageManager::callAsync([this]() {
                    manualPromptLabel.setText(juce::String::fromUTF8(u8"✓ Paquete sellado y verificado con éxito (CRC-32 & SHA-256 en dist_packages/)"), juce::dontSendNotification);
                    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
                    manualPromptLabel.setVisible(true);
                    hidePromptAfterDelay(5000);
                });
            }
        }
    });
}

void MainContentComponent::openCertificationReportHtml()
{
    if (!exportDirectory.exists())
        exportDirectory.createDirectory();

    juce::String hwId = drawer.getSelectedHardwareId();
    juce::String funcId = drawer.getSelectedFunctionId();
    if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();
    if (funcId.isEmpty()) funcId = hardwareRoutingPanel.getSelectedFunctionId();
    juce::String baseName = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();
    juce::File htmlFile = exportDirectory.getChildFile(baseName + "_Certification_Report.html");

    if (!htmlFile.existsAsFile())
    {
        exportProductionPackage();
    }

    if (htmlFile.existsAsFile())
    {
        bool launched = htmlFile.startAsProcess();
        if (!launched)
        {
            juce::URL(htmlFile).launchInDefaultBrowser();
        }
        exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"✓ Abriendo informe en el navegador: ") + htmlFile.getFileName());
    }
    else
    {
        exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"⚠️ No se pudo generar el informe HTML."), true);
    }
}

void MainContentComponent::prepareAuditionLut()
{
    const auto& sessionPoints = sessionManager.getMeasuredPoints();
    const int gridSize = 8;
    std::vector<dsp::AbdBatchedPoint> lut(gridSize * gridSize);

    if (!sessionPoints.empty())
    {
        for (int y = 0; y < gridSize; ++y)
        {
            for (int x = 0; x < gridSize; ++x)
            {
                int idx = y * gridSize + x;
                float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
                float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);

                size_t pointIdx = std::min(sessionPoints.size() - 1, static_cast<size_t>(normX * static_cast<float>(sessionPoints.size() - 1)));
                const auto& sp = sessionPoints[pointIdx];

                lut[idx].p1 = normX;
                lut[idx].p2 = normY;
                float gainLin = std::pow(10.0f, static_cast<float>(sp.secondaryValue.mean) / 20.0f);
                lut[idx].mu = juce::jlimit(0.01f, 1.0f, gainLin * (1.0f - normY * 0.3f));
                lut[idx].sigma = static_cast<float>(sp.thdPercent) * 0.01f;
            }
        }
    }
    else
    {
        for (int y = 0; y < gridSize; ++y)
        {
            for (int x = 0; x < gridSize; ++x)
            {
                int idx = y * gridSize + x;
                float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
                float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);
                lut[idx].p1 = normX;
                lut[idx].p2 = normY;
                lut[idx].mu = juce::jlimit(0.05f, 1.0f, 0.15f + 0.85f * normX * (1.0f - 0.25f * normY));
            }
        }
    }

    audioEngine.loadAuditionLut(lut, gridSize);
}

void MainContentComponent::publishCertificationToCloud()
{
    if (!exportDirectory.exists())
        exportDirectory.createDirectory();

    // Ensure production package is exported first
    exportProductionPackage();

    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"☁️ Conectando y auditando con la API Cloud de Certificación..."));

    juce::Thread::launch([dirPath = exportDirectory.getFullPathName(), this]() {
        juce::ChildProcess proc;
        juce::String cmd = "python tools/sync_certification_cloud.py --dir \"" + dirPath + "\" --target rest";
        if (proc.start(cmd))
        {
            proc.waitForProcessToFinish(10000);
            int exitCode = proc.getExitCode();

            juce::MessageManager::callAsync([this, exitCode]() {
                if (exitCode == 0)
                {
                    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"✓ ¡Certificación publicada con éxito en la Nube Comunitaria! (CERTIFIED_GOLD)"));
                    manualPromptLabel.setText(juce::String::fromUTF8(u8"✓ Publicación Cloud completada: Bundle .tar.gz verificado e indexado."), juce::dontSendNotification);
                    manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
                    manualPromptLabel.setVisible(true);
                    hidePromptAfterDelay(5000);
                }
                else
                {
                    exportReportPanel.showStatusMessage(juce::String::fromUTF8(u8"⚠️ API Cloud: Servidor no disponible o bundle rechazado. Revisa la consola."), true);
                }
            });
        }
    });
}

void MainContentComponent::promptNewSession()
{
    if (sessionManager.isDirty() && sessionManager.hasPoints())
    {
        confirmationModal.show(
            this,
            "Unsaved Changes",
            "The current session contains unsaved measurement points.\nDo you want to save before creating a new session?",
            "Save",
            "Don't Save",
            "Cancel",
            [this](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    handleSaveSession();
                    performNewSessionReset();
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    performNewSessionReset();
                }
            }
        );
    }
    else
    {
        performNewSessionReset();
    }
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
    catalogSelector.setHardwareLocked(false);
    mainHeader.clearHardware();

    // Reset session summary in sidebar stepper
    gui::SoundIdSidebarStepper::SessionSummaryInfo emptySummary;
    sidebarStepper.setSessionSummary(emptySummary);

    // Desbloquear Pasos 1 y 2 para la nueva sesión y volver a Paso 1
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::HardwareRouting, false);
    stepperBar.setStepLocked(gui::WorkflowStepperBar::Step::CalibrateLoopback, false);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::HardwareRouting, gui::WorkflowStepperBar::StepStatus::Current);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::CalibrateLoopback, gui::WorkflowStepperBar::StepStatus::Pending);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::RunSession, gui::WorkflowStepperBar::StepStatus::Pending);
    stepperBar.setStepStatus(gui::WorkflowStepperBar::Step::ExportReport, gui::WorkflowStepperBar::StepStatus::Pending);
    stepperBar.setCurrentStep(gui::WorkflowStepperBar::Step::HardwareRouting);

    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting, gui::SoundIdSidebarStepper::StepStatus::Current);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Pending);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::RunSession, gui::SoundIdSidebarStepper::StepStatus::Pending);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::ExportReport, gui::SoundIdSidebarStepper::StepStatus::Pending);
    sidebarStepper.setCurrentStep(gui::SoundIdSidebarStepper::Step::HardwareRouting);

    if (stepperBar.onStepSelected != nullptr)
        stepperBar.onStepSelected(gui::WorkflowStepperBar::Step::HardwareRouting);

    manualPromptLabel.setText(juce::String::fromUTF8(u8"Nueva sesión inicializada. Seleccione el dispositivo y objetivo a medir."), juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
    resized();
}

void MainContentComponent::handleOpenSession()
{
    if (sessionManager.isDirty() && sessionManager.hasPoints())
    {
        confirmationModal.show(
            this,
            "Unsaved Changes",
            "The current session contains unsaved measurement points.\nDo you want to save before opening another session?",
            "Save",
            "Don't Save",
            "Cancel",
            [this](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    handleSaveSession();
                    performOpenSessionFileChooser();
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    performOpenSessionFileChooser();
                }
            }
        );
    }
    else
    {
        performOpenSessionFileChooser();
    }
}

void MainContentComponent::performOpenSessionFileChooser()
{
    sessionReportManager.triggerLoadSessionAsync(exportDirectory, [this](const juce::File& file) {
        if (file.existsAsFile())
        {
            juce::String err;
            if (sessionManager.loadSessionFromPackage(file, err))
            {
                applyLoadedSession(sessionManager.getManifest(), sessionManager.getMeasuredPoints());
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Failed to Open Session",
                    err,
                    "OK"
                );
            }
        }
    });
}

void MainContentComponent::promptDeleteTest(int index, const gui::QueueItem& item)
{
    if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
    {
        confirmationModal.show(
            this,
            "Delete Measured Test",
            "Test '" + item.title + "' contains recorded measurement data.\n\nWhat would you like to do?",
            "Discard & Delete",
            "Mark as Invalid (Keep)",
            "Cancel",
            [this, index](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    suiteList.removeTestDirectly(index);
                    sessionManager.setDirty(true);
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    suiteList.invalidateTest(index);
                    sessionManager.setDirty(true);
                }
            }
        );
    }
    else
    {
        suiteList.removeTestDirectly(index);
        sessionManager.setDirty(true);
    }
}

void MainContentComponent::confirmAndExit()
{
    if (sessionManager.isDirty() && sessionManager.hasPoints())
    {
        confirmationModal.show(
            this,
            "Exit ABDAudioLab",
            "You have unsaved measurement points in this session. Do you want to save before exiting?",
            "Save and Exit",
            "Exit Without Saving",
            "Cancel",
            [this](gui::ConfirmationModalDialog::Result result) {
                if (result == gui::ConfirmationModalDialog::Result::Primary)
                {
                    handleSaveSession();
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                }
                else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                {
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                }
            }
        );
    }
    else
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
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
