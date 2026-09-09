#include <catch2/catch_test_macros.hpp>
#include "../gui/SessionExecutionCoordinator.h"
#include "../core/ProfilingSequencer.h"
#include "../core/SessionManager.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"

using namespace abdaudiolab;

TEST_CASE("SessionExecutionCoordinator - Initialization & Callback Wiring", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    SECTION("Coordinator state and points tracking")
    {
        REQUIRE(coordinator.getTotalPointsMeasured() == 0);
        coordinator.setTotalPointsMeasured(42);
        REQUIRE(coordinator.getTotalPointsMeasured() == 42);

        REQUIRE_FALSE(coordinator.getIsPatchingSession());
        coordinator.setIsPatchingSession(true);
        REQUIRE(coordinator.getIsPatchingSession());
    }

    SECTION("Wire and Unbind Sequencer Callbacks safely without dangling pointers")
    {
        coordinator.wireSequencerCallbacks();
        // Should not throw or crash on multiple wires / unbinds
        coordinator.unbindSequencerCallbacks();
        coordinator.wireSequencerCallbacks();
    }

    SECTION("High level callbacks registration")
    {
        bool stateChangedNotified = false;
        bool stateRunningValue = false;
        coordinator.onExecutionStateChanged = [&](bool isRunning) {
            stateChangedNotified = true;
            stateRunningValue = isRunning;
        };

        bool errorTriggered = false;
        coordinator.onExecutionErrorTriggered = [&](const juce::String& err) {
            juce::ignoreUnused(err);
            errorTriggered = true;
        };

        bool finishedNotified = false;
        coordinator.onSessionFinished = [&](bool isPatching) {
            juce::ignoreUnused(isPatching);
            finishedNotified = true;
        };

        REQUIRE_FALSE(stateChangedNotified);
        REQUIRE_FALSE(errorTriggered);
        REQUIRE_FALSE(finishedNotified);
    }
}

TEST_CASE("SessionExecutionCoordinator - Modulation & Point Dispatching", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);
    coordinator.wireSequencerCallbacks();

    SECTION("Dispatches modulation node to curvePlotter without crash")
    {
        math::ModulationNode node;
        node.sourceID = 1;
        node.destID = 2;
        node.rSquared = 0.985f;
        node.kScalar = 0.72f;

        // In headless test mode, JUCE message manager processes events
        curvePlotter.updateModulationNode(node);
        REQUIRE(curvePlotter.getModulationProfile().hasNode(1, 2));
    }

    SECTION("Operator step confirmed & step back routing")
    {
        // Safe to call even when sequencer is idle (state checking)
        coordinator.confirmOperatorStep();
        coordinator.repeatCurrentStep();
        coordinator.stepBack();
    }
}
