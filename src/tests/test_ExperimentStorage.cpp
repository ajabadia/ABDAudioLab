#include <catch2/catch_test_macros.hpp>
#include "core/ExperimentRecord.h"
#include "core/ExperimentStorage.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::core;

namespace
{

ExperimentRecord createDummyRecord(const std::string& id = "20260915T135500Z_ReferenceSynth_8c12ce90", uint32_t rev = 1)
{
    ExperimentRecord r;
    r.schemaVersion = 1;
    r.experimentId = id;
    r.revision = rev;
    r.kind = ExperimentKind::Measurement;
    r.status = ExperimentStatus::AuditedApproved;

    r.target.targetId = "ReferenceSynth";
    r.target.targetName = "Reference Ground Truth Synth VST3";
    r.target.manufacturer = "ABDSynths";
    r.target.version = "1.0.0";
    r.target.format = "VST3";
    r.target.binarySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    r.target.binaryPath = "fixtures/ReferenceSynth.vst3";
    r.target.isDeterministic = true;

    r.capture.sampleRate = 48000.0;
    r.capture.hostBufferSize = 480;
    r.capture.processingBlockSize = 256;
    r.capture.channels = 2;
    r.capture.durationSeconds = 2.5;
    r.capture.presetStateHash = "a1b2c3d4e5f6";
    r.capture.excitationPlanHash = "f6e5d4c3b2a1";
    r.capture.storageProfile = StorageProfile::Standard;

    r.provenance.appVersion = "1.1.0";
    r.provenance.buildNumber = 293;
    r.provenance.gitCommit = "bc6af12";
    r.provenance.executionMode = "OutOfProcessVST3";
    r.provenance.operatingSystem = "Windows 11 Pro 64-bit";
    r.provenance.machineName = "DEV-WORKSTATION";
    r.provenance.timestampUtc = "2026-09-15T13:55:00Z";
    r.provenance.operatorNotes = "Medicion automatizada en Release con worker aislado.";
    r.provenance.ambientTemperatureC = 23.5f;
    r.provenance.warmupTimeMinutes = 15;

    r.evaluation.hasEvaluation = true;
    r.evaluation.recommendedModelType = "LUT_SIMD_2D";
    r.evaluation.selectionStatus = "Accepted";
    r.evaluation.canonicalEvaluationHash = "8c12ce9037f9f1f4ab005a789b577e0b22325cb995b14ec415968930abc98c6c";
    r.evaluation.validationEsrDb = -120.0;
    r.evaluation.validationCorrelation = 1.0000;
    r.evaluation.criteriaCompliancePercent = 100.0;
    r.evaluation.validatedDomain = "C1-C6, Vel 1-127";
    r.evaluation.relativeCpuCost = 0.50;
    r.evaluation.hashVerified = true;

    r.limitations.modeledAspects = { "Cutoff response", "PolyBLEP oscillator", "VCA level" };
    r.limitations.unmodeledAspects = { "LFO phase drift", "Sub-oscillator noise" };
    r.limitations.validityDomain = "48 kHz, Block 256/480, Velocity 1-127";
    r.limitations.extrapolationWarnings = { "No extrapolable fuera de 48 kHz" };

    return r;
}

} // namespace

TEST_CASE("ExperimentStorage: Guardado y lectura transaccional (Roundtrip)", "[experiment][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExpStorage_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    // Crear un archivo WAV simulado
    juce::File dummyWav = tempRoot.getChildFile("source_test.wav");
    dummyWav.replaceWithText("RIFF....WAVEfmt ....dataFAKEAUDIOSTREAMCONTENT");

    auto record = createDummyRecord();
    std::vector<std::pair<std::string, juce::File>> audioList = {
        { "measurements/excitation_0001_target.wav", dummyWav }
    };

    juce::String err;
    bool saved = ExperimentStorage::saveExperiment(tempRoot, record, audioList, err);
    REQUIRE(saved);
    CHECK(err.isEmpty());

    juce::File expDir = tempRoot.getChildFile(record.experimentId);
    REQUIRE(expDir.isDirectory());
    CHECK(expDir.getChildFile("experiment.json").existsAsFile());
    CHECK(expDir.getChildFile("manifest.json").existsAsFile());
    CHECK(expDir.getChildFile("measurements/excitation_0001_target.wav").existsAsFile());

    // Lectura y verificación criptográfica
    auto readBack = ExperimentStorage::loadExperiment(expDir, err);
    REQUIRE(readBack.has_value());
    CHECK_FALSE(readBack->isCorrupt());
    CHECK(readBack->isExportable());
    CHECK(readBack->schemaVersion == 1);
    CHECK(readBack->experimentId == record.experimentId);
    CHECK(readBack->target.targetId == "ReferenceSynth");
    CHECK(readBack->evaluation.canonicalEvaluationHash == record.evaluation.canonicalEvaluationHash);
    CHECK(readBack->artifacts.size() >= 5); // target.json, provenance.json, evaluation.json, limitations.json, experiment.json, wav

    tempRoot.deleteRecursively();
}

TEST_CASE("ExperimentStorage: Regla de inmutabilidad y versiones (Revisiones)", "[experiment][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExpStorage_RevTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    auto recordRev1 = createDummyRecord("Exp_Invariance_001", 1);
    juce::String err;
    REQUIRE(ExperimentStorage::saveExperiment(tempRoot, recordRev1, {}, err));

    // Intento de sobrescritura de la revisión 1 debe fallar
    CHECK_FALSE(ExperimentStorage::saveExperiment(tempRoot, recordRev1, {}, err));
    CHECK(err.contains("Invariance violation"));

    // Guardar como revisión 2 (reevaluación)
    auto recordRev2 = recordRev1;
    recordRev2.revision = 2;
    recordRev2.parentExperimentId = "Exp_Invariance_001";
    recordRev2.evaluation.recommendedModelType = "LUT_SIMD_2D_CubicInterpolated";

    REQUIRE(ExperimentStorage::saveExperiment(tempRoot, recordRev2, {}, err));

    juce::File rev2Dir = tempRoot.getChildFile("Exp_Invariance_001_rev2");
    REQUIRE(rev2Dir.isDirectory());

    auto loadedRev2 = ExperimentStorage::loadExperiment(rev2Dir, err);
    REQUIRE(loadedRev2.has_value());
    CHECK(loadedRev2->revision == 2);
    REQUIRE(loadedRev2->parentExperimentId.has_value());
    CHECK(*loadedRev2->parentExperimentId == "Exp_Invariance_001");

    tempRoot.deleteRecursively();
}

TEST_CASE("ExperimentStorage: Detección de corrupción por alteración de datos", "[experiment][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExpStorage_CorruptTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    juce::File dummyWav = tempRoot.getChildFile("audio.wav");
    dummyWav.replaceWithText("RIFF...WAVEDATA_ORIGINAL");

    auto record = createDummyRecord("Exp_Tamper_001", 1);
    juce::String err;
    REQUIRE(ExperimentStorage::saveExperiment(tempRoot, record, { { "measurements/target.wav", dummyWav } }, err));

    juce::File expDir = tempRoot.getChildFile("Exp_Tamper_001");

    SECTION("Alterar 1 byte en un JSON marca estado Corrupt y bloquea exportación")
    {
        juce::File evalFile = expDir.getChildFile("evaluation.json");
        std::string content = evalFile.loadFileAsString().toStdString();
        content += " "; // Alteración de espacio/byte
        evalFile.replaceWithText(content);

        auto loaded = ExperimentStorage::loadExperiment(expDir, err);
        REQUIRE(loaded.has_value());
        CHECK(loaded->isCorrupt());
        CHECK_FALSE(loaded->isExportable());
        CHECK(loaded->failureOrCorruptionReason.find("Cryptographic mismatch") != std::string::npos);
    }

    SECTION("Alterar 1 byte en un WAV marca estado Corrupt")
    {
        juce::File wavFile = expDir.getChildFile("measurements/target.wav");
        std::string wavContent = wavFile.loadFileAsString().toStdString();
        wavContent[5] = 'X'; // Alteración en el payload
        wavFile.replaceWithText(wavContent);

        auto loaded = ExperimentStorage::loadExperiment(expDir, err);
        REQUIRE(loaded.has_value());
        CHECK(loaded->isCorrupt());
        CHECK_FALSE(loaded->isExportable());
        CHECK(loaded->failureOrCorruptionReason.find("Cryptographic mismatch") != std::string::npos);
    }

    SECTION("Archivo faltante detectado en manifest marca estado Corrupt")
    {
        juce::File wavFile = expDir.getChildFile("measurements/target.wav");
        wavFile.deleteFile();

        auto loaded = ExperimentStorage::loadExperiment(expDir, err);
        REQUIRE(loaded.has_value());
        CHECK(loaded->isCorrupt());
        CHECK(loaded->failureOrCorruptionReason.find("Missing required artifact") != std::string::npos);
    }

    tempRoot.deleteRecursively();
}

TEST_CASE("ExperimentStorage: Seguridad estricta anti-traversal en rutas relativas", "[experiment][storage]")
{
    CHECK_FALSE(isSafeRelativePath("../escape.txt"));
    CHECK_FALSE(isSafeRelativePath("measurements/../../system.dll"));
    CHECK_FALSE(isSafeRelativePath("/absolute/path"));
    CHECK_FALSE(isSafeRelativePath("\\windows\\path"));
    CHECK_FALSE(isSafeRelativePath("C:\\autoexec.bat"));
    CHECK_FALSE(isSafeRelativePath(""));

    CHECK(isSafeRelativePath("measurements/excitation_0001_target.wav"));
    CHECK(isSafeRelativePath("models/ModelPackage.h"));
    CHECK(isSafeRelativePath("logs/session.log"));
}

TEST_CASE("ExperimentStorage: Validación de schemaVersion", "[experiment][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExpStorage_VerTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    auto record = createDummyRecord("Exp_Ver_001", 1);
    juce::String err;
    REQUIRE(ExperimentStorage::saveExperiment(tempRoot, record, {}, err));

    juce::File expDir = tempRoot.getChildFile("Exp_Ver_001");
    juce::File expJson = expDir.getChildFile("experiment.json");

    // Alterar schemaVersion a una versión no soportada
    auto j = nlohmann::json::parse(expJson.loadFileAsString().toStdString());
    j["schemaVersion"] = 999;
    expJson.replaceWithText(j.dump(2));

    auto loaded = ExperimentStorage::loadExperiment(expDir, err);
    CHECK_FALSE(loaded.has_value());
    CHECK(err.contains("Unsupported or invalid schemaVersion: 999"));

    tempRoot.deleteRecursively();
}

TEST_CASE("ExperimentStorage: Archivo desconocido en el directorio es ignorado de forma segura", "[experiment][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExpStorage_UnknownTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    auto record = createDummyRecord("Exp_Unknown_001", 1);
    juce::String err;
    REQUIRE(ExperimentStorage::saveExperiment(tempRoot, record, {}, err));

    juce::File expDir = tempRoot.getChildFile("Exp_Unknown_001");
    // Crear un archivo arbitrario ajeno al manifiesto (por ejemplo, notas locales o un script de usuario)
    juce::File extraFile = expDir.getChildFile("extra_user_notes.txt");
    extraFile.replaceWithText("Notas adicionales no indexadas en el manifiesto FAIR.");

    auto loaded = ExperimentStorage::loadExperiment(expDir, err);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->isCorrupt());
    CHECK(loaded->isExportable());

    tempRoot.deleteRecursively();
}

