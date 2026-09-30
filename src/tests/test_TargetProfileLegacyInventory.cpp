// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy Inventory & Safety Gate
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"

#include <string>
#include <vector>
#include <unordered_set>

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

} // namespace

TEST_CASE("HITO-10E - 1. Legacy Contracts Directory Inventory and Non-Deletion Guard",
          "[targetprofile][legacy][inventory]")
{
    juce::File legacyDir = getContractsHardwareDir();
    REQUIRE(legacyDir.isDirectory());

    auto jsonFiles = legacyDir.findChildFiles(juce::File::findFiles, false, "*.json");

    std::size_t legacyFilesystemEntryCount = static_cast<std::size_t>(jsonFiles.size());
    std::size_t legacySchemaDocumentCount = 0;
    std::size_t migratedLegacyProfileCount = 0;
    std::size_t unmigratedLegacyProfileCount = 0;

    std::unordered_set<std::string> fileNames;
    for (const auto& f : jsonFiles)
    {
        std::string fname = f.getFileName().toStdString();
        fileNames.insert(fname);

        if (fname.ends_with(".schema.json"))
        {
            legacySchemaDocumentCount++;
        }
        else if (fname == "behringer_pro800.json" || fname == "yamaha_dx7.json" || fname == "boss_ds1_distortion.json")
        {
            migratedLegacyProfileCount++;
        }
        else
        {
            unmigratedLegacyProfileCount++;
        }
    }

    std::size_t legacyProfileDocumentCount = migratedLegacyProfileCount + unmigratedLegacyProfileCount;

    // Validación formal de las igualdades métricas de inventario (HITO-10E / E6 Post-Retirement + ABDSharedAssets sync)
    REQUIRE(legacyProfileDocumentCount == migratedLegacyProfileCount + unmigratedLegacyProfileCount);
    REQUIRE(legacyFilesystemEntryCount == legacyProfileDocumentCount + legacySchemaDocumentCount);

    // Catálogo físico versionado en contracts/hardware/ (40 = 34 perfiles + 6 schemas).
    //
    // Los schemas pasaron de 5 a 6 al sincronizar el snapshot con
    // ABDSharedAssets/contracts, que trae un esquema propio del patch_spec del AIRA
    // Modular (roland_aira_patch_spec.schema.json). Ese esquema se escribió porque el
    // patch_spec se leía entero sin mirar: las partes que se inventan sus propias
    // claves —`devices` y `protocol`— pasaban sin comprobar, y al cerrarlo
    // aparecieron cuatro campos que el contrato tenía y nadie declaraba.
    //
    // Los perfiles se quedan en 34: el nombre único del rango de un control pasó de
    // `min`/`max`/`default` a `minVal`/`maxVal`/`defaultVal` en seis ficheros, lo que
    // cambia el CONTENIDO de seis perfiles y no el número. Ese cambio obligó a tocar
    // los dos parsers de C++ (HardwareContractRegistry y SharedHardwareContractAdapter),
    // porque leían el nombre corto y con el nuevo caerían al 0.0/1.0/0.5 por defecto
    // en silencio: 68 controles tienen un defaultVal distinto de 0.5.
    CHECK(legacyFilesystemEntryCount == 40);
    CHECK(legacySchemaDocumentCount == 6);
    CHECK(legacyProfileDocumentCount == 34);
    CHECK(migratedLegacyProfileCount == 0);
    CHECK(unmigratedLegacyProfileCount == 34);

    // 1. Schemas obligatorios
    //
    // Cada uno se nombra uno a uno, y no se comprueba "hay 6 schemas" a secas: el
    // recuento de arriba dice CUÁNTOS hay, y esto dice CUÁLES. Un esquema que se
    // perdiera lo diría el primero; uno que se colara de más, el segundo. Y el
    // patch_spec del AIRA se nombra aparte porque es el único que C++ no lee: no hay
    // ningún .cpp que parsee `devices`, `protocol` ni `submodules`, así que su
    // entrada en el catálogo no la carga nadie todavía. Sigue siendo un contrato que
    // tiene que estar ahí —es el índice de la máquina— pero que solo lo vigila el
    // lado de ABDSharedAssets.
    CHECK(fileNames.count("hardware_profile.schema.json") == 1);
    CHECK(fileNames.count("modulation_matrix.schema.json") == 1);
    CHECK(fileNames.count("fx-effects.schema.json") == 1);
    CHECK(fileNames.count("s950-calibration.schema.json") == 1);
    CHECK(fileNames.count("s950-patch-fields.schema.json") == 1);
    CHECK(fileNames.count("roland_aira_patch_spec.schema.json") == 1);

    // 2. Los 3 targets homologados retirados físicamente de contracts/hardware/ en E6
    CHECK(fileNames.count("behringer_pro800.json") == 0);
    CHECK(fileNames.count("yamaha_dx7.json") == 0);
    CHECK(fileNames.count("boss_ds1_distortion.json") == 0);

    // 3. Salvaguarda estricta: Los 28 perfiles no migrados DEBEN seguir existiendo físicamente
    CHECK(fileNames.count("casio_cz101.json") == 1);
    CHECK(fileNames.count("roland_juno106.json") == 1);
    CHECK(fileNames.count("roland_juno60.json") == 1);
    CHECK(fileNames.count("behringer_deepmind12.json") == 1);
    CHECK(fileNames.count("korg_ms2000.json") == 1);
    CHECK(fileNames.count("korg_microkorg.json") == 1);
    CHECK(fileNames.count("roland_aira_bitrazer.json") == 1);
    CHECK(fileNames.count("manual_eurorack_vcf.json") == 1);
}

TEST_CASE("HITO-10E - 2. Canonical TargetProfile Directory Inventory",
          "[targetprofile][legacy][inventory]")
{
    juce::File canonicalDir = getCanonicalTargetsDir();
    REQUIRE(canonicalDir.isDirectory());

    auto targetFiles = canonicalDir.findChildFiles(juce::File::findFiles, false, "*.target.json");

    std::size_t canonicalTargetProfileCount = static_cast<std::size_t>(targetFiles.size());
    CHECK(canonicalTargetProfileCount == 5);

    std::unordered_set<std::string> targetNames;
    for (const auto& f : targetFiles)
        targetNames.insert(f.getFileName().toStdString());

    CHECK(targetNames.count("reference_synth.target.json") == 1);
    CHECK(targetNames.count("dexed.target.json") == 1);
    CHECK(targetNames.count("behringer_pro800.target.json") == 1);
    CHECK(targetNames.count("yamaha_dx7.target.json") == 1);
    CHECK(targetNames.count("boss_ds1_distortion.target.json") == 1);
}

TEST_CASE("HITO-10E - 3. Legacy Registry Coexistence and Resolution Gate Post-Retirement",
          "[targetprofile][legacy][inventory]")
{
    juce::File legacyDir = getContractsHardwareDir();
    REQUIRE(legacyDir.isDirectory());

    HardwareContractRegistry registry;
    bool loaded = registry.loadContractsFromDirectory(legacyDir);
    REQUIRE(loaded);
    REQUIRE(registry.hasContracts());

    // El registry físico contiene los perfiles válidos (los schemas se omiten).
    // 30 < 34, y son dos razones distintas, que se suman:
    //
    //   - el registry descarta los JSON que no son contratos: las tres matrices
    //     de modulación, fx-effects, s950_calibration y s950_patch_fields;
    //   - y RETIENE uno que sí es contrato: `roland_aira_submodules`, que lleva
    //     `status: "quarantined"` en el propio fichero. Su contenido no es el
    //     catálogo de 31 módulos que dice su cabecera —solo 7 de 31 coinciden
    //     con el patch_spec del AIRA— así que no se carga y no aparece en el
    //     cajón de hardware. Ver `getQuarantinedProfiles()`.
    CHECK(registry.getContracts().size() == 30);

    // Tras cargar el catálogo canónico (5 perfiles), la suite efectiva alcanza 36 contratos
    auto canonicalRes = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(canonicalRes.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    CHECK(registry.getContracts().size() == 35);

    // Targets retirados de contracts/hardware/ resuelven desde TargetProfile canónico con compatibilidad histórica
    auto resPro = registry.resolveContractById("behringer_pro800");
    CHECK(resPro.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    CHECK(registry.findContractById("behringer_pro800") != nullptr);
    CHECK(registry.findContractById("yamaha_dx7") != nullptr);
    CHECK(registry.findContractById("boss_ds1_distortion") != nullptr);

    // Targets no migrados siguen resolviéndose como contratos nativos sin degradación
    auto resCz = registry.resolveContractById("casio_cz101");
    CHECK(resCz.source == HardwareContractResolutionSource::NativeLegacyContract);
    CHECK(registry.findContractById("casio_cz101") != nullptr);
    CHECK(registry.findContractById("roland_juno106") != nullptr);
    CHECK(registry.findContractById("behringer_deepmind12") != nullptr);
}
