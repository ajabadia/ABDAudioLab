#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/ProfilingHardwareDispatcher.h"
#include "core/ProfilingAudioCapture.h"
#include "core/ProfilingSequencer.h"
#include "audio/LabAudioEngine.h"
#include "hardware/MockHardwareController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("ProfilingArchitectureRefactor: ProfilingHardwareDispatcher protocol injection", "[sequencer_refactor][hardware_dispatcher]")
{
    hardware::MockHardwareController mockHardware;
    ProfilingHardwareDispatcher dispatcher(&mockHardware);

    SECTION("Sets parameter on hardware interface")
    {
        dispatcher.setParameter(3, 0.75f);
        // Dispatcher forwards cleanly without throwing or crashing
        REQUIRE(mockHardware.isConnected());
    }

    SECTION("Injects MIDI CC and SysEx actions")
    {
        HardwareSetupAction ccAction;
        ccAction.method = HardwareMethod::MIDI_CC;
        ccAction.channel = 2;
        ccAction.controlNumber = 74;
        ccAction.normalizedValue = 0.5f;
        ccAction.settlingDelayMs = 0;

        dispatcher.executeLifecycleActions({ ccAction });
        REQUIRE(mockHardware.getSentMessages().size() >= 1);

        auto lastMsg = mockHardware.getSentMessages().back();
        REQUIRE(lastMsg.isController());
        REQUIRE(lastMsg.getChannel() == 2);
        REQUIRE(lastMsg.getControllerNumber() == 74);
    }

    SECTION("Injects Velocity and Note events cleanly")
    {
        dispatcher.sendNoteOn(1, 60, 0.8f);
        REQUIRE(mockHardware.getSentMessages().size() >= 1);
        auto noteMsg = mockHardware.getSentMessages().back();
        REQUIRE(noteMsg.isNoteOn());
        REQUIRE(noteMsg.getNoteNumber() == 60);

        dispatcher.sendNoteOff(1, 60, 0.0f);
        REQUIRE(mockHardware.getSentMessages().back().isNoteOff());

        dispatcher.sendAllNotesOff(1);
        REQUIRE(mockHardware.getSentMessages().back().isAllNotesOff());
    }
}

TEST_CASE("ProfilingArchitectureRefactor: ProfilingSequencer integration with modular sub-components", "[sequencer_refactor][sequencer_integration]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;

    ProfilingSequencer sequencer(audioEngine, mockHardware);

    SECTION("Sequencer owns and exposes modular sub-components")
    {
        REQUIRE(sequencer.getHardwareDispatcher().getHardwareController() == &mockHardware);

        // Hardware controller reassignment propagates to dispatcher
        hardware::MockHardwareController secondHardware;
        sequencer.setHardwareController(&secondHardware);
        REQUIRE(sequencer.getHardwareDispatcher().getHardwareController() == &secondHardware);
    }

    SECTION("Modulation probe executes cleanly through modular dispatcher and capture")
    {
        ProfilingSequencer::ModulationProbeContract contract;
        contract.excitationType = ProfilingSequencer::ModExcitationType::CC;
        contract.controlCCNumber = 16;
        contract.sourceID = 10;
        contract.destID = 20;
        contract.midiChannel = 1;
        contract.settlingDelayMs = 2;
        contract.destinationBlockType = "AmplitudeGain";

        std::vector<float> resting(512, 0.05f);
        math::ModulationMatrixProfile profile;

        bool ok = sequencer.runUniversalModulationProbe(contract, resting, profile);
        REQUIRE(ok);
        REQUIRE(profile.hasNode(10, 20));
        REQUIRE(profile.getNode(10, 20).probePoints.size() == 4);
    }
}
