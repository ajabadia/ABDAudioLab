#include <catch2/catch_test_macros.hpp>
#include "gui/HardwareWiringDiagramComponent.h"
#include "gui/HardwareDeviceDisplayCardComponent.h"
#include "gui/HardwareRoutingPanel.h"

using namespace abdaudiolab;

TEST_CASE("HardwareWiringDiagramComponent - State & text updates", "[HardwareRouting]")
{
    gui::HardwareWiringDiagramComponent comp;
    comp.setSize(400, 200);

    SECTION("Default values initialization")
    {
        CHECK(comp.isEmpty());
        CHECK(comp.getStimulusText().isEmpty());
        CHECK(comp.getResponseText().isEmpty());
        CHECK_FALSE(comp.getIsMidiAutonomous());
    }

    SECTION("Custom routing update - MIDI autonomous")
    {
        comp.setRouting("USB MIDI Port 1", "Hardware Stereo Out", "Conectar cable USB", true);
        CHECK(comp.getStimulusText() == "USB MIDI Port 1");
        CHECK(comp.getResponseText() == "Hardware Stereo Out");
        CHECK(comp.getNotesText() == "Conectar cable USB");
        CHECK(comp.getIsMidiAutonomous());
    }

    SECTION("Custom routing update - DAC loop")
    {
        comp.setRouting("DAC Channel 1", "ADC Channel 1", "Loopback analogico", false);
        CHECK(comp.getStimulusText() == "DAC Channel 1");
        CHECK(comp.getResponseText() == "ADC Channel 1");
        CHECK(comp.getNotesText() == "Loopback analogico");
        CHECK_FALSE(comp.getIsMidiAutonomous());
    }
}

TEST_CASE("HardwareDeviceDisplayCardComponent - Contract configuration", "[HardwareRouting]")
{
    gui::HardwareDeviceDisplayCardComponent card;
    card.setSize(350, 300);

    SECTION("Null contract reset")
    {
        card.setDevice(nullptr);
        CHECK(card.getDisplayName().isEmpty());
        CHECK(card.getBrand().isEmpty());
        CHECK(card.getCategory().isEmpty());
        CHECK_FALSE(card.hasModelImage());
        CHECK_FALSE(card.hasBrandLogo());
    }

    SECTION("Valid mock contract configuration")
    {
        core::HardwareContract contract;
        contract.id = "korg_ms2000_mock";
        contract.displayName = "MS2000 Virtual Analog";
        contract.brand = "Korg";
        contract.deviceType = "AUTOMATED_MIDI_CC";

        card.setDevice(&contract);
        CHECK(card.getDisplayName() == "MS2000 Virtual Analog");
        CHECK(card.getBrand() == "Korg");
        CHECK(card.getCategory() == "AUTOMATED_MIDI_CC");
    }
}

TEST_CASE("HardwareRoutingPanel - Contract and selection workflow", "[HardwareRouting]")
{
    gui::HardwareRoutingPanel panel;
    panel.setSize(1000, 700);

    std::vector<core::HardwareContract> contracts;
    {
        core::HardwareContract c1;
        c1.id = "roland_juno106";
        c1.displayName = "Roland Juno-106";
        c1.brand = "Roland";
        c1.deviceType = "AUTOMATED_MIDI_CC";

        core::HardwareFunction fn1;
        fn1.id = "vcf_cutoff";
        fn1.name = "VCF LowPass Filter";
        fn1.blockType = "SpectrumFilter";
        fn1.routingGuide.stimulusOutput = "MIDI In (Juno-106)";
        fn1.routingGuide.responseInput = "Mono Audio Out (Juno-106)";
        fn1.routingGuide.notes = "Envía ráfagas CC #74 al Juno";
        c1.functions.push_back(fn1);
        contracts.push_back(c1);

        core::HardwareContract c2;
        c2.id = "eurorack_vcf";
        c2.displayName = "Doepfer A-120";
        c2.brand = "Doepfer";
        c2.deviceType = "MANUAL_EURORACK";

        core::HardwareFunction fn2;
        fn2.id = "audio_filter";
        fn2.name = "Lowpass Filter Audio In";
        fn2.blockType = "SpectrumFilter";
        fn2.routingGuide.stimulusOutput = "DAC 1 -> Eurorack In";
        fn2.routingGuide.responseInput = "Eurorack Out -> ADC 1";
        fn2.routingGuide.notes = "Nivel de Eurorack atenuado";
        c2.functions.push_back(fn2);
        contracts.push_back(c2);
    }

    panel.setContracts(contracts);

    SECTION("Selection changes and delegates to subcomponents")
    {
        panel.setSelectedHardware("roland_juno106", "vcf_cutoff");
        CHECK(panel.getSelectedHardwareId() == "roland_juno106");
        CHECK(panel.getSelectedFunctionId() == "vcf_cutoff");

        const auto& wiring = panel.getWiringDiagram();
        CHECK(wiring.getIsMidiAutonomous());
        CHECK(wiring.getStimulusText() == "MIDI In (Juno-106)");
        CHECK(wiring.getResponseText() == "Mono Audio Out (Juno-106)");

        const auto& card = panel.getDeviceDisplayCard();
        CHECK(card.getDisplayName() == "Roland Juno-106");
        CHECK(card.getBrand() == "Roland");
    }

    SECTION("Hardware lock disables controls and updates status")
    {
        panel.setHardwareLocked(true);
        CHECK(panel.isHardwareLockedActive());

        panel.setHardwareLocked(false);
        CHECK_FALSE(panel.isHardwareLockedActive());
    }
}
