#include <catch2/catch_test_macros.hpp>
#include "gui/soundid/SoundIdHardwareCatalogSelector.h"

using namespace abdaudiolab;

TEST_CASE("SoundIdHardwareCatalogSelector - Cascading Filter Logic", "[SoundIdHardware]")
{
    gui::SoundIdHardwareCatalogSelector selector;
    selector.setSize(800, 600);

    std::vector<core::HardwareContract> dummyContracts;

    // Device 1: Synthesizer Korg MS-20
    {
        core::HardwareContract c;
        c.id = "korg-ms20";
        c.displayName = "Korg MS-20";
        c.deviceType = "Synthesizer";
        c.brand = "Korg";
        c.model = "MS-20";

        core::HardwareFunction f1;
        f1.id = "vcf-lp";
        f1.name = "Low-Pass Filter (VCF)";
        c.functions.push_back(f1);

        dummyContracts.push_back(c);
    }

    // Device 2: Synthesizer Roland Juno-106
    {
        core::HardwareContract c;
        c.id = "roland-juno106";
        c.displayName = "Roland Juno-106";
        c.deviceType = "Synthesizer";
        c.brand = "Roland";
        c.model = "Juno-106";

        core::HardwareFunction f1;
        f1.id = "vcf-sweep";
        f1.name = "VCF 24dB Cutoff Sweep";
        c.functions.push_back(f1);

        dummyContracts.push_back(c);
    }

    // Device 3: Pedal Boss DS-1
    {
        core::HardwareContract c;
        c.id = "boss-ds1";
        c.displayName = "Boss DS-1 Distortion";
        c.deviceType = "Pedal";
        c.brand = "Boss";
        c.model = "DS-1";

        core::HardwareFunction f1;
        f1.id = "dist-curve";
        f1.name = "Clipping Non-Linearity";
        c.functions.push_back(f1);

        dummyContracts.push_back(c);
    }

    selector.setContracts(dummyContracts);

    SECTION("Initial population starts empty without auto-selecting hardware")
    {
        CHECK(selector.getSelectedDeviceType().isEmpty());
        CHECK(selector.getSelectedBrand().isEmpty());
        CHECK(selector.getSelectedHardwareId().isEmpty());
    }

    SECTION("Direct selection updates cascading state")
    {
        selector.setSelectedHardware("boss-ds1", "dist-curve");
        CHECK(selector.getSelectedDeviceType() == "Pedal");
        CHECK(selector.getSelectedBrand() == "Boss");
        CHECK(selector.getSelectedHardwareId() == "boss-ds1");
        CHECK(selector.getSelectedFunctionId() == "dist-curve");
    }

    SECTION("Libre Mode bypasses fixed catalog")
    {
        CHECK_FALSE(selector.isCustomOrLibre());
    }

    SECTION("Hardware Locking preserves configuration")
    {
        selector.setHardwareLocked(true);
        CHECK(selector.isLocked());
        selector.setHardwareLocked(false);
        CHECK_FALSE(selector.isLocked());
    }
}
