#include <catch2/catch_test_macros.hpp>
#include "core/HardwareManager.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"

TEST_CASE("MidiSynthAutomation - Controller Note Dispatch & Tracking", "[hardware][midi]")
{
    using namespace abdaudiolab::hardware;

    MockHardwareController mock;
    REQUIRE(mock.getActiveNoteCount() == 0);

    // Note-On
    bool onOk = mock.sendNoteOn(1, 60, 0.8f);
    REQUIRE(onOk);
    REQUIRE(mock.hasReceivedNoteOn(60));
    REQUIRE(mock.getActiveNoteCount() == 1);

    // Second Note-On
    mock.sendNoteOn(1, 64, 0.7f);
    REQUIRE(mock.hasReceivedNoteOn(64));
    REQUIRE(mock.getActiveNoteCount() == 2);

    // Note-Off first note
    bool offOk = mock.sendNoteOff(1, 60, 0.0f);
    REQUIRE(offOk);
    REQUIRE(mock.hasReceivedNoteOff(60));
    REQUIRE(mock.getActiveNoteCount() == 1);

    // All-Notes-Off
    bool allOffOk = mock.sendAllNotesOff(1);
    REQUIRE(allOffOk);
    REQUIRE(mock.hasReceivedAllNotesOff());
    REQUIRE(mock.getActiveNoteCount() == 0);

    // Pitch Bend & Pressure
    REQUIRE(mock.sendPitchBend(1, 8192));
    REQUIRE(mock.sendChannelPressure(1, 0.5f));
}

TEST_CASE("MidiSynthAutomation - HardwareManager Autonomous Synth Heuristics", "[core][hardware]")
{
    using namespace abdaudiolab::core;

    HardwareManager hwManager;

    // Direct synth keyword detection
    REQUIRE(hwManager.isAutonomousSynth("ABDJUNIO601", "VCF_SWEEP"));
    REQUIRE(hwManager.isAutonomousSynth("behringer_deepmind12", "MAIN"));
    REQUIRE(hwManager.isAutonomousSynth("ABDMS2000", "FILTER"));
    REQUIRE(hwManager.isAutonomousSynth("pro800", "VOICE"));

    // Non-synth (pure effect / filter box requiring external audio stimulus)
    REQUIRE_FALSE(hwManager.isAutonomousSynth("MOCK_FILTER_BOX", "FILTER_IN"));
}

TEST_CASE("MidiSynthAutomation - ProfilingSequencer Autonomous Note Excitation", "[core][sequencer]")
{
    using namespace abdaudiolab;
    using namespace abdaudiolab::core;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mock;

    ProfilingSequencer sequencer(audioEngine, mock);

    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "TEST_SYNTH";
    meta.targetModule = "VCF";
    meta.operatorMode = "AUTOMATED_MIDI_CC";
    session.setMetadata(meta);

    TestCase tc;
    tc.testId = "SYNTH_NOTE_EXCITATION_001";
    tc.functionalBlockType = "SpectrumFilter";
    tc.stimulusType = audio::StimulusType::Silence;
    tc.stimulusDurationSec = 0.08;
    tc.isAutonomousSynth = true;
    tc.midiChannel = 1;
    tc.midiNoteNumber = 62; // D4
    tc.midiVelocity = 0.85f;
    tc.noteGateDurationSec = 0.05f;
    tc.numPasses = 1;
    tc.stabilizationWaitMs = 10.0;
    session.addTestCase(tc);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("midi_synth_test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    mock.clearSentMessages();
    REQUIRE(sequencer.startSession(session, tempDir, "synth_test"));

    REQUIRE(sequencer.waitForThreadToExit(8000));

    // Verify Note-On was dispatched
    REQUIRE(mock.hasReceivedNoteOn(62));
    // Verify Note-Off was dispatched after gate
    REQUIRE(mock.hasReceivedNoteOff(62));
    // Verify all notes are silenced at completion
    REQUIRE(mock.hasReceivedAllNotesOff());
    REQUIRE(mock.getActiveNoteCount() == 0);

    // Test stopSession safety: starting a note and immediately stopping session cleans all notes
    mock.clearSentMessages();
    mock.sendNoteOn(1, 65, 0.8f);
    REQUIRE(mock.getActiveNoteCount() == 1);
    sequencer.stopSession();
    REQUIRE(mock.hasReceivedAllNotesOff());
    REQUIRE(mock.getActiveNoteCount() == 0);

    // Clean up
    tempDir.deleteRecursively();
}
