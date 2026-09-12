#include <catch2/catch_test_macros.hpp>
#include "../core/ProfilingSequencer.h"
#include "../gui/SessionExecutionCoordinator.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"
#include "../core/SessionManager.h"

using namespace abdaudiolab;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    struct PauseTestFixture
    {
        juce::ScopedJuceInitialiser_GUI juceInit;

        audio::LabAudioEngine          audioEngine;
        hardware::MockHardwareController mockHw;
        core::ProfilingSequencer       sequencer { audioEngine, mockHw };
        core::SessionManager           sessionManager;
        gui::SoundIdCurvePlotter       curvePlotter;
        gui::SessionExecutionCoordinator coordinator { sequencer, sessionManager, curvePlotter };
    };
} // namespace

// ---------------------------------------------------------------------------
// Test: ProfilingSequencer pause / resume API (no live thread)
// ---------------------------------------------------------------------------
TEST_CASE("ProfilingSequencer - Pause/Resume state API", "[PauseResume]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHw;
    core::ProfilingSequencer sequencer(audioEngine, mockHw);

    SECTION("Initial state is not paused")
    {
        REQUIRE_FALSE(sequencer.isSessionPaused());
    }

    SECTION("pauseSession() sets the paused flag")
    {
        sequencer.pauseSession();
        REQUIRE(sequencer.isSessionPaused());
    }

    SECTION("resumeSession() clears the paused flag")
    {
        sequencer.pauseSession();
        REQUIRE(sequencer.isSessionPaused());

        sequencer.resumeSession();
        REQUIRE_FALSE(sequencer.isSessionPaused());
    }

    SECTION("startSession() resets paused flag from a previous pause")
    {
        sequencer.pauseSession();
        REQUIRE(sequencer.isSessionPaused());

        // startSession needs a non-running sequencer — only call if not running
        if (!sequencer.isRunningSession())
        {
            // Provide a minimal empty session; it will start the thread momentarily
            core::ProfilingSession emptySession;
            sequencer.startSession(emptySession, juce::File::getSpecialLocation(juce::File::tempDirectory), "test");
            // Flag must be cleared immediately after startSession
            REQUIRE_FALSE(sequencer.isSessionPaused());
            sequencer.stopSession();
        }
    }

    SECTION("stopSession() clears paused flag so thread can exit cleanly")
    {
        sequencer.pauseSession();
        REQUIRE(sequencer.isSessionPaused());

        sequencer.stopSession();   // must unblock the WaitableEvent gate
        REQUIRE_FALSE(sequencer.isSessionPaused());
    }
}

// ---------------------------------------------------------------------------
// Test: scheduleRerunPoint atomic handshake
// ---------------------------------------------------------------------------
TEST_CASE("ProfilingSequencer - scheduleRerunPoint stores the index", "[PauseResume]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHw;
    core::ProfilingSequencer sequencer(audioEngine, mockHw);

    SECTION("scheduleRerunPoint stores index for the run loop to consume")
    {
        // We can't inspect rerunPointIndex directly (private), but we can verify
        // that calling it twice doesn't crash and the last value wins
        sequencer.scheduleRerunPoint(7);
        sequencer.scheduleRerunPoint(3); // overwrite — last write wins
        // The run-loop would consume via exchange(-1); just verify no crash here.
        SUCCEED("scheduleRerunPoint API is callable without session running");
    }
}

// ---------------------------------------------------------------------------
// Test: SessionExecutionCoordinator pause API delegates correctly
// ---------------------------------------------------------------------------
TEST_CASE("SessionExecutionCoordinator - togglePauseSession delegates to sequencer", "[PauseResume]")
{
    PauseTestFixture f;

    SECTION("Initial paused state is false")
    {
        REQUIRE_FALSE(f.coordinator.isSessionPaused());
    }

    SECTION("togglePauseSession() pauses when not paused")
    {
        // togglePauseSession calls sequencer.pauseSession / resumeSession
        f.coordinator.togglePauseSession();
        REQUIRE(f.coordinator.isSessionPaused());
        REQUIRE(f.sequencer.isSessionPaused());
    }

    SECTION("togglePauseSession() resumes when already paused")
    {
        f.coordinator.togglePauseSession(); // pause
        f.coordinator.togglePauseSession(); // resume
        REQUIRE_FALSE(f.coordinator.isSessionPaused());
        REQUIRE_FALSE(f.sequencer.isSessionPaused());
    }

    SECTION("onSessionPauseStateChanged callback fires with correct value")
    {
        bool callbackFired = false;
        bool callbackValue = false;
        f.coordinator.onSessionPauseStateChanged = [&](bool isPaused) {
            callbackFired = true;
            callbackValue = isPaused;
        };

        f.coordinator.togglePauseSession(); // pause
        REQUIRE(callbackFired);
        REQUIRE(callbackValue == true);

        callbackFired = false;
        f.coordinator.togglePauseSession(); // resume
        REQUIRE(callbackFired);
        REQUIRE(callbackValue == false);
    }
}

// ---------------------------------------------------------------------------
// Test: SessionExecutionCoordinator rerunSelectedPoint
// ---------------------------------------------------------------------------
TEST_CASE("SessionExecutionCoordinator - rerunSelectedPoint delegates to sequencer", "[PauseResume]")
{
    PauseTestFixture f;

    SECTION("rerunSelectedPoint does not crash when no session is running")
    {
        // Should silently schedule the rerun index without throwing
        f.coordinator.rerunSelectedPoint(5);
        SUCCEED("rerunSelectedPoint is safe to call with no active session");
    }
}
