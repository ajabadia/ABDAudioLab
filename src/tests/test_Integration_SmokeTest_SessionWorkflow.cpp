/**
 * @file test_Integration_SmokeTest_SessionWorkflow.cpp
 * @brief End-to-End Integration Smoke Test:
 *        Real Profile Loading -> Target Function -> Control Snapshots ->
 *        Raw Capture Fixity -> Initial Analysis -> Session Closure ->
 *        Re-opening -> Historical Re-analysis without Recapturing.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/HardwareContractRegistry.h"
#include "core/plugins/PluginHardwareContractAdapter.h"
#include "measurement/MeasurementSessionContracts.h"
#include "measurement/AnalogDutCharacterizer.h"
#include "measurement/AnalogChainReversibleCompensator.h"
#include "synth/Sha256.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab;
using namespace abdaudiolab::measurement;

namespace
{
    std::vector<float> generateSynthesizedCapture(double sampleRate, double freqHz, double amp, size_t lengthSamples)
    {
        std::vector<float> sig(lengthSamples, 0.0f);
        const double phaseInc = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double phase = 0.0;
        for (size_t i = 0; i < lengthSamples; ++i)
        {
            // Fundamental + 1% H2
            sig[i] = static_cast<float>(amp * (std::sin(phase) + 0.01 * std::sin(2.0 * phase)));
            phase += phaseInc;
        }
        return sig;
    }

    juce::File findContractsDirectory()
    {
        auto cwd = juce::File::getCurrentWorkingDirectory();
        auto direct = cwd.getChildFile("contracts").getChildFile("hardware");
        if (direct.isDirectory()) return direct;

        auto parentDirect = cwd.getParentDirectory().getChildFile("contracts").getChildFile("hardware");
        if (parentDirect.isDirectory()) return parentDirect;

        return direct;
    }
}

TEST_CASE("Integration Smoke Test: Real Profile -> MeasurementSession -> Re-analysis", "[smoke_test][integration]")
{
    // 1. Cargar Perfil Real desde contracts/hardware/
    core::HardwareContractRegistry registry;
    auto contractsDir = findContractsDirectory();
    REQUIRE(contractsDir.isDirectory());

    auto profileFile = contractsDir.getChildFile("generic_midi_synth.json");
    REQUIRE(profileFile.existsAsFile());

    core::HardwareContract contract;
    juce::String warning;
    bool loadOk = registry.loadProfileResilient(profileFile, contract, warning);
    REQUIRE(loadOk);
    REQUIRE(contract.id == "generic_midi_synth");
    REQUIRE(contract.displayName == "Generic MIDI Synthesizer (CC & NRPN)");
    REQUIRE_FALSE(contract.functions.empty());

    std::string profileContent = profileFile.loadFileAsString().toStdString();
    std::string profileSha256 = synth::Sha256::computeHex(profileContent);
    REQUIRE_FALSE(profileSha256.empty());

    // 2. Selección de Objetivo (Función de Medición)
    const auto& targetFunction = contract.functions[0];
    REQUIRE(targetFunction.id == "filter_section");
    REQUIRE(targetFunction.suggestedStimulus == "LOG_SINE_SWEEP");

    // 3. Captura del Estado de Parámetros (ControlStateSnapshots)
    std::vector<ControlStateSnapshot> controlSnapshots;
    for (const auto& ctrl : targetFunction.controls)
    {
        ControlStateSnapshot snap;
        snap.controlId = ctrl.name;
        snap.controlMethod = ctrl.controlMethod;
        snap.normalizedValue = 0.75; // e.g. Cutoff al 75%
        snap.rawValue = 96.0;
        snap.displayValue = "1200 Hz";
        snap.confirmationStatus = (ctrl.controlMethod == "MANUAL") ? "confirmed" : "automated";
        controlSnapshots.push_back(snap);
    }
    REQUIRE(controlSnapshots.size() == targetFunction.controls.size());

    // 4. Captura Raw Determinista y Sellado SHA-256
    const double sampleRate = 48000.0;
    const size_t sampleCount = 4096;
    auto rawAudio = generateSynthesizedCapture(sampleRate, 1000.0, 0.8, sampleCount);

    std::string rawAudioSha256 = synth::Sha256::computeHex(rawAudio.data(), rawAudio.size() * sizeof(float));
    REQUIRE_FALSE(rawAudioSha256.empty());

    RawCaptureReference rawRef;
    rawRef.captureId = "cap_take_001";
    rawRef.rawAudioSha256 = rawAudioSha256;
    rawRef.filename = "dut_plus_chain_take1.raw.wav";
    rawRef.sampleRateHz = sampleRate;
    rawRef.sampleCount = sampleCount;
    rawRef.channels = 1;
    rawRef.activeControls = controlSnapshots;
    rawRef.saturationDiagnosis = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(rawAudio, 0.999f);
    REQUIRE(rawRef.saturationDiagnosis.isValidForDistortionMetrics());

    // 5. Construcción de MeasurementSession Canónica
    MeasurementSession session;
    session.sessionId = "session_20260918_generic_vcf";
    session.profileId = contract.id;
    session.profileSha256 = profileSha256;
    session.deviceType = contract.deviceType;
    session.targetFunction = targetFunction.id;
    session.excitationPlanId = targetFunction.suggestedStimulus;
    session.captureConfigHash = synth::Sha256::computeHex("sampleRate=48000&buffer=512");
    session.analyzerVersion = "abdaudiolab-analyzer-1.0";
    session.controlStates = controlSnapshots;
    session.rawCaptures.push_back(rawRef);

    // 6. Análisis Inicial (v1.0)
    SpectralAnalysisConfig configV1;
    configV1.sampleRateHz = sampleRate;
    configV1.fftSize = 4096;
    configV1.window = MeasurementWindow::Hann;
    configV1.harmonicIntegrationBins = 2;

    auto distResultV1 = AnalogDutCharacterizer::measureHarmonicDistortion(
        rawAudio, configV1.sampleRateHz, 1000.0, ThdConvention::FundamentalReferenced,
        2, 10, configV1.window, configV1.fftSize);

    DerivedArtifactReference derivV1;
    derivV1.artifactId = "deriv_01_thd_v1";
    derivV1.analyzerVersion = "abdaudiolab-analyzer-1.0";
    derivV1.configHash = synth::Sha256::computeHex("window=Hann&fft=4096");
    derivV1.artifactType = "harmonic_distortion";
    derivV1.targetRawCaptureSha256 = rawAudioSha256;
    derivV1.metrics = distResultV1.toCanonicalJson();
    derivV1.artifactSha256 = synth::Sha256::computeHex(derivV1.metrics.dump());

    session.derivedArtifacts.push_back(derivV1);

    // Serialización canónica de la sesión completada
    std::string canonicalJsonV1 = session.toCanonicalJson();
    REQUIRE_FALSE(canonicalJsonV1.empty());

    // 7. Simular Cierre de Sesión y Recarga desde JSON
    auto jsonParsed = nlohmann::ordered_json::parse(canonicalJsonV1);
    MeasurementSession reloadedSession;
    reloadedSession.sessionId = jsonParsed["sessionId"].get<std::string>();
    reloadedSession.profileId = jsonParsed["profileId"].get<std::string>();
    reloadedSession.profileSha256 = jsonParsed["profileSha256"].get<std::string>();
    reloadedSession.targetFunction = jsonParsed["targetFunction"].get<std::string>();
    reloadedSession.analyzerVersion = jsonParsed["analyzerVersion"].get<std::string>();

    for (const auto& rJson : jsonParsed["rawCaptures"])
    {
        RawCaptureReference r;
        r.captureId = rJson["captureId"].get<std::string>();
        r.rawAudioSha256 = rJson["rawAudioSha256"].get<std::string>();
        r.filename = rJson["filename"].get<std::string>();
        reloadedSession.rawCaptures.push_back(r);
    }

    for (const auto& dJson : jsonParsed["derivedArtifacts"])
    {
        DerivedArtifactReference d;
        d.artifactId = dJson["artifactId"].get<std::string>();
        d.analyzerVersion = dJson["analyzerVersion"].get<std::string>();
        d.configHash = dJson["configHash"].get<std::string>();
        d.artifactType = dJson["artifactType"].get<std::string>();
        d.targetRawCaptureSha256 = dJson["targetRawCaptureSha256"].get<std::string>();
        d.metrics = dJson["metrics"];
        d.artifactSha256 = dJson["artifactSha256"].get<std::string>();
        reloadedSession.derivedArtifacts.push_back(d);
    }

    // Verificación de Fixity tras Recarga
    REQUIRE(reloadedSession.profileSha256 == profileSha256);
    REQUIRE(reloadedSession.rawCaptures.size() == 1);
    REQUIRE(reloadedSession.rawCaptures[0].rawAudioSha256 == rawAudioSha256);
    REQUIRE(reloadedSession.derivedArtifacts.size() == 1);

    // 8. Re-análisis Histórico con Motor v2.0 (Sin Recapturar)
    SpectralAnalysisConfig configV2;
    configV2.sampleRateHz = sampleRate;
    configV2.fftSize = 4096;
    configV2.window = MeasurementWindow::FlatTop; // Nueva ventana con ganancia coherente calibrada
    configV2.harmonicIntegrationBins = 3;

    DerivedArtifactReference derivV2;
    std::string reanalysisError;
    bool reanalysisSuccess = SessionReanalysisEngine::reanalyzeHarmonicDistortion(
        reloadedSession,
        "cap_take_001",
        rawAudio,
        configV2,
        "abdaudiolab-analyzer-2.0",
        derivV2,
        reanalysisError);

    REQUIRE(reanalysisSuccess);
    REQUIRE(reanalysisError.empty());

    // Invariantes del Re-análisis Histórico
    REQUIRE(reloadedSession.derivedArtifacts.size() == 2);
    REQUIRE(reloadedSession.derivedArtifacts[0].analyzerVersion == "abdaudiolab-analyzer-1.0");
    REQUIRE(reloadedSession.derivedArtifacts[1].analyzerVersion == "abdaudiolab-analyzer-2.0");
    REQUIRE(reloadedSession.derivedArtifacts[0].targetRawCaptureSha256 == rawAudioSha256);
    REQUIRE(reloadedSession.derivedArtifacts[1].targetRawCaptureSha256 == rawAudioSha256);
    REQUIRE(reloadedSession.rawCaptures[0].rawAudioSha256 == rawAudioSha256); // Raw INALTERADO

    // 9. Intento de Re-análisis con Audio Manipulado (Anti-Tampering)
    auto tamperedAudio = rawAudio;
    tamperedAudio[50] = -tamperedAudio[50]; // Mutar una muestra

    DerivedArtifactReference tamperedArtifact;
    std::string tamperingError;
    bool tamperedResult = SessionReanalysisEngine::reanalyzeHarmonicDistortion(
        reloadedSession,
        "cap_take_001",
        tamperedAudio,
        configV2,
        "abdaudiolab-analyzer-3.0",
        tamperedArtifact,
        tamperingError);

    REQUIRE_FALSE(tamperedResult);
    REQUIRE(tamperingError.find("Fixity verification FAILED") != std::string::npos);
    REQUIRE(reloadedSession.derivedArtifacts.size() == 2); // No se añadió nada espurio
}

namespace
{
    class MockVst3PluginProcessor : public juce::AudioProcessor
    {
    public:
        MockVst3PluginProcessor()
        {
            addParameter(new juce::AudioParameterFloat({"cutoff", 1}, "Cutoff Filter", 20.0f, 20000.0f, 1000.0f));
            addParameter(new juce::AudioParameterFloat({"resonance", 1}, "Resonance", 0.0f, 1.0f, 0.2f));
            addParameter(new juce::AudioParameterFloat({"drive", 1}, "Drive Saturation", 0.0f, 1.0f, 0.1f));
        }
        const juce::String getName() const override { return "Mock VST3 Synth/Filter"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return "Default"; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock& destData) override
        {
            const char dummyState[] = "MOCK_VST3_PRESET_BINARY_BLOB_V1";
            destData.append(dummyState, sizeof(dummyState));
        }
        void setStateInformation(const void*, int) override {}
    };
}

TEST_CASE("Integration Smoke Test - VST3 Dexed / Mock DSP Workflow", "[smoke_test][vst3][integration]")
{
    // 1. Introspección y Generación de Contrato Dinámico VST3
    MockVst3PluginProcessor plugin;
    juce::PluginDescription desc;
    desc.name = "Mock VST3 Synth";
    desc.pluginFormatName = "VST3";
    desc.manufacturerName = "ABD Audio Research";
    desc.fileOrIdentifier = "mock_synth.vst3";
    desc.version = "2.1.0";
    desc.isInstrument = true;

    auto contract = core::PluginHardwareContractAdapter::createContractFromPlugin(plugin, desc);
    REQUIRE(contract.deviceType == "SOFTWARE_PLUGIN");
    REQUIRE(contract.functions.size() == 1);
    REQUIRE(contract.functions[0].controls.size() == 3);

    // 2. Extracción de Descriptores de Sesión VST3
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    auto vst3Desc = core::PluginHardwareContractAdapter::createSessionDescriptor(plugin, desc, sampleRate, blockSize);

    REQUIRE(vst3Desc.pluginIdentifier == "mock_synth.vst3");
    REQUIRE(vst3Desc.pluginVersion == "2.1.0");
    REQUIRE_FALSE(vst3Desc.parameterListHash.empty());
    REQUIRE_FALSE(vst3Desc.statePresetHash.empty());
    REQUIRE(vst3Desc.sampleRate == 48000.0);
    REQUIRE(vst3Desc.blockSize == 512);
    REQUIRE(vst3Desc.automationMode == "sample_accurate_gesture");

    // 3. Ciclo de Automatización VST3 Separado del Audio (beginEdit -> performEdit -> endEdit)
    core::PluginHardwareContractAdapter::beginParameterEdit(plugin, 1);
    core::PluginHardwareContractAdapter::performParameterEdit(plugin, 1, 0.85f);
    core::PluginHardwareContractAdapter::endParameterEdit(plugin, 1);

    float readVal = core::PluginHardwareContractAdapter::getParameterNormalized(plugin, 1);
    REQUIRE(std::abs(readVal - 0.85f) < 0.001f);

    // Snapshot del Estado del Control VST3
    ControlStateSnapshot snap;
    snap.controlId = contract.functions[0].controls[0].name;
    snap.controlMethod = "SOFTWARE_PLUGIN_PARAM";
    snap.normalizedValue = readVal;
    snap.rawValue = readVal;
    snap.displayValue = "17003.0 Hz";
    snap.confirmationStatus = "automated";

    // 4. Captura Determinista y Sellado SHA-256
    const size_t sampleCount = 4096;
    auto rawAudio = generateSynthesizedCapture(sampleRate, 1000.0, 0.707, sampleCount);
    std::string rawAudioSha256 = synth::Sha256::computeHex(rawAudio.data(), rawAudio.size() * sizeof(float));
    REQUIRE_FALSE(rawAudioSha256.empty());

    // Diagnóstico de Saturación Física (Digital Direct -> sin saturación)
    auto diag = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(rawAudio, 1.0f);
    REQUIRE_FALSE(diag.adcClipEvidence);

    // 5. Construcción de MeasurementSession con Metadatos VST3
    MeasurementSession session;
    session.sessionId = "session_vst3_dexed_mock_001";
    session.profileId = contract.id;
    session.profileSha256 = synth::Sha256::computeHex(contract.id);
    session.deviceType = "SOFTWARE_PLUGIN";
    session.targetFunction = contract.functions[0].id;
    session.excitationPlanId = "midi_note_c3_sweep";
    session.captureConfigHash = synth::Sha256::computeHex("sr=48000&block=512");
    session.analyzerVersion = "abdaudiolab-analyzer-1.0";
    session.pluginMetadata = vst3Desc.toJson();
    session.controlStates.push_back(snap);

    RawCaptureReference rawRef;
    rawRef.captureId = "vst3_raw_001";
    rawRef.rawAudioSha256 = rawAudioSha256;
    rawRef.filename = "vst3_raw_001.raw.wav";
    rawRef.sampleRateHz = sampleRate;
    rawRef.sampleCount = sampleCount;
    rawRef.channels = 1;
    rawRef.saturationDiagnosis = diag;
    rawRef.activeControls.push_back(snap);
    session.rawCaptures.push_back(rawRef);

    // 6. Análisis THD v1
    auto distResult = AnalogDutCharacterizer::measureHarmonicDistortion(
        rawAudio, sampleRate, 1000.0, ThdConvention::FundamentalReferenced,
        2, 10, MeasurementWindow::Blackman, 4096);

    DerivedArtifactReference art1;
    art1.artifactId = "deriv_vst3_thd_v1";
    art1.analyzerVersion = "abdaudiolab-analyzer-1.0";
    art1.configHash = synth::Sha256::computeHex("window=Blackman&fftSize=4096");
    art1.artifactType = "harmonic_distortion";
    art1.targetRawCaptureSha256 = rawAudioSha256;
    art1.createdAtUtc = "2026-09-18T12:00:00Z";
    art1.metrics = distResult.toCanonicalJson();
    art1.artifactSha256 = synth::Sha256::computeHex(art1.metrics.dump());
    session.derivedArtifacts.push_back(art1);

    // 7. Serialización RFC 8785 y Simulación de Persistencia
    std::string canonicalJson = session.toCanonicalJson();
    REQUIRE_FALSE(canonicalJson.empty());
    REQUIRE(canonicalJson.find("mock_synth.vst3") != std::string::npos);
    REQUIRE(canonicalJson.find("sample_accurate_gesture") != std::string::npos);

    // 8. Recarga y Re-análisis Histórico v2 sin Recapturar
    MeasurementSession reloaded;
    auto jsonParsed = nlohmann::ordered_json::parse(canonicalJson);
    reloaded.sessionId = jsonParsed["sessionId"].get<std::string>();
    reloaded.rawCaptures.push_back(rawRef);
    reloaded.derivedArtifacts.push_back(art1);

    SpectralAnalysisConfig configV2;
    configV2.sampleRateHz = sampleRate;
    configV2.fftSize = 4096;
    configV2.window = MeasurementWindow::Hann;
    configV2.harmonicIntegrationBins = 3;

    DerivedArtifactReference art2;
    std::string reanalysisErr;
    bool okReanalysis = SessionReanalysisEngine::reanalyzeHarmonicDistortion(
        reloaded, "vst3_raw_001", rawAudio, configV2, "abdaudiolab-analyzer-2.0", art2, reanalysisErr);

    REQUIRE(okReanalysis);
    REQUIRE(reanalysisErr.empty());
    REQUIRE(reloaded.derivedArtifacts.size() == 2);
    REQUIRE(reloaded.derivedArtifacts[0].analyzerVersion == "abdaudiolab-analyzer-1.0");
    REQUIRE(reloaded.derivedArtifacts[1].analyzerVersion == "abdaudiolab-analyzer-2.0");
    REQUIRE(reloaded.rawCaptures[0].rawAudioSha256 == rawAudioSha256);
}

TEST_CASE("Integration Smoke Test - Boss DS-1 Manual Analogue Pedal Workflow", "[smoke_test][pedal][integration]")
{
    // 1. Cargar Perfil Real de Pedal Analógico desde contracts/hardware/
    core::HardwareContractRegistry registry;
    auto contractsDir = findContractsDirectory();
    REQUIRE(contractsDir.isDirectory());

    auto profileFile = contractsDir.getChildFile("boss_ds1_distortion.json");
    REQUIRE(profileFile.existsAsFile());

    core::HardwareContract contract;
    juce::String warning;
    bool loadOk = registry.loadProfileResilient(profileFile, contract, warning);
    REQUIRE(loadOk);
    REQUIRE(contract.id == "boss_ds1_distortion");
    REQUIRE(contract.deviceType == "ANALOGUE_PEDAL");
    REQUIRE(contract.functions.size() == 1);

    const auto& fn = contract.functions[0];
    REQUIRE(fn.controls.size() == 3);

    // 2. Control Manual de Operador Humano: confirmación de perillas físicas (TONE, LEVEL, DIST)
    std::vector<ControlStateSnapshot> manualSnapshots;
    for (const auto& ctrl : fn.controls)
    {
        ControlStateSnapshot snap;
        snap.controlId = ctrl.name;
        snap.controlMethod = "MANUAL";
        snap.normalizedValue = 0.5; // Posición al medio (12 en punto)
        snap.rawValue = 5.0;
        snap.displayValue = "12 o'clock (Mid)";
        // Para controles manuales, confirmationStatus = confirmed significa que el operador
        // confirmó la acción ante la GUI (barra espaciadora), no que el software midió el potenciómetro
        snap.confirmationStatus = "confirmed";
        manualSnapshots.push_back(snap);
    }
    REQUIRE(manualSnapshots.size() == 3);

    // 3. Captura Sintética con Saturación Analógica Suave
    const double sampleRate = 48000.0;
    const size_t sampleCount = 4096;
    auto rawPedalAudio = generateSynthesizedCapture(sampleRate, 1000.0, 0.6, sampleCount);

    // Simular distorsión analógica (armónicos pares e impares típicos de diodos de clipping)
    for (size_t i = 0; i < sampleCount; ++i)
    {
        float x = rawPedalAudio[i];
        rawPedalAudio[i] = std::tanh(2.0f * x) * 0.7f; // Diodo clipping
    }

    std::string pedalRawSha256 = synth::Sha256::computeHex(rawPedalAudio.data(), rawPedalAudio.size() * sizeof(float));
    REQUIRE_FALSE(pedalRawSha256.empty());

    // Diagnóstico de Saturación Física (Cadena de interfaz analógica)
    auto diag = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(rawPedalAudio, 1.0f);
    REQUIRE_FALSE(diag.adcClipEvidence); // Saturación interna del pedal, pero interfaz en rango seguro

    // 4. Construcción de MeasurementSession para Pedal
    MeasurementSession session;
    session.sessionId = "session_boss_ds1_001";
    session.profileId = contract.id;
    session.profileSha256 = synth::Sha256::computeHex(profileFile.loadFileAsString().toStdString());
    session.deviceType = "ANALOGUE_PEDAL";
    session.targetFunction = fn.id;
    session.excitationPlanId = "analog_sweep_1khz";
    session.captureConfigHash = synth::Sha256::computeHex("interface=Focusrite&sampleRate=48000");
    session.analyzerVersion = "abdaudiolab-analyzer-1.0";
    session.controlStates = manualSnapshots;

    RawCaptureReference rawRef;
    rawRef.captureId = "ds1_take_001";
    rawRef.rawAudioSha256 = pedalRawSha256;
    rawRef.filename = "ds1_take_001.raw.wav";
    rawRef.sampleRateHz = sampleRate;
    rawRef.sampleCount = sampleCount;
    rawRef.channels = 1;
    rawRef.saturationDiagnosis = diag;
    rawRef.activeControls = manualSnapshots;
    session.rawCaptures.push_back(rawRef);

    // 5. Análisis Inicial de Distorsión Armónica (v1 - IEEE Fundamental Referenced)
    auto distResult = AnalogDutCharacterizer::measureHarmonicDistortion(
        rawPedalAudio, sampleRate, 1000.0, ThdConvention::FundamentalReferenced,
        2, 10, MeasurementWindow::Blackman, 4096);

    DerivedArtifactReference art1;
    art1.artifactId = "deriv_ds1_thd_v1";
    art1.analyzerVersion = "abdaudiolab-analyzer-1.0";
    art1.configHash = synth::Sha256::computeHex("window=Blackman&fftSize=4096&convention=IEEE");
    art1.artifactType = "harmonic_distortion";
    art1.targetRawCaptureSha256 = pedalRawSha256;
    art1.createdAtUtc = "2026-09-18T12:00:00Z";
    art1.metrics = distResult.toCanonicalJson();
    art1.artifactSha256 = synth::Sha256::computeHex(art1.metrics.dump());
    session.derivedArtifacts.push_back(art1);

    // 6. Serialización RFC 8785 y Re-análisis Histórico (v2 - IEC Total RMS Referenced)
    std::string canonicalJson = session.toCanonicalJson();
    REQUIRE_FALSE(canonicalJson.empty());

    MeasurementSession reloaded;
    reloaded.sessionId = session.sessionId;
    reloaded.rawCaptures.push_back(rawRef);
    reloaded.derivedArtifacts.push_back(art1);

    SpectralAnalysisConfig configV2;
    configV2.sampleRateHz = sampleRate;
    configV2.fftSize = 4096;
    configV2.window = MeasurementWindow::FlatTop;
    configV2.harmonicIntegrationBins = 3;

    DerivedArtifactReference art2;
    std::string err;
    bool okReanalysis = SessionReanalysisEngine::reanalyzeHarmonicDistortion(
        reloaded, "ds1_take_001", rawPedalAudio, configV2, "abdaudiolab-analyzer-2.0", art2, err);

    REQUIRE(okReanalysis);
    REQUIRE(err.empty());
    REQUIRE(reloaded.derivedArtifacts.size() == 2);
    REQUIRE(reloaded.derivedArtifacts[0].analyzerVersion == "abdaudiolab-analyzer-1.0");
    REQUIRE(reloaded.derivedArtifacts[1].analyzerVersion == "abdaudiolab-analyzer-2.0");
    REQUIRE(reloaded.rawCaptures[0].rawAudioSha256 == pedalRawSha256);
}

