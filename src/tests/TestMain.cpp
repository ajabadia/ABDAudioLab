/**
 * @file TestMain.cpp
 * @brief Custom Catch2 test runner entry point for ABDAudioLab_Tests.
 *
 * JUCE LIFECYCLE POLICY:
 *   juce::ScopedJuceInitialiser_GUI initializes the MessageManager and
 *   COM/WASAPI subsystems exactly ONCE for the entire process lifetime.
 *   Individual TEST_CASEs must NOT create their own ScopedJuceInitialiser_GUI.
 *
 * MIGRATION STATUS:
 *   - New tests: do NOT use ScopedJuceInitialiser_GUI in TEST_CASE bodies.
 *   - Existing tests: being migrated. During transition, ABD_REQUIRE_JUCE_GUI_FRESH_PROCESS()
 *     in TestJuceGuard.h provides skip-guard for legacy patterns.
 *
 * IMPORTANT: This file must be compiled instead of Catch2WithMain.
 *   CMakeLists.txt: link Catch2::Catch2 (not Catch2::Catch2WithMain)
 */

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main(int argc, char* argv[])
{
    // Initialise JUCE GUI subsystem (MessageManager + COM/WASAPI) once for the
    // entire process. All TEST_CASEs share this single context; none should
    // create their own ScopedJuceInitialiser_GUI.
    juce::ScopedJuceInitialiser_GUI juceGuard;

    return Catch::Session().run(argc, argv);
}
