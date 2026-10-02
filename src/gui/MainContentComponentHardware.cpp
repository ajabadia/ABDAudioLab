/**
 * @file MainContentComponentHardware.cpp
 * @brief Target hardware selection, workspace navigation and auxiliary windows.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"
#include "hardware/AudioMidiInterfaceDetector.h"
#include "gui/measurement/MeasurementViewerPanel.h"
#include "gui/measurement/MeasurementComparisonPanel.h"
#include "gui/MeasurementFloatingWindow.h"
#include "synth/Sha256.h"

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
        measurementViewerWindow = std::make_unique<gui::MeasurementFloatingWindow>(
            juce::String::fromUTF8(u8"ABDAudioLab — Visor de Mediciones FAIR / LNL"),
            panel,
            1050, 720, 800, 550
        );
    }

    measurementViewerWindow->updateTheme();
    measurementViewerWindow->setVisible(true);
    measurementViewerWindow->toFront(true);
}

void MainContentComponent::openMeasurementComparisonWindow()
{
    if (measurementComparisonWindow == nullptr)
    {
        auto* panel = new gui::measurement::MeasurementComparisonPanel();
        measurementComparisonWindow = std::make_unique<gui::MeasurementFloatingWindow>(
            juce::String::fromUTF8(u8"ABDAudioLab — Comparador Multivariante de Mediciones FAIR / LNL"),
            panel,
            1150, 750, 900, 600
        );
    }

    measurementComparisonWindow->updateTheme();
    measurementComparisonWindow->setVisible(true);
    measurementComparisonWindow->toFront(true);
}

void MainContentComponent::toggleStudioTopologyWindow()
{
    abd::topology::TopologyTargetInfo target;

    const auto canonicalTarget = resolveCanonicalTarget();
    if (canonicalTarget.has_value() && canonicalTarget->kind == gui::session::TargetKind::PluginVST3)
    {
        auto desc = pluginUiCoordinator.getActivePluginDescription();
        juce::String pluginName = desc.name.isNotEmpty() ? desc.name : juce::String(canonicalTarget->targetName);
        target.name = pluginName;
        target.category = desc.isInstrument ? "VST3 Virtual Instrument" : "VST3 Virtual Effect";
        target.details = "Virtual VST3 | Internal Direct Bus (ITB)";
        target.imageRelPath = desc.isInstrument ? "models/generic-digital-keyboard.png" : "models/generic-audio-rack.png";
        target.hasMidi = true;
        target.isVirtualPlugin = true;
    }
    else if (canonicalTarget.has_value())
    {
        const auto* contract = hardwareManager.findContractById(canonicalTarget->targetId);
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
        const auto canonicalTarget = resolveCanonicalTarget();
        const auto* contract = canonicalTarget.has_value() ? hardwareManager.findContractById(canonicalTarget->targetId) : nullptr;
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
    sessionCoordinator.setHardwareContext(&hardwareManager, hwId);

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

    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting, gui::SoundIdSidebarStepper::StepStatus::Completed);

    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName = juce::String(contract->displayName);
    summary.hardwareCategory = juce::String(contract->deviceType);
    sidebarStepper.setSessionSummary(summary);

    // Sincronizar TargetSelectionState con ProfilingSessionController y SoundIdTargetView
    gui::session::TargetSelectionState target;
    target.targetId = contract->id;
    target.targetName = contract->displayName;
    target.manufacturer = !contract->manufacturer.empty() ? contract->manufacturer : (!contract->brand.empty() ? contract->brand : "Hardware Manufacturer");
    target.version = "1.0";
    target.kind = gui::session::TargetKind::HardwareAnalogue;
    target.isConnected = (connStatus == gui::HardwareConnectionStatus::Connected);
    target.isDeterministic = false;
    target.availableDomainDescription = funcName.toStdString() + " (" + contract->deviceType + ")";
    target.parameterCount = static_cast<int>(contract->functions.size());

    profilingSessionController.selectTarget(target);
    profilingSessionController.updateAuditResult(
        synth::ApprovalStatus::ApprovedWithWarnings,
        "Repetibilidad analogica / audio loopback",
        "Reset de compuerta requerido",
        100.0,
        true,
        { "Latencia y calibracion analogica requerida" },
        "Hardware conectado y validado para ruteo");

    targetView.updateFromSnapshot(profilingSessionController.getCurrentSnapshot());

    auto calStatus = sidebarStepper.getStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback);
    if (calStatus != gui::SoundIdSidebarStepper::StepStatus::Completed && calStatus != gui::SoundIdSidebarStepper::StepStatus::Skipped)
    {
        sidebarStepper.setCurrentStep(gui::SoundIdSidebarStepper::Step::CalibrateLoopback);
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::CalibrateLoopback);
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

} // namespace abdaudiolab
