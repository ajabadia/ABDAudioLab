/**
 * @file test_LabDataDirectories.cpp
 * @brief Unit tests for deterministic LabDataDirectories resolution and self-contained experiment storage.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "core/LabDataDirectories.h"
#include "core/ExperimentStorage.h"
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
#include <windows.h>
#endif

using namespace abdaudiolab::core;

namespace
{

ExperimentRecord createTestRecord(const std::string& id)
{
    ExperimentRecord r;
    r.schemaVersion = 1;
    r.experimentId = id;
    r.revision = 1;
    r.kind = ExperimentKind::Measurement;
    r.status = ExperimentStatus::AuditedApproved;
    r.target.targetId = "ReferenceSynth";
    r.target.targetName = "Reference Ground Truth Synth VST3";
    r.evaluation.hasEvaluation = true;
    r.evaluation.recommendedModelType = "LUT_SIMD_2D";
    r.evaluation.canonicalEvaluationHash = "8c12ce9037f9f1f4ab005a789b577e0b22325cb995b14ec415968930abc98c6c";
    r.evaluation.hashVerified = true;
    return r;
}

void setTestEnvVar(const char* name, const char* value)
{
#if JUCE_WINDOWS
    SetEnvironmentVariableA(name, value);
#else
    if (value != nullptr)
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

} // namespace

TEST_CASE("LabDataDirectories: Prioridad estricta de resolución", "[directories]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_DirsTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    juce::File explicitDir = tempRoot.getChildFile("ExplicitRoot");
    juce::File envDir = tempRoot.getChildFile("EnvRoot");
    juce::File workspaceDir = tempRoot.getChildFile("WorkspaceRoot");
    workspaceDir.createDirectory();
    workspaceDir.getChildFile("ABDAudioLab.workspace").replaceWithText("version=1.0.0");

    clearExplicitDataRootOverride();
    setTestEnvVar("ABDAUDIOLAB_DATA_ROOT", nullptr);

    SECTION("1. Marcador único de workspace detectado cuando no hay overrides")
    {
        // Simulamos búsqueda desde un subdirectorio del workspace
        juce::File subDir = workspaceDir.getChildFile("build/Release");
        subDir.createDirectory();

        juce::File found = findWorkspaceMarker(subDir);
        REQUIRE(found.existsAsFile());
        CHECK(found.getParentDirectory().getFullPathName() == workspaceDir.getFullPathName());
    }

    SECTION("2. Variable de entorno gana al marcador de workspace")
    {
        envDir.createDirectory();
        setTestEnvVar("ABDAUDIOLAB_DATA_ROOT", envDir.getFullPathName().toRawUTF8());

        juce::String diag;
        auto dirs = resolveLabDataDirectories(&diag);

        CHECK(dirs.origin == DataRootOrigin::EnvironmentVariable);
        CHECK(dirs.dataRoot.getFullPathName() == envDir.getFullPathName());
        CHECK(dirs.experiments.getFullPathName() == envDir.getChildFile("experiments").getFullPathName());
        CHECK(dirs.exports.getFullPathName() == envDir.getChildFile("exports").getFullPathName());
        CHECK(dirs.isValid());

        setTestEnvVar("ABDAUDIOLAB_DATA_ROOT", nullptr);
    }

    SECTION("3. Override explícito gana a variable de entorno y marcador")
    {
        envDir.createDirectory();
        setTestEnvVar("ABDAUDIOLAB_DATA_ROOT", envDir.getFullPathName().toRawUTF8());

        juce::String setErr;
        bool ok = setExplicitDataRootOverride(explicitDir, setErr);
        REQUIRE(ok);

        juce::String diag;
        auto dirs = resolveLabDataDirectories(&diag);

        CHECK(dirs.origin == DataRootOrigin::ExplicitOverride);
        CHECK(dirs.dataRoot.getFullPathName() == explicitDir.getFullPathName());
        CHECK(dirs.experiments.getFullPathName() == explicitDir.getChildFile("experiments").getFullPathName());
        CHECK(dirs.exports.getFullPathName() == explicitDir.getChildFile("exports").getFullPathName());
        CHECK(dirs.isValid());

        clearExplicitDataRootOverride();
        setTestEnvVar("ABDAUDIOLAB_DATA_ROOT", nullptr);
    }

    SECTION("4. Ruta configurada como archivo es rechazada con error")
    {
        juce::File dummyFile = tempRoot.getChildFile("fake_root.txt");
        dummyFile.replaceWithText("I am a file, not a directory");

        juce::String err;
        bool ok = setExplicitDataRootOverride(dummyFile, err);
        CHECK_FALSE(ok);
        CHECK(err.contains("cannot be an existing file"));

        clearExplicitDataRootOverride();
    }

    SECTION("5. Raíz con espacios se resuelve y crea correctamente")
    {
        juce::File spacedDir = tempRoot.getChildFile("Ruta Con Espacios Y Acentos");
        juce::String err;
        bool ok = setExplicitDataRootOverride(spacedDir, err);
        REQUIRE(ok);

        auto dirs = resolveLabDataDirectories();
        CHECK(dirs.isValid());
        CHECK(dirs.experiments.isDirectory());
        CHECK(dirs.exports.isDirectory());

        clearExplicitDataRootOverride();
    }

    tempRoot.deleteRecursively();
}

TEST_CASE("LabDataDirectories: Paquete autocontenido e igualdad de hashes embedded/export", "[directories][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_SelfContainedTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    juce::File experimentsDir = tempRoot.getChildFile("experiments");
    juce::File exportsDir = tempRoot.getChildFile("exports");
    experimentsDir.createDirectory();
    exportsDir.createDirectory();

    auto record = createTestRecord("Exp_SelfContained_001");
    std::string modelCode = "// Generated C++20 Model\nconstexpr int kVersion = 1;\n";

    juce::File convenienceFile = exportsDir.getChildFile("ReferenceSynth_LUT_SIMD_2D_convenience.h");

    EmbeddedModelPayload payload;
    payload.relativePathInsideExperiment = "models/ModelPackage.h";
    payload.modelSourceCode = modelCode;
    payload.convenienceExportFile = convenienceFile;

    juce::String err;
    bool saved = ExperimentStorage::saveExperiment(experimentsDir, record, {}, err, payload);
    REQUIRE(saved);
    CHECK(err.isEmpty());

    juce::File expFolder = experimentsDir.getChildFile(record.experimentId);
    REQUIRE(expFolder.isDirectory());

    juce::File embeddedFile = expFolder.getChildFile("models/ModelPackage.h");
    REQUIRE(embeddedFile.existsAsFile());
    REQUIRE(convenienceFile.existsAsFile());

    // 1. Verificación de que el contenido es idéntico
    CHECK(embeddedFile.loadFileAsString() == convenienceFile.loadFileAsString());

    // 2. Verificación de que los hashes SHA-256 calculados son idénticos
    std::string embeddedSha = ExperimentStorage::computeFileSha256(embeddedFile);
    std::string exportSha = ExperimentStorage::computeFileSha256(convenienceFile);
    CHECK(embeddedSha == exportSha);
    CHECK_FALSE(embeddedSha.empty());

    // 3. Verificación de que manifest.json indexa el modelo con role "embedded_model"
    auto loaded = ExperimentStorage::loadExperiment(expFolder, err);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->isCorrupt());
    CHECK(loaded->isExportable());

    bool foundEmbedded = false;
    for (const auto& art : loaded->artifacts)
    {
        if (art.relativePath == "models/ModelPackage.h")
        {
            foundEmbedded = true;
            CHECK(art.role == "embedded_model");
            CHECK(art.sha256 == embeddedSha);
        }
    }
    CHECK(foundEmbedded);

    // 4. Autocontención: borrar exports/ no afecta en absoluto a la integridad del experimento
    convenienceFile.deleteFile();
    exportsDir.deleteRecursively();

    auto loadedWithoutExports = ExperimentStorage::loadExperiment(expFolder, err);
    REQUIRE(loadedWithoutExports.has_value());
    CHECK_FALSE(loadedWithoutExports->isCorrupt());
    CHECK(loadedWithoutExports->isExportable());

    tempRoot.deleteRecursively();
}

TEST_CASE("LabDataDirectories: Fallo transaccional limpia archivos temporales y aborta commit", "[directories][storage]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_AbortTest_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    juce::File experimentsDir = tempRoot.getChildFile("experiments");
    experimentsDir.createDirectory();

    auto record = createTestRecord("Exp_Abort_001");

    // Proporcionamos una ruta insegura con path traversal en embeddedModel
    EmbeddedModelPayload payload;
    payload.relativePathInsideExperiment = "../escape_models/Bad.h";
    payload.modelSourceCode = "// Malicious";

    juce::String err;
    bool saved = ExperimentStorage::saveExperiment(experimentsDir, record, {}, err, payload);
    CHECK_FALSE(saved);
    CHECK(err.contains("Security error: Unsafe relative path"));

    // El experimento no debe haberse creado
    CHECK_FALSE(experimentsDir.getChildFile(record.experimentId).exists());

    // Ninguna carpeta temporal (.tmp) debe haber quedado residualmente
    juce::Array<juce::File> tmpDirs;
    experimentsDir.findChildFiles(tmpDirs, juce::File::findDirectories, false);
    CHECK(tmpDirs.size() == 0);

    tempRoot.deleteRecursively();
}

TEST_CASE("LabDataDirectories: Higiene de sondas de escritura y limpieza absoluta", "[directories]")
{
    juce::File tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ProbeHygiene_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempRoot.createDirectory();

    juce::String diag;
    juce::String setErr;
    REQUIRE(setExplicitDataRootOverride(tempRoot, setErr));

    auto dirs = resolveLabDataDirectories(&diag);
    REQUIRE(dirs.isValid());

    // 1. Verificar que no quedan archivos .probe_write_*.tmp en dataRoot, experiments ni exports
    juce::Array<juce::File> probeFiles;
    tempRoot.findChildFiles(probeFiles, juce::File::findFiles, true, ".probe_write_*.tmp");
    CHECK(probeFiles.size() == 0);

    // 2. Verificar que no quedan archivos .tmp_* en experiments ni dataRoot
    juce::Array<juce::File> tmpFiles;
    tempRoot.findChildFiles(tmpFiles, juce::File::findFiles | juce::File::findDirectories, true, "*.tmp*");
    CHECK(tmpFiles.size() == 0);

    clearExplicitDataRootOverride();
    tempRoot.deleteRecursively();
}

