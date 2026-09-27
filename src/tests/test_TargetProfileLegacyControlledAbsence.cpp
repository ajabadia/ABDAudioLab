// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy Controlled Absence Tests (E5)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

juce::File getContractsHardwareDir()
{
    juce::File current = juce::File::getCurrentWorkingDirectory();
    auto dir = current.getChildFile("contracts").getChildFile("hardware");
    if (dir.isDirectory()) return dir;
    dir = current.getParentDirectory().getChildFile("contracts").getChildFile("hardware");
    if (dir.isDirectory()) return dir;
    return {};
}

juce::File getCanonicalTargetsDir()
{
    juce::File current = juce::File::getCurrentWorkingDirectory();
    auto dir = current.getChildFile("profiles").getChildFile("targets");
    if (dir.isDirectory()) return dir;
    dir = current.getParentDirectory().getChildFile("profiles").getChildFile("targets");
    if (dir.isDirectory()) return dir;
    return {};
}

} // namespace

TEST_CASE("HITO-10E / E5 - 1. Fixture de Ausencia Controlada (Simulacion Hermetica)",
          "[targetprofile][legacy][absence]")
{
    juce::File realLegacyDir = getContractsHardwareDir();
    REQUIRE(realLegacyDir.isDirectory());

    juce::File canonicalTargetsDir = getCanonicalTargetsDir();
    REQUIRE(canonicalTargetsDir.isDirectory());

    // Crear directorio temporal de fixture aislada
    juce::File tempFixtureDir = juce::File::createTempFile("e5_absence_fixture");
    tempFixtureDir.deleteFile();
    tempFixtureDir.createDirectory();

    // Copiar todos los archivos del catálogo físico post-retirada (28 perfiles + 1 schema)
    auto realFiles = realLegacyDir.findChildFiles(juce::File::findFiles, false, "*.json");
    std::size_t copiedProfiles = 0;
    std::size_t copiedSchemas = 0;

    for (const auto& rf : realFiles)
    {
        std::string fname = rf.getFileName().toStdString();
        auto targetFile = tempFixtureDir.getChildFile(rf.getFileName());
        rf.copyFileTo(targetFile);

        if (fname == "hardware_profile.schema.json")
            copiedSchemas++;
        else
            copiedProfiles++;
    }

    // 1. Verificación de cardinalidad estricta en la fixture de ausencia
    CHECK(copiedSchemas == 1);
    CHECK(copiedProfiles == 28);
    CHECK(tempFixtureDir.findChildFiles(juce::File::findFiles, false, "*.json").size() == 29); // 28 perfiles + 1 schema

    // 2. Cargar el registro desde la fixture aislada (sin los 3 JSON migrados)
    HardwareContractRegistry registry;
    bool legacyLoaded = registry.loadContractsFromDirectory(tempFixtureDir);
    REQUIRE(legacyLoaded);
    CHECK(registry.getContracts().size() == 28);

    // 3. Cargar el catálogo canónico (all-or-nothing)
    auto canonicalLoadResult = registry.loadCanonicalTargetProfiles(canonicalTargetsDir);
    REQUIRE(canonicalLoadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    CHECK(registry.getCanonicalAdaptedContractCount() == 5);

    // 4. Verificación de resolución: Los 3 IDs homologados resuelven como CanonicalTargetProfileAdapted
    auto resPro = registry.resolveContractById("behringer_pro800");
    CHECK(resPro.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(resPro.contract.has_value());
    CHECK(resPro.contract->id == "hw-behringer-pro800-canonical");

    auto resDx7 = registry.resolveContractById("yamaha_dx7");
    CHECK(resDx7.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(resDx7.contract.has_value());
    CHECK(resDx7.contract->id == "hw-yamaha-dx7-canonical");

    auto resDs1 = registry.resolveContractById("boss_ds1_distortion");
    CHECK(resDs1.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(resDs1.contract.has_value());
    CHECK(resDs1.contract->id == "hw-boss-ds1-canonical");

    // 5. Los perfiles no migrados siguen resolviéndose como NativeLegacyContract
    auto resCz101 = registry.resolveContractById("casio_cz101");
    CHECK(resCz101.source == HardwareContractResolutionSource::NativeLegacyContract);
    REQUIRE(resCz101.contract.has_value());
    CHECK(resCz101.contract->id == "casio_cz101");

    auto resJuno = registry.resolveContractById("roland_juno106");
    CHECK(resJuno.source == HardwareContractResolutionSource::NativeLegacyContract);
    REQUIRE(resJuno.contract.has_value());

    auto resDeepmind = registry.resolveContractById("behringer_deepmind12");
    CHECK(resDeepmind.source == HardwareContractResolutionSource::NativeLegacyContract);
    REQUIRE(resDeepmind.contract.has_value());

    // 6. Objetivos no existentes resuelven explícitamente NotFound
    auto resNonExistent = registry.resolveContractById("non_existent_target");
    CHECK(resNonExistent.source == HardwareContractResolutionSource::NotFound);
    CHECK_FALSE(resNonExistent.contract.has_value());

    // 7. Carga idempotente: re-ejecutar loadCanonicalTargetProfiles no altera conteos
    auto reloadResult = registry.loadCanonicalTargetProfiles(canonicalTargetsDir);
    CHECK(reloadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    CHECK(registry.getCanonicalAdaptedContractCount() == 5);

    // Limpieza hermética de la fixture temporal (0 modificaciones en árbol real)
    tempFixtureDir.deleteRecursively();
}
