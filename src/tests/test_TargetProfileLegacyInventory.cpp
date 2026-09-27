// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy Inventory & Safety Gate
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"

#include <string>
#include <vector>
#include <unordered_set>

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

juce::File getContractsHardwareDir()
{
    juce::File current = juce::File::getCurrentWorkingDirectory();
    auto dir = current.getChildFile("contracts").getChildFile("hardware");
    if (dir.isDirectory()) return dir;

    // Fallback hacia raíz si se corre desde build
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

        if (fname == "hardware_profile.schema.json")
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

    // Validación formal de las igualdades métricas de inventario (HITO-10E / E6 Post-Retirement)
    REQUIRE(legacyProfileDocumentCount == migratedLegacyProfileCount + unmigratedLegacyProfileCount);
    REQUIRE(legacyFilesystemEntryCount == legacyProfileDocumentCount + legacySchemaDocumentCount);

    CHECK(legacyFilesystemEntryCount == 29);
    CHECK(legacySchemaDocumentCount == 1);
    CHECK(legacyProfileDocumentCount == 28);
    CHECK(migratedLegacyProfileCount == 0);
    CHECK(unmigratedLegacyProfileCount == 28);

    // 1. Schema legacy obligatorio
    CHECK(fileNames.count("hardware_profile.schema.json") == 1);

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

    // El registry físico contiene exactamente 28 perfiles (el schema se omite metrológicamente)
    CHECK(registry.getContracts().size() == 28);

    // Tras cargar el catálogo canónico (5 perfiles), la suite efectiva alcanza 33 contratos
    auto canonicalRes = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(canonicalRes.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    CHECK(registry.getContracts().size() == 33);

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
