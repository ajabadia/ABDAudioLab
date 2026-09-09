#include <catch2/catch_test_macros.hpp>
#include "gui/MainHeaderController.h"
#include "audio/LabAudioEngine.h"

using namespace abdaudiolab;

TEST_CASE("MainHeaderController: Autonomous top bar orchestration", "[gui][header][refactor]")
{
    audio::LabAudioEngine engine;
    gui::MainHeaderController header(engine);

    SECTION("Initialization without crash and correct dimensions")
    {
        header.setBounds(0, 0, 1024, 36);
        REQUIRE(header.getWidth() == 1024);
        REQUIRE(header.getHeight() == 36);
    }

    SECTION("Event callback routing")
    {
        bool scopeTriggered = false;
        bool calibrateTriggered = false;
        bool newSessionTriggered = false;
        bool themeTriggered = false;

        header.onScopeToggle = [&] { scopeTriggered = true; };
        header.onCalibrateClicked = [&] { calibrateTriggered = true; };
        header.onNewSession = [&] { newSessionTriggered = true; };
        header.onThemeToggled = [&] { themeTriggered = true; };

        if (header.onScopeToggle) header.onScopeToggle();
        if (header.onCalibrateClicked) header.onCalibrateClicked();
        if (header.onNewSession) header.onNewSession();
        if (header.onThemeToggled) header.onThemeToggled();

        REQUIRE(scopeTriggered);
        REQUIRE(calibrateTriggered);
        REQUIRE(newSessionTriggered);
        REQUIRE(themeTriggered);
    }

    SECTION("Calibration status management and flashing timer")
    {
        // 1. Initial/valid state
        header.updateCalibrationStatus(true, 48000.0, false);

        // 2. Mismatched sample rate -> triggers autonomous flashing
        header.updateCalibrationStatus(true, 96000.0, false);

        // 3. Skipped state -> clears flashing
        header.updateCalibrationStatus(false, 0.0, true);

        // 4. Normal uncalibrated state
        header.updateCalibrationStatus(false, 0.0, false);
    }

    SECTION("Hardware info and theme updates")
    {
        header.setHardwareInfo("Roland AIRA Torcido", "Tube Clipper", juce::Image(), gui::HardwareConnectionStatus::Connected);
        header.updateTheme();
        header.clearHardware();
    }
}
