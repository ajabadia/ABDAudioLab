#include <catch2/catch_test_macros.hpp>
#include "core/HardwareContractRegistry.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::core;

TEST_CASE("HardwareContractRegistry Resilient Profile Loading", "[HardwareContract][Resilience]")
{
    HardwareContractRegistry registry;

    SECTION("Valid minimal JSON profile loads successfully")
    {
        juce::File tempFile = juce::File::createTempFile("valid_hw_test.json");
        juce::String validJson = R"({
            "id": "test_synth_device",
            "displayName": "Test Synthesizer Device",
            "deviceType": "AUTOMATED_MIDI_CC",
            "functions": [
                {
                    "id": "vcf_main",
                    "name": "Resonant Low-Pass",
                    "blockType": "SpectrumFilter"
                }
            ]
        })";
        tempFile.replaceWithText(validJson);

        HardwareContract contract;
        juce::String warning;
        bool success = registry.loadProfileResilient(tempFile, contract, warning);

        REQUIRE(success == true);
        REQUIRE(warning.isEmpty());
        REQUIRE(contract.id == "test_synth_device");
        REQUIRE(contract.displayName == "Test Synthesizer Device");
        REQUIRE(contract.deviceType == "AUTOMATED_MIDI_CC");
        REQUIRE(contract.functions.size() == 1);
        REQUIRE(contract.functions[0].id == "vcf_main");

        tempFile.deleteFile();
    }

    SECTION("Missing 'id' is rejected cleanly with warning without crashing")
    {
        juce::File tempFile = juce::File::createTempFile("missing_id_test.json");
        juce::String invalidJson = R"({
            "displayName": "Broken Hardware Missing ID",
            "deviceType": "MANUAL_EURORACK"
        })";
        tempFile.replaceWithText(invalidJson);

        HardwareContract contract;
        juce::String warning;
        bool success = registry.loadProfileResilient(tempFile, contract, warning);

        REQUIRE(success == false);
        REQUIRE(warning.isNotEmpty());
        REQUIRE(warning.contains("id"));

        tempFile.deleteFile();
    }

    SECTION("Missing 'displayName' is rejected cleanly with warning")
    {
        juce::File tempFile = juce::File::createTempFile("missing_name_test.json");
        juce::String invalidJson = R"({
            "id": "broken_hw_no_name",
            "deviceType": "MANUAL_EURORACK"
        })";
        tempFile.replaceWithText(invalidJson);

        HardwareContract contract;
        juce::String warning;
        bool success = registry.loadProfileResilient(tempFile, contract, warning);

        REQUIRE(success == false);
        REQUIRE(warning.isNotEmpty());
        REQUIRE(warning.contains("displayName"));

        tempFile.deleteFile();
    }

    SECTION("Corrupted syntax JSON is caught gracefully without crash")
    {
        juce::File tempFile = juce::File::createTempFile("corrupt_syntax.json");
        juce::String corruptedJson = "{ id: broken, displayName: 'Unclosed quotes... ";
        tempFile.replaceWithText(corruptedJson);

        HardwareContract contract;
        juce::String warning;
        bool success = registry.loadProfileResilient(tempFile, contract, warning);

        REQUIRE(success == false);
        REQUIRE(warning.isNotEmpty());

        tempFile.deleteFile();
    }

    SECTION("Non-existent file reports failure safely")
    {
        juce::File nonExistentFile("d:/non_existent_folder_xyz/invalid_hw.json");
        HardwareContract contract;
        juce::String warning;
        bool success = registry.loadProfileResilient(nonExistentFile, contract, warning);

        REQUIRE(success == false);
        REQUIRE(warning.isNotEmpty());
    }
}
