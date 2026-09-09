#include <catch2/catch_test_macros.hpp>
#include "hardware/MidiDeviceHotplugMonitor.h"
#include "core/HardwareContractRegistry.h"

TEST_CASE("MidiDeviceHotplugMonitor Plug and Unplug Events", "[hardware][midi][hotplug]")
{
    using namespace abdaudiolab;
    using namespace abdaudiolab::hardware;

    core::HardwareContractRegistry registry;
    juce::File contractsDir("D:/desarrollos/ABDSynths/ABDSharedAssets/contracts");
    if (!contractsDir.isDirectory())
        contractsDir = juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("ABDSharedAssets").getChildFile("contracts");
    registry.loadContractsFromDirectory(contractsDir);

    MidiDeviceHotplugMonitor monitor;
    monitor.setContracts(registry.getContracts());

    // 1. Initial baseline with no devices
    juce::Array<juce::MidiDeviceInfo> initialOuts;
    juce::Array<juce::MidiDeviceInfo> initialIns;
    monitor.evaluateLists(initialOuts, initialIns);

    // 2. Simulate plugging in a Behringer DeepMind 12 via USB
    juce::Array<juce::MidiDeviceInfo> step1Outs;
    juce::Array<juce::MidiDeviceInfo> step1Ins;
    step1Outs.add(juce::MidiDeviceInfo { "DeepMind 12", "id_dm12_out" });
    step1Ins.add(juce::MidiDeviceInfo { "DeepMind 12", "id_dm12_in" });

    bool pluggedFired = false;
    DiscoveredDevice pluggedDev;
    monitor.onDevicePlugged = [&](const DiscoveredDevice& dev) {
        pluggedFired = true;
        pluggedDev = dev;
    };

    monitor.evaluateLists(step1Outs, step1Ins);

    REQUIRE(pluggedFired);
    REQUIRE(pluggedDev.hardwareId == "behringer_deepmind12");
    REQUIRE(pluggedDev.displayName.find("DeepMind") != std::string::npos);

    // 3. Simulate plugging in a Roland Bitrazer
    juce::Array<juce::MidiDeviceInfo> step2Outs = step1Outs;
    juce::Array<juce::MidiDeviceInfo> step2Ins = step1Ins;
    step2Outs.add(juce::MidiDeviceInfo { "BITRAZER", "id_bitrazer_out" });
    step2Ins.add(juce::MidiDeviceInfo { "BITRAZER", "id_bitrazer_in" });

    pluggedFired = false;
    monitor.evaluateLists(step2Outs, step2Ins);

    REQUIRE(pluggedFired);
    REQUIRE(pluggedDev.hardwareId == "roland_aira_bitrazer");

    // 4. Simulate unplugging the DeepMind 12
    juce::Array<juce::MidiDeviceInfo> step3Outs;
    juce::Array<juce::MidiDeviceInfo> step3Ins;
    step3Outs.add(juce::MidiDeviceInfo { "BITRAZER", "id_bitrazer_out" });
    step3Ins.add(juce::MidiDeviceInfo { "BITRAZER", "id_bitrazer_in" });

    bool unpluggedFired = false;
    juce::String unpluggedName;
    monitor.onDeviceUnplugged = [&](const juce::String& name) {
        unpluggedFired = true;
        unpluggedName = name;
    };

    monitor.evaluateLists(step3Outs, step3Ins);

    REQUIRE(unpluggedFired);
    REQUIRE(unpluggedName == "DeepMind 12");
}
