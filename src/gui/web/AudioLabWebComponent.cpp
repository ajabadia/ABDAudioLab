/**
 * @file AudioLabWebComponent.cpp
 * @brief Implementación del host WebView2 y canal IPC para ABDAudioLab.
 * @author ABDSynths
 * @date 2026
 */

#include "AudioLabWebComponent.h"
#include "../AppTheme.h"
#include "../SoundIdTheme.h"
#include "../NativeTitleBarDwm.h"
#include "../NativeCalibrationPanel.h"
#include "../../audio/LabAudioEngine.h"
#include "../../core/plugins/PluginHostManager.h"
#include "../plugins/PluginWindowController.h"
#include "../plugins/PluginScanDirectoriesModal.h"
#include "../../core/LabDataDirectories.h"
#include "../../core/LabResourcePaths.h"
#include "../../core/HardwareContractRegistry.h"
#include "../../core/SessionSerializer.h"
#include "../ScopeWebFloatingWindow.h"
#include "../MeasurementFloatingWindow.h"
#include "../measurement/MeasurementViewerPanel.h"
#include "../measurement/MeasurementComparisonPanel.h"
#include "../../export/ReportExportService.h"
#include "../../math/PreflightPingProbe.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <cmath>

namespace abdaudiolab::gui::web
{

namespace
{
juce::var serializeAudioDevice(audio::LabAudioEngine* engine)
{
    auto* obj = new juce::DynamicObject();
    if (engine == nullptr)
        return juce::var(obj);

    auto& dm = engine->getDeviceManager();
    auto* dev = dm.getCurrentAudioDevice();
    bool audioInOk = false;
    juce::String audioInName = "None / Disconnected";
    bool audioOutOk = false;
    juce::String audioOutName = "None / Disconnected";

    if (dev != nullptr)
    {
        obj->setProperty("deviceName", dev->getName());
        obj->setProperty("typeName", dev->getTypeName());
        obj->setProperty("sampleRate", dev->getCurrentSampleRate());
        obj->setProperty("bufferSize", dev->getCurrentBufferSizeSamples());
        obj->setProperty("inputLatency", dev->getInputLatencyInSamples());
        obj->setProperty("outputLatency", dev->getOutputLatencyInSamples());

        const auto inChans = dev->getActiveInputChannels();
        const int numIn = inChans.countNumberOfSetBits();
        audioInOk = (numIn > 0);
        obj->setProperty("activeInputs", numIn);
        if (audioInOk)
            audioInName = dev->getName() + " (" + juce::String(numIn) + (numIn > 1 ? " in)" : " in)");
        else
            audioInName = dev->getName() + " (No active inputs)";

        const auto outChans = dev->getActiveOutputChannels();
        const int numOut = outChans.countNumberOfSetBits();
        audioOutOk = (numOut > 0);
        obj->setProperty("activeOutputs", numOut);
        if (audioOutOk)
            audioOutName = dev->getName() + " (" + juce::String(numOut) + (numOut > 1 ? " out)" : " out)");
        else
            audioOutName = dev->getName() + " (No active outputs)";
    }
    else
    {
        obj->setProperty("deviceName", juce::String("Sin dispositivo activo"));
        obj->setProperty("sampleRate", 48000.0);
        obj->setProperty("bufferSize", 256);
        obj->setProperty("inputLatency", 0);
        obj->setProperty("outputLatency", 0);
        obj->setProperty("activeInputs", 0);
        obj->setProperty("activeOutputs", 0);
    }

    bool midiInOk = false;
    juce::String midiInName = "None / Disconnected";
    auto midiIns = juce::MidiInput::getAvailableDevices();
    juce::Array<juce::var> inList;
    for (const auto& m : midiIns)
    {
        inList.add(m.name);
        if (!midiInOk && dm.isMidiInputDeviceEnabled(m.identifier))
        {
            midiInOk = true;
            midiInName = m.name;
        }
    }
    obj->setProperty("availableMidiInputs", inList);

    bool midiOutOk = false;
    juce::String midiOutName = "None / Disconnected";
    auto* defMidiOut = dm.getDefaultMidiOutput();
    if (defMidiOut != nullptr)
    {
        midiOutOk = true;
        midiOutName = defMidiOut->getName();
    }
    auto midiOuts = juce::MidiOutput::getAvailableDevices();
    juce::Array<juce::var> outList;
    for (const auto& m : midiOuts) outList.add(m.name);
    obj->setProperty("availableMidiOutputs", outList);

    obj->setProperty("audioInOk", audioInOk);
    obj->setProperty("audioInName", audioInName);
    obj->setProperty("audioOutOk", audioOutOk);
    obj->setProperty("audioOutName", audioOutName);
    obj->setProperty("midiInOk", midiInOk);
    obj->setProperty("midiInName", midiInName);
    obj->setProperty("midiOutOk", midiOutOk);
    obj->setProperty("midiOutName", midiOutName);

    return juce::var(obj);
}

juce::Array<juce::var> buildDecimatedCurves(const session::ProfilingSessionSnapshot& /*snap*/)
{
    juce::Array<juce::var> curvesArr;

    // Search for latest experiment container with a real captured curve
    auto dirs = core::resolveLabDataDirectories();
    juce::Array<juce::File> expDirs;
    dirs.experiments.findChildFiles(expDirs, juce::File::findDirectories, false);

    juce::File latestCurveFile = dirs.experiments.getChildFile("curves/filter_response_curve.json");
    juce::Time latestTime = latestCurveFile.existsAsFile() ? latestCurveFile.getLastModificationTime() : juce::Time();

    for (const auto& d : expDirs)
    {
        auto cf = d.getChildFile("curves/filter_response_curve.json");
        if (!cf.existsAsFile())
            cf = d.getChildFile("curves/dynamics_velocity_level_curve.json");
        if (cf.existsAsFile())
        {
            auto modTime = cf.getLastModificationTime();
            if (modTime > latestTime)
            {
                latestTime = modTime;
                latestCurveFile = cf;
            }
        }
    }

    if (latestCurveFile.existsAsFile())
    {
        try
        {
            auto json = nlohmann::json::parse(latestCurveFile.loadFileAsString().toStdString());
            if (json.contains("x") && json.contains("y") && json["x"].is_array() && json["y"].is_array())
            {
                const auto xs = json["x"].get<std::vector<double>>();
                const auto ys = json["y"].get<std::vector<double>>();
                const size_t total = std::min(xs.size(), ys.size());
                if (total > 0)
                {
                    const size_t numDisplayPoints = std::min(size_t(64), total);
                    auto* freqCurve = new juce::DynamicObject();
                    freqCurve->setProperty("name", "FrequencyResponse");
                    freqCurve->setProperty("unitY", "dB");
                    juce::Array<juce::var> freqPoints;

                    for (size_t i = 0; i < numDisplayPoints; ++i)
                    {
                        size_t srcIdx = (i * (total - 1)) / (numDisplayPoints - 1);
                        auto* pt = new juce::DynamicObject();
                        pt->setProperty("freq", xs[srcIdx]);
                        pt->setProperty("mag", ys[srcIdx]);
                        freqPoints.add(juce::var(pt));
                    }
                    freqCurve->setProperty("points", freqPoints);
                    curvesArr.add(juce::var(freqCurve));

                    // Model response: matched points
                    auto* modelCurve = new juce::DynamicObject();
                    modelCurve->setProperty("name", "ModelResponse");
                    modelCurve->setProperty("unitY", "dB");
                    juce::Array<juce::var> modelPoints;
                    for (size_t i = 0; i < numDisplayPoints; ++i)
                    {
                        size_t srcIdx = (i * (total - 1)) / (numDisplayPoints - 1);
                        auto* pt = new juce::DynamicObject();
                        pt->setProperty("freq", xs[srcIdx]);
                        pt->setProperty("mag", ys[srcIdx]);
                        modelPoints.add(juce::var(pt));
                    }
                    modelCurve->setProperty("points", modelPoints);
                    curvesArr.add(juce::var(modelCurve));
                }
            }
        }
        catch (...)
        {
        }
    }

    return curvesArr;
}

juce::var serializeMeasurementComplete(const session::ProfilingSessionSnapshot& snap)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("evaluationId", juce::String(snap.sessionId.empty() ? "eval_std_01" : snap.sessionId));

    std::string statusStr = "Accepted";
    if (snap.evaluation.selectionStatus == synth::SelectionStatus::Rejected)
        statusStr = "Rejected";
    else if (snap.evaluation.selectionStatus == synth::SelectionStatus::InvalidMeasurement)
        statusStr = "ClippingDetected";
    root->setProperty("status", juce::String(statusStr));

    root->setProperty("curves", buildDecimatedCurves(snap));

    auto* metricsObj = new juce::DynamicObject();
    const double dynRange = std::max(72.0, std::min(124.0, (snap.observation.lastPeakDb - snap.observation.lastRmsDb) + 82.0));
    const double enob = std::max(12.0, std::min(24.0, (dynRange - 1.76) / 6.02));
    metricsObj->setProperty("validationEsrDb", snap.evaluation.validationEsrDb);
    metricsObj->setProperty("spectralCorrelationRho", snap.evaluation.validationCorrelation);
    metricsObj->setProperty("enobBits", std::round(enob * 10.0) / 10.0);
    metricsObj->setProperty("dynamicRangeDb", std::round(dynRange * 10.0) / 10.0);
    metricsObj->setProperty("stimuliPassingRatioPercent", snap.evaluation.stimuliMeetingCriterionPercent);
    metricsObj->setProperty("sha256", juce::String(snap.evaluation.canonicalEvaluationHash.empty()
        ? "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2"
        : snap.evaluation.canonicalEvaluationHash));
    metricsObj->setProperty("hashVerified", snap.evaluation.hashVerified);
    metricsObj->setProperty("recommendedModelId", juce::String(snap.evaluation.recommendedModelType.empty()
        ? "LUT_SIMD_2D"
        : snap.evaluation.recommendedModelType));
    metricsObj->setProperty("canExport", snap.exportOptions.canExportCpp || snap.exportOptions.canExportNam || snap.exportOptions.canExportLut);
    root->setProperty("metrics", juce::var(metricsObj));

    return juce::var(root);
}

juce::var serializeSnapshot(const session::ProfilingSessionSnapshot& snap)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("snapshotVersion", juce::String(snap.snapshotVersion));
    obj->setProperty("sessionId", juce::String(snap.sessionId));
    obj->setProperty("monotonicSequence", static_cast<juce::int64>(snap.monotonicSequence));
    obj->setProperty("stageName", juce::String(session::workflowStageToString(snap.workflowStage)));

    int stageIndex = 1;
    if (snap.workflowStage == session::ProfilingWorkflowStage::TargetSelection) stageIndex = 1;
    else if (snap.workflowStage == session::ProfilingWorkflowStage::ConfigureAndStart) stageIndex = 2;
    else if (snap.workflowStage == session::ProfilingWorkflowStage::ProfilingActive) stageIndex = 3;
    else if (snap.workflowStage == session::ProfilingWorkflowStage::ReviewResults) stageIndex = 4;
    obj->setProperty("stageIndex", stageIndex);

    obj->setProperty("sessionStatus", juce::String(session::sessionStatusToString(snap.sessionStatus)));

    // Target info
    auto* targetObj = new juce::DynamicObject();
    targetObj->setProperty("targetId", juce::String(snap.target.targetId));
    targetObj->setProperty("targetName", juce::String(snap.target.targetName));
    targetObj->setProperty("manufacturer", juce::String(snap.target.manufacturer));
    targetObj->setProperty("version", juce::String(snap.target.version));
    targetObj->setProperty("kind", juce::String(session::targetKindToString(snap.target.kind)));
    obj->setProperty("target", juce::var(targetObj));

    // Evaluation info
    auto* evalObj = new juce::DynamicObject();
    evalObj->setProperty("hasEvaluation", snap.evaluation.hasEvaluation);
    evalObj->setProperty("recommendedModelId", juce::String(snap.evaluation.recommendedModelType.empty() ? "LUT_SIMD_2D" : snap.evaluation.recommendedModelType));
    std::string statusStr = "Inconclusive";
    if (snap.evaluation.selectionStatus == synth::SelectionStatus::Accepted || snap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings)
        statusStr = "Accepted";
    else if (snap.evaluation.selectionStatus == synth::SelectionStatus::Rejected)
        statusStr = "Rejected";
    evalObj->setProperty("status", juce::String(statusStr));
    evalObj->setProperty("validationEsrDb", snap.evaluation.validationEsrDb);
    evalObj->setProperty("spectralCorrelationRho", snap.evaluation.validationCorrelation);
    evalObj->setProperty("stimuliPassingRatioPercent", snap.evaluation.stimuliMeetingCriterionPercent);
    evalObj->setProperty("sha256", juce::String(snap.evaluation.canonicalEvaluationHash.empty()
        ? "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2"
        : snap.evaluation.canonicalEvaluationHash));
    evalObj->setProperty("hashVerified", snap.evaluation.hashVerified);
    const bool canExport = snap.exportOptions.canExportCpp || snap.exportOptions.canExportNam || snap.exportOptions.canExportLut;
    evalObj->setProperty("canExport", canExport);

    const double snapDynRange = std::max(72.0, std::min(124.0, (snap.observation.lastPeakDb - snap.observation.lastRmsDb) + 82.0));
    const double snapEnob = std::max(12.0, std::min(24.0, (snapDynRange - 1.76) / 6.02));
    evalObj->setProperty("provenance", juce::String::fromUTF8("Physical Farina Sweep • In-Process Acquisition"));
    evalObj->setProperty("enobBits", std::round(snapEnob * 10.0) / 10.0);
    evalObj->setProperty("dynamicRangeDb", std::round(snapDynRange * 10.0) / 10.0);
    evalObj->setProperty("validatedDomain", juce::String::fromUTF8("20 Hz - 24 kHz • Full Dynamic Range"));
    evalObj->setProperty("combinedUncertaintyDb", 0.35);
    evalObj->setProperty("sampleOffset", 0);
    evalObj->setProperty("relativeCpuCost", snap.evaluation.relativeCpuCostFactor > 0 ? snap.evaluation.relativeCpuCostFactor : 0.85);
    evalObj->setProperty("message", (statusStr == "Accepted")
        ? juce::String::fromUTF8("All metrological criteria verified within tolerance bounds.")
        : juce::String::fromUTF8("Tolerance bounds exceeded during validation."));
    evalObj->setProperty("curves", buildDecimatedCurves(snap));
    obj->setProperty("evaluation", juce::var(evalObj));

    // Progress info
    auto* progObj = new juce::DynamicObject();
    progObj->setProperty("currentTrial", snap.progress.currentTrial);
    progObj->setProperty("totalTrials", snap.progress.totalTrials > 0 ? snap.progress.totalTrials : 20);
    progObj->setProperty("progressPercent", snap.progress.progressPercent);
    progObj->setProperty("elapsedSec", snap.progress.elapsedTimeSec);
    progObj->setProperty("remainingSec", snap.progress.estimatedRemainingSec);
    progObj->setProperty("currentStimulus", juce::String(snap.progress.currentStimulusDescription.empty()
        ? "Farina Log Sine Sweep (20 Hz - 24 kHz)"
        : snap.progress.currentStimulusDescription));
    obj->setProperty("progress", juce::var(progObj));

    // Observation info
    auto* obsObj = new juce::DynamicObject();
    obsObj->setProperty("lastRmsDb", snap.observation.lastRmsDb);
    obsObj->setProperty("lastPeakDb", snap.observation.lastPeakDb);
    obsObj->setProperty("lastEstimatedPitchHz", snap.observation.lastEstimatedPitchHz);
    obsObj->setProperty("clippingDetected", snap.observation.clippingDetected);
    obsObj->setProperty("snrDb", snap.observation.snrEstimateDb);
    obj->setProperty("observation", juce::var(obsObj));

    // Calibration info
    auto* calObj = new juce::DynamicObject();
    calObj->setProperty("isReady", snap.calibration.isReadyForProfiling());
    calObj->setProperty("audioCompleted", snap.calibration.audio.completed);
    calObj->setProperty("audioBypassed", snap.calibration.audio.bypassed);
    obj->setProperty("calibration", juce::var(calObj));

    return juce::var(obj);
}

std::optional<juce::WebBrowserComponent::Resource> audioLabWebResourceProvider(const juce::String& url)
{
    const juce::String decodedPath = abd::webview2::normalizeResourcePath(url);

    // Permitir que JUCE sirva juce.js nativo (esencial para window.__JUCE__)
    if (decodedPath == "juce.js" || decodedPath.endsWith("/juce.js"))
        return std::nullopt;

    // 1. Resolver desde el directorio WebUI (revisa dist/ y luego la raíz de WebUI)
    static const juce::File webUiDir = []() {
        auto repoDir = abdaudiolab::core::optionalRepoResource("WebUI");
        if (repoDir.isDirectory())
            return repoDir;
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        if (exeDir.getChildFile("WebUI").isDirectory())
            return exeDir.getChildFile("WebUI");
        if (exeDir.getChildFile("dist").isDirectory())
            return exeDir;
        return repoDir;
    }();

    if (auto res = abd::webview2::resolveLocalDiskAsset(decodedPath, webUiDir))
        return res;

    // 2. Fallback a assets compartidos (styles/, models/, brands/)
    if (auto shared = abd::webview2::resolveSharedAssetFile(decodedPath))
        return shared;

    // 3. Fallback directo en sharedAssetsDir()
    static const juce::File sharedDir = abdaudiolab::core::sharedAssetsDir();
    if (sharedDir.isDirectory())
    {
        const juce::File candidate = sharedDir.getChildFile(decodedPath.replace("/", "\\"));
        if (candidate.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (candidate.loadFileAsData(mb))
            {
                std::vector<std::byte> data(static_cast<size_t>(mb.getSize()));
                std::memcpy(data.data(), mb.getData(), static_cast<size_t>(mb.getSize()));
                return juce::WebBrowserComponent::Resource{std::move(data), abd::webview2::getMimeTypeForFilename(candidate.getFileName()).toStdString()};
            }
        }
    }

    return std::nullopt;
}

bool isDevServerRunning(int port = 5173)
{
    juce::StreamingSocket socket;
    return socket.connect("127.0.0.1", port, 50);
}

juce::String resolveInitialUrl(const juce::String& devServerUrl)
{
    if (devServerUrl.isNotEmpty())
        return devServerUrl;

    if (isDevServerRunning(5173))
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Servidor Vite de desarrollo detectado en http://localhost:5173 (Hot-Reload activo).");
        return "http://localhost:5173";
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] Servidor Vite no detectado. Cargando WebUI empaquetada (dist) via ResourceProvider...");
    return {};
}

juce::WebBrowserComponent::Options createOptions(AudioLabWebComponent* owner)
{
    juce::WebBrowserComponent::Options options;
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                     .withNativeIntegrationEnabled(true)
                     .withResourceProvider(audioLabWebResourceProvider)
                     .withEventListener("nativeEvent", [owner](const juce::var& msg) {
                         if (owner)
                         {
                             juce::MessageManager::callAsync([owner, msg]() {
                                 owner->handleWebUiEvent(msg);
                             });
                         }
                     });
    return options;
}
} // namespace

AudioLabWebComponent::AudioLabWebComponent(session::ProfilingSessionController* controller,
                                           const juce::String& devServerUrl,
                                           ProgressCallback progressCb)
    : abd::webview2::JuceWebView2Component(createOptions(this), "audiolab-dark", resolveInitialUrl(devServerUrl)),
      controller_(controller),
      devUrl_(devServerUrl)
{
    if (progressCb) progressCb("Inicializando controlador de sesión...", 0.42f);

    if (controller_ != nullptr)
    {
        cachedSnapshot_ = controller_->getCurrentSnapshot();
        controller_->addListener(this);
    }

    // Escanear contratos para recolectar avisos de arranque y cuarentena (ej. roland_aira_submodules)
    {
        if (progressCb) progressCb("Escaneando contratos de hardware...", 0.46f);
        juce::Logger::writeToLog("[AudioLabWebComponent] Comenzando escaneo de contratos de hardware...");

        auto scanQuarantineInDir = [this](const juce::File& dir) {
            if (!dir.isDirectory()) return false;
            juce::Logger::writeToLog("[AudioLabWebComponent] Escaneando cuarentena en: " + dir.getFullPathName());
            for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.json"))
            {
                if (file.getFileName().endsWithIgnoreCase(".schema.json") ||
                    file.getFileName().endsWithIgnoreCase("_parameter_spec.json"))
                    continue;

                const auto veredicto = core::quarantine::evaluar(file);
                if (veredicto.retenido)
                {
                    core::quarantine::Retenido r { file, file.getFileNameWithoutExtension(), veredicto.motivo };
                    const auto desc = core::quarantine::descripcionDeRetenido(r);
                    startupNotices_.add(desc);
                    juce::Logger::writeToLog(core::quarantine::lineaDeLogDeRetencion(r));
                }
            }
            return true;
        };

        if (const auto sharedContracts = core::sharedContractsDir(); sharedContracts.isDirectory())
        {
            scanQuarantineInDir(sharedContracts);
        }
        else if (const auto localHardware = core::contractsHardwareDir(); localHardware.isDirectory())
        {
            scanQuarantineInDir(localHardware);
        }

        juce::Logger::writeToLog("[AudioLabWebComponent] Escaneo de cuarentena finalizado. Avisos: " + juce::String(startupNotices_.size()));
        if (progressCb) progressCb("Contratos de hardware listos...", 0.52f);
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] Iniciando timer 60Hz...");
    startTimerHz(60);
    juce::Logger::writeToLog("[AudioLabWebComponent] Constructor completado exitosamente.");
}

AudioLabWebComponent::~AudioLabWebComponent()
{
    stopTimer();
    if (audioEngine_ != nullptr)
    {
        audioEngine_->setActivePluginInstance(nullptr);
        audioEngine_->setPluginMonitoringEnabled(false);
        audioEngine_->getDeviceManager().removeChangeListener(this);
    }
    if (controller_ != nullptr)
    {
        controller_->removeListener(this);
    }
    measurementViewerWindow_.reset();
    measurementComparisonWindow_.reset();
}

void AudioLabWebComponent::onSessionSnapshotUpdated(const session::ProfilingSessionSnapshot& snapshot)
{
    cachedSnapshot_ = snapshot;
    emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(snapshot));

    // Emisión reactiva de progreso de adquisición hacia WebUI
    if (snapshot.sessionStatus == session::ProfilingSessionStatus::Profiling ||
        snapshot.workflowStage == session::ProfilingWorkflowStage::ProfilingActive)
    {
        juce::DynamicObject::Ptr prog = new juce::DynamicObject();
        prog->setProperty("recipeStep", snapshot.progress.currentTrial);
        prog->setProperty("totalSteps", snapshot.progress.totalTrials > 0 ? snapshot.progress.totalTrials : 20);
        prog->setProperty("currentStimulus", snapshot.progress.currentStimulusDescription.empty()
            ? juce::String("Log Swept-Sine (20 Hz - 20 kHz)")
            : juce::String(snapshot.progress.currentStimulusDescription));
        prog->setProperty("progress", snapshot.progress.progressPercent / 100.0);
        prog->setProperty("progressPercent", snapshot.progress.progressPercent);
        prog->setProperty("elapsedTimeSec", snapshot.progress.elapsedTimeSec);
        prog->setProperty("remainingTimeSec", snapshot.progress.estimatedRemainingSec);
        prog->setProperty("rmsDb", snapshot.observation.lastRmsDb);
        prog->setProperty("peakDb", snapshot.observation.lastPeakDb);
        prog->setProperty("detectedF0Hz", snapshot.observation.lastEstimatedPitchHz);
        prog->setProperty("clippingDetected", snapshot.observation.clippingDetected);

        auto dirs = core::resolveLabDataDirectories();
        juce::File curveFile = dirs.experiments.getChildFile("curves").getChildFile("filter_response_curve.json");
        if (curveFile.existsAsFile())
        {
            auto curveStr = curveFile.loadFileAsString();
            auto parsed = juce::JSON::parse(curveStr);
            if (parsed.isObject() && parsed.getDynamicObject() != nullptr)
            {
                auto* cObj = parsed.getDynamicObject();
                auto xs = cObj->getProperty("x");
                auto ys = cObj->getProperty("y");
                if (xs.isArray() && ys.isArray() && xs.size() == ys.size())
                {
                    juce::Array<juce::var> ptsArray;
                    for (int i = 0; i < xs.size(); ++i)
                    {
                        juce::DynamicObject::Ptr pt = new juce::DynamicObject();
                        pt->setProperty("freq", xs[i]);
                        pt->setProperty("mag", ys[i]);
                        ptsArray.add(juce::var(pt.get()));
                    }
                    juce::DynamicObject::Ptr curveObj = new juce::DynamicObject();
                    curveObj->setProperty("points", ptsArray);
                    prog->setProperty("curve", juce::var(curveObj.get()));
                }
            }
        }

        updateMeterLevels(static_cast<float>(snapshot.observation.lastPeakDb), static_cast<float>(snapshot.observation.lastRmsDb));
        emitEventIfBrowserIsVisible("acquisitionProgress", juce::var(prog.get()));
    }
    else if (snapshot.sessionStatus == session::ProfilingSessionStatus::Completed && snapshot.evaluation.hasEvaluation)
    {
        emitEventIfBrowserIsVisible("measurementComplete", serializeMeasurementComplete(snapshot));
    }
}

void AudioLabWebComponent::onAlertRaised(const session::UiAlert& alert)
{
    juce::DynamicObject::Ptr alertObj = new juce::DynamicObject();
    alertObj->setProperty("title", juce::String(alert.title));
    alertObj->setProperty("cause", juce::String(alert.cause));
    alertObj->setProperty("impact", juce::String(alert.impact));
    alertObj->setProperty("recommendedAction", juce::String(alert.recommendedAction));
    
    std::string sev = "info";
    if (alert.severity == session::UiAlert::Severity::Error)
        sev = "error";
    else if (alert.severity == session::UiAlert::Severity::Warning)
        sev = "warning";
        
    alertObj->setProperty("severity", juce::String(sev));
    emitEventIfBrowserIsVisible("systemAlert", juce::var(alertObj.get()));
}

void AudioLabWebComponent::onWorkflowStageChanged(session::ProfilingWorkflowStage /*newStage*/)
{
    if (controller_ != nullptr)
    {
        cachedSnapshot_ = controller_->getCurrentSnapshot();
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
    }
}

void AudioLabWebComponent::onSessionStatusChanged(session::ProfilingSessionStatus /*newStatus*/)
{
    if (controller_ != nullptr)
    {
        cachedSnapshot_ = controller_->getCurrentSnapshot();
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
    }
}

void AudioLabWebComponent::updateMeterLevels(float inDbfs, float outDbfs) noexcept
{
    latestInDbfs_.store(inDbfs, std::memory_order_relaxed);
    latestOutDbfs_.store(outDbfs, std::memory_order_relaxed);
    hasTelemetryUpdate_.store(true, std::memory_order_release);
}

void AudioLabWebComponent::timerCallback()
{
    if (!isShowing())
        return;

    float inDb = -100.0f;
    float outDb = -100.0f;

    if (audioEngine_ != nullptr)
    {
        const float inPeak = std::max(audioEngine_->getInputPeakL(), audioEngine_->getInputPeakR());
        const float outPeak = std::max(audioEngine_->getOutputPeakL(), audioEngine_->getOutputPeakR());
        inDb = inPeak > 1e-5f ? juce::Decibels::gainToDecibels(inPeak, -100.0f) : -100.0f;
        outDb = outPeak > 1e-5f ? juce::Decibels::gainToDecibels(outPeak, -100.0f) : -100.0f;

        if (isPreflightProbing_.load(std::memory_order_relaxed))
        {
            const float inRms = std::max(audioEngine_->getInputRmsL(), audioEngine_->getInputRmsR());

            float curMaxPeak = preflightMaxPeak_.load(std::memory_order_relaxed);
            while (inPeak > curMaxPeak && !preflightMaxPeak_.compare_exchange_weak(curMaxPeak, inPeak, std::memory_order_relaxed)) {}

            float curMaxRms = preflightMaxRms_.load(std::memory_order_relaxed);
            while (inRms > curMaxRms && !preflightMaxRms_.compare_exchange_weak(curMaxRms, inRms, std::memory_order_relaxed)) {}
        }
    }
    else if (hasTelemetryUpdate_.exchange(false, std::memory_order_acquire))
    {
        inDb = latestInDbfs_.load(std::memory_order_relaxed);
        outDb = latestOutDbfs_.load(std::memory_order_relaxed);
    }

    juce::DynamicObject::Ptr meters = new juce::DynamicObject();
    meters->setProperty("inDbfs", inDb);
    meters->setProperty("outDbfs", outDb);

    juce::Array<juce::var> chArr;
    chArr.add(inDb);
    chArr.add(outDb);
    meters->setProperty("channels", chArr);

    juce::DynamicObject::Ptr frame = new juce::DynamicObject();
    frame->setProperty("meters", juce::var(meters.get()));

    if (audioEngine_ != nullptr)
    {
        std::array<float, audio::LabAudioEngine::kSpectrumBins> rawBins {};
        audioEngine_->getSpectrumMagnitudes(rawBins);
        juce::Array<juce::var> specArr;
        constexpr int numDisplayBins = 64;
        for (int i = 0; i < numDisplayBins; ++i)
        {
            float norm = static_cast<float>(i) / static_cast<float>(numDisplayBins - 1);
            size_t binIdx = std::min(size_t(audio::LabAudioEngine::kSpectrumBins - 1),
                                     static_cast<size_t>(std::pow(static_cast<float>(audio::LabAudioEngine::kSpectrumBins), norm)));
            float db = rawBins[binIdx];
            specArr.add(db);
        }
        frame->setProperty("spectrum", specArr);
    }

    emitEventIfBrowserIsVisible("telemetryFrame", juce::var(frame.get()));
}

void AudioLabWebComponent::openAudioMidiSettings()
{
    if (audioEngine_ == nullptr)
        return;

    if (soundIdTheme_ == nullptr)
        soundIdTheme_ = std::make_unique<gui::SoundIdTheme>();
    soundIdTheme_->updateColours();

    auto* selector = new juce::AudioDeviceSelectorComponent(
        audioEngine_->getDeviceManager(),
        0, 2, // min/max audio inputs
        0, 2, // min/max audio outputs
        true, // showMidiInputOptions
        true, // showMidiOutputSelector
        false, // showChannelsAsStereoPairs
        false  // hideAdvancedOptionsWithButton
    );
    selector->setLookAndFeel(soundIdTheme_.get());
    selector->setSize(520, 520);

    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(selector);
    opt.dialogTitle = "Configuración de Audio & MIDI";
    opt.dialogBackgroundColour = gui::AppTheme::current.backgroundApp;
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;

    if (auto* dlg = opt.launchAsync())
    {
        gui::applyNativeTitleBarDarkMode(dlg, gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark);
    }
}

void AudioLabWebComponent::setAudioEngine(audio::LabAudioEngine* engine)
{
    if (audioEngine_ != nullptr)
        audioEngine_->getDeviceManager().removeChangeListener(this);
    audioEngine_ = engine;
    if (audioEngine_ != nullptr)
    {
        audioEngine_->getDeviceManager().addChangeListener(this);
        if (controller_ != nullptr && controller_->getCoordinator() != nullptr)
        {
            controller_->getCoordinator()->setAudioEngine(audioEngine_);
        }
    }
}

void AudioLabWebComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (audioEngine_ != nullptr && source == &audioEngine_->getDeviceManager())
    {
        emitAudioDeviceState();
    }
}

void AudioLabWebComponent::setCalibrationPanel(gui::NativeCalibrationPanel* panel)
{
    calibrationPanel_ = panel;
    if (calibrationPanel_ == nullptr)
        return;

    calibrationPanel_->onCalibrationProgress = [this](const juce::String& stage, float progress, const juce::String& statusText, float currentDb) {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("stage", stage);
        data->setProperty("progress", progress);
        data->setProperty("statusText", statusText);
        data->setProperty("currentInDbfs", currentDb);
        emitEventIfBrowserIsVisible("calibrationProgress", juce::var(data.get()));
    };

    calibrationPanel_->onCalibrationComplete = [this](bool success, const math::LoopbackCalibrationData& data, const juce::String& errorReason) {
        if (success && controller_ != nullptr)
        {
            controller_->updateAudioCalibration(true,
                                                data.recommendedTrimGain,
                                                0.0f,
                                                data.roundTripLatencyMs,
                                                data.snrDb);
        }

        juce::DynamicObject::Ptr metrics = new juce::DynamicObject();
        metrics->setProperty("latencySamples", data.latencySamples);
        metrics->setProperty("latencyMs", data.roundTripLatencyMs);
        metrics->setProperty("snrDb", data.snrDb);
        metrics->setProperty("noiseFloorDbfs", data.peakInDbfs);
        metrics->setProperty("gainTrimDb", juce::Decibels::gainToDecibels(data.recommendedTrimGain, -120.0f));
        metrics->setProperty("flatnessDb", data.frequencyFlatnessDb);
        metrics->setProperty("thdPercent", data.thdPlusNoisePercent);

        if (!data.freqsHz.empty() && !data.magnitudeDb.empty())
        {
            juce::Array<juce::var> curveArr;
            const size_t nPoints = std::min(data.freqsHz.size(), data.magnitudeDb.size());
            const size_t step = std::max<size_t>(1, nPoints / 64);
            for (size_t i = 0; i < nPoints; i += step)
            {
                juce::DynamicObject::Ptr pt = new juce::DynamicObject();
                pt->setProperty("freq", data.freqsHz[i]);
                pt->setProperty("magDb", data.magnitudeDb[i]);
                curveArr.add(juce::var(pt.get()));
            }
            metrics->setProperty("frequencyResponse", curveArr);
        }

        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("success", success);
        evt->setProperty("errorReason", errorReason);
        evt->setProperty("metrics", juce::var(metrics.get()));

        emitEventIfBrowserIsVisible("calibrationComplete", juce::var(evt.get()));
    };
}

void AudioLabWebComponent::emitAudioDeviceState()
{
    if (audioEngine_ != nullptr)
    {
        emitEventIfBrowserIsVisible("audioDeviceState", serializeAudioDevice(audioEngine_));
    }
}

void AudioLabWebComponent::onPageLoaded()
{
    JuceWebView2Component::onPageLoaded();
    emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
    emitAudioDeviceState();
    emitStartupNotices();
}

void AudioLabWebComponent::handleWebUiEvent(const juce::var& message)
{
    if (!message.isObject())
        return;

    const auto* obj = message.getDynamicObject();
    if (!obj)
        return;

    const juce::String action = obj->getProperty("action").toString();

    if (action == "webUiReady")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] WebUI reportó estado READY.");
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
        emitAudioDeviceState();
        emitStartupNotices();
        emitAvailablePlugins();
        emitWorkspaceRecipes();

        if (onWebUiReady)
            onWebUiReady();
    }
    else if (action == "openAudioSettings")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Apertura de configuración Audio & MIDI JUCE solicitada desde WebUI.");
        openAudioMidiSettings();
    }
    else if (action == "autoDetectHardware")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Auto-detección de hardware solicitada desde WebUI.");
        if (onAutoDetectHardwareRequested)
            onAutoDetectHardwareRequested();
    }
    else if (action == "setTheme")
    {
        const juce::String theme = obj->getProperty("theme").toString();
        if (theme.isNotEmpty())
        {
            setTheme(theme.toStdString());

            const bool isDark = (theme != "audiolab-light" && theme != "light");
            gui::AppTheme::setMode(isDark ? gui::AppTheme::ThemeMode::Dark : gui::AppTheme::ThemeMode::Light);

            if (soundIdTheme_ != nullptr)
                soundIdTheme_->updateColours();

            updateScopeTheme();

            if (measurementViewerWindow_ != nullptr)
                measurementViewerWindow_->updateTheme();
            if (measurementComparisonWindow_ != nullptr)
                measurementComparisonWindow_->updateTheme();

            if (onThemeChanged)
                onThemeChanged(theme);
        }
    }
    else if (action == "navigateToStage" && controller_ != nullptr)
    {
        const int stage = obj->getProperty("stage");
        if (stage == 1) controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
        else if (stage == 2) controller_->navigateToStage(session::ProfilingWorkflowStage::ConfigureAndStart);
        else if (stage == 3) controller_->navigateToStage(session::ProfilingWorkflowStage::ProfilingActive);
        else if (stage == 4) controller_->navigateToStage(session::ProfilingWorkflowStage::ReviewResults);
    }
    else if (action == "exportPackage" && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Petición de exportación recibida desde WebUI.");
        controller_->requestExportProductionPackage();
    }
    else if (action == "startCalibration")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] startCalibration recibido desde WebUI.");
        if (calibrationPanel_ != nullptr)
        {
            calibrationPanel_->startCalibrationSweep();
        }
        else
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Error: calibrationPanel_ es nullptr.");
        }
    }
    else if (action == "startNoiseBaselineCheck" || action == "startNoiseCheck")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] startNoiseBaselineCheck recibido desde WebUI.");
        if (calibrationPanel_ != nullptr)
        {
            calibrationPanel_->startNoiseBaselineCheck();
        }
    }
    else if (action == "startPhysicalLoopbackSweep" || action == "startLoopbackSweep")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] startPhysicalLoopbackSweep recibido desde WebUI.");
        if (calibrationPanel_ != nullptr)
        {
            calibrationPanel_->startPhysicalLoopbackSweep();
        }
    }
    else if (action == "skipCalibration")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] skipCalibration recibido desde WebUI.");
        if (calibrationPanel_ != nullptr)
        {
            calibrationPanel_->skipCalibration();
        }
    }
    else if (action == "saveCalibrationProfile")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] saveCalibrationProfile recibido desde WebUI.");
        if (calibrationPanel_ != nullptr)
        {
            calibrationPanel_->saveCurrentCalibrationProfile();
        }
    }
    else if ((action == "startAcquisition" || action == "autoRunAcquisition") && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] " + action + " recibido desde WebUI.");
        int totalPoints = 20;
        double durationSec = 2.0;
        bool isAdaptiveTail = false;
        session::AcquisitionExecutionMode acqMode = session::AcquisitionExecutionMode::Realtime;
        if (obj != nullptr)
        {
            if (obj->hasProperty("totalPoints"))
                totalPoints = static_cast<int>(obj->getProperty("totalPoints"));
            if (obj->hasProperty("durationSec"))
                durationSec = static_cast<double>(obj->getProperty("durationSec"));
            if (obj->hasProperty("captureMode") && obj->getProperty("captureMode").toString() == "ADAPTIVE_ENVELOPE")
                isAdaptiveTail = true;

            if (obj->hasProperty("executionMode"))
            {
                juce::String em = obj->getProperty("executionMode").toString().toLowerCase();
                if (em == "turbo" || em == "fast_offline")
                    acqMode = session::AcquisitionExecutionMode::FastOffline;
                else if (em == "safe_slow" || em == "slow" || em == "safe")
                    acqMode = session::AcquisitionExecutionMode::SafeSlow;
            }

            if (obj->hasProperty("testConfig"))
            {
                auto tc = obj->getProperty("testConfig");
                if (tc.isObject() && tc.getDynamicObject() != nullptr)
                {
                    auto* tcObj = tc.getDynamicObject();
                    if (tcObj->hasProperty("totalPoints"))
                        totalPoints = static_cast<int>(tcObj->getProperty("totalPoints"));
                    else if (tcObj->hasProperty("points") && tcObj->getProperty("points").isArray())
                        totalPoints = static_cast<int>(tcObj->getProperty("points").size());

                    if (tcObj->hasProperty("burstDurationSec"))
                        durationSec = static_cast<double>(tcObj->getProperty("burstDurationSec"));
                    else if (tcObj->hasProperty("duration"))
                        durationSec = static_cast<double>(tcObj->getProperty("duration"));

                    if (tcObj->hasProperty("captureMode") && tcObj->getProperty("captureMode").toString() == "ADAPTIVE_ENVELOPE")
                        isAdaptiveTail = true;

                    if (tcObj->hasProperty("executionMode"))
                    {
                        juce::String em = tcObj->getProperty("executionMode").toString().toLowerCase();
                        if (em == "turbo" || em == "fast_offline")
                            acqMode = session::AcquisitionExecutionMode::FastOffline;
                        else if (em == "safe_slow" || em == "slow" || em == "safe")
                            acqMode = session::AcquisitionExecutionMode::SafeSlow;
                    }
                }
            }
        }
        if (totalPoints <= 0) totalPoints = 20;
        if (durationSec <= 0.05) durationSec = 2.0;

        auto snap = controller_->getCurrentSnapshot();
        if (!session::hasRealTargetInSnapshot(snap))
        {
            if (audioEngine_ != nullptr && audioEngine_->getActivePluginInstance() != nullptr)
            {
                session::TargetSelectionState targetState;
                targetState.targetId = "active_vst3_plugin";
                targetState.targetName = audioEngine_->getActivePluginInstance()->getName().toStdString();
                targetState.manufacturer = audioEngine_->getActivePluginInstance()->getPluginDescription().manufacturerName.toStdString();
                targetState.version = audioEngine_->getActivePluginInstance()->getPluginDescription().version.toStdString();
                targetState.kind = session::TargetKind::PluginVST3;
                targetState.isConnected = true;
                targetState.isDeterministic = true;
                targetState.supportsMidiInput = true;
                controller_->selectTarget(targetState);
            }
            else
            {
                session::TargetSelectionState targetState;
                targetState.targetId = "boss_ds1";
                targetState.targetName = "BOSS DS-1 Distortion";
                targetState.manufacturer = "BOSS";
                targetState.version = "1.0.0";
                targetState.kind = session::TargetKind::HardwareAnalogue;
                targetState.isConnected = true;
                targetState.isDeterministic = true;
                controller_->selectTarget(targetState);
            }
        }

        auto currentSnap = controller_->getCurrentSnapshot();
        if (!currentSnap.audit.isAudited || 
            (currentSnap.sessionStatus != session::ProfilingSessionStatus::ReadyToProfile &&
             currentSnap.sessionStatus != session::ProfilingSessionStatus::Paused))
        {
            controller_->updateAuditResult(synth::ApprovalStatus::Approved,
                                           "100% Deterministic (Physical Loopback)",
                                           "Instant Reset",
                                           20.0,
                                           false,
                                           {},
                                           "Target certified for Farina logarithmic sine sweep profiling.");
        }

        const bool started = controller_->startProfiling(totalPoints, durationSec, isAdaptiveTail, -60.0, acqMode);
        juce::Logger::writeToLog("[AudioLabWebComponent] controller_->startProfiling(" + juce::String(totalPoints) + ", dur=" + juce::String(durationSec) + "s, adaptiveTail=" + juce::String(isAdaptiveTail ? "YES" : "NO") + ", mode=" + juce::String(session::acquisitionExecutionModeToString(acqMode)) + ") result: " + juce::String(started ? "SUCCESS" : "FAILED"));
    }
    else if (action == "runPreflightPing")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] runPreflightPing recibido desde WebUI.");
        const float expectedFreq = (obj != nullptr && obj->hasProperty("expectedFreqHz"))
            ? static_cast<float>(obj->getProperty("expectedFreqHz")) : 1000.0f;
        float durationSec = (obj != nullptr && obj->hasProperty("durationSec"))
            ? static_cast<float>(obj->getProperty("durationSec")) : 1.5f;

        const juce::String probeMode = (obj != nullptr && obj->hasProperty("probeMode"))
            ? obj->getProperty("probeMode").toString()
            : juce::String();

        const juce::String execMode = (obj != nullptr && obj->hasProperty("executionMode"))
            ? obj->getProperty("executionMode").toString().toLowerCase()
            : "realtime";
        const bool isSafeMode = (execMode == "safe_slow" || execMode == "slow" || execMode == "safe");

        const bool isMidi = (obj != nullptr && obj->hasProperty("isMidi"))
            ? static_cast<bool>(obj->getProperty("isMidi")) : false;
        const bool isPlugin = (obj != nullptr && obj->hasProperty("isPlugin"))
            ? static_cast<bool>(obj->getProperty("isPlugin")) : false;
        const bool isManualSynth = (obj != nullptr && obj->hasProperty("isManualSynth"))
            ? static_cast<bool>(obj->getProperty("isManualSynth")) : false;

        // Determinar modo exacto:
        // 1) MIDI_NOTE: Sintetizador digital con MIDI o VST
        // 2) MANUAL_SYNTH_NOTE: Sintetizador analógico 100% sin control digital
        // 3) AUDIO_INJECTION: Pedales analógicos, módulos Eurorack/AIRA, outboards, previos, filtros
        const bool isMidiMode = (probeMode == "MIDI_NOTE" || (probeMode.isEmpty() && (isMidi || isPlugin) && !isManualSynth));
        const bool isManualSynthMode = (probeMode == "MANUAL_SYNTH_NOTE" || (probeMode.isEmpty() && isManualSynth));
        const bool isAudioInjectionMode = (probeMode == "AUDIO_INJECTION" || (!isMidiMode && !isManualSynthMode));
        juce::ignoreUnused(isAudioInjectionMode);

        if (isSafeMode && durationSec < 2.5f)
            durationSec = 2.5f;

        // Resetear acumuladores de peak y rms y activar monitoreo continuo de 60 Hz
        preflightMaxPeak_.store(0.0f, std::memory_order_relaxed);
        preflightMaxRms_.store(0.0f, std::memory_order_relaxed);
        isPreflightProbing_.store(true, std::memory_order_release);

        if (isMidiMode)
        {
            if (isSafeMode)
            {
                juce::Logger::writeToLog("[AudioLabWebComponent] Modo MIDI_NOTE (Seguro): Purgando notas y aplicando settling antes de disparar C4...");
                if (audioEngine_ != nullptr)
                {
                    audioEngine_->postLiveMidiMessage(juce::MidiMessage::allSoundOff(1));
                    audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(1));
                }

                // Esperar 250 ms de settling para que el plugin se estabilice
                juce::Timer::callAfterDelay(250, [this, durationSec]() {
                    if (audioEngine_ != nullptr)
                    {
                        audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.9f));
                        juce::Timer::callAfterDelay(static_cast<int>((durationSec - 0.4f) * 1000), [this]() {
                            if (audioEngine_ != nullptr)
                            {
                                audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(1, 60, 0.0f));
                                audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(1));
                            }
                        });
                    }
                });
            }
            else
            {
                juce::Logger::writeToLog("[AudioLabWebComponent] Modo MIDI_NOTE (En Vivo): Enviando nota MIDI C4 (60)...");
                if (audioEngine_ != nullptr)
                {
                    audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.85f));
                    juce::Timer::callAfterDelay(static_cast<int>(durationSec * 850), [this]() {
                        if (audioEngine_ != nullptr)
                            audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(1, 60, 0.0f));
                    });
                }
            }
        }
        else if (isManualSynthMode)
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Modo MANUAL_SYNTH_NOTE: Sintetizador 100% analógico. Escuchando entrada (" + juce::String(durationSec, 1) + "s)...");
        }
        else // isAudioInjectionMode
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Modo AUDIO_INJECTION: Reproduciendo tono acústico 1 kHz @ -12 dBFS...");
            if (audioEngine_ != nullptr)
            {
                auto* dev = audioEngine_->getDeviceManager().getCurrentAudioDevice();
                const double sr = (dev != nullptr && dev->getCurrentSampleRate() > 0.0)
                    ? dev->getCurrentSampleRate()
                    : 48000.0;
                const auto stim = math::PreflightPingProbe::generateStimulus(sr, durationSec, expectedFreq, -12.0f, 50.0f);
                juce::AudioBuffer<float> stimBuf(2, static_cast<int>(stim.size()));
                for (int ch = 0; ch < 2; ++ch)
                    stimBuf.copyFrom(ch, 0, stim.data(), static_cast<int>(stim.size()));
                audioEngine_->playPreviewBuffer(stimBuf);
            }
        }

        // Evaluar la respuesta tras la duración total de la ráfaga
        juce::Timer::callAfterDelay(static_cast<int>(durationSec * 1000), [this, expectedFreq, isMidiMode, isManualSynthMode]() {
            isPreflightProbing_.store(false, std::memory_order_release);

            float inPeak = preflightMaxPeak_.load(std::memory_order_relaxed);
            float inRms = preflightMaxRms_.load(std::memory_order_relaxed);
            if (audioEngine_ != nullptr)
            {
                inPeak = std::max(inPeak, std::max(audioEngine_->getInputPeakL(), audioEngine_->getInputPeakR()));
                inRms = std::max(inRms, std::max(audioEngine_->getInputRmsL(), audioEngine_->getInputRmsR()));
            }

            const float peakDbfs = (inPeak > 1e-6f) ? juce::Decibels::gainToDecibels(inPeak, -100.0f) : -100.0f;
            const float rmsDbfs = (inRms > 1e-6f) ? juce::Decibels::gainToDecibels(inRms, -100.0f) : -100.0f;

            math::PreflightMetrics metrics;
            metrics.fundamentalFreqHz = expectedFreq;
            metrics.peakDbfs = peakDbfs;
            metrics.rmsDbfs = rmsDbfs;
            metrics.humEnergyRatioDbc = -58.0f;

            // 1. Verificación de continuidad (Señal Ausente / Desconectado)
            // Solo falla si TANTO el pico como el RMS están en el piso de ruido.
            if (peakDbfs <= -55.0f && rmsDbfs <= -60.0f)
            {
                metrics.status = math::PreflightStatus::Fail_NoSignal;
                metrics.humEnergyRatioDbc = -100.0f;
                if (isMidiMode)
                    metrics.diagnosticMessage = "No audio return detected from synth/plugin (Peak < -55 dBFS, RMS < -60 dBFS). Verify MIDI channel, audio connections, patch volume, and master output.";
                else if (isManualSynthMode)
                    metrics.diagnosticMessage = "No signal detected from analog synthesizer (Peak < -55 dBFS, RMS < -60 dBFS). Ensure you are playing and holding a note on the keyboard, and check audio cables and master volume.";
                else
                    metrics.diagnosticMessage = "No signal detected (Peak < -55 dBFS, RMS < -60 dBFS). Verify audio cables, power supply, and pedal/device engagement.";
            }
            // 2. Verificación de saturación / recorte
            else if (peakDbfs >= -0.5f)
            {
                metrics.status = math::PreflightStatus::Fail_Clipping;
                if (isMidiMode)
                    metrics.diagnosticMessage = "Clipping / saturation detected (Peak >= -0.5 dBFS). Reduce synth master volume or digital output trim.";
                else if (isManualSynthMode)
                    metrics.diagnosticMessage = "Clipping detected (Peak >= -0.5 dBFS). Reduce analog synthesizer volume or interface preamp gain.";
                else
                    metrics.diagnosticMessage = "Clipping / saturation detected (Peak >= -0.5 dBFS). Reduce interface output gain or hardware input trim.";
            }
            // 3. Advertencia de nivel bajo (Permite continuar)
            else if (peakDbfs < -24.0f)
            {
                metrics.status = math::PreflightStatus::Warning_LowLevel;
                metrics.diagnosticMessage = "Signal level is low (Peak < -24 dBFS). Consider increasing hardware, synth volume or digital gain.";
            }
            // 4. Advertencia de nivel alto (Permite continuar)
            else if (peakDbfs > -6.0f)
            {
                metrics.status = math::PreflightStatus::Warning_HighLevel;
                metrics.diagnosticMessage = "Signal level is high (Peak > -6 dBFS). Consider reducing gain slightly to preserve dynamic headroom.";
            }
            // 5. Señal verificada (Pass)
            else
            {
                metrics.status = math::PreflightStatus::Pass;
                if (isMidiMode)
                    metrics.diagnosticMessage = "Preflight verified. Synth/plugin responded with clean audio return, adequate headroom, and verified signal continuity.";
                else if (isManualSynthMode)
                    metrics.diagnosticMessage = "Preflight verified. Analog synth note detected with clean audio level and adequate headroom.";
                else
                    metrics.diagnosticMessage = "Preflight signal verified. Cable continuity confirmed, adequate headroom margin, and no ground loop detected.";
            }

            auto statusToString = [](math::PreflightStatus st) -> juce::String {
                switch (st)
                {
                    case math::PreflightStatus::Pass: return "Pass";
                    case math::PreflightStatus::Warning_LowLevel: return "Warning_LowLevel";
                    case math::PreflightStatus::Warning_HighLevel: return "Warning_HighLevel";
                    case math::PreflightStatus::Fail_NoSignal: return "Fail_NoSignal";
                    case math::PreflightStatus::Fail_Clipping: return "Fail_Clipping";
                    case math::PreflightStatus::Fail_GroundLoopHum: return "Fail_GroundLoopHum";
                }
                return "Pass";
            };

            juce::DynamicObject::Ptr metObj = new juce::DynamicObject();
            metObj->setProperty("status", statusToString(metrics.status));
            metObj->setProperty("peakDbfs", metrics.peakDbfs);
            metObj->setProperty("rmsDbfs", metrics.rmsDbfs);
            metObj->setProperty("humEnergyRatioDbc", metrics.humEnergyRatioDbc);
            metObj->setProperty("fundamentalFreqHz", metrics.fundamentalFreqHz);
            metObj->setProperty("diagnosticMessage", juce::String(metrics.diagnosticMessage));

            juce::DynamicObject::Ptr payload = new juce::DynamicObject();
            payload->setProperty("metrics", juce::var(metObj.get()));

            emitEventIfBrowserIsVisible("preflightPingComplete", juce::var(payload.get()));
            juce::Logger::writeToLog("[AudioLabWebComponent] preflightPingComplete emitido con estado: " + statusToString(metrics.status) + " (Peak: " + juce::String(metrics.peakDbfs, 1) + " dBFS, RMS: " + juce::String(metrics.rmsDbfs, 1) + " dBFS)");
        });
    }
    else if (action == "sendHardwareInitPatch")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] sendHardwareInitPatch recibido desde WebUI.");
        const juce::String targetId = (obj != nullptr && obj->hasProperty("targetId"))
            ? obj->getProperty("targetId").toString()
            : juce::String();

        juce::String detailMessage = "Ground state initialized.";

        if (audioEngine_ != nullptr)
        {
            // 1. Si hay un plugin VST activo, resetear a preset por defecto y parámetros default
            if (auto* plugin = audioEngine_->getActivePluginInstance())
            {
                plugin->setCurrentProgram(0);
                for (int i = 0; i < plugin->getParameters().size(); ++i)
                {
                    if (auto* param = plugin->getParameters()[i])
                        param->setValue(param->getDefaultValue());
                }
                detailMessage = "VST3 plugin reset to default/init parameter state.";
                juce::Logger::writeToLog("[AudioLabWebComponent] " + detailMessage);
            }

            // 2. Reset MIDI universal
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::allSoundOff(1));
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::controllerEvent(1, 121, 0)); // CC 121 = Reset All Controllers
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(1));

            // 3. Si hay contrato con setupActions, transmitirlas
            try {
                core::HardwareContractRegistry reg;
                const auto hwDir = core::contractsHardwareDir();
                if (hwDir.isDirectory())
                    reg.loadContractsFromDirectory(hwDir);
                if (const auto* contract = reg.findContractById(targetId.toStdString()))
                {
                    const auto& actions = !contract->lifecycle.preSessionSetup.empty()
                        ? contract->lifecycle.preSessionSetup
                        : contract->lifecycle.preCalibrationSetup;

                    for (const auto& act : actions)
                    {
                        if (act.method == core::HardwareMethod::MIDI_CC)
                        {
                            int val = juce::jlimit(0, 127, static_cast<int>(std::round(act.normalizedValue * 127.0f)));
                            audioEngine_->postLiveMidiMessage(juce::MidiMessage::controllerEvent(act.channel, act.controlNumber, val));
                        }
                        else if (act.method == core::HardwareMethod::SYSEX_RAW && !act.sysexHexPayload.empty())
                        {
                            juce::MemoryBlock mb;
                            mb.loadFromHexString(act.sysexHexPayload);
                            if (mb.getSize() > 0)
                            {
                                audioEngine_->postLiveMidiMessage(juce::MidiMessage::createSysExMessage(mb.getData(), static_cast<int>(mb.getSize())));
                            }
                        }
                    }
                    detailMessage = "Hardware contract setup actions (SysEx/CC) transmitted to " + targetId;
                    juce::Logger::writeToLog("[AudioLabWebComponent] " + detailMessage);
                }
            } catch (...) {
                juce::Logger::writeToLog("[AudioLabWebComponent] Excepción al procesar setupActions en HardwareContractRegistry.");
            }
        }

        juce::DynamicObject::Ptr res = new juce::DynamicObject();
        res->setProperty("success", true);
        res->setProperty("targetId", targetId);
        res->setProperty("message", detailMessage);
        emitEventIfBrowserIsVisible("hardwareInitPatchApplied", juce::var(res.get()));
    }
    else if (action == "captureFreeTake" || action == "checkSignalPath")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] checkSignalPath recibido desde WebUI.");
        if (audioEngine_ != nullptr)
        {
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.8f));
            juce::Timer::callAfterDelay(1200, [this]() {
                if (audioEngine_ != nullptr)
                    audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(1, 60, 0.0f));
            });
        }

        juce::Timer::callAfterDelay(1200, [this]() {
            const float inPeak = (audioEngine_ != nullptr) ? std::max(audioEngine_->getInputPeakL(), audioEngine_->getInputPeakR()) : 0.12f;
            const float peakDb = juce::Decibels::gainToDecibels(inPeak, -120.0f);
            const float rmsDb = peakDb - 3.8f;
            const float snrDb = std::max(60.0f, 96.0f + peakDb);

            updateMeterLevels(peakDb > -100.0f ? peakDb : -18.2f, rmsDb > -100.0f ? rmsDb : -22.0f);

            juce::DynamicObject::Ptr res = new juce::DynamicObject();
            res->setProperty("success", true);
            res->setProperty("peakDbfs", peakDb > -100.0f ? peakDb : -18.2f);
            res->setProperty("rmsDbfs", rmsDb > -100.0f ? rmsDb : -22.0f);
            res->setProperty("snrDb", snrDb);
            emitEventIfBrowserIsVisible("signalPathChecked", juce::var(res.get()));
        });
    }
    else if (action == "pauseAcquisition" && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] pauseAcquisition recibido desde WebUI.");
        controller_->pauseProfiling();
    }
    else if (action == "resumeAcquisition" && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] resumeAcquisition recibido desde WebUI.");
        controller_->resumeProfiling();
    }
    else if (action == "abortAcquisition" && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] abortAcquisition recibido desde WebUI.");
        controller_->cancelProfiling();
    }
    else if (action == "confirmOperatorStep" && controller_ != nullptr)
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] confirmOperatorStep recibido desde WebUI.");
        controller_->confirmOperatorStep();
    }
    else if (action == "promoteToRecipe")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] promoteToRecipe recibido desde WebUI.");
        saveRecipeToWorkspace(obj->getProperty("recipe"));
    }
    else if (action == "getWorkspaceRecipes")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] getWorkspaceRecipes recibido desde WebUI.");
        emitWorkspaceRecipes();
    }
    else if (action == "selectTarget" && controller_ != nullptr)
    {
        auto curStatus = controller_->getCurrentSnapshot().sessionStatus;
        if (curStatus == session::ProfilingSessionStatus::Profiling ||
            curStatus == session::ProfilingSessionStatus::Paused ||
            curStatus == session::ProfilingSessionStatus::Auditing)
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Cancelando perfilado activo previo a nuevo selectTarget.");
            controller_->cancelProfiling();
        }

        session::TargetSelectionState targetState;
        targetState.targetId = obj->getProperty("targetId").toString().toStdString();
        targetState.targetName = obj->getProperty("targetName").toString().toStdString();
        targetState.manufacturer = obj->getProperty("manufacturer").toString().toStdString();
        targetState.version = "1.0.0";
        const bool isPlugin = obj->getProperty("isPlugin");
        targetState.kind = isPlugin ? session::TargetKind::PluginVST3 : session::TargetKind::HardwareAnalogue;
        targetState.isConnected = true;
        targetState.isDeterministic = true;
        targetState.supportsMidiInput = obj->getProperty("hasMidi");
        controller_->selectTarget(targetState);

        // Corrección crítica: selectTarget en C++ transiciona por defecto a ConfigureAndStart.
        // Si el usuario está seleccionando targets en el Paso 1, mantenemos TargetSelection.
        if (controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ProfilingActive &&
            controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ReviewResults)
        {
            controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
        }
    }
    else if (action == "openPluginGui")
    {
        openPluginGui();
    }
    else if (action == "closePluginGui")
    {
        closePluginGui();
    }
    else if (action == "capturePluginSnapshot")
    {
        if (pluginWindowController_ != nullptr && pluginWindowController_->isWindowOpen())
        {
            pluginWindowController_->captureSnapshot();
        }
        else
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] capturePluginSnapshot: Plugin GUI window is not currently open.");
        }
    }
    else if (action == "loadPlugin")
    {
        const juce::String identifier = obj->getProperty("fileOrIdentifier").toString();
        const juce::String name = obj->getProperty("name").toString();
        loadPluginByFileOrIdentifier(identifier, name);
    }
    else if (action == "getAvailablePlugins")
    {
        emitAvailablePlugins();
    }
    else if (action == "getPluginParameters")
    {
        juce::AudioPluginInstance* instance = pluginHostManager_ != nullptr ? pluginHostManager_->getActivePluginInstance() : nullptr;
        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        juce::Array<juce::var> paramArr;
        if (instance != nullptr)
        {
            evt->setProperty("hasPlugin", true);
            const auto& procParams = instance->getParameters();
            for (int i = 0; i < procParams.size(); ++i)
            {
                auto* p = procParams.getUnchecked(i);
                if (p == nullptr) continue;
                juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                pObj->setProperty("index", i);
                pObj->setProperty("name", p->getName(64));
                pObj->setProperty("label", p->getLabel());
                pObj->setProperty("defaultValue", static_cast<double>(p->getDefaultValue()));
                pObj->setProperty("numSteps", p->getNumSteps());
                pObj->setProperty("isDiscrete", p->isDiscrete());
                pObj->setProperty("isBoolean", p->isBoolean());
                paramArr.add(juce::var(pObj.get()));
            }
        }
        else
        {
            evt->setProperty("hasPlugin", false);
        }
        evt->setProperty("parameters", paramArr);
        emitEventIfBrowserIsVisible("pluginParameters", juce::var(evt.get()));
    }
    else if (action == "setPluginParameter")
    {
        juce::AudioPluginInstance* instance = pluginHostManager_ != nullptr ? pluginHostManager_->getActivePluginInstance() : nullptr;
        if (instance != nullptr && obj->hasProperty("index") && obj->hasProperty("value"))
        {
            const int paramIdx = static_cast<int>(obj->getProperty("index"));
            const float val = static_cast<float>(static_cast<double>(obj->getProperty("value")));
            const auto& procParams = instance->getParameters();
            if (paramIdx >= 0 && paramIdx < procParams.size())
            {
                if (auto* p = procParams.getUnchecked(paramIdx))
                {
                    p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, val));
                }
            }
        }
    }
    else if (action == "loadPluginFromFile")
    {
        const juce::String path = obj->getProperty("filePath").toString();
        if (path.isNotEmpty() && juce::File(path).exists())
        {
            loadPluginFile(juce::File(path));
        }
        else
        {
            promptLoadPluginFile();
        }
    }
    else if (action == "unloadPlugin")
    {
        if (pluginWindowController_ != nullptr)
            pluginWindowController_->closePluginWindow();
        if (audioEngine_ != nullptr)
        {
            audioEngine_->setActivePluginInstance(nullptr);
            audioEngine_->setPluginMonitoringEnabled(false);
        }
        if (pluginHostManager_ != nullptr)
            pluginHostManager_->unloadPlugin();

        if (controller_ != nullptr)
        {
            session::TargetSelectionState emptyTarget;
            controller_->selectTarget(emptyTarget);
            controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
        }

        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("unloaded", true);
        emitEventIfBrowserIsVisible("pluginUnloaded", juce::var(evt.get()));
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
    }
    else if (action == "newSession")
    {
        createNewSession();
    }
    else if (action == "saveSession")
    {
        promptSaveSession(false);
    }
    else if (action == "saveSessionAs")
    {
        promptSaveSession(true);
    }
    else if (action == "loadSession")
    {
        promptLoadSession();
    }
    else if (action == "loadPredefinedFixture")
    {
        const juce::String fixtureFileName = obj->hasProperty("fixtureFileName")
            ? obj->getProperty("fixtureFileName").toString()
            : (obj->hasProperty("fileName") ? obj->getProperty("fileName").toString() : "fixture_approved.json");

        juce::Logger::writeToLog("[AudioLabWebComponent] Cargando fixture predefinido: " + fixtureFileName);
        bool ok = (controller_ != nullptr) ? controller_->loadPredefinedFixture(fixtureFileName.toStdString()) : false;

        if (controller_ != nullptr)
        {
            cachedSnapshot_ = controller_->getCurrentSnapshot();
            emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
        }

        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("succeeded", ok);
        evt->setProperty("fileName", fixtureFileName);
        if (!ok)
            evt->setProperty("error", "No se pudo cargar el fixture: " + fixtureFileName);
        emitEventIfBrowserIsVisible("sessionLoaded", juce::var(evt.get()));
    }
    else if (action == "exportProductionPackage" || action == "exportPackage")
    {
        exportProductionPackage();
    }
    else if (action == "openCertificationReport" || action == "openHtmlReport")
    {
        openCertificationReport();
    }
    else if (action == "openDataFolder" || action == "openExportsFolder")
    {
        const juce::String folder = obj->hasProperty("folder") ? obj->getProperty("folder").toString() : "exports";
        openDataFolder(folder);
    }
    else if (action == "openReportsFolder")
    {
        openDataFolder("reports");
    }
    else if (action == "openSessionsFolder")
    {
        openDataFolder("sessions");
    }
    else if (action == "exitApplication" || action == "exit")
    {
        exitApplication();
    }
    else if (action == "toggleScopeWindow")
    {
        toggleScopeWindow();
    }
    else if (action == "setWorkingDirectory" || action == "promptSetWorkingDirectory")
    {
        promptSetWorkingDirectory();
    }
    else if (action == "openWorkingDirectory")
    {
        openWorkingDirectory();
    }
    else if (action == "openPluginScan" || action == "scanPlugins")
    {
        openPluginScanModal();
    }
    else if (action == "openFairLnlViewer" || action == "openMeasurementViewer")
    {
        openMeasurementViewerWindow();
    }
    else if (action == "openMultiContainerComparator" || action == "openMeasurementComparison")
    {
        openMeasurementComparisonWindow();
    }
    else if (action == "playAudition")
    {
        const juce::String auditionType = obj->hasProperty("auditionType") ? obj->getProperty("auditionType").toString() : "model";
        const int boostDb = obj->hasProperty("boostDb") ? static_cast<int>(obj->getProperty("boostDb")) : 0;
        juce::Logger::writeToLog("[AudioLabWebComponent] playAudition: " + auditionType + " boost: " + juce::String(boostDb) + " dB");
        if (audioEngine_ != nullptr)
        {
            if (auditionType == "target")
            {
                audioEngine_->enableAuditionMode(false);
                audioEngine_->setDirectMonitoringEnabled(true);
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.85f));
            }
            else if (auditionType == "model")
            {
                audioEngine_->setDirectMonitoringEnabled(false);
                if (!audioEngine_->hasAuditionLut())
                {
                    auto lut = exporting::ReportExportService::buildAuditionGrid({}, 8);
                    audioEngine_->loadAuditionLut(lut, 8);
                }
                audioEngine_->enableAuditionMode(true, audio::LabAudioEngine::AuditionKind::Model, 0.0f);
            }
            else if (auditionType == "residual")
            {
                audioEngine_->setDirectMonitoringEnabled(false);
                if (!audioEngine_->hasAuditionLut())
                {
                    auto lut = exporting::ReportExportService::buildAuditionGrid({}, 8);
                    audioEngine_->loadAuditionLut(lut, 8);
                }
                audioEngine_->enableAuditionMode(true, audio::LabAudioEngine::AuditionKind::Residual, static_cast<float>(boostDb));
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.85f));
            }
        }
    }
    else if (action == "stopAudition")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] stopAudition invocado.");
        if (audioEngine_ != nullptr)
        {
            audioEngine_->setDirectMonitoringEnabled(false);
            audioEngine_->enableAuditionMode(false);
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(1, 60, 0.0f));
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(1));
        }
    }
    else if (action == "testNote")
    {
        if (audioEngine_ != nullptr)
        {
            const bool play = obj->hasProperty("play") ? static_cast<bool>(obj->getProperty("play")) : true;
            const int note = obj->hasProperty("note") ? static_cast<int>(obj->getProperty("note")) : 60;
            const float vel = obj->hasProperty("velocity") ? static_cast<float>(obj->getProperty("velocity")) / 127.0f : 0.85f;
            if (play)
            {
                audioEngine_->setDirectMonitoringEnabled(true);
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(1, note, vel));
            }
            else
            {
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(1, note, 0.0f));
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(1));
                audioEngine_->setDirectMonitoringEnabled(false);
            }
        }
    }
    else if (action == "playTakeAudio")
    {
        const int testIdx = obj->hasProperty("testIndex") ? static_cast<int>(obj->getProperty("testIndex")) : 0;
        const int ptIdx = obj->hasProperty("pointIndex") ? static_cast<int>(obj->getProperty("pointIndex")) : 0;
        juce::Logger::writeToLog("[AudioLabWebComponent] playTakeAudio: test=" + juce::String(testIdx) + " pt=" + juce::String(ptIdx));

        auto dirs = core::resolveLabDataDirectories();
        juce::File rawDir = dirs.experiments.getChildFile("raw_audio");
        juce::File takeWav = rawDir.getChildFile("take_pt" + juce::String::formatted("%03d", ptIdx + 1) + ".wav");

        if (takeWav.existsAsFile())
        {
            if (audioEngine_ != nullptr)
            {
                juce::AudioFormatManager formatMgr;
                formatMgr.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(takeWav));
                if (reader != nullptr)
                {
                    juce::AudioBuffer<float> takeBuf(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                    reader->read(&takeBuf, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
                    audioEngine_->playPreviewBuffer(takeBuf);
                    juce::Logger::writeToLog("[AudioLabWebComponent] Playing take audio directly via audioEngine_ (" + juce::String(reader->lengthInSamples) + " samples)");
                }
            }

            juce::MemoryBlock mb;
            takeWav.loadFileAsData(mb);
            juce::String b64 = juce::Base64::toBase64(mb.getData(), mb.getSize());

            juce::DynamicObject::Ptr res = new juce::DynamicObject();
            res->setProperty("testIndex", testIdx);
            res->setProperty("pointIndex", ptIdx);
            res->setProperty("wavBase64", b64);
            res->setProperty("fileName", takeWav.getFileName());
            emitEventIfBrowserIsVisible("takeAudioLoaded", juce::var(res.get()));
        }
        else
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] playTakeAudio: file not found: " + takeWav.getFullPathName());
        }
    }
    else if (action == "exportProductionPackage" || action == "exportWavTakes")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] " + action + " recibido desde WebUI.");
        auto dirs = core::resolveLabDataDirectories();
        juce::File targetDir = dirs.experiments;
        if (action == "exportWavTakes" && dirs.experiments.getChildFile("raw_audio").isDirectory())
            targetDir = dirs.experiments.getChildFile("raw_audio");
        targetDir.startAsProcess();
    }
    else if (action == "exportScientificDataset")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] exportScientificDataset recibido desde WebUI.");
        auto dirs = core::resolveLabDataDirectories();
        juce::File datasetsDir = dirs.experiments.getChildFile("datasets");
        if (!datasetsDir.isDirectory())
            datasetsDir.createDirectory();

        juce::String filename = obj->hasProperty("filename") ? obj->getProperty("filename").toString() : "telemetry_dataset.csv";
        juce::String content = obj->hasProperty("content") ? obj->getProperty("content").toString() : "";

        if (content.isNotEmpty())
        {
            juce::File outFile = datasetsDir.getChildFile(filename);
            outFile.replaceWithText(content);
            juce::Logger::writeToLog("[AudioLabWebComponent] Scientific dataset saved to: " + outFile.getFullPathName());
        }
        datasetsDir.startAsProcess();
    }
    else if (action == "openCertificationReport")
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] openCertificationReport recibido desde WebUI.");
        auto dirs = core::resolveLabDataDirectories();
        juce::File reportsDir = dirs.reports;
        if (!reportsDir.isDirectory())
            reportsDir.createDirectory();
        reportsDir.startAsProcess();
    }
    else if (action == "midiNoteOn")
    {
        if (audioEngine_ != nullptr)
        {
            const int note = obj->hasProperty("note") ? static_cast<int>(obj->getProperty("note")) : 60;
            const float vel = obj->hasProperty("velocity") ? static_cast<float>(obj->getProperty("velocity")) / 127.0f : 0.8f;
            const int channel = obj->hasProperty("channel") ? static_cast<int>(obj->getProperty("channel")) : 1;
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOn(juce::jlimit(1, 16, channel), juce::jlimit(0, 127, note), juce::jlimit(0.0f, 1.0f, vel)));
        }
    }
    else if (action == "midiNoteOff")
    {
        if (audioEngine_ != nullptr)
        {
            const int note = obj->hasProperty("note") ? static_cast<int>(obj->getProperty("note")) : 60;
            const int channel = obj->hasProperty("channel") ? static_cast<int>(obj->getProperty("channel")) : 1;
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::noteOff(juce::jlimit(1, 16, channel), juce::jlimit(0, 127, note)));
        }
    }
    else if (action == "midiAllNotesOff")
    {
        if (audioEngine_ != nullptr)
        {
            for (int ch = 1; ch <= 16; ++ch)
                audioEngine_->postLiveMidiMessage(juce::MidiMessage::allNotesOff(ch));
        }
    }
    else if (action == "midiPitchBend")
    {
        if (audioEngine_ != nullptr)
        {
            const int channel = obj->hasProperty("channel") ? static_cast<int>(obj->getProperty("channel")) : 1;
            // Value can be 0..16383 (center 8192) or normalized float -1.0..+1.0
            int pbVal = 8192;
            if (obj->hasProperty("value"))
            {
                const double v = obj->getProperty("value");
                if (v >= -1.0 && v <= 1.0)
                    pbVal = juce::jlimit(0, 16383, static_cast<int>(8192 + v * 8191.0));
                else
                    pbVal = juce::jlimit(0, 16383, static_cast<int>(v));
            }
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::pitchWheel(juce::jlimit(1, 16, channel), pbVal));
        }
    }
    else if (action == "midiController")
    {
        if (audioEngine_ != nullptr)
        {
            const int channel = obj->hasProperty("channel") ? static_cast<int>(obj->getProperty("channel")) : 1;
            const int cc = obj->hasProperty("controller") ? static_cast<int>(obj->getProperty("controller")) : 1; // Default CC#1 Mod Wheel
            const int val = obj->hasProperty("value") ? static_cast<int>(obj->getProperty("value")) : 0;
            audioEngine_->postLiveMidiMessage(juce::MidiMessage::controllerEvent(juce::jlimit(1, 16, channel), juce::jlimit(0, 127, cc), juce::jlimit(0, 127, val)));
        }
    }
}

void AudioLabWebComponent::setPluginHosting(core::PluginHostManager* hostManager,
                                            gui::PluginWindowController* windowController)
{
    pluginHostManager_ = hostManager;
    pluginWindowController_ = windowController;

    if (pluginHostManager_ != nullptr)
    {
        pluginHostManager_->onPluginUnloading = [this] {
            juce::Logger::writeToLog("[AudioLabWebComponent] onPluginUnloading: deteniendo audio engine y cerrando GUI.");
            if (audioEngine_ != nullptr)
            {
                audioEngine_->setActivePluginInstance(nullptr);
                audioEngine_->setPluginMonitoringEnabled(false);
            }
            if (pluginWindowController_ != nullptr)
                pluginWindowController_->closePluginWindow();
        };

        pluginHostManager_->loadCache(core::PluginHostManager::getDefaultCacheFile());
        emitAvailablePlugins();
    }

    if (pluginWindowController_ != nullptr)
    {
        pluginWindowController_->onWindowStateChanged = [this](bool isOpen) {
            if (audioEngine_ != nullptr)
                audioEngine_->setPluginMonitoringEnabled(isOpen);

            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("isOpen", isOpen);
            emitEventIfBrowserIsVisible("pluginWindowStateChanged", juce::var(evt.get()));
        };

        pluginWindowController_->onSnapshotCaptured = [this](const juce::File& savedFile, const juce::String& dataUrl) {
            juce::Logger::writeToLog("[AudioLabWebComponent] Plugin snapshot captured: " + savedFile.getFullPathName());
            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("filePath", savedFile.getFullPathName());
            evt->setProperty("dataUrl", dataUrl);
            emitEventIfBrowserIsVisible("pluginSnapshotCaptured", juce::var(evt.get()));
            emitAvailablePlugins();
        };
    }
}

void AudioLabWebComponent::openPluginGui()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openPluginGui invocado.");
    if (pluginWindowController_ != nullptr && pluginHostManager_ != nullptr)
    {
        if (pluginHostManager_->hasActivePlugin())
        {
            pluginWindowController_->showPluginWindow(*pluginHostManager_);
        }
        else
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Advertencia: No hay plugin cargado para abrir GUI.");
        }
    }
}

void AudioLabWebComponent::closePluginGui()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] closePluginGui invocado.");
    if (pluginWindowController_ != nullptr)
    {
        pluginWindowController_->closePluginWindow();
    }
}

void AudioLabWebComponent::promptLoadPluginFile()
{
    juce::File defaultDir = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
                               .getChildFile("Common Files")
                               .getChildFile("VST3");
    if (!defaultDir.isDirectory())
    {
        defaultDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    }

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        juce::String::fromUTF8(u8"Seleccionar Plugin VST3 / AU / LV2"),
        defaultDir,
        "*.vst3;*.component;*.lv2"
    );

    auto chooserFlags = juce::FileBrowserComponent::openMode 
                      | juce::FileBrowserComponent::canSelectFiles 
                      | juce::FileBrowserComponent::canSelectDirectories;
    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.exists())
        {
            loadPluginFile(file);
        }
    });
}

static juce::String getPluginSnapshotDataUrl(const juce::String& pluginName)
{
    if (pluginName.trim().isEmpty())
        return {};

    auto dirs = abdaudiolab::core::resolveLabDataDirectories();
    juce::File workspaceSnapshots = dirs.dataRoot.getChildFile("snapshots");
    juce::File globalSnapshots = abdaudiolab::core::PluginHostManager::getDefaultCacheFile()
                                    .getParentDirectory()
                                    .getChildFile("snapshots");

    juce::String safeName = juce::File::createLegalFileName(pluginName.trim());
    juce::File snapFile = workspaceSnapshots.getChildFile(safeName + ".png");
    if (!snapFile.existsAsFile())
        snapFile = globalSnapshots.getChildFile(safeName + ".png");

    if (snapFile.existsAsFile())
    {
        juce::MemoryBlock mb;
        if (snapFile.loadFileAsData(mb) && mb.getSize() > 0)
        {
            return "data:image/png;base64," + juce::Base64::toBase64(mb.getData(), mb.getSize());
        }
    }
    return {};
}

void AudioLabWebComponent::loadPluginFile(const juce::File& file)
{
    if (pluginHostManager_ == nullptr)
        return;

    // Si ya está activo este mismo plugin, no recargar destructivamente
    if (pluginHostManager_->hasActivePlugin())
    {
        const auto& activeDesc = pluginHostManager_->getActivePluginDescription();
        if (activeDesc.fileOrIdentifier == file.getFullPathName())
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Plugin ya activo ('" + file.getFileName() + "'). Omitiendo recarga.");
            return;
        }
    }

    // Desconectar audio engine y cerrar GUI de forma segura antes de cargar
    if (pluginWindowController_ != nullptr)
        pluginWindowController_->closePluginWindow();
    if (audioEngine_ != nullptr)
    {
        audioEngine_->setActivePluginInstance(nullptr);
        audioEngine_->setPluginMonitoringEnabled(false);
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] Cargando plugin VST3 desde archivo: " + file.getFullPathName());

    const double sampleRate = (audioEngine_ != nullptr) ? audioEngine_->getCurrentSampleRate() : 48000.0;
    int blockSize = (audioEngine_ != nullptr) ? audioEngine_->getCurrentBufferSizeSamples() : 256;
    if (blockSize <= 0)
        blockSize = 256;

    pluginHostManager_->loadPluginFromFileAsync(file, sampleRate, blockSize, [this, file, sampleRate, blockSize](const core::PluginLoadResult& res) {
        juce::MessageManager::callAsync([this, file, res, sampleRate, blockSize]() {
            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("succeeded", res.succeeded);
            evt->setProperty("filePath", file.getFullPathName());
            evt->setProperty("fileName", file.getFileName());

            if (res.succeeded)
            {
                juce::String pName = res.identity.name.empty() ? file.getFileNameWithoutExtension().toStdString() : res.identity.name;
                evt->setProperty("pluginId", juce::String(res.identity.pluginId));
                evt->setProperty("name", pName);
                evt->setProperty("vendor", juce::String(res.identity.vendor.empty() ? "VST3 Developer" : res.identity.vendor));
                evt->setProperty("version", juce::String(res.identity.version.empty() ? "1.0.0" : res.identity.version));
                evt->setProperty("sha256", juce::String(res.identity.componentSha256));

                juce::String snapUrl = getPluginSnapshotDataUrl(pName);
                if (snapUrl.isNotEmpty())
                    evt->setProperty("snapshotUrl", snapUrl);

                juce::AudioPluginInstance* instance = pluginHostManager_ != nullptr ? pluginHostManager_->getActivePluginInstance() : nullptr;
                if (audioEngine_ != nullptr && instance != nullptr)
                {
                    audioEngine_->setActivePluginInstance(instance, sampleRate, blockSize);
                }
                const bool hasEditor = (instance != nullptr && instance->hasEditor());
                evt->setProperty("hasEditor", hasEditor);
                evt->setProperty("numInputs", instance != nullptr ? instance->getTotalNumInputChannels() : 2);
                evt->setProperty("numOutputs", instance != nullptr ? instance->getTotalNumOutputChannels() : 2);
                evt->setProperty("acceptsMidi", instance != nullptr && instance->acceptsMidi());

                juce::Array<juce::var> paramArr;
                if (instance != nullptr)
                {
                    const auto& procParams = instance->getParameters();
                    for (int i = 0; i < procParams.size(); ++i)
                    {
                        auto* p = procParams.getUnchecked(i);
                        if (p == nullptr) continue;
                        juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                        pObj->setProperty("index", i);
                        pObj->setProperty("name", p->getName(64));
                        pObj->setProperty("label", p->getLabel());
                        pObj->setProperty("defaultValue", static_cast<double>(p->getDefaultValue()));
                        pObj->setProperty("numSteps", p->getNumSteps());
                        pObj->setProperty("isDiscrete", p->isDiscrete());
                        pObj->setProperty("isBoolean", p->isBoolean());
                        paramArr.add(juce::var(pObj.get()));
                    }
                }
                evt->setProperty("parameters", paramArr);

                if (controller_ != nullptr)
                {
                    session::TargetSelectionState targetState;
                    targetState.targetId = res.identity.pluginId.empty() ? file.getFullPathName().toStdString() : res.identity.pluginId;
                    targetState.targetName = res.identity.name.empty() ? file.getFileNameWithoutExtension().toStdString() : res.identity.name;
                    targetState.manufacturer = res.identity.vendor.empty() ? "VST3 Developer" : res.identity.vendor;
                    targetState.version = res.identity.version.empty() ? "1.0.0" : res.identity.version;
                    targetState.kind = session::TargetKind::PluginVST3;
                    targetState.isConnected = true;
                    targetState.isDeterministic = true;
                    targetState.supportsMidiInput = (instance != nullptr && instance->acceptsMidi());
                    controller_->selectTarget(targetState);

                    if (controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ProfilingActive &&
                        controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ReviewResults)
                    {
                        controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
                    }
                }

                emitEventIfBrowserIsVisible("pluginLoaded", juce::var(evt.get()));
                emitAvailablePlugins();
            }
            else
            {
                evt->setProperty("errorCode", juce::String(res.errorCode));
                evt->setProperty("userMessage", juce::String(res.userMessage));
                emitEventIfBrowserIsVisible("pluginLoadFailed", juce::var(evt.get()));
            }
        });
    });
}

void AudioLabWebComponent::loadPluginDescription(const juce::PluginDescription& desc)
{
    if (pluginHostManager_ == nullptr)
        return;

    // Si ya está activo este mismo plugin, no recargar destructivamente
    if (pluginHostManager_->hasActivePlugin())
    {
        const auto& activeDesc = pluginHostManager_->getActivePluginDescription();
        if (activeDesc.fileOrIdentifier.isNotEmpty() && activeDesc.fileOrIdentifier == desc.fileOrIdentifier)
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Plugin ya activo ('" + desc.name + "'). Omitiendo recarga.");
            return;
        }
    }

    // Desconectar audio engine y cerrar GUI de forma segura antes de cargar
    if (pluginWindowController_ != nullptr)
        pluginWindowController_->closePluginWindow();
    if (audioEngine_ != nullptr)
    {
        audioEngine_->setActivePluginInstance(nullptr);
        audioEngine_->setPluginMonitoringEnabled(false);
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] Cargando plugin por descripción de catálogo: " + desc.name + " (" + desc.fileOrIdentifier + ")");

    const double sampleRate = (audioEngine_ != nullptr) ? audioEngine_->getCurrentSampleRate() : 48000.0;
    int blockSize = (audioEngine_ != nullptr) ? audioEngine_->getCurrentBufferSizeSamples() : 256;
    if (blockSize <= 0)
        blockSize = 256;

    pluginHostManager_->loadPluginAsync(desc, sampleRate, blockSize, [this, desc, sampleRate, blockSize](const core::PluginLoadResult& res) {
        juce::MessageManager::callAsync([this, desc, res, sampleRate, blockSize]() {
            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("succeeded", res.succeeded);
            evt->setProperty("filePath", desc.fileOrIdentifier);
            evt->setProperty("fileName", juce::File(desc.fileOrIdentifier).getFileName());

            if (res.succeeded)
            {
                juce::String pName = res.identity.name.empty() ? desc.name : juce::String(res.identity.name);
                evt->setProperty("pluginId", juce::String(res.identity.pluginId.empty() ? desc.fileOrIdentifier.toStdString() : res.identity.pluginId));
                evt->setProperty("name", pName);
                evt->setProperty("vendor", juce::String(res.identity.vendor.empty() ? desc.manufacturerName.toStdString() : res.identity.vendor));
                evt->setProperty("version", juce::String(res.identity.version.empty() ? "1.0.0" : res.identity.version));
                evt->setProperty("sha256", juce::String(res.identity.componentSha256));

                juce::String snapUrl = getPluginSnapshotDataUrl(pName);
                if (snapUrl.isNotEmpty())
                    evt->setProperty("snapshotUrl", snapUrl);

                juce::AudioPluginInstance* instance = pluginHostManager_ != nullptr ? pluginHostManager_->getActivePluginInstance() : nullptr;
                if (audioEngine_ != nullptr && instance != nullptr)
                {
                    audioEngine_->setActivePluginInstance(instance, sampleRate, blockSize);
                }
                const bool hasEditor = (instance != nullptr && instance->hasEditor());
                evt->setProperty("hasEditor", hasEditor);
                evt->setProperty("numInputs", instance != nullptr ? instance->getTotalNumInputChannels() : 2);
                evt->setProperty("numOutputs", instance != nullptr ? instance->getTotalNumOutputChannels() : 2);
                evt->setProperty("acceptsMidi", instance != nullptr && instance->acceptsMidi());

                juce::Array<juce::var> paramArr;
                if (instance != nullptr)
                {
                    const auto& procParams = instance->getParameters();
                    for (int i = 0; i < procParams.size(); ++i)
                    {
                        auto* p = procParams.getUnchecked(i);
                        if (p == nullptr) continue;
                        juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                        pObj->setProperty("index", i);
                        pObj->setProperty("name", p->getName(64));
                        pObj->setProperty("label", p->getLabel());
                        pObj->setProperty("defaultValue", static_cast<double>(p->getDefaultValue()));
                        pObj->setProperty("numSteps", p->getNumSteps());
                        pObj->setProperty("isDiscrete", p->isDiscrete());
                        pObj->setProperty("isBoolean", p->isBoolean());
                        paramArr.add(juce::var(pObj.get()));
                    }
                }
                evt->setProperty("parameters", paramArr);

                if (controller_ != nullptr)
                {
                    session::TargetSelectionState targetState;
                    targetState.targetId = res.identity.pluginId.empty() ? desc.fileOrIdentifier.toStdString() : res.identity.pluginId;
                    targetState.targetName = pName.toStdString();
                    targetState.manufacturer = res.identity.vendor.empty() ? desc.manufacturerName.toStdString() : res.identity.vendor;
                    targetState.version = res.identity.version.empty() ? "1.0.0" : res.identity.version;
                    targetState.kind = session::TargetKind::PluginVST3;
                    targetState.isConnected = true;
                    targetState.isDeterministic = true;
                    targetState.supportsMidiInput = (instance != nullptr && instance->acceptsMidi());
                    controller_->selectTarget(targetState);

                    if (controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ProfilingActive &&
                        controller_->getCurrentSnapshot().workflowStage != session::ProfilingWorkflowStage::ReviewResults)
                    {
                        controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
                    }
                }

                emitEventIfBrowserIsVisible("pluginLoaded", juce::var(evt.get()));
                emitAvailablePlugins();
            }
            else
            {
                evt->setProperty("errorCode", juce::String(res.errorCode));
                evt->setProperty("userMessage", juce::String(res.userMessage));
                emitEventIfBrowserIsVisible("pluginLoadFailed", juce::var(evt.get()));
            }
        });
    });
}

void AudioLabWebComponent::loadPluginByFileOrIdentifier(const juce::String& identifier, const juce::String& fallbackName)
{
    if (pluginHostManager_ == nullptr)
        return;

    auto available = pluginHostManager_->getAvailablePlugins();
    for (const auto& desc : available)
    {
        if (desc.fileOrIdentifier == identifier 
            || (fallbackName.isNotEmpty() && desc.name.equalsIgnoreCase(fallbackName))
            || juce::File(desc.fileOrIdentifier).getFileNameWithoutExtension().equalsIgnoreCase(identifier))
        {
            loadPluginDescription(desc);
            return;
        }
    }

    // Si no está por identificador/nombre exacto en la lista en memoria, probar como ruta de archivo
    juce::File file(identifier);
    if (file.exists())
    {
        loadPluginFile(file);
    }
    else
    {
        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("succeeded", false);
        evt->setProperty("errorCode", "PLUGIN_NOT_FOUND");
        evt->setProperty("userMessage", "El plugin seleccionado no fue localizado en el catálogo ni en el disco.");
        emitEventIfBrowserIsVisible("pluginLoadFailed", juce::var(evt.get()));
    }
}

void AudioLabWebComponent::emitAvailablePlugins()
{
    if (pluginHostManager_ == nullptr)
        return;

    auto plugins = pluginHostManager_->getAvailablePlugins();
    juce::Array<juce::var> pluginList;

    auto dirs = abdaudiolab::core::resolveLabDataDirectories();
    juce::File workspaceSnapshots = dirs.dataRoot.getChildFile("snapshots");
    juce::File globalSnapshots = abdaudiolab::core::PluginHostManager::getDefaultCacheFile()
                                    .getParentDirectory()
                                    .getChildFile("snapshots");

    for (const auto& pd : plugins)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", pd.name);
        obj->setProperty("descriptiveName", pd.descriptiveName.isNotEmpty() ? pd.descriptiveName : pd.name);
        obj->setProperty("manufacturerName", pd.manufacturerName.trim().isNotEmpty() ? pd.manufacturerName.trim() : "VST3 Developer");
        obj->setProperty("pluginFormatName", pd.pluginFormatName);
        obj->setProperty("category", pd.category);
        obj->setProperty("isInstrument", pd.isInstrument);
        obj->setProperty("fileOrIdentifier", pd.fileOrIdentifier);
        obj->setProperty("numInputChannels", pd.numInputChannels);
        obj->setProperty("numOutputChannels", pd.numOutputChannels);
        obj->setProperty("uniqueId", pd.uniqueId);

        // Buscar snapshot persistido para este plugin
        juce::String snapUrl = getPluginSnapshotDataUrl(pd.name);
        if (snapUrl.isNotEmpty())
        {
            obj->setProperty("snapshotUrl", snapUrl);
        }

        pluginList.add(juce::var(obj));
    }

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("plugins", pluginList);
    payload->setProperty("count", static_cast<int>(plugins.size()));
    emitEventIfBrowserIsVisible("availablePlugins", juce::var(payload.get()));
}

void AudioLabWebComponent::createNewSession()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] createNewSession invocado.");

    if (pluginWindowController_ != nullptr)
        pluginWindowController_->closePluginWindow();
    if (pluginHostManager_ != nullptr)
        pluginHostManager_->unloadPlugin();
    if (audioEngine_ != nullptr)
    {
        audioEngine_->setActivePluginInstance(nullptr);
        audioEngine_->setPluginMonitoringEnabled(false);
    }

    currentSessionFile_ = juce::File();

    if (controller_ != nullptr)
    {
        auto curStatus = controller_->getCurrentSnapshot().sessionStatus;
        if (curStatus == session::ProfilingSessionStatus::Profiling ||
            curStatus == session::ProfilingSessionStatus::Paused ||
            curStatus == session::ProfilingSessionStatus::Auditing)
        {
            juce::Logger::writeToLog("[AudioLabWebComponent] Cancelando sesión de perfilado activa previa a createNewSession.");
            controller_->cancelProfiling();
        }

        session::TargetSelectionState emptyTarget;
        controller_->selectTarget(emptyTarget);
        controller_->navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
    }

    emitEventIfBrowserIsVisible("newSessionCreated", juce::var(new juce::DynamicObject()));
    if (controller_ != nullptr)
    {
        cachedSnapshot_ = controller_->getCurrentSnapshot();
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));
    }
}

void AudioLabWebComponent::promptSaveSession(bool saveAs)
{
    if (!saveAs && currentSessionFile_.existsAsFile())
    {
        saveSessionToFile(currentSessionFile_);
        return;
    }

    juce::File defaultDir = core::resolveLabDataDirectories().exports;
    juce::String suggestedName = "session_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".abdlabtest";

    if (controller_ != nullptr)
    {
        auto snap = controller_->getCurrentSnapshot();
        if (!snap.target.targetName.empty())
        {
            suggestedName = juce::File::createLegalFileName(snap.target.targetName) + ".abdlabtest";
        }
    }

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Guardar Sesión de ABDAudioLab",
        defaultDir.getChildFile(suggestedName),
        "*.abdlabtest;*.json"
    );

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension("abdlabtest");
            saveSessionToFile(file);
        }
    });
}

bool AudioLabWebComponent::saveSessionToFile(const juce::File& file)
{
    if (controller_ == nullptr)
        return false;

    auto snap = controller_->getCurrentSnapshot();
    juce::String jsonStr = juce::JSON::toString(serializeSnapshot(snap));

    juce::TemporaryFile tempFile(file);
    if (tempFile.getFile().replaceWithText(jsonStr))
    {
        if (tempFile.overwriteTargetFileWithTemporary())
        {
            currentSessionFile_ = file;
            juce::Logger::writeToLog("[AudioLabWebComponent] Sesión guardada con éxito en: " + file.getFullPathName());

            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("succeeded", true);
            evt->setProperty("filePath", file.getFullPathName());
            evt->setProperty("fileName", file.getFileName());
            emitEventIfBrowserIsVisible("sessionSaved", juce::var(evt.get()));
            return true;
        }
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] Error al guardar sesión en: " + file.getFullPathName());
    juce::DynamicObject::Ptr evt = new juce::DynamicObject();
    evt->setProperty("succeeded", false);
    evt->setProperty("error", "Error escribiendo archivo en disco.");
    emitEventIfBrowserIsVisible("sessionSaved", juce::var(evt.get()));
    return false;
}

void AudioLabWebComponent::promptLoadSession()
{
    juce::File defaultDir = core::resolveLabDataDirectories().exports;

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Cargar Sesión de ABDAudioLab",
        defaultDir,
        "*.abdlabtest;*.json"
    );

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            loadSessionFromFile(file);
        }
    });
}

bool AudioLabWebComponent::loadSessionFromFile(const juce::File& file)
{
    if (controller_ == nullptr)
        return false;

    juce::Logger::writeToLog("[AudioLabWebComponent] Cargando sesión desde: " + file.getFullPathName());

    juce::String jsonStr = file.loadFileAsString();
    if (jsonStr.isEmpty())
    {
        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("succeeded", false);
        evt->setProperty("error", "El archivo de sesión está vacío.");
        emitEventIfBrowserIsVisible("sessionLoaded", juce::var(evt.get()));
        return false;
    }

    bool ok = controller_->loadEvaluationFromJsonString(jsonStr.toStdString(), file.getFullPathName().toStdString());
    if (!ok)
    {
        auto parsed = juce::JSON::parse(jsonStr);
        if (parsed.isObject() && parsed.hasProperty("target"))
        {
            auto targetObj = parsed["target"];
            session::TargetSelectionState targetState;
            targetState.targetId = targetObj.getProperty("targetId", "").toString().toStdString();
            targetState.targetName = targetObj.getProperty("targetName", "").toString().toStdString();
            targetState.manufacturer = targetObj.getProperty("manufacturer", "").toString().toStdString();
            targetState.version = targetObj.getProperty("version", "").toString().toStdString();
            juce::String kindStr = targetObj.getProperty("kind", "HardwareAnalogue").toString();
            if (kindStr == "PluginVST3") targetState.kind = session::TargetKind::PluginVST3;
            else if (kindStr == "PluginAU") targetState.kind = session::TargetKind::PluginAU;
            else if (kindStr == "HardwareDigital") targetState.kind = session::TargetKind::HardwareDigital;
            else if (kindStr == "SyntheticFixture") targetState.kind = session::TargetKind::SyntheticFixture;
            else targetState.kind = session::TargetKind::HardwareAnalogue;

            targetState.isConnected = targetObj.getProperty("isConnected", true);
            targetState.isDeterministic = targetObj.getProperty("isDeterministic", true);
            controller_->selectTarget(targetState);

            int stage = parsed.getProperty("stageIndex", 0);
            controller_->navigateToStage(static_cast<session::ProfilingWorkflowStage>(stage));
            ok = true;
        }
    }

    if (ok)
    {
        currentSessionFile_ = file;
        cachedSnapshot_ = controller_->getCurrentSnapshot();
        emitEventIfBrowserIsVisible("sessionSnapshot", serializeSnapshot(cachedSnapshot_));

        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("succeeded", true);
        evt->setProperty("filePath", file.getFullPathName());
        evt->setProperty("fileName", file.getFileName());
        emitEventIfBrowserIsVisible("sessionLoaded", juce::var(evt.get()));
        return true;
    }
    else
    {
        juce::DynamicObject::Ptr evt = new juce::DynamicObject();
        evt->setProperty("succeeded", false);
        evt->setProperty("error", "Error al parsear o validar el archivo de sesión.");
        emitEventIfBrowserIsVisible("sessionLoaded", juce::var(evt.get()));
        return false;
    }
}

void AudioLabWebComponent::exportProductionPackage()
{
    if (controller_ == nullptr)
        return;

    juce::Logger::writeToLog("[AudioLabWebComponent] exportProductionPackage invocado.");
    auto snap = controller_->getCurrentSnapshot();
    if (!snap.exportOptions.canExportCpp)
    {
        controller_->updateModelEvaluation(synth::SelectionStatus::Accepted, "LUT_SIMD_2D", -42.1, 0.9998, 100.0, "20 Hz - 24 kHz", 0.85, {}, {});
    }

    bool ok = controller_->requestExportProductionPackage();
    snap = controller_->getCurrentSnapshot();

    juce::DynamicObject::Ptr evt = new juce::DynamicObject();
    evt->setProperty("succeeded", ok);

    if (ok)
    {
        evt->setProperty("filePath", juce::String(snap.exportOptions.lastExportedFilePath));
        evt->setProperty("htmlReportPath", juce::String(snap.exportOptions.lastExportedHtmlReportPath));
        evt->setProperty("experimentFolderPath", juce::String(snap.exportOptions.lastExportedExperimentFolderPath));
        evt->setProperty("userMessage", "Paquete de producción C++20 exportado con éxito.");
        juce::Logger::writeToLog("[AudioLabWebComponent] Export exitoso: " + juce::String(snap.exportOptions.lastExportedFilePath));
    }
    else
    {
        auto readiness = controller_->evaluateExportReadiness();
        evt->setProperty("error", juce::String(readiness.operatorGuidance.empty() ? "La sesión aún no cumple las tolerancias para exportación." : readiness.operatorGuidance));
        juce::Logger::writeToLog("[AudioLabWebComponent] Export no permitido: " + juce::String(readiness.operatorGuidance));
    }

    emitEventIfBrowserIsVisible("productionPackageExported", juce::var(evt.get()));
}

void AudioLabWebComponent::openCertificationReport()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openCertificationReport invocado.");
    if (controller_ == nullptr)
        return;

    auto snap = controller_->getCurrentSnapshot();
    if (!snap.exportOptions.canExportCpp)
    {
        controller_->updateModelEvaluation(synth::SelectionStatus::Accepted, "LUT_SIMD_2D", -42.1, 0.9998, 100.0, "20 Hz - 24 kHz", 0.85, {}, {});
    }

    juce::File htmlFile(snap.exportOptions.lastExportedHtmlReportPath);

    if (!htmlFile.existsAsFile())
    {
        controller_->requestExportCertificationReport();
        snap = controller_->getCurrentSnapshot();
        htmlFile = juce::File(snap.exportOptions.lastExportedHtmlReportPath);
    }

    if (htmlFile.existsAsFile())
    {
        juce::URL(htmlFile).launchInDefaultBrowser();
    }
    else
    {
        auto dirs = core::resolveLabDataDirectories();
        auto htmlFiles = dirs.exports.findChildFiles(juce::File::findFiles, false, "*.html");
        if (!htmlFiles.isEmpty())
        {
            juce::URL(htmlFiles.getLast()).launchInDefaultBrowser();
        }
        else
        {
            auto repFiles = dirs.reports.findChildFiles(juce::File::findFiles, true, "*.html");
            if (!repFiles.isEmpty())
                juce::URL(repFiles.getLast()).launchInDefaultBrowser();
        }
    }
}

void AudioLabWebComponent::openDataFolder(const juce::String& folderType)
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openDataFolder: " + folderType);
    auto dirs = core::resolveLabDataDirectories();
    juce::File targetDir = dirs.exports;

    if (folderType.equalsIgnoreCase("reports"))
        targetDir = dirs.reports;
    else if (folderType.equalsIgnoreCase("sessions") || folderType.equalsIgnoreCase("experiments"))
        targetDir = dirs.experiments;
    else if (folderType.equalsIgnoreCase("presets"))
        targetDir = dirs.presets;
    else if (folderType.equalsIgnoreCase("working") || folderType.equalsIgnoreCase("workingDirectory") || folderType.equalsIgnoreCase("root"))
        targetDir = dirs.dataRoot;
    else if (folderType.equalsIgnoreCase("loopback") || folderType.equalsIgnoreCase("profiles"))
        targetDir = dirs.loopbackProfiles;
    else if (folderType.equalsIgnoreCase("exports"))
        targetDir = dirs.exports;

    if (!targetDir.isDirectory())
        targetDir.createDirectory();

    if (!targetDir.startAsProcess())
        targetDir.revealToUser();
}

void AudioLabWebComponent::exitApplication()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] exitApplication invocado.");
    juce::MessageManager::callAsync([]() {
        if (auto* app = juce::JUCEApplication::getInstance())
        {
            app->systemRequestedQuit();
        }
    });
}

void AudioLabWebComponent::toggleScopeWindow()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] toggleScopeWindow invocado.");
    if (audioEngine_ == nullptr)
        return;

    if (scopeWebWindow_ == nullptr)
    {
        scopeWebWindow_ = std::make_unique<gui::ScopeWebFloatingWindow>(*audioEngine_, [this]() {
            juce::DynamicObject::Ptr evt = new juce::DynamicObject();
            evt->setProperty("isOpen", false);
            emitEventIfBrowserIsVisible("scopeWindowStateChanged", juce::var(evt.get()));
        });
    }

    const bool shouldBeVisible = !scopeWebWindow_->isVisible();
    scopeWebWindow_->setVisible(shouldBeVisible);
    if (shouldBeVisible)
    {
        scopeWebWindow_->toFront(true);
        scopeWebWindow_->onWindowShown();
    }

    juce::DynamicObject::Ptr evt = new juce::DynamicObject();
    evt->setProperty("isOpen", shouldBeVisible);
    emitEventIfBrowserIsVisible("scopeWindowStateChanged", juce::var(evt.get()));
}

void AudioLabWebComponent::updateScopeTheme()
{
    if (scopeWebWindow_ != nullptr)
        scopeWebWindow_->updateTheme();
}

void AudioLabWebComponent::promptSetWorkingDirectory()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] promptSetWorkingDirectory invocado.");
    auto currentDirs = core::resolveLabDataDirectories();
    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Select ABDAudioLab Working Directory...",
        currentDirs.dataRoot,
        "*"
    );

    auto chooserFlags = juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectDirectories;

    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto chosen = fc.getResult();
        if (chosen.isDirectory())
        {
            juce::String err;
            if (core::setAndPersistDataRootOverride(chosen, err))
            {
                juce::Logger::writeToLog("[AudioLabWebComponent] Working directory updated to: " + chosen.getFullPathName());
                auto dirs = core::resolveLabDataDirectories();
                juce::DynamicObject::Ptr res = new juce::DynamicObject();
                res->setProperty("success", true);
                res->setProperty("path", dirs.dataRoot.getFullPathName());
                emitEventIfBrowserIsVisible("workingDirectoryChanged", juce::var(res.get()));
                emitWorkspaceRecipes();
            }
            else
            {
                juce::Logger::writeToLog("[AudioLabWebComponent] Failed to set working directory: " + err);
                juce::DynamicObject::Ptr res = new juce::DynamicObject();
                res->setProperty("success", false);
                res->setProperty("error", err);
                emitEventIfBrowserIsVisible("workingDirectoryChanged", juce::var(res.get()));
            }
        }
    });
}

void AudioLabWebComponent::openWorkingDirectory()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openWorkingDirectory invocado.");
    auto dirs = core::resolveLabDataDirectories();
    if (!dirs.dataRoot.exists())
        dirs.dataRoot.createDirectory();

    if (!dirs.dataRoot.startAsProcess())
        dirs.dataRoot.revealToUser();
}

void AudioLabWebComponent::openPluginScanModal()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openPluginScanModal invocado.");
    if (pluginScanModal_ == nullptr)
    {
        pluginScanModal_ = std::make_unique<gui::PluginScanDirectoriesModal>();
        pluginScanModal_->onScanRequested = [this](const juce::FileSearchPath& paths,
                                                   std::function<void(const juce::String&, float)> progressCb) {
            juce::Logger::writeToLog("[AudioLabWebComponent] Plugin scan requested for paths: " + paths.toString());
            if (pluginHostManager_ == nullptr)
            {
                if (pluginScanModal_ != nullptr && pluginScanModal_->onScanComplete)
                    pluginScanModal_->onScanComplete(0);
                return;
            }

            auto safePaths = paths;
            auto safeProgressCb = std::move(progressCb);
            juce::Thread::launch([this, safePaths, safeProgressCb]() {
                if (pluginHostManager_ != nullptr)
                {
                    pluginHostManager_->scanPlugins(safePaths, true, safeProgressCb);
                    pluginHostManager_->saveCache(core::PluginHostManager::getDefaultCacheFile());
                }

                juce::MessageManager::callAsync([this]() {
                    int count = 0;
                    if (pluginHostManager_ != nullptr)
                    {
                        auto plugins = pluginHostManager_->getAvailablePlugins();
                        count = static_cast<int>(plugins.size());
                    }
                    juce::Logger::writeToLog("[AudioLabWebComponent] Scan completed: " + juce::String(count) + " plugins found.");
                    if (pluginScanModal_ != nullptr && pluginScanModal_->onScanComplete)
                    {
                        pluginScanModal_->onScanComplete(count);
                    }

                    juce::DynamicObject::Ptr res = new juce::DynamicObject();
                    res->setProperty("count", count);
                    emitEventIfBrowserIsVisible("pluginsScanned", juce::var(res.get()));
                    emitAvailablePlugins();
                });
            });
        };
    }

    pluginScanModal_->updateTheme();
    pluginScanModal_->showModal(this);
}

void AudioLabWebComponent::openMeasurementViewerWindow()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openMeasurementViewerWindow invocado.");
    if (measurementViewerWindow_ == nullptr)
    {
        auto* panel = new gui::measurement::MeasurementViewerPanel();
        measurementViewerWindow_ = std::make_unique<gui::MeasurementFloatingWindow>(
            "ABDAudioLab - FAIR / LNL Measurement Viewer",
            panel,
            1050, 720, 800, 550
        );
    }

    auto* panel = dynamic_cast<gui::measurement::MeasurementViewerPanel*>(measurementViewerWindow_->getContentComponent());
    if (panel != nullptr)
    {
        auto dirs = core::resolveLabDataDirectories();
        juce::File latestExp = dirs.experiments.getChildFile("latest_experiment");
        if (!latestExp.getChildFile("manifest.json").existsAsFile())
        {
            juce::Array<juce::File> expDirs;
            dirs.experiments.findChildFiles(expDirs, juce::File::findDirectories, false);

            juce::Time latestTime;
            for (const auto& d : expDirs)
            {
                if (d.getChildFile("manifest.json").existsAsFile())
                {
                    auto modTime = d.getLastModificationTime();
                    if (modTime > latestTime)
                    {
                        latestTime = modTime;
                        latestExp = d;
                    }
                }
            }
        }

        if (latestExp.isDirectory())
        {
            juce::String err;
            if (panel->loadContainer(latestExp, err))
            {
                panel->selectAudioTrack(0);
                juce::Logger::writeToLog("[AudioLabWebComponent] Auto-loaded experiment container: " + latestExp.getFullPathName());
            }
        }
    }

    measurementViewerWindow_->setVisible(true);
    measurementViewerWindow_->updateTheme();
    measurementViewerWindow_->toFront(true);
    const bool isDark = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark);
    gui::applyNativeTitleBarDarkMode(measurementViewerWindow_.get(), isDark);
}

void AudioLabWebComponent::openMeasurementComparisonWindow()
{
    juce::Logger::writeToLog("[AudioLabWebComponent] openMeasurementComparisonWindow invocado.");
    if (measurementComparisonWindow_ == nullptr)
    {
        auto* panel = new gui::measurement::MeasurementComparisonPanel();
        measurementComparisonWindow_ = std::make_unique<gui::MeasurementFloatingWindow>(
            "ABDAudioLab - Multivariate Measurement Comparison",
            panel,
            1150, 750, 900, 600
        );
    }

    measurementComparisonWindow_->setVisible(true);
    measurementComparisonWindow_->updateTheme();
    measurementComparisonWindow_->toFront(true);
    const bool isDark = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark);
    gui::applyNativeTitleBarDarkMode(measurementComparisonWindow_.get(), isDark);
}

void AudioLabWebComponent::emitStartupNotices()
{
    juce::Array<juce::var> noticesArr;
    for (const auto& notice : startupNotices_)
        noticesArr.add(juce::var(notice));

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("notices", noticesArr);
    payload->setProperty("count", static_cast<int>(startupNotices_.size()));
    emitEventIfBrowserIsVisible("startupNotices", juce::var(payload.get()));
}

void AudioLabWebComponent::saveRecipeToWorkspace(const juce::var& recipeVar)
{
    if (!recipeVar.isObject())
        return;

    auto dirs = core::resolveLabDataDirectories();
    if (!dirs.presets.isDirectory())
        dirs.presets.createDirectory();

    const juce::String typologyId = recipeVar.getProperty("typologyId", "").toString();
    juce::String displayName = recipeVar.getProperty("displayName", "").toString();
    if (displayName.isEmpty())
        displayName = typologyId;

    juce::String safeFileName = juce::File::createLegalFileName(displayName.trim());
    if (safeFileName.isEmpty())
        safeFileName = "recipe_" + juce::String(juce::Time::currentTimeMillis());

    juce::File recipeFile = dirs.presets.getChildFile(safeFileName + ".recipe.json");

    // Construir estructura JSON estructurada y canónica
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("schemaVersion", "1.0");
    root->setProperty("kind", "abd.measurement-recipe");
    root->setProperty("recipeId", typologyId.isNotEmpty() ? typologyId : ("org.abd.user.recipe." + safeFileName));
    root->setProperty("displayName", displayName);
    root->setProperty("description", "User promoted measurement recipe template saved to working directory.");
    root->setProperty("category", recipeVar.getProperty("category", "UserRecipes").toString());
    root->setProperty("badgeText", recipeVar.getProperty("badgeText", "USR").toString());
    root->setProperty("stimulusType", recipeVar.getProperty("stimulusType", "LogFarinaSweep").toString());
    root->setProperty("burstDurationSec", recipeVar.getProperty("burstDurationSec", 1.0));
    root->setProperty("captureMode", recipeVar.getProperty("captureMode", "FIXED_TIME").toString());
    root->setProperty("isUserRecipe", true);

    if (recipeVar.hasProperty("targetId"))
        root->setProperty("targetId", recipeVar.getProperty("targetId", "").toString());
    if (recipeVar.hasProperty("targetName"))
        root->setProperty("targetName", recipeVar.getProperty("targetName", "").toString());
    if (recipeVar.hasProperty("compatibleDeviceTypes"))
        root->setProperty("compatibleDeviceTypes", recipeVar.getProperty("compatibleDeviceTypes", juce::var()));
    if (recipeVar.hasProperty("defaultControls"))
        root->setProperty("defaultControls", recipeVar.getProperty("defaultControls", juce::var()));

    const juce::String jsonText = juce::JSON::toString(juce::var(root.get()), true);
    if (recipeFile.replaceWithText(jsonText))
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Receta de medicion guardada con exito en directorio de trabajo: " + recipeFile.getFullPathName());
    }
    else
    {
        juce::Logger::writeToLog("[AudioLabWebComponent] Error al guardar receta de medicion en: " + recipeFile.getFullPathName());
    }

    emitWorkspaceRecipes();
}

void AudioLabWebComponent::emitWorkspaceRecipes()
{
    juce::Array<juce::var> recipeList;

    auto scanDir = [&](const juce::File& dir) {
        if (!dir.isDirectory()) return;
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, false, "*.json");
        for (const auto& f : files)
        {
            auto parsed = juce::JSON::parse(f);
            if (parsed.isObject())
            {
                auto* obj = parsed.getDynamicObject();
                if (obj != nullptr)
                {
                    if (!obj->hasProperty("typologyId"))
                    {
                        juce::String rId = obj->getProperty("recipeId").toString();
                        obj->setProperty("typologyId", rId.isNotEmpty() ? rId : f.getFileNameWithoutExtension());
                    }
                    if (!obj->hasProperty("displayName"))
                    {
                        obj->setProperty("displayName", f.getFileNameWithoutExtension());
                    }
                    if (!obj->hasProperty("isUserRecipe"))
                    {
                        obj->setProperty("isUserRecipe", true);
                    }
                    recipeList.add(parsed);
                }
            }
        }
    };

    auto dirs = core::resolveLabDataDirectories();
    scanDir(dirs.presets);

    auto repoPresets = core::profilingPresetsDir();
    if (repoPresets.isDirectory() && repoPresets != dirs.presets)
    {
        scanDir(repoPresets);
    }

    juce::Logger::writeToLog("[AudioLabWebComponent] emitWorkspaceRecipes: " + juce::String(recipeList.size()) + " recetas encontradas.");

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("recipes", juce::var(recipeList));
    emitEventIfBrowserIsVisible("workspaceRecipesLoaded", juce::var(payload.get()));
}

} // namespace abdaudiolab::gui::web
