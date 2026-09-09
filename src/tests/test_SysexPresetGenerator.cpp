#include <catch2/catch_test_macros.hpp>
#include "hardware/SysexPresetGenerator.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::hardware;

TEST_CASE("SysexPresetGenerator Vendor Header and Byte Structure", "[hardware][sysex][preset]")
{
    SECTION("Roland Juno-106 neutral patch generation")
    {
        auto syx = SysexPresetGenerator::createNeutralCalibrationPatch("roland_juno106");
        REQUIRE(syx.size() > 10);
        REQUIRE(syx.front() == 0xF0);
        REQUIRE(syx.back() == 0xF7);
        REQUIRE(syx[1] == 0x41); // Roland Manufacturer ID
        REQUIRE(syx[2] == 0x32); // Juno-106 Model ID
    }

    SECTION("Behringer PRO-800 neutral patch generation")
    {
        auto syx = SysexPresetGenerator::createNeutralCalibrationPatch("behringer_pro800");
        REQUIRE(syx.size() > 10);
        REQUIRE(syx.front() == 0xF0);
        REQUIRE(syx.back() == 0xF7);
        REQUIRE(syx[1] == 0x00);
        REQUIRE(syx[2] == 0x20);
        REQUIRE(syx[3] == 0x32); // Behringer Extended ID
    }

    SECTION("Export to file roundtrip")
    {
        auto syx = SysexPresetGenerator::createNeutralCalibrationPatch("korg_ms2000");
        juce::File tempSyx = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_neutral.syx");
        tempSyx.deleteFile();

        bool ok = SysexPresetGenerator::exportToSyxFile(syx, tempSyx);
        REQUIRE(ok == true);
        REQUIRE(tempSyx.existsAsFile());
        REQUIRE(tempSyx.getSize() == static_cast<juce::int64>(syx.size()));

        tempSyx.deleteFile();
    }
}
