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

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <crtdbg.h>
#include <cstdlib>
#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    // Suprimir cuadros de diálogo modales interactivos en runner no interactivo / CI / headless
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

    // Initialise JUCE GUI subsystem (MessageManager + COM/WASAPI) once for the
    // entire process. All TEST_CASEs share this single context; none should
    // create their own ScopedJuceInitialiser_GUI.
    juce::ScopedJuceInitialiser_GUI juceGuard;

    return Catch::Session().run(argc, argv);
}
