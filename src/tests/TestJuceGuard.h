/**
 * @file TestJuceGuard.h
 * @brief Guard helpers for Catch2 tests that require JUCE GUI/audio subsystems.
 *
 * Problem: juce::ScopedJuceInitialiser_GUI constructs and destructs the
 * juce::MessageManager singleton. In a Catch2 single-process runner, if a
 * previous TEST_CASE has already initialised and destroyed that singleton,
 * a subsequent ScopedJuceInitialiser_GUI may crash (SIGSEGV) on Windows
 * during COM/WASAPI re-initialisation.
 *
 * Fix: Use a PROCESS-LEVEL counter (not per-call-site) to detect re-entry.
 * The function getJuceGuiProcessInitCount() is inline with a static local,
 * guaranteed by C++17 to be shared across all translation units that include
 * this header.
 *
 * Tests that SKIP in the full suite MUST still pass in isolation:
 *   ABDAudioLab_Tests.exe "[ui_governance]"
 */
#pragma once

#include <catch2/catch_test_macros.hpp>
#include <atomic>

namespace abdaudiolab::test
{

/**
 * @brief Returns and increments the process-level JUCE GUI initialisation counter.
 *
 * The static local is shared across ALL translation units (C++17 inline ODR guarantee).
 * The first caller returns 1, subsequent callers return 2, 3, ...
 */
[[nodiscard]] inline int claimJuceGuiInitSlot()
{
    // Single process-level counter.
    // inline + static local = one instance across all TUs (C++17 §6.2, [basic.def.odr])
    static std::atomic<int> s_count { 0 };
    return ++s_count;
}

} // namespace abdaudiolab::test

// ---------------------------------------------------------------------------
// ABD_REQUIRE_JUCE_GUI_FRESH_PROCESS()
//
// Place BEFORE juce::ScopedJuceInitialiser_GUI in any TEST_CASE that:
//   (a) constructs a ScopedJuceInitialiser_GUI, AND
//   (b) creates JUCE audio/plugin objects (LabAudioEngine, AudioDeviceManager...)
//
// Only the FIRST test in the entire process to reach this macro proceeds.
// All subsequent callers (in any TU) receive SKIP, preventing SIGSEGV from
// COM/WASAPI singleton contamination.
// ---------------------------------------------------------------------------
#define ABD_REQUIRE_JUCE_GUI_FRESH_PROCESS()                                       \
    do {                                                                             \
        const int _abdSlot = abdaudiolab::test::claimJuceGuiInitSlot();             \
        if (_abdSlot > 1)                                                            \
        {                                                                            \
            SKIP("JUCE GUI subsystem: re-initialisation within the same process "   \
                 "is unsafe (COM/WASAPI singleton). Run in isolation to verify: "   \
                 "ABDAudioLab_Tests.exe \"[ui_governance]\"");                       \
        }                                                                            \
    } while (false)
