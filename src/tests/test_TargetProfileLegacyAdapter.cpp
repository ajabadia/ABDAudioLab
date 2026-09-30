// ==============================================================================
// ABDAudioLab - HITO-10E: TargetProfile Legacy Adapter Compatibility Tests
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "profiling/TargetProfileService.h"
#include "profiling/TargetProfileLegacyAdapter.h"
#include "core/HardwareContractRegistry.h"

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

} // namespace

TEST_CASE("HITO-10E - 4. TargetProfile to Legacy HardwareContract Projection (E3)",
          "[targetprofile][legacy][adapter]")
{
    TargetProfileService service;
    juce::File targetsDir = getCanonicalTargetsDir();
    REQUIRE(targetsDir.isDirectory());

    // 1. Caso Behringer PRO-800 (MIDI CC)
    auto pro800File = targetsDir.getChildFile("behringer_pro800.target.json");
    REQUIRE(pro800File.existsAsFile());
    auto resPro800 = service.loadAndValidateProfile(pro800File);
    REQUIRE(resPro800.isSuccess());

    HardwareContract legacyPro800 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resPro800.profile);
    CHECK(legacyPro800.id == "hw-behringer-pro800-canonical");
    CHECK(legacyPro800.displayName == "Behringer PRO-800");
    CHECK(legacyPro800.manufacturer == "Behringer");
    CHECK(legacyPro800.deviceType == "AUTOMATED_MIDI_CC");
    REQUIRE_FALSE(legacyPro800.functions.empty());
    CHECK_FALSE(legacyPro800.functions[0].controls.empty());

    // Verificar que un control CC se proyectó correctamente
    bool foundCutoff = false;
    for (const auto& ctrl : legacyPro800.functions[0].controls)
    {
        if (ctrl.name == "Filter Cutoff")
        {
            foundCutoff = true;
            CHECK(ctrl.controlMethod == "MIDI_CC");
            CHECK(ctrl.ccNumber == 19);
        }
    }
    CHECK(foundCutoff);

    // 2. Caso Yamaha DX7 (SysEx)
    auto dx7File = targetsDir.getChildFile("yamaha_dx7.target.json");
    REQUIRE(dx7File.existsAsFile());
    auto resDx7 = service.loadAndValidateProfile(dx7File);
    REQUIRE(resDx7.isSuccess());

    HardwareContract legacyDx7 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resDx7.profile);
    CHECK(legacyDx7.id == "hw-yamaha-dx7-canonical");
    CHECK(legacyDx7.displayName == "Yamaha DX7 (Mark I)");
    CHECK(legacyDx7.deviceType == "AUTOMATED_SYSEX");
    REQUIRE_FALSE(legacyDx7.functions.empty());

    bool foundAlgorithm = false;
    for (const auto& ctrl : legacyDx7.functions[0].controls)
    {
        if (ctrl.name == "Algorithm Select")
        {
            foundAlgorithm = true;
            CHECK(ctrl.controlMethod == "SYSEX");
            CHECK_FALSE(ctrl.sysexAddress.empty());
        }
    }
    CHECK(foundAlgorithm);

    // 3. Caso BOSS DS-1 (Manual Analog Operator)
    auto ds1File = targetsDir.getChildFile("boss_ds1_distortion.target.json");
    REQUIRE(ds1File.existsAsFile());
    auto resDs1 = service.loadAndValidateProfile(ds1File);
    REQUIRE(resDs1.isSuccess());

    HardwareContract legacyDs1 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resDs1.profile);
    CHECK(legacyDs1.id == "hw-boss-ds1-canonical");
    CHECK(legacyDs1.displayName == "BOSS DS-1 Distortion (Analogue Pedal)");
    CHECK(legacyDs1.deviceType == "ANALOGUE_PEDAL");
    REQUIRE_FALSE(legacyDs1.functions.empty());

    bool foundDist = false;
    for (const auto& ctrl : legacyDs1.functions[0].controls)
    {
        if (ctrl.name == "DIST Potentiometer")
        {
            foundDist = true;
            CHECK(ctrl.controlMethod == "MANUAL");
            CHECK(ctrl.type == "Knob");
        }
    }
    CHECK(foundDist);
}

TEST_CASE("HITO-10E - Parity and Invariant Preservation for Certified Profiles (E3)",
          "[targetprofile][legacy][adapter][parity]")
{
    TargetProfileService service;
    juce::File targetsDir = getCanonicalTargetsDir();
    REQUIRE(targetsDir.isDirectory());

    juce::File legacyDir = getContractsHardwareDir();
    REQUIRE(legacyDir.isDirectory());

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(legacyDir));
    auto canonicalLoad = registry.loadCanonicalTargetProfiles(targetsDir);
    REQUIRE(canonicalLoad.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // 1. Paridad PRO-800
    auto pro800CanonicalFile = targetsDir.getChildFile("behringer_pro800.target.json");
    auto resPro800 = service.loadAndValidateProfile(pro800CanonicalFile);
    REQUIRE(resPro800.isSuccess());

    const auto* resolvedPro800 = registry.findContractById("behringer_pro800");
    REQUIRE(resolvedPro800 != nullptr);

    HardwareContract adaptedPro800 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resPro800.profile);
    CHECK(adaptedPro800.manufacturer == resolvedPro800->manufacturer);
    CHECK(adaptedPro800.deviceType == resolvedPro800->deviceType);
    CHECK(adaptedPro800.functions.size() == 1);

    // 2. Paridad DX7
    auto dx7CanonicalFile = targetsDir.getChildFile("yamaha_dx7.target.json");
    auto resDx7 = service.loadAndValidateProfile(dx7CanonicalFile);
    REQUIRE(resDx7.isSuccess());

    const auto* resolvedDx7 = registry.findContractById("yamaha_dx7");
    REQUIRE(resolvedDx7 != nullptr);

    HardwareContract adaptedDx7 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resDx7.profile);
    CHECK(adaptedDx7.deviceType == resolvedDx7->deviceType);
    CHECK(adaptedDx7.functions.size() == 1);

    // 3. Paridad DS-1
    auto ds1CanonicalFile = targetsDir.getChildFile("boss_ds1_distortion.target.json");
    auto resDs1 = service.loadAndValidateProfile(ds1CanonicalFile);
    REQUIRE(resDs1.isSuccess());

    const auto* resolvedDs1 = registry.findContractById("boss_ds1_distortion");
    REQUIRE(resolvedDs1 != nullptr);

    HardwareContract adaptedDs1 = TargetProfileLegacyAdapter::toLegacyHardwareContract(resDs1.profile);
    CHECK(adaptedDs1.deviceType == resolvedDs1->deviceType);
    CHECK(adaptedDs1.functions.size() == 1);
}
