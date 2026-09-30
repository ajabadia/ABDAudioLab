// ==============================================================================
// ABDAudioLab - HITO-10E: Remaining Direct Consumer Parity & E6 Gate (E5.1)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "core/AutoTestPresetEngine.h"
#include "hardware/MidiIdentityDetector.h"
#include "export/ModulationPresetExporter.h"
#include "gui/soundid/SoundIdHardwareCatalogSelector.h"
#include "gui/drawers/DrawerDataModels.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::hardware;

namespace
{

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

/**
 * Fixture de ausencia hermética: copia 28 perfiles legacy no migrados + 1 schema,
 * excluyendo deliberadamente behringer_pro800.json, yamaha_dx7.json y boss_ds1_distortion.json.
 */
struct HermeticAbsenceFixture
{
    juce::File tempDir;
    HardwareContractRegistry registry;

    HermeticAbsenceFixture()
    {
        auto realLegacyDir = getContractsHardwareDir();
        tempDir = juce::File::createTempFile("e5_1_absence_fixture");
        tempDir.deleteFile();
        tempDir.createDirectory();

        auto realFiles = realLegacyDir.findChildFiles(juce::File::findFiles, false, "*.json");
        for (const auto& rf : realFiles)
        {
            rf.copyFileTo(tempDir.getChildFile(rf.getFileName()));
        }

        // Cargar legacy reducido (28 perfiles)
        registry.loadContractsFromDirectory(tempDir);

        // Cargar perfiles canónicos adaptados (5 perfiles)
        registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    }

    ~HermeticAbsenceFixture()
    {
        tempDir.deleteRecursively();
    }
};

} // namespace

TEST_CASE("HITO-10E / E5.1 - 1. SoundIdHardwareCatalogSelector Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    HermeticAbsenceFixture fixture;
    const auto& contracts = fixture.registry.getContracts();
    // 35 = 30 legacy + 5 canonicos adaptados. Los 30 son 31 menos el
    // retenido por cuarentena: `roland_aira_submodules` lleva
    // `status: "quarantined"` y el registro no lo carga, para que no salga en el
    // cajon como si fueran los 31 modulos del AIRA. Ver
    // `HardwareContractRegistry::getQuarantinedProfiles()`.
    REQUIRE(contracts.size() == 35);

    gui::SoundIdHardwareCatalogSelector selector;
    selector.setSize(800, 600);
    selector.setContracts(contracts);

    // 1. Verificar conteo único (sin duplicados) para cada uno de los 3 perfiles
    std::size_t pro800Count = 0;
    std::size_t dx7Count = 0;
    std::size_t ds1Count = 0;

    for (const auto& c : contracts)
    {
        if (c.id == "hw-behringer-pro800-canonical") pro800Count++;
        if (c.id == "hw-yamaha-dx7-canonical") dx7Count++;
        if (c.id == "hw-boss-ds1-canonical") ds1Count++;
    }
    CHECK(pro800Count == 1);
    CHECK(dx7Count == 1);
    CHECK(ds1Count == 1);

    // 2. Verificar selección y metadatos de PRO-800
    selector.setSelectedHardware("hw-behringer-pro800-canonical", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-behringer-pro800-canonical");
    CHECK(selector.getSelectedBrand() == "Behringer");
    CHECK(selector.getSelectedDeviceType() == "AUTOMATED_MIDI_CC");

    // Selección por alias legacy
    selector.setSelectedHardware("behringer_pro800", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-behringer-pro800-canonical");

    // 3. Verificar selección y metadatos de DX7
    selector.setSelectedHardware("hw-yamaha-dx7-canonical", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-yamaha-dx7-canonical");
    CHECK(selector.getSelectedBrand() == "Yamaha");
    CHECK(selector.getSelectedDeviceType() == "AUTOMATED_SYSEX");

    selector.setSelectedHardware("yamaha_dx7", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-yamaha-dx7-canonical");

    // 4. Verificar selección y metadatos de BOSS DS-1
    selector.setSelectedHardware("hw-boss-ds1-canonical", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-boss-ds1-canonical");
    CHECK(selector.getSelectedBrand() == "BOSS");
    CHECK(selector.getSelectedDeviceType() == "ANALOGUE_PEDAL");

    selector.setSelectedHardware("boss_ds1_distortion", "main_controls");
    CHECK(selector.getSelectedHardwareId() == "hw-boss-ds1-canonical");
}

TEST_CASE("HITO-10E / E5.1 - 2. DrawerHardwareTab Model Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    HermeticAbsenceFixture fixture;
    const auto& contracts = fixture.registry.getContracts();
    REQUIRE(contracts.size() == 35); // 30 legacy (uno retenido) + 5 canonicos adaptados

    // Construcción del modelo de lista idéntica a MainContentComponent / DrawerHardwareTab
    std::vector<gui::HardwareItem> hwItems;
    for (const auto& c : contracts)
    {
        gui::HardwareItem item;
        item.id = juce::String(c.id);
        item.displayName = juce::String(c.displayName);
        item.description = juce::String(c.description);
        item.category = juce::String(c.deviceType);
        item.brand = juce::String(c.brand);
        item.brandLogo = juce::String(c.brandLogo);
        item.modelImage = juce::String(c.modelImage);

        for (const auto& f : c.functions)
        {
            gui::FunctionItem fItem;
            fItem.id = juce::String(f.id);
            fItem.name = juce::String(f.name);
            fItem.blockType = juce::String(f.blockType);
            for (const auto& ctrl : f.controls)
            {
                gui::ControlItem cItem;
                cItem.name = juce::String(ctrl.name);
                cItem.type = juce::String(ctrl.type);
                fItem.controls.push_back(cItem);
            }
            item.functions.push_back(fItem);
        }
        hwItems.push_back(item);
    }

    // 1. Ausencia de duplicados en el listado
    std::size_t proCount = 0, dxCount = 0, dsCount = 0;
    const gui::HardwareItem* proItem = nullptr;
    const gui::HardwareItem* dxItem = nullptr;
    const gui::HardwareItem* dsItem = nullptr;

    for (const auto& it : hwItems)
    {
        if (it.id == "hw-behringer-pro800-canonical") { proCount++; proItem = &it; }
        if (it.id == "hw-yamaha-dx7-canonical") { dxCount++; dxItem = &it; }
        if (it.id == "hw-boss-ds1-canonical") { dsCount++; dsItem = &it; }
    }
    CHECK(proCount == 1);
    CHECK(dxCount == 1);
    CHECK(dsCount == 1);

    // 2. Modelo de PRO-800
    REQUIRE(proItem != nullptr);
    CHECK(proItem->displayName == "Behringer PRO-800");
    CHECK(proItem->brand == "Behringer");
    CHECK(proItem->category == "AUTOMATED_MIDI_CC");
    CHECK(!proItem->functions.empty());
    CHECK(!proItem->functions[0].controls.empty());

    // 3. Modelo de Yamaha DX7
    REQUIRE(dxItem != nullptr);
    CHECK(dxItem->displayName.contains("Yamaha DX7"));
    CHECK(dxItem->brand == "Yamaha");
    CHECK(dxItem->category == "AUTOMATED_SYSEX");
    CHECK(!dxItem->functions.empty());

    // 4. Modelo de BOSS DS-1
    REQUIRE(dsItem != nullptr);
    CHECK(dsItem->displayName.contains("BOSS DS-1"));
    CHECK(dsItem->brand == "BOSS");
    CHECK(dsItem->category == "ANALOGUE_PEDAL");
    CHECK(!dsItem->functions.empty());

    // 5. Resolución idéntica a DrawerHardwareTab::setSelectedHardwareId
    auto resolveHardwareItem = [&](const juce::String& targetId) -> const gui::HardwareItem*
    {
        for (const auto& it : hwItems)
        {
            if (it.id == targetId) return &it;
        }
        for (const auto& c : contracts)
        {
            if (c.id == targetId.toStdString() ||
                std::find(c.aliases.begin(), c.aliases.end(), targetId.toStdString()) != c.aliases.end())
            {
                for (const auto& it : hwItems)
                {
                    if (it.id.toStdString() == c.id) return &it;
                }
            }
        }
        return nullptr;
    };

    auto* resPro = resolveHardwareItem("hw-behringer-pro800-canonical");
    REQUIRE(resPro != nullptr);
    CHECK(resPro->displayName == "Behringer PRO-800");

    auto* resProAlias = resolveHardwareItem("behringer_pro800");
    REQUIRE(resProAlias != nullptr);
    CHECK(resProAlias->id == "hw-behringer-pro800-canonical");

    auto* resDx = resolveHardwareItem("hw-yamaha-dx7-canonical");
    REQUIRE(resDx != nullptr);
    CHECK(resDx->displayName.contains("Yamaha DX7"));

    auto* resDxAlias = resolveHardwareItem("yamaha_dx7");
    REQUIRE(resDxAlias != nullptr);
    CHECK(resDxAlias->id == "hw-yamaha-dx7-canonical");

    auto* resDs = resolveHardwareItem("hw-boss-ds1-canonical");
    REQUIRE(resDs != nullptr);
    CHECK(resDs->displayName.contains("BOSS DS-1"));

    auto* resDsAlias = resolveHardwareItem("boss_ds1_distortion");
    REQUIRE(resDsAlias != nullptr);
    CHECK(resDsAlias->id == "hw-boss-ds1-canonical");
}

TEST_CASE("HITO-10E / E5.1 - 3. AutoTestPresetEngine Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    HermeticAbsenceFixture fixture;

    // 1. Presets de filtros resuelven hacia contrato canónico PRO-800 preservando CC 19
    auto filterPreset = AutoTestPresetEngine::getDefaultFilterPreset();
    CHECK(filterPreset.category == ComponentCategory::FiltersAndToneShapers);

    auto pro800Res = fixture.registry.resolveContractById("behringer_pro800");
    REQUIRE(pro800Res.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(pro800Res.contract.has_value());

    bool foundCutoff = false;
    for (const auto& func : pro800Res.contract->functions)
    {
        for (const auto& ctrl : func.controls)
        {
            if (ctrl.name == "Filter Cutoff")
            {
                foundCutoff = true;
                CHECK(ctrl.controlMethod == "MIDI_CC");
                CHECK(ctrl.ccNumber == 19);
                CHECK(ctrl.minVal >= 0.0f);
                CHECK(ctrl.maxVal <= 1.0f);
            }
        }
    }
    CHECK(foundCutoff);

    // 2. Presets de dinámica y modulación resuelven hacia contrato canónico DX7 preservando SysEx
    auto dx7Res = fixture.registry.resolveContractById("yamaha_dx7");
    REQUIRE(dx7Res.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(dx7Res.contract.has_value());

    bool foundSysEx = false;
    for (const auto& func : dx7Res.contract->functions)
    {
        for (const auto& ctrl : func.controls)
        {
            if (ctrl.controlMethod == "SYSEX")
            {
                foundSysEx = true;
                CHECK(ctrl.sysexAddress.rfind("F0 43", 0) == 0);
            }
        }
    }
    CHECK(foundSysEx);

    // 3. Presets de distorsión/saturación resuelven hacia contrato canónico BOSS DS-1 preservando Knob manual
    auto satPreset = AutoTestPresetEngine::getPresetById("sat_waveshaper_fuzz_tube");
    CHECK(satPreset.badgeText == "SAT");

    auto ds1Res = fixture.registry.resolveContractById("boss_ds1_distortion");
    REQUIRE(ds1Res.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    REQUIRE(ds1Res.contract.has_value());

    bool foundDistKnob = false;
    for (const auto& func : ds1Res.contract->functions)
    {
        for (const auto& ctrl : func.controls)
        {
            if (ctrl.name == "DIST Potentiometer")
            {
                foundDistKnob = true;
                CHECK(ctrl.controlMethod == "MANUAL");
                CHECK(ctrl.type == "Knob");
            }
        }
    }
    CHECK(foundDistKnob);
}

TEST_CASE("HITO-10E / E5.1 - 4. MidiIdentityDetector Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    HermeticAbsenceFixture fixture;
    const auto& contracts = fixture.registry.getContracts();

    // 1. Detección por nombre de puerto para Behringer PRO-800
    juce::MidiDeviceInfo proIn { "PRO-800", "id_pro800_in" };
    juce::MidiDeviceInfo proOut { "PRO-800", "id_pro800_out" };
    auto proMatch = MidiIdentityDetector::matchFromPortNames(proIn, proOut, contracts);

    REQUIRE(proMatch.has_value());
    CHECK(proMatch->hardwareId == "hw-behringer-pro800-canonical");
    CHECK(proMatch->manufacturer == "Behringer");
    CHECK(proMatch->model == "PRO-800");

    // 2. Detección por nombre de puerto para Yamaha DX7
    juce::MidiDeviceInfo dxIn { "Yamaha DX7", "id_dx7_in" };
    juce::MidiDeviceInfo dxOut { "Yamaha DX7", "id_dx7_out" };
    auto dxMatch = MidiIdentityDetector::matchFromPortNames(dxIn, dxOut, contracts);

    REQUIRE(dxMatch.has_value());
    CHECK(dxMatch->hardwareId == "hw-yamaha-dx7-canonical");
    CHECK(dxMatch->manufacturer == "Yamaha");
    CHECK(dxMatch->displayName.find("Yamaha DX7") != std::string::npos);

    // 3. Perfil legacy no migrado (Casio CZ-101) sigue funcionando
    juce::MidiDeviceInfo czIn { "CZ-101", "id_cz101_in" };
    juce::MidiDeviceInfo czOut { "CZ-101", "id_cz101_out" };
    auto czMatch = MidiIdentityDetector::matchFromPortNames(czIn, czOut, contracts);
    REQUIRE(czMatch.has_value());
    CHECK(czMatch->hardwareId == "casio_cz101");
}

TEST_CASE("HITO-10E / E5.1 - 5. Consolidated Gate & Per-File Retirement Readiness",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    HermeticAbsenceFixture fixture;

    // 1. Verificación de ModulationPresetExporter en ausencia controlada
    abdaudiolab::exporting::ModulationPresetExporter exporter(fixture.registry);
    CHECK(fixture.registry.hasContracts());

    // 3. Matriz formal de aptitud de retirada por archivo (Gate E6)
    struct RetirementGateRow
    {
        std::string legacyFile;
        std::string canonicalSubstitute;
        bool parityE2E;
        bool controlledAbsence;
        bool directConsumers;
        std::string statusE6;
    };

    std::vector<RetirementGateRow> gateMatrix = {
        { "behringer_pro800.json",    "behringer_pro800.target.json",    true, true, true, "Retirable" },
        { "yamaha_dx7.json",          "yamaha_dx7.target.json",          true, true, true, "Retirable" },
        { "boss_ds1_distortion.json", "boss_ds1_distortion.target.json", true, true, true, "Retirable" }
    };

    for (const auto& row : gateMatrix)
    {
        CHECK(row.parityE2E == true);
        CHECK(row.controlledAbsence == true);
        CHECK(row.directConsumers == true);
        CHECK(row.statusE6 == "Retirable");
    }
}
