// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy Controlled Absence Tests (E5)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

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

    // Copiar TODOS los ficheros del catálogo físico post-retirada. No se copia una
    // lista: se copia el directorio entero y se cuenta lo que sale, para que este
    // test no pueda quedarse verde con un snapshot a medias.
    auto realFiles = realLegacyDir.findChildFiles(juce::File::findFiles, false, "*.json");
    std::size_t copiedProfiles = 0;
    std::size_t copiedSchemas = 0;

    for (const auto& rf : realFiles)
    {
        std::string fname = rf.getFileName().toStdString();
        auto targetFile = tempFixtureDir.getChildFile(rf.getFileName());
        rf.copyFileTo(targetFile);

        if (fname.ends_with(".schema.json"))
            copiedSchemas++;
        else
            copiedProfiles++;
    }

    // 1. Verificación de cardinalidad estricta en la fixture de ausencia (34 perfiles + 6 schemas)
    //
    // Los 6 schemas son los 5 de siempre mas el del patch_spec del AIRA Modular, que
    // entra al sincronizar el snapshot con ABDSharedAssets/contracts. Los perfiles se
    // quedan en 34: el renombre min/max/default -> minVal/maxVal/defaultVal cambia
    // el contenido de seis de ellos, no el numero.
    CHECK(copiedSchemas == 6);
    CHECK(copiedProfiles == 34);
    CHECK(tempFixtureDir.findChildFiles(juce::File::findFiles, false, "*.json").size() == 40); // 34 perfiles + 6 schemas

    // 2. Cargar el registro desde la fixture aislada (sin los 3 JSON migrados)
    HardwareContractRegistry registry;
    bool legacyLoaded = registry.loadContractsFromDirectory(tempFixtureDir);
    REQUIRE(legacyLoaded);
    // 30 < 34, y son dos razones distintas que se suman:
    //
    //   - el registry descarta los JSON que no son contratos, que son los tres
    //     sin `displayName`: fx-effects, s950_calibration y s950_patch_fields.
    //     Las tres matrices de modulacion si tienen `id` y `displayName`, asi que
    //     si se cargan.
    //   - y RETIENE uno que si es contrato: `roland_aira_submodules`, que lleva
    //     `status: "quarantined"` en el propio fichero. Su contenido no es el
    //     catalogo de 31 modulos que dice su cabecera —solo 7 de 31 coinciden con
    //     el patch_spec del AIRA— asi que no se carga y no sale en el cajon de
    //     hardware. Ver `getQuarantinedProfiles()`.
    CHECK(registry.getContracts().size() == 30);

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
