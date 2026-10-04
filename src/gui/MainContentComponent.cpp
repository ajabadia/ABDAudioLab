/**
 * @file MainContentComponent.cpp
 * @brief Construction, destruction and lifecycle of MainContentComponent.
 * @author ABDSynths
 * @date 2026
 *
 * Lo que queda aqui es el nucleo del componente: el mapeo de hardware del SECTION 1,
 * el constructor, el destructor y los dos callbacks de superclase. El resto de los
 * metodos se movio a las unidades `MainContentComponent_*.cpp`, una por seccion, sin
 * tocar ni una linea de su codigo.
 *
 * Este fichero era de 3925 lineas con 80 metodos y 15 secciones cuyos numeros de
 * banner ya no seguian un orden. En un fichero asi, el numero de un metodo no dice
 * nada: hay que recorrerlo entero para saber si esta en esta seccion o en la
 * siguiente. El coste no es leer, que es lo de siempre, es modificar.
 *
 * Los ficheros nuevos son `MainContentComponentUiGovernance.cpp`,
 * `MainContentComponentHardware.cpp`, `MainContentComponentProfilingSession.cpp`,
 * `MainContentComponentReport.cpp`, `MainContentComponentPersistence.cpp`,
 * `MainContentComponentPluginUiHost.cpp` y `MainContentComponentWorkflowMode.cpp`.
 * Todos definian metodos de la MISMA clase, asi que no ha hecho falta tocar la
 * cabecera: no hay metodos nuevos, ni privado, ni cambios de visibilidad, ni nada
 * que pueda romper a un test que use la clase.
 */

#include "MainContentComponent.h"
#include "hardware/AudioMidiInterfaceDetector.h"
#include "gui/MeasurementFloatingWindow.h"
#include "core/LabDataDirectories.h"
#include "core/LabResourcePaths.h"
#include <cmath>

namespace abdaudiolab
{

// ==============================================================================
// FICHERO RAIZ DE MainContentComponent.
//
// El constructor son 1450 lineas y sigue aqui a proposito: trocearlo en metodos
// auxiliares habria significado anadir quince firmas nuevas a la cabecera para partir
// un metodo que ya se lee de arriba abajo. Un fichero largo que se puede leer de
// principio a fin no es un fichero largo; uno del que hay que buscar una pieza es lo
// que no se puede leer.
// ==============================================================================

// ==============================================================================
// SECTION 1: AUDIO ENGINE, TELEMETRY & FFT BRIDGE
// Owns UI-side audio device initialization and telemetry stream subscriptions.
// Real-time audio processing delegated to AudioEngine; FFT computation to AudioBridge/SpectrumAnalyzer.
// The measurement floating window this section used to host now lives in
// gui/MeasurementFloatingWindow.h, which is where it can be found at all.
// ==============================================================================

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
    juce::Logger::writeToLog("[MainComponent] Constructor entry point reached.");
    auto report = [&](const juce::String& msg, float prog) {
        juce::Logger::writeToLog("[Startup] " + msg + " (" + juce::String(static_cast<int>(prog * 100.0f)) + "%)");
        if (onProgress)
            onProgress(msg, prog);
    };

    report("Iniciando Motor de Audio & Controladores ASIO...", 0.15f);

    setLookAndFeel(&soundIdTheme);
    juce::LookAndFeel::setDefaultLookAndFeel(&soundIdTheme);

    // 1. Initialize Audio Engine & Restore State
    juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
    settingsFile = appData.getChildFile("AudioSettings.xml");
    juce::Logger::writeToLog("[MainComponent] Initializing audio devices from: " + settingsFile.getFullPathName());
    audioEngine.initializeAudioDevices(settingsFile);
    audioEngine.setMockHardware(hardwareManager.getMockController());

    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    juce::String devName = (dev != nullptr) ? dev->getName() : "Drivers de Audio WASAPI/ASIO OK";
    juce::Logger::writeToLog("[MainComponent] Audio verified: " + devName);
    report("Audio verificado: " + devName, 0.32f);

    hardwareManager.getContractRegistry().onProfileWarning = [this](const juce::String& warning) {
        juce::MessageManager::callAsync([this, warning]() {
            // Acumula el aviso en el panel (que arranca oculto)
            startupWarningsPanel.addNotice(warning);

            // Actualiza el badge de la campana en la cabecera.
            // El panel NO se muestra en linea: el usuario lo abre pulsando la campana.
            mainHeader.setNoticeCount(startupWarningsPanel.getNoticeCount());
        });
    };

    // Abrir el JSON desde el panel de avisos. Va al mismo sitio que el boton de
    // la ficha del cajon --el editor del sistema-- porque es la misma accion
    // sobre el mismo fichero. Dos acciones que hacen lo mismo de dos maneras
    // son dos que se quedan sin arreglar una de las dos.
    startupWarningsPanel.onOpenQuarantineJson = [](const core::quarantine::Retenido& retenido) {
        if (retenido.fichero.existsAsFile())
            retenido.fichero.startAsProcess();
    };

    startupWarningsPanel.onDismissed = [this] {
        startupWarningsPanel.setVisible(false);
        mainHeader.setNoticeCount(0);
        repaint();
    };

    // El reescaneo vacia el panel antes de volver a llenarlo. Sin esto los
    // retenidos de antes se quedan mezclados con los de ahora, y quien mire
    // la lista creera que hay mas de los que hay.
    drawer.onContractsReloadRequested = [this] {
        startupWarningsPanel.clearNotices();
        startupWarningsPanel.setVisible(false);
        mainHeader.setNoticeCount(0);
        reescargarCatalogoDeContratos();
    };

    report("Escaneando contratos de hardware en ABDSharedAssets...", 0.45f);

    // Load Contract Specifications: ABDSharedAssets/contracts es la fuente de verdad,
    // y contracts/hardware el catalogo local versionado. Ambas rutas las resuelve
    // LabResourcePaths; antes la primera era una ruta absoluta valida solo en la
    // maquina del autor y la lista arrancaba por el directorio de trabajo.
    const bool hayContratos = cargarCatalogoDeContratos();
    juce::ignoreUnused(hayContratos);

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
        auto presetsDir = core::optionalRepoResource("assets/presets");

        // Sin arbol del repositorio no hay donde escribir. Antes este bloque
        // creaba assets/presets dentro del directorio de trabajo; ahora no se
        // escribe nada fuera del repo.
        if (presetsDir.getParentDirectory().isDirectory())
        {
            presetsDir.createDirectory();
            auto czSessionFile = presetsDir.getChildFile("casio_cz101_mame_ves_session.json");
            if (!czSessionFile.existsAsFile())
            {
                auto czSuite = core::ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 100, 8);
                czSuite.exportSessionToJsonFile(czSessionFile.getFullPathName().toStdString());
            }
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

    volcarCatalogoEnLaInterfaz(false);


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
        startupWarningsPanel.repaint();
        suiteList.updateTheme();

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

        // Sin `dynamic_cast`: los dos punteros ya son de tipo `MeasurementFloatingWindow`.
        // Antes se declaraban como `juce::DocumentWindow` y habia que volver al tipo
        // real en cada uso, que es trabajo de mas para no comprobar nada.
        if (measurementViewerWindow != nullptr)
            measurementViewerWindow->updateTheme();

        if (measurementComparisonWindow != nullptr)
            measurementComparisonWindow->updateTheme();

        drawer.updateTheme();
        operatorStepModal.updateTheme();
        setupInfoTab.updateTheme();
        catalogSelector.updateTheme();
        repaint();
    };

    // Wire notification bell: toggle the warnings modal on click
    mainHeader.onNotificationBellClicked = [this] {
        if (startupWarningsPanel.isVisible())
        {
            startupWarningsPanel.setVisible(false);
        }
        else if (startupWarningsPanel.hasNotices())
        {
            // Full-screen overlay — the panel draws its own scrim + centered card
            startupWarningsPanel.setBounds(getLocalBounds());
            startupWarningsPanel.setVisible(true);
            startupWarningsPanel.toFront(true);
        }
    };

    addAndMakeVisible(mainHeader);
    addChildComponent(startupWarningsPanel);  // starts hidden, shown by bell click
    startupWarningsPanel.setVisible(false);


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


    nativeCalibrationPanel.onCalibrationApplied = [this](const math::LoopbackCalibrationData& cal) {
        float gainDb = 20.0f * std::log10(std::max(cal.recommendedTrimGain, 1e-4f));
        juce::String sign = (gainDb >= 0.0f) ? "+" : "";
        juce::String msg = "Calibración completada. Auto-trim aplicado: " + sign + juce::String(gainDb, 1) + " dB";
        manualPromptLabel.setText(msg, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);

        canonicalCalibrationState.isCalibrated = true;
        canonicalCalibrationState.sampleRate = cal.sampleRate;
        canonicalCalibrationState.isSkipped = false;
        mainHeader.updateCalibrationStatus(true, cal.sampleRate, false);

        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Completed);
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::CalibrateLoopback,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);

        auto summary = sidebarStepper.getSessionSummary();
        summary.loopbackCalibrated = true;
        summary.loopbackSnrDb = cal.snrDb > 0.0f ? cal.snrDb : 90.0f;
        sidebarStepper.setSessionSummary(summary);
        resized();
    };
    nativeCalibrationPanel.onCalibrationSkipped = [this] {
        canonicalCalibrationState.isCalibrated = false;
        canonicalCalibrationState.sampleRate = 0.0;
        canonicalCalibrationState.isSkipped = true;
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Skipped);
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::CalibrateLoopback,
                                            gui::SoundIdSidebarStepper::StepStatus::Skipped);
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

    // Coordinate WorkflowNavigationController as the Single Source of Truth
    workflowNavController.onStepChanged = [this](gui::WorkflowNavigationController::Step targetStep) {
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
            auto desc = pluginUiCoordinator.getActivePluginDescription();

            // Update standard test button state
            suiteList.setStandardTestAvailable(pluginUiCoordinator.hasActivePlugin());

            // Actualizar ProfilingSessionController como única autoridad (HITO-03.1)
            gui::session::TargetSelectionState target;
            target.targetId = "plugin_" + juce::File::createLegalFileName(desc.fileOrIdentifier).toStdString();
            target.targetName = desc.name.isNotEmpty() ? desc.name.toStdString() : "Plugin Virtual";
            target.manufacturer = desc.manufacturerName.toStdString();
            target.version = desc.version.toStdString();
            target.kind = gui::session::TargetKind::PluginVST3;
            target.isConnected = pluginUiCoordinator.hasActivePlugin();
            target.isDeterministic = true;
            target.supportsMidiInput = desc.isInstrument;
            target.supportsParameterAutomation = true;
            target.availableDomainDescription = desc.isInstrument ? "Notas MIDI C1-C6, Vel 1-127, Parámetros VST3" : "Procesamiento de Audio, Parámetros VST3";
            target.parameterCount = 0;
            profilingSessionController.selectTarget(target);
            return;
        }

        // Switching to physical hardware: safely disconnect virtual plugin if any
        pluginUiCoordinator.unloadPlugin();

        drawer.setSelectedHardwareId(hwId);
        onHardwareSelected(hwId, funcId);
        gui::SoundIdSidebarStepper::SessionSummaryInfo summary = sidebarStepper.getSessionSummary();
        summary.hardwareName = hwId;
        sidebarStepper.setSessionSummary(summary);

        const auto* c = hardwareManager.findContractById(hwId.toStdString());
        suiteList.setStandardTestAvailable(c != nullptr && !c->functions.empty());

        // Actualizar ProfilingSessionController para target hardware físico (HITO-03.1)
        gui::session::TargetSelectionState targetState;
        targetState.targetId = hwId.toStdString();
        targetState.targetName = (c != nullptr) ? c->displayName : hwId.toStdString();
        targetState.manufacturer = (c != nullptr) ? c->manufacturer : "";
        targetState.version = (c != nullptr) ? c->schemaVersion : "1.0";
        bool isAnalogue = (c != nullptr && (c->deviceType == "MANUAL_EURORACK" || c->deviceType == "ANALOGUE_PEDAL"));
        targetState.kind = isAnalogue ? gui::session::TargetKind::HardwareAnalogue : gui::session::TargetKind::HardwareDigital;
        targetState.isConnected = true;
        targetState.isDeterministic = !isAnalogue;
        targetState.supportsMidiInput = (c != nullptr && (c->deviceType == "AUTOMATED_MIDI_CC" || c->deviceType == "AUTOMATED_SYSEX"));
        targetState.supportsMidiCc = (c != nullptr && c->deviceType == "AUTOMATED_MIDI_CC");
        targetState.supportsSysEx = (c != nullptr && c->deviceType == "AUTOMATED_SYSEX");
        targetState.supportsParameterAutomation = false;
        targetState.availableDomainDescription = isAnalogue ? "Controles analógicos manuales" : "Canal MIDI, Notas y CC";
        targetState.parameterCount = (c != nullptr) ? static_cast<int>(c->functions.size()) : 0;
        profilingSessionController.selectTarget(targetState);
    };
    catalogSelector.onContinueRequested = [this] {
        if (catalogSelector.isPluginVirtualMode())
        {
            catalogSelector.setHardwareLocked(true);
            drawer.setHardwareLocked(true);

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
    addChildComponent(targetView);
    workflowNavController.setTargetView(&targetView);
    addChildComponent(excitationConfigPanel);
    workflowNavController.setExcitationConfigPanel(&excitationConfigPanel);
    recipeEditorComponent.setController(&recipeExecutionController);
    addChildComponent(recipeEditorComponent);
    workflowNavController.setRecipeEditorComponent(&recipeEditorComponent);
    profilingSessionController.onOperatorStepConfirmed = [this] { confirmManualStep(); };
    nativeCalibrationPanel.onVerifyDigitalRequested = [this] {
        profilingSessionController.verifyDigitalCalibration();
    };
    nativeCalibrationPanel.onContinueToSession = [this] {
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::RunSession);
    };

    // Prioridad 1: Sincronización atómica del paso inicial del Wizard (Tarea 1: Target & Routing)
    workflowNavController.resetToNewSession();

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
        sessionCoordinator.setHardwareContext(&hardwareManager, hid);
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

    btnPromoteToRecipe.setButtonText(gui::strings::PROMOTE_TO_RECIPE);
    btnPromoteToRecipe.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::accentBlue.withAlpha(0.2f));
    btnPromoteToRecipe.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentBlue);
    btnPromoteToRecipe.setTooltip(gui::strings::TOOLTIP_PROMOTE_TO_RECIPE);
    btnPromoteToRecipe.onClick = [this] {
        const auto* activeSession = sessionCoordinator.getActiveMeasurementSession();
        profiling::ExplorationContext ctx;
        if (activeSession != nullptr)
        {
            ctx.explorationSessionId = activeSession->sessionId;
            ctx.targetId = activeSession->profileId;
            ctx.targetName = activeSession->targetFunction;
            ctx.targetDeviceType = activeSession->deviceType;
            ctx.controlSnapshots = activeSession->controlStates;
        }
        else
        {
            auto optTarget = resolveCanonicalTarget();
            if (optTarget.has_value())
            {
                ctx.targetId = optTarget->targetId;
                ctx.targetName = optTarget->targetName;
                ctx.targetDeviceType = (optTarget->kind == gui::session::TargetKind::SyntheticFixture) ? "SyntheticFixture" : "VST3_Instrument";
            }
        }
        ctx.sampleRate = audioEngine.getSampleRate() > 0.0 ? audioEngine.getSampleRate() : 48000.0;
        ctx.channels = 2;

        profiling::RecipePromotionService promoService;
        auto result = promoService.promoteExploration(ctx);

        if (result.isExecutableRecipe())
        {
            recipeExecutionController.promoteExploration(*result.promotedRecipe, result.recipeDocumentHash);
            recipeEditorComponent.updateFromState();
            recipeEditorComponent.setVisible(true);
            workflowNavController.setStep(gui::WorkflowNavigationController::Step::CalibrateLoopback);
            lblActionReasonBanner.setText(juce::String::fromUTF8(u8"Nueva receta temporal derivada de exploración en Paso 2 (Calibration & Setup). La exploración original no ha sido certificada como medición. Esta receta no se ha guardado aún en disco."), juce::dontSendNotification);
            lblActionReasonBanner.setVisible(true);
        }
        else
        {
            juce::String msg = juce::String::fromUTF8(u8"No se puede crear una receta ejecutable: el control observado no tiene semanticId o valor normalizado.\nLa exploración se ha conservado sin modificaciones.");
            lblActionReasonBanner.setText(msg, juce::dontSendNotification);
            lblActionReasonBanner.setVisible(true);
        }
    };
    addChildComponent(btnPromoteToRecipe);

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
    };
    drawer.onDeviceDetected = [this](const juce::String& displayName) {
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
    // El boton de reescanear de la ficha de cuarentena. Es lo que hace que
    // levantar una retencion termine en un clic y no en un reinicio: se edita
    // el JSON, se vuelve, y el cajon relee el catalogo entero.

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
                    }
                }
            );
        }
        else
        {
            performNewSessionReset();
        }
    };
    addChildComponent(drawer);

    initializeAutoUpdater();

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
    startTimerHz(25);

    // Ensure initial state starts completely clean with no hardware selected
    drawer.clearSelectedHardware();
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

    setupGuidedWorkflowInitialData();

    profilingSessionController.addListener(this);
    targetView.updateFromSnapshot(profilingSessionController.getCurrentSnapshot());
    excitationConfigPanel.updateFromSnapshot(profilingSessionController.getCurrentSnapshot());
    if (profilingRunView != nullptr)
        profilingRunView->updateFromSnapshot(profilingSessionController.getCurrentSnapshot());

    report("Listo.", 1.0f);
    juce::Logger::writeToLog("[MainComponent] Constructor finished successfully.");
}

MainContentComponent::~MainContentComponent()
{
    profilingSessionController.removeListener(this);
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

} // namespace abdaudiolab
