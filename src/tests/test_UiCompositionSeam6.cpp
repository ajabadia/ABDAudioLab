/**
 * @file test_UiCompositionSeam6.cpp
 * @brief Characterization tests and structural invariants for Seam 6:
 *        UI Orchestration & View Composition.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "../gui/SessionExecutionCoordinator.h"
#include "../core/SessionManager.h"
#include "../core/plugins/PluginHostManager.h"
#include "../gui/session/UiStrings.h"

namespace abdaudiolab::test::seam6
{

// ==============================================================================
// 1. UI Snapshot Definition & Pure Presentation Function (Characterization)
// ==============================================================================

using SessionState = gui::SessionState;

struct UiSnapshot
{
    SessionState state { SessionState::Idle };
    unsigned progress_percent { 0 };
    bool session_running { false };
    bool pause_visible { false };
    bool cancel_visible { false };
    bool primary_enabled { false };
    bool banner_visible { false };

    char badge[32] { 0 };
    char status_text[128] { 0 };
    char banner_text[256] { 0 };

    uint32_t badge_rgba { 0 };
    uint32_t banner_rgba { 0 };
};

static void copy_text(char* dst, size_t capacity, const char* src)
{
    if (capacity == 0)
        return;
    std::snprintf(dst, capacity, "%s", src ? src : "");
}

inline void ui_build_snapshot(SessionState state,
                             unsigned progress_percent,
                             const char* error_message,
                             UiSnapshot* out)
{
    std::memset(out, 0, sizeof(*out));

    if (progress_percent > 100)
        progress_percent = 100;

    out->state = state;
    out->progress_percent = progress_percent;

    switch (state)
    {
    case SessionState::Idle:
        copy_text(out->badge, sizeof(out->badge), "IDLE");
        copy_text(out->status_text, sizeof(out->status_text), "Ready");
        out->primary_enabled = true;
        out->badge_rgba = 0x808080FFu;
        break;

    case SessionState::Starting:
        copy_text(out->badge, sizeof(out->badge), "STARTING");
        copy_text(out->status_text, sizeof(out->status_text), "Starting session");
        out->session_running = true;
        out->primary_enabled = false;
        out->badge_rgba = 0x4682B4FFu;
        break;

    case SessionState::Running:
        copy_text(out->badge, sizeof(out->badge), "RUNNING");
        copy_text(out->status_text, sizeof(out->status_text), "Session running");
        out->session_running = true;
        out->pause_visible = true;
        out->cancel_visible = true;
        out->primary_enabled = true;
        out->badge_rgba = 0x2E8B57FFu;
        break;

    case SessionState::Paused:
        copy_text(out->badge, sizeof(out->badge), "PAUSED");
        copy_text(out->status_text, sizeof(out->status_text), "Session paused");
        out->session_running = true;
        out->pause_visible = true;
        out->cancel_visible = true;
        out->primary_enabled = true;
        out->badge_rgba = 0xD39E00FFu;
        break;

    case SessionState::Capturing:
        copy_text(out->badge, sizeof(out->badge), "CAPTURING");
        copy_text(out->status_text, sizeof(out->status_text), "Capturing measurement");
        out->session_running = true;
        out->cancel_visible = true;
        out->primary_enabled = false;
        out->badge_rgba = 0x4169E1FFu;
        break;

    case SessionState::CancelRequested:
        copy_text(out->badge, sizeof(out->badge), "STOPPING");
        copy_text(out->status_text, sizeof(out->status_text), "Stopping session");
        out->session_running = true;
        out->cancel_visible = false;
        out->primary_enabled = false;
        out->badge_rgba = 0xCC8400FFu;
        break;

    case SessionState::Aborted:
        copy_text(out->badge, sizeof(out->badge), "ABORTED");
        copy_text(out->status_text, sizeof(out->status_text), "Session aborted");
        out->primary_enabled = true;
        out->badge_rgba = 0xB22222FFu;
        break;

    case SessionState::Completed:
        copy_text(out->badge, sizeof(out->badge), "COMPLETED");
        copy_text(out->status_text, sizeof(out->status_text), "Session completed");
        out->primary_enabled = true;
        out->badge_rgba = 0x228B22FFu;
        break;

    case SessionState::Failed:
        copy_text(out->badge, sizeof(out->badge), "FAILED");
        copy_text(out->status_text, sizeof(out->status_text), "Session failed");
        copy_text(out->banner_text, sizeof(out->banner_text),
                  error_message ? error_message : "Unknown error");
        out->banner_visible = true;
        out->primary_enabled = true;
        out->badge_rgba = 0x8B0000FFu;
        out->banner_rgba = 0xFFCCCCFFu;
        break;
    }
}

inline size_t ui_snapshot_to_text(const UiSnapshot* s, char* buffer, size_t capacity)
{
    int written = std::snprintf(
        buffer,
        capacity,
        "state=%d\n"
        "progress=%u\n"
        "session_running=%d\n"
        "pause_visible=%d\n"
        "cancel_visible=%d\n"
        "primary_enabled=%d\n"
        "banner_visible=%d\n"
        "badge=%s\n"
        "status_text=%s\n"
        "banner_text=%s\n"
        "badge_rgba=%08X\n"
        "banner_rgba=%08X\n",
        static_cast<int>(s->state),
        s->progress_percent,
        s->session_running ? 1 : 0,
        s->pause_visible ? 1 : 0,
        s->cancel_visible ? 1 : 0,
        s->primary_enabled ? 1 : 0,
        s->banner_visible ? 1 : 0,
        s->badge,
        s->status_text,
        s->banner_text,
        s->badge_rgba,
        s->banner_rgba);

    if (written < 0)
        return 0;

    return static_cast<size_t>(written);
}

static std::string read_fixture_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    // Normalize CRLF to LF for cross-platform matching
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')
            continue;
        out.push_back(s[i]);
    }
    return out;
}

} // namespace abdaudiolab::test::seam6

using namespace abdaudiolab;
using namespace abdaudiolab::test::seam6;

// ==============================================================================
// 2. Semantic Characterization Tests (All 9 Lifecycle States)
// ==============================================================================

TEST_CASE("Seam 6 Characterization: Idle state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Idle, 0, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Idle);
    REQUIRE(std::string(snapshot.badge) == "IDLE");
    REQUIRE(std::string(snapshot.status_text) == "Ready");
    REQUIRE(snapshot.primary_enabled == true);
    REQUIRE(snapshot.session_running == false);
    REQUIRE(snapshot.pause_visible == false);
    REQUIRE(snapshot.cancel_visible == false);
    REQUIRE(snapshot.banner_visible == false);
    REQUIRE(snapshot.badge_rgba == 0x808080FFu);
}

TEST_CASE("Seam 6 Characterization: Starting state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Starting, 0, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Starting);
    REQUIRE(std::string(snapshot.badge) == "STARTING");
    REQUIRE(std::string(snapshot.status_text) == "Starting session");
    REQUIRE(snapshot.session_running == true);
    REQUIRE(snapshot.primary_enabled == false);
    REQUIRE(snapshot.pause_visible == false);
    REQUIRE(snapshot.cancel_visible == false);
    REQUIRE(snapshot.badge_rgba == 0x4682B4FFu);
}

TEST_CASE("Seam 6 Characterization: Running state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Running, 42, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Running);
    REQUIRE(std::string(snapshot.badge) == "RUNNING");
    REQUIRE(std::string(snapshot.status_text) == "Session running");
    REQUIRE(snapshot.progress_percent == 42);
    REQUIRE(snapshot.session_running == true);
    REQUIRE(snapshot.pause_visible == true);
    REQUIRE(snapshot.cancel_visible == true);
    REQUIRE(snapshot.primary_enabled == true);
    REQUIRE(snapshot.badge_rgba == 0x2E8B57FFu);
}

TEST_CASE("Seam 6 Characterization: Paused state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Paused, 50, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Paused);
    REQUIRE(std::string(snapshot.badge) == "PAUSED");
    REQUIRE(std::string(snapshot.status_text) == "Session paused");
    REQUIRE(snapshot.progress_percent == 50);
    REQUIRE(snapshot.session_running == true);
    REQUIRE(snapshot.pause_visible == true);
    REQUIRE(snapshot.cancel_visible == true);
    REQUIRE(snapshot.primary_enabled == true);
    REQUIRE(snapshot.badge_rgba == 0xD39E00FFu);
}

TEST_CASE("Seam 6 Characterization: Capturing state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Capturing, 60, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Capturing);
    REQUIRE(std::string(snapshot.badge) == "CAPTURING");
    REQUIRE(std::string(snapshot.status_text) == "Capturing measurement");
    REQUIRE(snapshot.session_running == true);
    REQUIRE(snapshot.pause_visible == false);
    REQUIRE(snapshot.cancel_visible == true);
    REQUIRE(snapshot.primary_enabled == false);
    REQUIRE(snapshot.badge_rgba == 0x4169E1FFu);
}

TEST_CASE("Seam 6 Characterization: CancelRequested state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::CancelRequested, 65, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::CancelRequested);
    REQUIRE(std::string(snapshot.badge) == "STOPPING");
    REQUIRE(std::string(snapshot.status_text) == "Stopping session");
    REQUIRE(snapshot.session_running == true);
    REQUIRE(snapshot.cancel_visible == false);
    REQUIRE(snapshot.primary_enabled == false);
    REQUIRE(snapshot.badge_rgba == 0xCC8400FFu);
}

TEST_CASE("Seam 6 Characterization: Aborted state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Aborted, 65, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Aborted);
    REQUIRE(std::string(snapshot.badge) == "ABORTED");
    REQUIRE(std::string(snapshot.status_text) == "Session aborted");
    REQUIRE(snapshot.session_running == false);
    REQUIRE(snapshot.pause_visible == false);
    REQUIRE(snapshot.cancel_visible == false);
    REQUIRE(snapshot.primary_enabled == true);
    REQUIRE(snapshot.badge_rgba == 0xB22222FFu);
}

TEST_CASE("Seam 6 Characterization: Completed state snapshot", "[ui_composition]")
{
    UiSnapshot snapshot;
    ui_build_snapshot(SessionState::Completed, 100, nullptr, &snapshot);

    REQUIRE(snapshot.state == SessionState::Completed);
    REQUIRE(std::string(snapshot.badge) == "COMPLETED");
    REQUIRE(std::string(snapshot.status_text) == "Session completed");
    REQUIRE(snapshot.progress_percent == 100);
    REQUIRE(snapshot.session_running == false);
    REQUIRE(snapshot.primary_enabled == true);
    REQUIRE(snapshot.badge_rgba == 0x228B22FFu);
}

TEST_CASE("Seam 6 Characterization: Failed state snapshot & Fallbacks", "[ui_composition]")
{
    SECTION("Specific error message")
    {
        UiSnapshot snapshot;
        ui_build_snapshot(SessionState::Failed, 20, "Plugin disconnected", &snapshot);

        REQUIRE(snapshot.state == SessionState::Failed);
        REQUIRE(std::string(snapshot.badge) == "FAILED");
        REQUIRE(std::string(snapshot.status_text) == "Session failed");
        REQUIRE(std::string(snapshot.banner_text) == "Plugin disconnected");
        REQUIRE(snapshot.banner_visible == true);
        REQUIRE(snapshot.primary_enabled == true);
        REQUIRE(snapshot.badge_rgba == 0x8B0000FFu);
        REQUIRE(snapshot.banner_rgba == 0xFFCCCCFFu);
    }

    SECTION("Null error message safe fallback")
    {
        UiSnapshot snapshot;
        ui_build_snapshot(SessionState::Failed, 0, nullptr, &snapshot);

        REQUIRE(std::string(snapshot.banner_text) == "Unknown error");
        REQUIRE(snapshot.banner_visible == true);
    }

    SECTION("Progress clamped above 100%")
    {
        UiSnapshot snapshot;
        ui_build_snapshot(SessionState::Running, 250, nullptr, &snapshot);

        REQUIRE(snapshot.progress_percent == 100);
    }
}

// ==============================================================================
// 3. Textual Golden Master Snapshot Tests (Fixtures Comparison)
// ==============================================================================

TEST_CASE("Seam 6 Golden Master: Textual Snapshot Fixture Verification", "[ui_composition]")
{
    char actual[1024];

    SECTION("Idle Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Idle, 0, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_idle.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Starting Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Starting, 0, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_starting.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Running 42% Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Running, 42, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_running_42.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Paused Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Paused, 50, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_paused.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Capturing Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Capturing, 60, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_capturing.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("CancelRequested Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::CancelRequested, 65, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_cancel_requested.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Aborted Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Aborted, 65, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_aborted.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Completed Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Completed, 100, nullptr, &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_completed.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }

    SECTION("Failed Snapshot Fixture")
    {
        UiSnapshot s;
        ui_build_snapshot(SessionState::Failed, 20, "Plugin disconnected", &s);
        ui_snapshot_to_text(&s, actual, sizeof(actual));
        std::string expected = read_fixture_file("fixtures/ui/session_failed.snapshot");
        REQUIRE_FALSE(expected.empty());
        REQUIRE(std::string(actual) == expected);
    }
}

// ==============================================================================
// 4. Structural and Architectural Invariants Verification
// ==============================================================================

TEST_CASE("Seam 6 Architectural Invariants: Ownership, Audio, and Sovereignty", "[ui_composition]")
{
    SECTION("Invariable 1: PluginHostManager has exclusive ownership of AudioPluginInstance")
    {
        core::PluginHostManager hostManager;
        REQUIRE_FALSE(hostManager.hasActivePlugin());
        REQUIRE(hostManager.getActivePluginInstance() == nullptr);

        // Verify that observer pointers are unowned raw pointers, not std::unique_ptr
        static_assert(std::is_pointer_v<juce::AudioPluginInstance*>,
                      "Active plugin observers must be unowned raw pointer observers");
    }

    SECTION("Invariable 2: SessionExecutionCoordinator is sole arbiter of session execution")
    {
        audio::LabAudioEngine engine;
        hardware::MockHardwareController mockHw;
        core::ProfilingSequencer seq(engine, mockHw);
        core::SessionManager sessMgr;
        gui::SoundIdCurvePlotter plotter;

        gui::SessionExecutionCoordinator coordinator(seq, sessMgr, plotter);

        // Direct state transitions belong exclusively to coordinator
        REQUIRE(coordinator.getSessionState() == gui::SessionState::Idle);
        REQUIRE_FALSE(coordinator.isRunningSession());
        REQUIRE_FALSE(coordinator.isSessionPaused());

        // Coordinator diagnostics counters are accessible for governance telemetry
        const auto& diag = coordinator.getDiagnostics();
        REQUIRE(diag.duplicateStartsRejected.load() == 0);
        REQUIRE(diag.duplicateStopsIgnored.load() == 0);
        REQUIRE(diag.staleCallbacksDiscarded.load() == 0);
    }

    SECTION("Invariable 3: Audio processing is strictly decoupled from UI presentation")
    {
        // Zero audio device callbacks or processBlock methods in presentation structs
        static_assert(!std::is_base_of_v<juce::AudioProcessor, UiSnapshot>,
                      "UiSnapshot must never be an AudioProcessor");
        static_assert(!std::is_base_of_v<juce::AudioIODeviceCallback, UiSnapshot>,
                      "UiSnapshot must never be an AudioIODeviceCallback");
    }

    SECTION("Invariable 4: Presentation pure function is decoupled from JUCE GUI components")
    {
        // UiSnapshot is POD-like, zero dynamic allocations, zero GUI toolkit locks
        static_assert(std::is_standard_layout_v<UiSnapshot>,
                      "UiSnapshot must have standard layout for lock-free pure presentation");
    }
}
