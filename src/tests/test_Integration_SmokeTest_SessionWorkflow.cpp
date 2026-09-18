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
