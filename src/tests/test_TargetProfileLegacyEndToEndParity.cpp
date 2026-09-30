// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy End-to-End Parity Tests (E5)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "core/HardwareManager.h"
#include "export/ModulationPresetExporter.h"
#include "profiling/TargetProfileService.h"
#include "profiling/TargetProfileLegacyAdapter.h"

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

} // namespace

TEST_CASE("HITO-10E / E5 - 1. E2E Parity: Route A (Legacy) vs Route B (Canonical Adapted)",
          "[targetprofile][legacy][e2e][parity]")
{
    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));
    auto loadResult = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(loadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // --------------------------------------------------------------------------
    // 1. Behringer PRO-800 Parity
    // --------------------------------------------------------------------------
    SECTION("PRO-800 Invariant Preservation")
    {
        auto resolution = registry.resolveContractById("behringer_pro800");
        REQUIRE(resolution.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
        REQUIRE(resolution.contract.has_value());
        const auto& adapted = *resolution.contract;

        CHECK(adapted.deviceType == "AUTOMATED_MIDI_CC");
        CHECK(adapted.manufacturer == "Behringer");

        // Validar preservación de CC 19 para Filter Cutoff
        bool foundCutoff = false;
        for (const auto& func : adapted.functions)
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
    }

    // --------------------------------------------------------------------------
    // 2. Yamaha DX7 Parity
    // --------------------------------------------------------------------------
    SECTION("Yamaha DX7 Invariant Preservation")
    {
        auto resolution = registry.resolveContractById("yamaha_dx7");
        REQUIRE(resolution.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
        REQUIRE(resolution.contract.has_value());
        const auto& adapted = *resolution.contract;

        CHECK(adapted.deviceType == "AUTOMATED_SYSEX");
        CHECK(adapted.displayName.find("Yamaha DX7") != std::string::npos);

        // Validar framing y template SysEx
        bool foundAlgorithm = false;
        for (const auto& func : adapted.functions)
        {
            for (const auto& ctrl : func.controls)
            {
                if (ctrl.name == "Algorithm Select")
                {
                    foundAlgorithm = true;
                    CHECK(ctrl.controlMethod == "SYSEX");
                    CHECK(ctrl.sysexAddress.rfind("F0 43", 0) == 0); // Framing fabricante Yamaha
                    CHECK(ctrl.sysexAddress.find("F7") != std::string::npos);
                }
            }
        }
        CHECK(foundAlgorithm);
    }

    // --------------------------------------------------------------------------
    // 3. BOSS DS-1 Parity
    // --------------------------------------------------------------------------
    SECTION("BOSS DS-1 Invariant Preservation")
    {
        auto resolution = registry.resolveContractById("boss_ds1_distortion");
        REQUIRE(resolution.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
        REQUIRE(resolution.contract.has_value());
        const auto& adapted = *resolution.contract;

        CHECK(adapted.deviceType == "ANALOGUE_PEDAL");
        CHECK(adapted.displayName.find("BOSS DS-1") != std::string::npos);

        // Validar control manual DIST
        bool foundDist = false;
        for (const auto& func : adapted.functions)
        {
            for (const auto& ctrl : func.controls)
            {
                if (ctrl.name == "DIST Potentiometer")
                {
                    foundDist = true;
                    CHECK(ctrl.controlMethod == "MANUAL");
                    CHECK(ctrl.type == "Knob");
                }
            }
        }
        CHECK(foundDist);
    }
}

TEST_CASE("HITO-10E / E5 - 2. Direct Consumers Behavioral Equivalence",
          "[targetprofile][legacy][e2e][parity]")
{
    // 1. Verificación en HardwareManager
    HardwareManager hwManager;
    hwManager.getContractRegistry().loadContractsFromDirectory(getContractsHardwareDir());
    auto resLoad = hwManager.getContractRegistry().loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(resLoad.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // Consumo vía findContractById transparente
    const auto* pro800Contract = hwManager.findContractById("behringer_pro800");
    REQUIRE(pro800Contract != nullptr);
    CHECK(pro800Contract->id == "hw-behringer-pro800-canonical");

    const auto* dx7Contract = hwManager.findContractById("yamaha_dx7");
    REQUIRE(dx7Contract != nullptr);
    CHECK(dx7Contract->id == "hw-yamaha-dx7-canonical");

    const auto* cz101Contract = hwManager.findContractById("casio_cz101");
    REQUIRE(cz101Contract != nullptr);
    CHECK(cz101Contract->id == "casio_cz101"); // Resuelve legacy nativo

    // 2. Verificación en ModulationPresetExporter
    abdaudiolab::exporting::ModulationPresetExporter exporter(hwManager.getContractRegistry());
    // El exporter consulta la registry sin excepciones
    CHECK(hwManager.getContractRegistry().hasContracts());
    auto resolvedPro = hwManager.getContractRegistry().resolveContractById("behringer_pro800");
    CHECK(resolvedPro.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
}
