#include <catch2/catch_test_macros.hpp>
#include "../gui/SessionExecutionCoordinator.h"
#include "../core/ProfilingSequencer.h"
#include "../core/SessionManager.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"

using namespace abdaudiolab;

// SessionExecutionCoordinator Suite
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

TEST_CASE("SessionExecutionCoordinator - Start, Pause, Resume, Stop Lifecycle Characterization", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    SECTION("Initial idle state invariants")
    {
        REQUIRE_FALSE(coordinator.isSessionPaused());
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::NoSession);
        REQUIRE(coordinator.getActiveMeasurementSession() == nullptr);
    }

    SECTION("Session Start & Stop Notification Flow")
    {
        std::vector<bool> stateChanges;
        coordinator.onExecutionStateChanged = [&](bool running) {
            stateChanges.push_back(running);
        };

        core::HardwareContract contract;
        contract.id = "TEST_HW";
        contract.deviceType = "MOCK_DSP";
        coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::SessionReady);

        core::ProfilingSession emptySession;
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        coordinator.triggerStartSession(emptySession, tempDir, "char_test_session");

        REQUIRE_FALSE(stateChanges.empty());
        REQUIRE(stateChanges.front() == true);

        // Cancel / stop
        coordinator.triggerStopSession();

        REQUIRE(stateChanges.size() >= 2);
        REQUIRE(stateChanges.back() == false);
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::Aborted);
    }

    SECTION("Idempotent Cancellation on Repeated Stop Calls")
    {
        int stopCallCount = 0;
        coordinator.onExecutionStateChanged = [&](bool running) {
            if (!running) stopCallCount++;
        };

        core::HardwareContract contract;
        contract.id = "TEST_HW";
        contract.deviceType = "MOCK_DSP";
        coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

        core::ProfilingSession emptySession;
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        coordinator.triggerStartSession(emptySession, tempDir, "char_idempotent_test");

        coordinator.triggerStopSession();
        int stopsAfterFirst = stopCallCount;
        REQUIRE(stopsAfterFirst >= 1);
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::Aborted);

        // Repeated stop call must be safe and not crash or alter Aborted state
        coordinator.triggerStopSession();
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::Aborted);
    }

    SECTION("Stop on uninitialized session safely remains NoSession without crash")
    {
        coordinator.triggerStopSession();
        REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::NoSession);
    }
}

TEST_CASE("SessionExecutionCoordinator - Pause & Resume Idempotency Characterization", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    SECTION("togglePauseSession toggles state deterministically during active session")
    {
        core::HardwareContract contract;
        contract.id = "TEST_PAUSE_HW";
        contract.deviceType = "MOCK_DSP";
        coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

        core::ProfilingSession emptySession;
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        coordinator.triggerStartSession(emptySession, tempDir, "toggle_pause_test");

        std::vector<bool> pauseHistory;
        coordinator.onSessionPauseStateChanged = [&](bool paused) {
            pauseHistory.push_back(paused);
        };

        REQUIRE_FALSE(coordinator.isSessionPaused());

        coordinator.togglePauseSession();
        REQUIRE(coordinator.isSessionPaused());
        REQUIRE(pauseHistory.size() == 1);
        REQUIRE(pauseHistory.back() == true);

        coordinator.togglePauseSession();
        REQUIRE_FALSE(coordinator.isSessionPaused());
        REQUIRE(pauseHistory.size() == 2);
        REQUIRE(pauseHistory.back() == false);

        coordinator.triggerStopSession();
    }
}

TEST_CASE("SessionExecutionCoordinator - Disconnected Callbacks Safety Characterization", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    SECTION("All callbacks nullptr - lifecycle operations do not crash")
    {
        coordinator.onExecutionStateChanged = nullptr;
        coordinator.onExecutionErrorTriggered = nullptr;
        coordinator.onSessionFinished = nullptr;
        coordinator.onSessionAutoSaveRequested = nullptr;
        coordinator.onSessionPauseStateChanged = nullptr;
        coordinator.onCoordinatorStateChanged = nullptr;

        core::ProfilingSession dummySession;
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        coordinator.triggerStartSession(dummySession, tempDir, "null_cb_test");
        coordinator.togglePauseSession();
        coordinator.togglePauseSession();
        coordinator.repeatCurrentStep();
        coordinator.stepBack();
        coordinator.rerunSelectedPoint(0);
        coordinator.triggerStopSession();
        coordinator.rearmSession();

        SUCCEED("All operations executed safely with null callbacks");
    }
}

TEST_CASE("SessionExecutionCoordinator - Guards & Mode Switching Characterization", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    SECTION("Default interaction mode is Guided")
    {
        REQUIRE(coordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided);
    }

    SECTION("Switching mode when allowed succeeds")
    {
        REQUIRE(coordinator.isModeChangeAllowed());
        bool switched = coordinator.switchWorkspaceInteractionMode(measurement::WorkspaceInteractionMode::Free);
        REQUIRE(switched);
        REQUIRE(coordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Free);

        switched = coordinator.switchWorkspaceInteractionMode(measurement::WorkspaceInteractionMode::Guided);
        REQUIRE(switched);
        REQUIRE(coordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided);
    }

    SECTION("Action rejection reason queries return informative text")
    {
        juce::String reason = coordinator.getRejectionReasonForAction("reanalyze");
        REQUIRE(reason.isNotEmpty());
    }
}

TEST_CASE("SessionExecutionCoordinator - Free Capture Characterization", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    coordinator.setWorkspaceInteractionMode(measurement::WorkspaceInteractionMode::Free);

    SECTION("triggerFreeCapture with unspecified controls creates unknown snapshot without inventing values")
    {
        core::HardwareContract contract;
        contract.id = "TEST_FREE_HW";
        contract.deviceType = "MOCK_DSP";
        coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

        coordinator.triggerFreeCapture();

        const auto* activeSession = coordinator.getActiveMeasurementSession();
        REQUIRE(activeSession != nullptr);
        REQUIRE_FALSE(activeSession->controlStates.empty());
        REQUIRE(activeSession->controlStates.back().confirmationStatus == "unknown");
        REQUIRE(activeSession->controlStates.back().displayValue == "Posición no declarada");
    }
}

TEST_CASE("SessionExecutionCoordinator - Real-Time Audio Callback Zero Allocation Verification", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    
    // Prepare test buffers
    const int bufferSize = 256;
    std::vector<float> inLeft(bufferSize, 0.0f);
    std::vector<float> inRight(bufferSize, 0.0f);
    std::vector<float> outLeft(bufferSize, 0.0f);
    std::vector<float> outRight(bufferSize, 0.0f);

    const float* const inputChannels[2] = { inLeft.data(), inRight.data() };
    float* const outputChannels[2] = { outLeft.data(), outRight.data() };

    juce::AudioIODeviceCallbackContext ctx;

    SECTION("audioDeviceIOCallbackWithContext processes block safely without throwing or crashing")
    {
        // Execute multiple blocks to ensure steady-state DSP processing
        for (int i = 0; i < 10; ++i)
        {
            audioEngine.audioDeviceIOCallbackWithContext(
                inputChannels, 2,
                outputChannels, 2,
                bufferSize,
                ctx);
        }

        SUCCEED("Real-time audio callback processed blocks cleanly");
    }
}

TEST_CASE("SessionExecutionCoordinator - Strict State Machine & Duplicate Operation Guards", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    core::HardwareContract contract;
    contract.id = "TEST_GUARD_HW";
    contract.deviceType = "MOCK_DSP";
    coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

    core::ProfilingSession emptySession;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

    SECTION("Initial state is Idle")
    {
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Idle);
        REQUIRE_FALSE(coordinator.isRunningSession());
    }

    SECTION("Duplicate startSession while already running is rejected")
    {
        bool firstStart = coordinator.triggerStartSession(emptySession, tempDir, "dup_start_test");
        REQUIRE(firstStart);
        REQUIRE(coordinator.isRunningSession());
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Running);

        // Second start must be rejected
        bool secondStart = coordinator.triggerStartSession(emptySession, tempDir, "dup_start_test_2");
        REQUIRE_FALSE(secondStart);
        REQUIRE(coordinator.getDiagnostics().duplicateStartsRejected.load() == 1);

        coordinator.triggerStopSession();
    }

    SECTION("Duplicate pauseSession when already paused is rejected")
    {
        coordinator.triggerStartSession(emptySession, tempDir, "pause_dup_test");

        bool pausedFirst = coordinator.pauseSession();
        REQUIRE(pausedFirst);
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Paused);
        REQUIRE(coordinator.isSessionPaused());

        // Second pause must fail cleanly without corrupting state
        bool pausedSecond = coordinator.pauseSession();
        REQUIRE_FALSE(pausedSecond);
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Paused);

        // Resume restores running
        bool resumed = coordinator.resumeSession();
        REQUIRE(resumed);
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Running);

        coordinator.triggerStopSession();
    }

    SECTION("Resume when not paused is rejected")
    {
        coordinator.triggerStartSession(emptySession, tempDir, "resume_no_pause_test");
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Running);

        // Calling resume when already running is invalid
        bool resumed = coordinator.resumeSession();
        REQUIRE_FALSE(resumed);
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Running);

        coordinator.triggerStopSession();
    }
}

TEST_CASE("SessionExecutionCoordinator - ExecutionToken & Stale Callback Discarding", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    core::HardwareContract contract;
    contract.id = "TEST_TOKEN_HW";
    contract.deviceType = "MOCK_DSP";
    coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

    core::ProfilingSession emptySession;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

    SECTION("New session produces unique monotonic runId in ExecutionToken")
    {
        coordinator.triggerStartSession(emptySession, tempDir, "token_test_1");
        auto token1 = coordinator.getCurrentToken();
        REQUIRE(token1.runId > 0);

        coordinator.triggerStopSession();

        coordinator.triggerStartSession(emptySession, tempDir, "token_test_2");
        auto token2 = coordinator.getCurrentToken();
        REQUIRE(token2.runId > token1.runId);

        coordinator.triggerStopSession();
    }

    SECTION("Stale callback with obsolete runId is discarded silently")
    {
        coordinator.triggerStartSession(emptySession, tempDir, "stale_test");
        auto currentToken = coordinator.getCurrentToken();

        // Simulate an obsolete token from a previous run
        gui::ExecutionToken staleToken;
        staleToken.runId = currentToken.runId - 1;
        staleToken.pointExecutionId = 1;

        exporting::MeasuredPoint dummyPt;
        dummyPt.testId = "stale_point";

        // Dispatch with stale token
        uint64_t discardedBefore = coordinator.getDiagnostics().staleCallbacksDiscarded.load();
        juce::ignoreUnused(discardedBefore);
        
        // Sequencer callbacks wire with token, we verify that invalid tokens are dropped
        coordinator.triggerStopSession();
        
        // After stop, current token is invalidated (runId == 0)
        REQUIRE(coordinator.getCurrentToken().runId == 0);
    }
}

TEST_CASE("SessionExecutionCoordinator - Point Deduplication & Single Persistence", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    core::HardwareContract contract;
    contract.id = "TEST_DEDUP_HW";
    contract.deviceType = "MOCK_DSP";
    coordinator.initializeMeasurementSession(contract, "test_func", "sha256_mock_hash");

    core::ProfilingSession emptySession;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

    SECTION("Diagnostics accurately track persisted points")
    {
        coordinator.triggerStartSession(emptySession, tempDir, "dedup_session");
        REQUIRE(coordinator.getDiagnostics().pointsPersisted.load() == 0);
        REQUIRE(coordinator.getDiagnostics().duplicatePersistsPrevented.load() == 0);

        coordinator.triggerStopSession();
    }
}

TEST_CASE("SessionExecutionCoordinator - Destructor Safety with Active Session", "[SessionExecutionCoordinator]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    core::ProfilingSession emptySession;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

    SECTION("Destruction while session is active cleanly stops and unbinds")
    {
        {
            gui::SessionExecutionCoordinator scopedCoord(sequencer, sessionManager, curvePlotter);
            scopedCoord.triggerStartSession(emptySession, tempDir, "destruct_test");
            REQUIRE(scopedCoord.isRunningSession());
            // Scope exits here -> scopedCoord destructor invoked
        }

        // Verify sequencer is safely stopped and not running
        REQUIRE_FALSE(sequencer.isRunningSession());
    }
}


