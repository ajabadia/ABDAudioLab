#include <catch2/catch_test_macros.hpp>
#include "gui/HardwareWiringDiagramComponent.h"
#include "gui/HardwareDeviceDisplayCardComponent.h"

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
