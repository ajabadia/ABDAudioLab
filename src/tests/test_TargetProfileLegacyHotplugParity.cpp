// ==============================================================================
// ABDAudioLab - HITO-10E: MidiDeviceHotplugMonitor Parity Under Controlled Absence (E5.1)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "core/HardwareManager.h"
#include "hardware/MidiDeviceHotplugMonitor.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::hardware;

namespace
{

inline juce::File getContractsHardwareDir() { return abdaudiolab::core::contractsHardwareDir(); }

inline juce::File getCanonicalTargetsDir() { return abdaudiolab::core::canonicalTargetsDir(); }

} // namespace

TEST_CASE("HITO-10E / E5.1 - 5. MidiDeviceHotplugMonitor Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    auto realLegacyDir = getContractsHardwareDir();
    juce::File tempDir = juce::File::createTempFile("e5_1_hotplug_fixture");
    tempDir.deleteFile();
    tempDir.createDirectory();

    auto realFiles = realLegacyDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& rf : realFiles)
    {
        rf.copyFileTo(tempDir.getChildFile(rf.getFileName()));
    }

    HardwareContractRegistry registry;
    registry.loadContractsFromDirectory(tempDir);
    registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());

    MidiDeviceHotplugMonitor monitor;
    monitor.setContracts(registry.getContracts());

    // 1. Estado inicial sin dispositivos
    juce::Array<juce::MidiDeviceInfo> initialOuts, initialIns;
    monitor.evaluateLists(initialOuts, initialIns);

    // 2. Conectar Behringer PRO-800
    bool pluggedFired = false;
    DiscoveredDevice pluggedDev;
    monitor.onDevicePlugged = [&](const DiscoveredDevice& dev) {
        pluggedFired = true;
        pluggedDev = dev;
    };

    juce::Array<juce::MidiDeviceInfo> proOuts, proIns;
    proOuts.add(juce::MidiDeviceInfo { "PRO-800", "id_pro_out" });
    proIns.add(juce::MidiDeviceInfo { "PRO-800", "id_pro_in" });
    monitor.evaluateLists(proOuts, proIns);

    REQUIRE(pluggedFired);
    CHECK(pluggedDev.hardwareId == "hw-behringer-pro800-canonical");
    CHECK(pluggedDev.displayName == "Behringer PRO-800");

    // 3. Conectar dispositivo legacy no migrado (DeepMind 12)
    pluggedFired = false;
    juce::Array<juce::MidiDeviceInfo> dmOuts = proOuts;
    juce::Array<juce::MidiDeviceInfo> dmIns = proIns;
    dmOuts.add(juce::MidiDeviceInfo { "DeepMind 12", "id_dm12_out" });
    dmIns.add(juce::MidiDeviceInfo { "DeepMind 12", "id_dm12_in" });
    monitor.evaluateLists(dmOuts, dmIns);

    REQUIRE(pluggedFired);
    CHECK(pluggedDev.hardwareId == "behringer_deepmind12");

    tempDir.deleteRecursively();
}

TEST_CASE("HITO-10E / E5.1 - 6. HardwareManager Under Controlled Absence",
          "[targetprofile][legacy][consumer][parity][retirement_gate]")
{
    auto realLegacyDir = getContractsHardwareDir();
    juce::File tempDir = juce::File::createTempFile("e5_1_hwmanager_fixture");
    tempDir.deleteFile();
    tempDir.createDirectory();

    auto realFiles = realLegacyDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& rf : realFiles)
    {
        rf.copyFileTo(tempDir.getChildFile(rf.getFileName()));
    }

    HardwareManager hwManager;
    hwManager.getContractRegistry().loadContractsFromDirectory(tempDir);
    hwManager.getContractRegistry().loadCanonicalTargetProfiles(getCanonicalTargetsDir());

    const auto* proContract = hwManager.findContractById("behringer_pro800");
    REQUIRE(proContract != nullptr);
    CHECK(proContract->id == "hw-behringer-pro800-canonical");

    const auto* dxContract = hwManager.findContractById("yamaha_dx7");
    REQUIRE(dxContract != nullptr);
    CHECK(dxContract->id == "hw-yamaha-dx7-canonical");

    const auto* dsContract = hwManager.findContractById("boss_ds1_distortion");
    REQUIRE(dsContract != nullptr);
    CHECK(dsContract->id == "hw-boss-ds1-canonical");

    // Perfil no migrado resuelve nativo
    const auto* czContract = hwManager.findContractById("casio_cz101");
    REQUIRE(czContract != nullptr);
    CHECK(czContract->id == "casio_cz101");

    tempDir.deleteRecursively();
}
