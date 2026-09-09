#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/HardwareContractRegistry.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"

using namespace abdaudiolab;

TEST_CASE("Measurement Preset Recipe - JSON Parsing and Model Population", "[preset][recipe][contract]")
{
    const std::string testContractJson = R"({
        "schemaVersion": "2.0",
        "id": "test_synth_filter_recipe",
        "displayName": "Test Synth Filter Profiling",
        "deviceType": "AUTOMATED_MIDI_CC",
        "functions": [
            {
                "id": "filter_noise_test",
                "name": "Resonant Filter Isolated with White Noise",
                "blockType": "SpectrumFilter",
                "measurementRecipe": {
                    "recipeType": "INTERNAL_NOISE_EXCITATION",
                    "description": "Mute oscillators 1 and 2, unmute white noise generator, bypass FX",
                    "setupActions": [
                        { "description": "Osc 1 Level = 0", "method": "MIDI_CC", "cc": 20, "value": 0.0, "settlingDelayMs": 10 },
                        { "description": "Osc 2 Level = 0", "method": "MIDI_CC", "cc": 21, "value": 0.0, "settlingDelayMs": 10 },
                        { "description": "Noise Level = 100%", "method": "MIDI_CC", "cc": 22, "value": 1.0, "settlingDelayMs": 15 },
                        { "description": "FX Bypass", "method": "MIDI_CC", "cc": 82, "value": 0.0, "settlingDelayMs": 25 }
                    ],
                    "excitationNotes": [
                        { "noteNumber": 48, "velocity": 90, "startDelayMs": 0, "durationMs": 0, "isLegato": true }
                    ],
                    "postSettlingDelayMs": 50
                },
                "controls": [
                    { "index": 1, "name": "Cutoff", "type": "Knob", "cc": 74, "min": 0.0, "max": 1.0, "default": 0.5 }
                ]
            }
        ]
    })";

    juce::File tempDir = juce::File::createTempFile("recipe_test_dir");
    tempDir.deleteFile();
    tempDir.createDirectory();
    auto jsonFile = tempDir.getChildFile("test_synth_filter_recipe.json");
    jsonFile.replaceWithText(testContractJson);

    core::HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(tempDir));

    const auto* contract = registry.findContractById("test_synth_filter_recipe");
    REQUIRE(contract != nullptr);
    REQUIRE(contract->functions.size() == 1);

    const auto& fn = contract->functions[0];
    CHECK(fn.id == "filter_noise_test");
    CHECK(fn.blockType == "SpectrumFilter");
    CHECK(fn.measurementRecipe.recipeType == "INTERNAL_NOISE_EXCITATION");
    CHECK(fn.measurementRecipe.description.find("Mute oscillators") != std::string::npos);
    CHECK(fn.measurementRecipe.setupActions.size() == 4);
    CHECK(fn.measurementRecipe.setupActions[0].controlNumber == 20);
    CHECK(fn.measurementRecipe.setupActions[2].controlNumber == 22);
    CHECK(fn.measurementRecipe.setupActions[2].normalizedValue == 1.0f);
    CHECK(fn.measurementRecipe.excitationNotes.size() == 1);
    CHECK(fn.measurementRecipe.excitationNotes[0].noteNumber == 48);
    CHECK(fn.measurementRecipe.excitationNotes[0].isLegato == true);
    CHECK(fn.measurementRecipe.postSettlingDelayMs == 50);

    tempDir.deleteRecursively();
}

TEST_CASE("Measurement Preset Recipe - ProfilingSequencer Dispatch Execution", "[preset][recipe][sequencer]")
{
    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHw;

    core::ProfilingSequencer sequencer(audioEngine, mockHw);

    core::MeasurementPresetRecipe recipe;
    recipe.recipeType = "INTERNAL_NOISE_EXCITATION";
    recipe.description = "Test Filter Recipe Setup";
    recipe.postSettlingDelayMs = 10;

    core::HardwareSetupAction act1;
    act1.description = "Mute Osc 1";
    act1.method = core::HardwareMethod::MIDI_CC;
    act1.controlNumber = 12;
    act1.normalizedValue = 0.0f;
    act1.settlingDelayMs = 5;
    recipe.setupActions.push_back(act1);

    core::HardwareSetupAction act2;
    act2.description = "Noise Generator to 100%";
    act2.method = core::HardwareMethod::MIDI_CC;
    act2.controlNumber = 22;
    act2.normalizedValue = 1.0f;
    act2.settlingDelayMs = 5;
    recipe.setupActions.push_back(act2);

    core::NoteSequenceEvent noteEv;
    noteEv.noteNumber = 60;
    noteEv.velocity = 100;
    noteEv.durationMs = 20;
    noteEv.isLegato = false;
    recipe.excitationNotes.push_back(noteEv);

    // Execute the recipe directly
    sequencer.executeMeasurementRecipe(recipe);

    // Verify actions and note dispatch in mock controller
    const auto& sent = mockHw.getSentMessages();
    REQUIRE(sent.size() >= 4); // CC 12, CC 22, NoteOn 60, NoteOff 60

    bool foundCc12 = false, foundCc22 = false, foundNoteOn = false, foundNoteOff = false;
    for (const auto& msg : sent)
    {
        if (msg.isController() && msg.getControllerNumber() == 12 && msg.getControllerValue() == 0)
            foundCc12 = true;
        if (msg.isController() && msg.getControllerNumber() == 22 && msg.getControllerValue() == 127)
            foundCc22 = true;
        if (msg.isNoteOn() && msg.getNoteNumber() == 60)
            foundNoteOn = true;
        if (msg.isNoteOff() && msg.getNoteNumber() == 60)
            foundNoteOff = true;
    }

    CHECK(foundCc12);
    CHECK(foundCc22);
    CHECK(foundNoteOn);
    CHECK(foundNoteOff);
}
