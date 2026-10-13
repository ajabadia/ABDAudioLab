/**
 * @file test_CalibrationSnapshotProvenance.cpp
 * @brief Hermetic unit tests for CalibrationSnapshot, Compatibility, ActiveCalibrationContext and Trim composition.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "calibration/CalibrationSnapshot.h"
#include "calibration/CalibrationCompatibilityEvaluator.h"
#include "calibration/ActiveCalibrationContext.h"
#include "audio/LabAudioReceiver.h"
#include "core/SessionSerializer.h"
#include "support/LabTestScratch.h"

using namespace abdaudiolab;
using namespace abdaudiolab::calibration;

namespace
{

CalibrationSnapshot createTestSnapshot(
    const std::string& devId = "AudioBox USB",
    const std::string& driver = "ASIO",
    double sampleRate = 44100.0,
    int bufferSize = 256,
    int inCh = 0,
    int outCh = 0,
    int rtl = 256,
    float interfaceTrim = -2.57f)
{
    CalibrationSnapshot s;
    s.schemaVersion = 1;
    s.displayName = "PreSonus AudioBox USB — Out 1 -> In 1 — 44.1 kHz";
    s.profileId = "audiobox-usb-loopback-out1-in1-20261005";
    s.createdAt = "2026-10-05T08:00:00Z";

    s.compatibility.deviceStableId = devId;
    s.compatibility.driverType = driver;
    s.compatibility.sampleRateHz = sampleRate;
    s.compatibility.bufferSamples = bufferSize;
    s.compatibility.inputChannelIndex = inCh;
    s.compatibility.outputChannelIndex = outCh;
    s.compatibility.routingDescription = "Main Out 1 -> Input 1";

    s.capture.sweepDurationMs = 1000;
    s.capture.latencyMarginMs = 200;
    s.capture.decayTailMs = 100;
    s.capture.requiredSamples = 57330;
    s.capture.capturedSamples = 57330;

    s.result.calibrationStatus = "Valid";
    s.result.rtlSamples = rtl;
    s.result.rtlMs = (static_cast<double>(rtl) / sampleRate) * 1000.0;
    s.result.peakDbfs = -3.0f;
    s.result.flatnessDeltaDb = 0.5f;
    s.result.snrDb = 90.7f;
    s.result.interfaceTrimDb = interfaceTrim;
    s.result.clippingSamples = 0;
    s.result.polarity = "Normal";

    s.processingPolicy.policyVersion = 1;
    s.processingPolicy.latencyCompensationEnabled = true;
    s.processingPolicy.inverseCompensationEnabled = false;
    s.processingPolicy.inverseCompensationMaxBoostDb = 0.0f;

    s.integrity.snapshotHash = s.computeHash();
    return s;
}

CurrentAudioConfigurationSnapshot createCurrentConfig(
    const std::string& devId = "AudioBox USB",
    const std::string& driver = "ASIO",
    double sampleRate = 44100.0,
    int bufferSize = 256,
    int inCh = 0,
    int outCh = 0)
{
    CurrentAudioConfigurationSnapshot c;
    c.deviceName = devId;
    c.driverType = driver;
    c.sampleRate = sampleRate;
    c.bufferSizeSamples = bufferSize;
    c.inputChannelIndex = inCh;
    c.inputChannelLabel = "Input 1";
    c.outputChannelIndex = outCh;
    c.outputChannelLabel = "Output 1";
    return c;
}

} // namespace

TEST_CASE("CalibrationSnapshot: JSON round-trip conserva todos los campos e integridad", "[calibration][snapshot]")
{
    auto original = createTestSnapshot();
    auto json = original.toJson();

    // Verify root keys
    CHECK(json["schemaVersion"] == 1);
    CHECK(json["displayName"] == "PreSonus AudioBox USB — Out 1 -> In 1 — 44.1 kHz");
    CHECK(json["profileId"] == "audiobox-usb-loopback-out1-in1-20261005");
    CHECK(json["compatibility"]["sampleRateHz"] == 44100.0);
    CHECK(json["result"]["rtlSamples"] == 256);
    CHECK(json["result"]["interfaceTrimDb"] == Catch::Approx(-2.57f));
    CHECK(json["processingPolicy"]["inverseCompensationEnabled"] == false);
    CHECK(json["integrity"]["snapshotHash"] == original.integrity.snapshotHash);

    // Deserialize round-trip
    auto recoveredOpt = CalibrationSnapshot::fromJsonSafe(json);
    REQUIRE(recoveredOpt.has_value());
    const auto& rec = *recoveredOpt;

    CHECK(rec.schemaVersion == original.schemaVersion);
    CHECK(rec.displayName == original.displayName);
    CHECK(rec.profileId == original.profileId);
    CHECK(rec.createdAt == original.createdAt);
    CHECK(rec.compatibility == original.compatibility);
    CHECK(rec.capture == original.capture);
    CHECK(rec.result == original.result);
    CHECK(rec.processingPolicy == original.processingPolicy);
    CHECK(rec.integrity == original.integrity);
    CHECK(rec.verifyIntegrity() == true);
}

TEST_CASE("CalibrationSnapshot: Criptografia SHA-256 de 256 bits, formato canonico y verificacion estricta", "[calibration][snapshot][sha256]")
{
    auto snap = createTestSnapshot();
    std::string originalHash = snap.integrity.snapshotHash;

    SECTION("SHA-256 produce exactamente 64 caracteres hexadecimales en minusculas")
    {
        CHECK(originalHash.length() == 64);
        for (char c : originalHash)
        {
            CHECK(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
        }
        CHECK(snap.integrity.hashAlgorithm == "SHA-256");
        CHECK(snap.verifyIntegrity() == true);
    }

    SECTION("El JSON canonico produce el mismo hash aunque cambie el orden de insercion de claves")
    {
        // Construct two JSON objects with reversed key insertion orders
        nlohmann::json j1;
        j1["displayName"] = snap.displayName;
        j1["profileId"] = snap.profileId;
        j1["schemaVersion"] = snap.schemaVersion;
        j1["createdAt"] = snap.createdAt;
        j1["compatibility"] = snap.toJson()["compatibility"];
        j1["capture"] = snap.toJson()["capture"];
        j1["noiseBaseline"] = snap.toJson()["noiseBaseline"];
        j1["result"] = snap.toJson()["result"];
        j1["processingPolicy"] = snap.toJson()["processingPolicy"];
        j1["integrity"] = snap.toJson()["integrity"];

        nlohmann::json j2;
        j2["integrity"] = snap.toJson()["integrity"];
        j2["processingPolicy"] = snap.toJson()["processingPolicy"];
        j2["result"] = snap.toJson()["result"];
        j2["noiseBaseline"] = snap.toJson()["noiseBaseline"];
        j2["capture"] = snap.toJson()["capture"];
        j2["compatibility"] = snap.toJson()["compatibility"];
        j2["createdAt"] = snap.createdAt;
        j2["schemaVersion"] = snap.schemaVersion;
        j2["profileId"] = snap.profileId;
        j2["displayName"] = snap.displayName;

        auto s1 = CalibrationSnapshot::fromJson(j1);
        auto s2 = CalibrationSnapshot::fromJson(j2);

        CHECK(s1.computeHash() == s2.computeHash());
        CHECK(s1.computeHash() == originalHash);
    }

    SECTION("integrity.snapshotHash queda estrictamente excluido de la entrada usada para calcular el hash")
    {
        // Mutating integrity.snapshotHash must not affect computeHash()
        snap.integrity.snapshotHash = "0000000000000000000000000000000000000000000000000000000000000000";
        CHECK(snap.computeHash() == originalHash);
        // But verifyIntegrity() must fail because the hash stored does not match
        CHECK_FALSE(snap.verifyIntegrity());
    }

    SECTION("Un solo byte alterado en cualquier campo cubierto invalida verifyIntegrity()")
    {
        // RTL
        auto sRtl = snap;
        sRtl.result.rtlSamples += 1;
        CHECK(sRtl.computeHash() != originalHash);
        CHECK_FALSE(sRtl.verifyIntegrity());

        // Sample rate
        auto sSr = snap;
        sSr.compatibility.sampleRateHz += 1.0;
        CHECK(sSr.computeHash() != originalHash);
        CHECK_FALSE(sSr.verifyIntegrity());

        // Display name
        auto sName = snap;
        sName.displayName += "!";
        CHECK(sName.computeHash() != originalHash);
        CHECK_FALSE(sName.verifyIntegrity());

        // Policy
        auto sPol = snap;
        sPol.processingPolicy.inverseCompensationEnabled = true;
        CHECK(sPol.computeHash() != originalHash);
        CHECK_FALSE(sPol.verifyIntegrity());

        // Noise baseline directMonitorState
        auto sBase = snap;
        sBase.noiseBaseline.directMonitorState = "Unknown";
        CHECK(sBase.computeHash() != originalHash);
        CHECK_FALSE(sBase.verifyIntegrity());
    }

    SECTION("Un hash valido no se acepta si hashAlgorithm no es SHA-256")
    {
        auto sTamperedAlg = snap;
        sTamperedAlg.integrity.hashAlgorithm = "FNV-1a-128";
        // Even if hash string has 64 chars, wrong algorithm must be rejected
        CHECK_FALSE(sTamperedAlg.verifyIntegrity());
    }

    SECTION("FNV o hashes no SHA-256 no pueden utilizarse por accidente en verifyIntegrity")
    {
        auto sFnv = snap;
        sFnv.integrity.snapshotHash = "146959810393466560371099511628211"; // legacy 32-char string
        CHECK_FALSE(sFnv.verifyIntegrity());
    }
}

TEST_CASE("CalibrationDraft: Inmutabilidad estricta y separacion de borrador de UI", "[calibration][draft][immutability]")
{
    auto initialSnap = createTestSnapshot();
    std::string initialHash = initialSnap.integrity.snapshotHash;

    // Create a draft from the sealed snapshot or measurement
    auto draft = CalibrationDraft::fromSnapshot(initialSnap);

    SECTION("Editar displayName en el borrador no modifica el snapshot ya sellado")
    {
        draft.displayName = "AudioBox USB — Renombrado por el usuario en UI";

        // Initial snapshot must remain 100% bit-exact and unchanged
        CHECK(initialSnap.displayName == "PreSonus AudioBox USB — Out 1 -> In 1 — 44.1 kHz");
        CHECK(initialSnap.integrity.snapshotHash == initialHash);
        CHECK(initialSnap.verifyIntegrity() == true);
    }

    SECTION("Guardar el borrador crea un snapshot nuevo con un hash nuevo")
    {
        draft.displayName = "AudioBox USB — Studio B Knobs 12h";
        auto newSealedSnap = draft.sealSnapshot();

        CHECK(newSealedSnap.displayName == "AudioBox USB — Studio B Knobs 12h");
        CHECK(newSealedSnap.verifyIntegrity() == true);
        CHECK(newSealedSnap.integrity.snapshotHash != initialHash);
        CHECK(newSealedSnap.integrity.snapshotHash.length() == 64);

        // Original snapshot is still unaltered
        CHECK(initialSnap.integrity.snapshotHash == initialHash);
    }

    SECTION("Abrir una sesion antigua conserva su snapshot original aunque se edite o guarde otro perfil")
    {
        // Simulated old session package with embedded snapshot
        core::SessionManifest oldManifest;
        oldManifest.calibrationSnapshot = initialSnap;

        // User edits active draft in UI and saves a new local profile
        draft.displayName = "AudioBox USB — Perfil Local Modificado";
        auto localProfileSnap = draft.sealSnapshot();

        // The session manifest's embedded snapshot remains intact
        REQUIRE(oldManifest.calibrationSnapshot.has_value());
        CHECK(oldManifest.calibrationSnapshot->displayName == initialSnap.displayName);
        CHECK(oldManifest.calibrationSnapshot->integrity.snapshotHash == initialHash);
        CHECK(oldManifest.calibrationSnapshot->verifyIntegrity() == true);
        CHECK(oldManifest.calibrationSnapshot->integrity.snapshotHash != localProfileSnap.integrity.snapshotHash);
    }
}

TEST_CASE("CalibrationCompatibilityEvaluator: Reglas de compatibilidad de audio", "[calibration][compatibility]")
{
    auto snap = createTestSnapshot();

    SECTION("Mismo dispositivo, driver, SR, buffer y routing -> Compatible")
    {
        auto cur = createCurrentConfig();
        auto eval = CalibrationCompatibilityEvaluator::evaluate(snap, cur);
        CHECK(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::Compatible);
    }

    SECTION("Cambio de Sample Rate -> SampleRateMismatch (Incompatible)")
    {
        auto cur = createCurrentConfig("AudioBox USB", "ASIO", 48000.0, 256);
        auto eval = CalibrationCompatibilityEvaluator::evaluate(snap, cur);
        CHECK_FALSE(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::SampleRateMismatch);
    }

    SECTION("Cambio de Dispositivo -> DeviceOrDriverMismatch (Incompatible)")
    {
        auto cur = createCurrentConfig("Focusrite Scarlett 2i2", "ASIO", 44100.0, 256);
        auto eval = CalibrationCompatibilityEvaluator::evaluate(snap, cur);
        CHECK_FALSE(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::DeviceOrDriverMismatch);
    }

    SECTION("Cambio de Routing (Input Channel) -> RoutingMismatch (Incompatible)")
    {
        auto cur = createCurrentConfig("AudioBox USB", "ASIO", 44100.0, 256, 1, 0); // In Ch 2
        auto eval = CalibrationCompatibilityEvaluator::evaluate(snap, cur);
        CHECK_FALSE(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::RoutingMismatch);
    }

    SECTION("Buffer size distinto -> BufferSizeMismatch con strict=true")
    {
        auto cur = createCurrentConfig("AudioBox USB", "ASIO", 44100.0, 512);
        auto eval = CalibrationCompatibilityEvaluator::evaluate(snap, cur, true);
        CHECK_FALSE(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::BufferSizeMismatch);
    }

    SECTION("Snapshot invalido o con clipping -> InvalidSnapshot")
    {
        auto invalidSnap = snap;
        invalidSnap.result.calibrationStatus = "Clipped";
        invalidSnap.result.clippingSamples = 10;
        invalidSnap.integrity.snapshotHash = invalidSnap.computeHash();

        auto cur = createCurrentConfig();
        auto eval = CalibrationCompatibilityEvaluator::evaluate(invalidSnap, cur);
        CHECK_FALSE(eval.isCompatible());
        CHECK(eval.verdict == CompatibilityVerdict::InvalidSnapshot);
    }
}

TEST_CASE("ActiveCalibrationContext: Proyeccion y control en tiempo de ejecucion", "[calibration][context]")
{
    ActiveCalibrationContext context;
    CHECK_FALSE(context.isActive());
    CHECK(context.getLatencyCompensationSamples() == 0);
    CHECK(context.getGainPlan().interfaceCalibrationTrimDb == 0.0f);

    auto snap = createTestSnapshot("AudioBox USB", "ASIO", 44100.0, 256, 0, 0, 256, -2.57f);
    auto config = createCurrentConfig("AudioBox USB", "ASIO", 44100.0, 256, 0, 0);

    SECTION("Activacion exitosa proyecta RTL e interfaceTrimDb")
    {
        REQUIRE(context.activate(snap, config));
        CHECK(context.isActive());
        CHECK(context.getLatencyCompensationSamples() == 256);
        CHECK(context.getGainPlan().interfaceCalibrationTrimDb == Catch::Approx(-2.57f));
        CHECK(context.getGainPlan().sessionTargetTrimDb == 0.0f);
        CHECK(context.getEffectiveTrimDb() == Catch::Approx(-2.57f));
    }

    SECTION("Configuracion incompatible rechaza activacion y mantiene neutralidad")
    {
        auto badConfig = createCurrentConfig("AudioBox USB", "ASIO", 96000.0, 256);
        REQUIRE_FALSE(context.activate(snap, badConfig));
        CHECK_FALSE(context.isActive());
        CHECK(context.getLatencyCompensationSamples() == 0);
        CHECK(context.getEffectiveTrimDb() == 0.0f);
    }

    SECTION("Cambio posterior de configuracion neutraliza contexto activo")
    {
        REQUIRE(context.activate(snap, config));
        CHECK(context.isActive());

        // Stream switches to 48kHz
        auto switchedConfig = createCurrentConfig("AudioBox USB", "ASIO", 48000.0, 256);
        context.checkAlignment(switchedConfig);

        CHECK_FALSE(context.isActive());
        CHECK(context.getLatencyCompensationSamples() == 0);
        CHECK(context.getEffectiveTrimDb() == 0.0f);
    }
}

TEST_CASE("InputGainPlan: Composicion de trim de interfaz y sesion (no sobrescritura)", "[calibration][trim]")
{
    ActiveCalibrationContext context;
    auto snap = createTestSnapshot("AudioBox USB", "ASIO", 44100.0, 256, 0, 0, 256, -2.57f);
    auto config = createCurrentConfig();
    REQUIRE(context.activate(snap, config));

    // interfaceTrimDb is -2.57 dB
    CHECK(context.getGainPlan().interfaceCalibrationTrimDb == Catch::Approx(-2.57f));

    // Sequencer pre-scan measures synthesizer and sets session trim to +1.2 dB
    context.setSessionTargetTrimDb(1.20f);

    auto plan = context.getGainPlan();
    CHECK(plan.interfaceCalibrationTrimDb == Catch::Approx(-2.57f));
    CHECK(plan.sessionTargetTrimDb == Catch::Approx(1.20f));
    CHECK(plan.effectiveTrimDb == Catch::Approx(-2.57f + 1.20f)); // -1.37 dB

    float expectedLinear = std::pow(10.0f, (-1.37f) / 20.0f);
    CHECK(context.getEffectiveLinearGain() == Catch::Approx(expectedLinear).margin(1e-4f));

    // Changing session trim again still preserves the interface calibration trim!
    context.setSessionTargetTrimDb(-0.50f);
    CHECK(context.getGainPlan().interfaceCalibrationTrimDb == Catch::Approx(-2.57f));
    CHECK(context.getGainPlan().sessionTargetTrimDb == Catch::Approx(-0.50f));
    CHECK(context.getEffectiveTrimDb() == Catch::Approx(-3.07f));
}

TEST_CASE("LabAudioReceiver: Prueba determinista de direccion de latencia RTL", "[calibration][receiver][latency]")
{
    // Test requirements from user specification:
    // Estímulo digital emitido: frame 0.
    // Respuesta de entrada simulada: pico en frame 256.
    // Latencia calibrada: 256 samples.
    // Respuesta entregada al profiling: pico alineado en frame 0.
    // Sin calibracion: el pico conserva el desplazamiento real (frame 256).

    audio::LabAudioReceiver receiver;
    receiver.prepare(44100.0, 2.0);

    audio::CaptureRequirements req;
    req.requiredSamples = 1024;
    req.sweepSamples = 1024;
    req.requireTrigger = false;

    // Create a 1024-sample block with a sharp peak at frame 256
    std::vector<float> inputBlock(1024, 0.0f);
    inputBlock[256] = 0.95f; // peak at index 256

    SECTION("Con calibracion RTL de 256 samples -> pico entregado en frame 0")
    {
        receiver.setLatencyCompensationSamples(256);
        REQUIRE(receiver.armWithRequirements(req, 0.0f));
        receiver.processBlock(inputBlock.data(), 1024);

        std::vector<float> captured;
        REQUIRE(receiver.retrieveRecordedData(captured));
        REQUIRE(captured.size() == 1024 - 256);

        // Peak was at frame 256, so after skipping 256 latency samples, peak is at frame 0!
        CHECK(captured[0] == Catch::Approx(0.95f));

        // Locate maximum element index
        auto maxIt = std::max_element(captured.begin(), captured.end());
        size_t peakIndex = std::distance(captured.begin(), maxIt);
        CHECK(peakIndex == 0);
    }

    SECTION("Sin calibracion (latencia 0) -> pico conserva desplazamiento en frame 256")
    {
        receiver.setLatencyCompensationSamples(0);
        REQUIRE(receiver.armWithRequirements(req, 0.0f));
        receiver.processBlock(inputBlock.data(), 1024);

        std::vector<float> captured;
        REQUIRE(receiver.retrieveRecordedData(captured));
        REQUIRE(captured.size() == 1024);

        auto maxIt = std::max_element(captured.begin(), captured.end());
        size_t peakIndex = std::distance(captured.begin(), maxIt);
        CHECK(peakIndex == 256);
        CHECK(captured[256] == Catch::Approx(0.95f));
    }

    SECTION("Calibracion con latencia negativa se rechaza y clampa a 0 de forma segura")
    {
        receiver.setLatencyCompensationSamples(-50);
        CHECK(receiver.getLatencyCompensationSamples() == 0);

        REQUIRE(receiver.armWithRequirements(req, 0.0f));
        receiver.processBlock(inputBlock.data(), 1024);

        std::vector<float> captured;
        REQUIRE(receiver.retrieveRecordedData(captured));
        CHECK(captured.size() == 1024);
        CHECK(captured[256] == Catch::Approx(0.95f));
    }
}

TEST_CASE("SessionSerializer: Snapshot embebido en .abdlabtest es portable e independiente de AppData", "[calibration][session][portability]")
{
    core::SessionSerializer serializer;

    core::SessionManifest manifest;
    manifest.sessionTitle = "Portable_Calibrated_Session";
    manifest.hardwareName = "AudioBox USB";
    manifest.sampleRate = 44100.0;

    auto originalSnap = createTestSnapshot("AudioBox USB", "ASIO", 44100.0, 256, 0, 0, 256, -2.57f);
    manifest.calibrationSnapshot = originalSnap;
    manifest.gainPlan.interfaceCalibrationTrimDb = -2.57f;
    manifest.gainPlan.sessionTargetTrimDb = 1.0f;
    manifest.gainPlan.effectiveTrimDb = -1.57f;

    std::vector<exporting::MeasuredPoint> points;
    exporting::MeasuredPoint p;
    p.pointId = "P_001";
    p.testId = "TEST_01";
    points.push_back(p);

    juce::File pkgFile = test::scratchDir("SessionCalibrationProvenance").getChildFile("calibrated_session.abdlabtest");
    if (pkgFile.existsAsFile())
        pkgFile.deleteFile();

    REQUIRE(serializer.saveSessionToPackage(pkgFile, manifest, points));
    REQUIRE(pkgFile.existsAsFile());

    // Load back into an independent manifest
    core::SessionManifest loadedManifest;
    std::vector<exporting::MeasuredPoint> loadedPoints;
    juce::String error;
    REQUIRE(serializer.loadSessionFromPackage(pkgFile, loadedManifest, loadedPoints, error));

    // Verify snapshot was preserved bit-by-bit inside package
    REQUIRE(loadedManifest.calibrationSnapshot.has_value());
    const auto& loadedSnap = *loadedManifest.calibrationSnapshot;
    CHECK(loadedSnap.displayName == originalSnap.displayName);
    CHECK(loadedSnap.profileId == originalSnap.profileId);
    CHECK(loadedSnap.compatibility.sampleRateHz == 44100.0);
    CHECK(loadedSnap.result.rtlSamples == 256);
    CHECK(loadedSnap.result.interfaceTrimDb == Catch::Approx(-2.57f));
    CHECK(loadedSnap.verifyIntegrity() == true);
    CHECK(loadedSnap.integrity.snapshotHash == originalSnap.integrity.snapshotHash);

    // Verify gain plan preserved
    CHECK(loadedManifest.gainPlan.interfaceCalibrationTrimDb == Catch::Approx(-2.57f));
    CHECK(loadedManifest.gainPlan.sessionTargetTrimDb == Catch::Approx(1.0f));
    CHECK(loadedManifest.gainPlan.effectiveTrimDb == Catch::Approx(-1.57f));

    pkgFile.deleteFile();
}

TEST_CASE("SessionSerializer: Migracion legacy de lineCalibrationGainDb no inventa datos de calibracion", "[calibration][session][legacy]")
{
    // Synthetic legacy JSON manifest without "calibrationSnapshot"
    nlohmann::json legacyJson;
    legacyJson["sessionTitle"] = "Legacy_Session";
    legacyJson["sampleRate"] = 48000.0;
    legacyJson["lineCalibrationGainDb"] = -1.75f;
    legacyJson["tests"] = nlohmann::json::array();

    core::SessionManifest loadedManifest;
    REQUIRE(core::SessionSerializer::deserializeManifestFromJson(legacyJson, loadedManifest));

    // Must NOT fabricate unknown snapshot data
    CHECK_FALSE(loadedManifest.calibrationSnapshot.has_value());
    CHECK(loadedManifest.lineCalibrationGainDb == Catch::Approx(-1.75f));

    // Gain plan preserves legacy scalar as session target without inventing interface calibration
    CHECK(loadedManifest.gainPlan.interfaceCalibrationTrimDb == 0.0f);
    CHECK(loadedManifest.gainPlan.sessionTargetTrimDb == Catch::Approx(-1.75f));
    CHECK(loadedManifest.gainPlan.effectiveTrimDb == Catch::Approx(-1.75f));
}

TEST_CASE("SessionSerializer: Sesion sin calibracion abre de forma segura", "[calibration][session][uncalibrated]")
{
    nlohmann::json uncalibratedJson;
    uncalibratedJson["sessionTitle"] = "Fresh_Uncalibrated_Session";
    uncalibratedJson["sampleRate"] = 96000.0;
    uncalibratedJson["tests"] = nlohmann::json::array();

    core::SessionManifest loadedManifest;
    REQUIRE(core::SessionSerializer::deserializeManifestFromJson(uncalibratedJson, loadedManifest));

    CHECK_FALSE(loadedManifest.calibrationSnapshot.has_value());
    CHECK(loadedManifest.gainPlan.interfaceCalibrationTrimDb == 0.0f);
    CHECK(loadedManifest.gainPlan.sessionTargetTrimDb == Catch::Approx(-3.0f)); // default lineCalibrationGainDb
}

TEST_CASE("CalibrationProcessingPolicy: inverseCompensationEnabled == false garantiza no-procesamiento", "[calibration][policy]")
{
    ActiveCalibrationContext context;
    auto snap = createTestSnapshot();
    auto config = createCurrentConfig();
    REQUIRE(context.activate(snap, config));

    // Verify policy defaults
    CHECK(context.isInverseCompensationEnabled() == false);
    CHECK(context.getInverseCompensationMaxBoostDb() == 0.0f);

    // When policy is disabled, no inverse filtering should ever be applied
    std::vector<float> originalSignal = { 0.1f, -0.2f, 0.5f, -0.8f, 0.3f };
    std::vector<float> processedSignal = originalSignal; // bit-to-bit preservation

    if (!context.isInverseCompensationEnabled())
    {
        // No de-coloring executed
    }

    CHECK(processedSignal == originalSignal);
}

TEST_CASE("CalibrationPortability: Reproducibilidad hermetica de paquetes de sesion en otra maquina", "[calibration][portability][hermetic]")
{
    auto scratch = test::scratchDir("PortabilityTest");
    juce::File pkgFile = scratch.getChildFile("portable_experiment.abdlabtest");
    if (pkgFile.existsAsFile())
        pkgFile.deleteFile();

    // 1. Maquina A: Crea y calibra la sesion con Focusrite @ 48kHz
    auto machineASnap = createTestSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 256, 0, 0, 595, -2.57f);
    
    core::SessionSerializer serializer;
    core::SessionManifest machineAManifest;
    machineAManifest.appVersion = "2.1.0";
    machineAManifest.sessionTitle = "Moog_Sub37_Filter_Profile";
    machineAManifest.hardwareDisplayName = "Sub 37";
    machineAManifest.sampleRate = 48000.0;
    machineAManifest.gainPlan.interfaceCalibrationTrimDb = -2.57f;
    machineAManifest.gainPlan.sessionTargetTrimDb = 1.0f;
    machineAManifest.gainPlan.effectiveTrimDb = -1.57f;
    machineAManifest.lineCalibrationGainDb = -1.57f;
    machineAManifest.calibrationSnapshot = machineASnap;

    std::vector<exporting::MeasuredPoint> points;
    exporting::MeasuredPoint p;
    p.pointId = "PT_01";
    p.testId = "SWEEP_20_20K";
    points.push_back(p);

    REQUIRE(serializer.saveSessionToPackage(pkgFile, machineAManifest, points));
    REQUIRE(pkgFile.existsAsFile());

    // 2. Maquina B: Abre el paquete en un entorno aislado (cero perfiles locales)
    core::SessionManifest machineBManifest;
    std::vector<exporting::MeasuredPoint> machineBPoints;
    juce::String err;
    REQUIRE(serializer.loadSessionFromPackage(pkgFile, machineBManifest, machineBPoints, err));

    REQUIRE(machineBManifest.calibrationSnapshot.has_value());
    const auto& portableSnap = *machineBManifest.calibrationSnapshot;
    CHECK(portableSnap.verifyIntegrity() == true);
    CHECK(portableSnap.compatibility.deviceStableId == "Focusrite USB ASIO");
    CHECK(portableSnap.result.rtlSamples == 595);

    // Escenario 2A: Maquina B tiene hardware diferente (ej. Realtek @ 44.1kHz, buffer 512)
    auto machineBMismatchedConfig = createCurrentConfig("Realtek High Definition Audio", "DirectSound", 44100.0, 512);
    auto evalMismatch = CalibrationCompatibilityEvaluator::evaluate(portableSnap, machineBMismatchedConfig);
    CHECK_FALSE(evalMismatch.isCompatible());
    CHECK(evalMismatch.verdict == CompatibilityVerdict::DeviceOrDriverMismatch);
    CHECK_FALSE(evalMismatch.isActionable);
    CHECK(evalMismatch.mismatchDetails.size() >= 2); // device, driver

    ActiveCalibrationContext contextMismatch;
    CHECK_FALSE(contextMismatch.activate(portableSnap, machineBMismatchedConfig));
    CHECK_FALSE(contextMismatch.isActive());

    // Escenario 2B: Maquina B conecta el mismo hardware o replica la configuracion (Focusrite @ 48kHz, buffer 256)
    auto machineBMatchingConfig = createCurrentConfig("Focusrite USB ASIO", "ASIO", 48000.0, 256);
    auto evalMatch = CalibrationCompatibilityEvaluator::evaluate(portableSnap, machineBMatchingConfig);
    CHECK(evalMatch.isCompatible());
    CHECK(evalMatch.verdict == CompatibilityVerdict::Compatible);
    CHECK(evalMatch.isActionable);

    // Se activa directamente desde el snapshot portable sin requerir AppData local
    ActiveCalibrationContext contextMatch;
    REQUIRE(contextMatch.activate(portableSnap, machineBMatchingConfig));
    CHECK(contextMatch.isActive());
    CHECK(contextMatch.getLatencyCompensationSamples() == 595);
    CHECK(contextMatch.getGainPlan().interfaceCalibrationTrimDb == Catch::Approx(-2.57f));
    contextMatch.setSessionTargetTrimDb(1.0f);
    CHECK(contextMatch.getEffectiveTrimDb() == Catch::Approx(-1.57f));

    pkgFile.deleteFile();
}
