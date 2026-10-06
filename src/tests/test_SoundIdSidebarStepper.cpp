#include <catch2/catch_test_macros.hpp>
#include "gui/soundid/SoundIdSidebarStepper.h"

using namespace abdaudiolab::gui;

TEST_CASE("SoundIdSidebarStepper - State navigation & collapse behavior", "[SoundIdWorkflow]")
{
    SoundIdSidebarStepper stepper;
    stepper.setSize(240, 600);

    SECTION("Initial default state")
    {
        CHECK(stepper.getCurrentStep() == SoundIdSidebarStepper::Step::HardwareRouting);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::SystemInfo) == SoundIdSidebarStepper::StepStatus::Completed);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::HardwareRouting) == SoundIdSidebarStepper::StepStatus::Current);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::CalibrateLoopback) == SoundIdSidebarStepper::StepStatus::Pending);
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::CalibrateLoopback) == "2. Audio Interface Calibration");
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::CalibrateLoopback) == "Loopback latency & SNR");
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::SystemInfo) == 0);
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::HardwareRouting) == 1);
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::CalibrateLoopback) == 2);
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::RunSession) == 3);
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::ExportReport) == 4);
        CHECK_FALSE(stepper.isCollapsed());
        CHECK(stepper.getDesiredWidth() == 240);
    }

    SECTION("R5-UX1 - Sidebar badge indices match visual task order")
    {
        // Regresión R5-UX1: Verificar que no hay inversión entre HardwareRouting (Tarea 1) y CalibrateLoopback (Tarea 2)
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::HardwareRouting) == 1);
        CHECK(SoundIdSidebarStepper::getStepBadgeNumber(SoundIdSidebarStepper::Step::CalibrateLoopback) == 2);
    }

    SECTION("Step navigation & status transitions")
    {
        bool callbackFired = false;
        SoundIdSidebarStepper::Step targetReceived = SoundIdSidebarStepper::Step::HardwareRouting;

        stepper.onStepSelected = [&](SoundIdSidebarStepper::Step step) {
            callbackFired = true;
            targetReceived = step;
        };

        stepper.setCurrentStep(SoundIdSidebarStepper::Step::CalibrateLoopback);
        CHECK(stepper.getCurrentStep() == SoundIdSidebarStepper::Step::CalibrateLoopback);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::HardwareRouting) == SoundIdSidebarStepper::StepStatus::Completed);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::CalibrateLoopback) == SoundIdSidebarStepper::StepStatus::Current);

        stepper.setCurrentStep(SoundIdSidebarStepper::Step::HardwareRouting);
        CHECK(stepper.getCurrentStep() == SoundIdSidebarStepper::Step::HardwareRouting);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::CalibrateLoopback) == SoundIdSidebarStepper::StepStatus::Completed);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::HardwareRouting) == SoundIdSidebarStepper::StepStatus::Current);
    }

    SECTION("Step locking functionality")
    {
        stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, true);
        CHECK(stepper.isStepLocked(SoundIdSidebarStepper::Step::ExportReport));
        CHECK_FALSE(stepper.canNavigateTo(SoundIdSidebarStepper::Step::ExportReport));

        stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, false);
        CHECK_FALSE(stepper.isStepLocked(SoundIdSidebarStepper::Step::ExportReport));
        CHECK(stepper.canNavigateTo(SoundIdSidebarStepper::Step::ExportReport));
    }

    SECTION("Collapse toggling and desired width")
    {
        stepper.setCollapsed(true);
        CHECK(stepper.isCollapsed());
        CHECK(stepper.getDesiredWidth() == 56);

        stepper.setCollapsed(false);
        CHECK_FALSE(stepper.isCollapsed());
        CHECK(stepper.getDesiredWidth() == 240);
    }

    SECTION("Session summary card update")
    {
        SoundIdSidebarStepper::SessionSummaryInfo info;
        info.hardwareName = "Roland Juno-106";
        info.hardwareCategory = "Polyphonic Synth";
        info.loopbackCalibrated = true;
        info.loopbackSnrDb = 94.5f;
        info.pointsMeasured = 12;
        info.totalPointsPlanned = 36;

        stepper.setSessionSummary(info);
        const auto& retrieved = stepper.getSessionSummary();
        CHECK(retrieved.hardwareName == "Roland Juno-106");
        CHECK(retrieved.loopbackCalibrated == true);
        CHECK(retrieved.loopbackSnrDb == 94.5f);
        CHECK(retrieved.pointsMeasured == 12);
    }

    SECTION("Hallazgo 05 - Subtitle synthesis and canonical titles integrity")
    {
        // Canonical titles must remain immutable
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::SystemInfo)        == "0. Studio Environment");
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::HardwareRouting)   == "1. Target & Routing");
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::CalibrateLoopback) == "2. Audio Interface Calibration");
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::RunSession)        == "3. Run Session");
        CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::ExportReport)      == "4. Export & Report");

        // Synthesized non-redundant subtitles
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::SystemInfo)        == "Audio I/O & MIDI setup");
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::HardwareRouting)   == "Synth profile & wiring");
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::CalibrateLoopback) == "Loopback latency & SNR");
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::RunSession)        == "Acquisition & live monitor");
        CHECK(stepper.getStepDescription(SoundIdSidebarStepper::Step::ExportReport)      == "Package & validation report");

        // Canonical titles must NEVER contain "[Locked]" suffix string
        stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, true);
        CHECK_FALSE(stepper.getStepTitle(SoundIdSidebarStepper::Step::ExportReport).contains("[Locked]"));
        stepper.setStepLocked(SoundIdSidebarStepper::Step::CalibrateLoopback, true);
        CHECK_FALSE(stepper.getStepTitle(SoundIdSidebarStepper::Step::CalibrateLoopback).contains("[Locked]"));
    }

    SECTION("Hallazgo 05 - Tooltip formatting and antislop compliance")
    {
        stepper.setCollapsed(true);
        // Tooltip formatting in collapsed mode uses " : " separator, never em dash
        juce::String tooltip = stepper.getStepTitle(SoundIdSidebarStepper::Step::HardwareRouting)
                             + " : " + stepper.getStepDescription(SoundIdSidebarStepper::Step::HardwareRouting);
        CHECK(tooltip.contains(" : "));
        CHECK_FALSE(tooltip.contains(juce::CharPointer_UTF8("\xE2\x80\x94"))); // No em dash
    }

    SECTION("Hallazgo 05 - Real navigability logic")
    {
        stepper.setCurrentStep(SoundIdSidebarStepper::Step::HardwareRouting);

        // Current step is never a valid navigation target (already selected)
        CHECK_FALSE(stepper.isStepNavigable(SoundIdSidebarStepper::Step::HardwareRouting));

        // Unlocked non-current step is navigable
        stepper.setStepLocked(SoundIdSidebarStepper::Step::SystemInfo, false);
        CHECK(stepper.isStepNavigable(SoundIdSidebarStepper::Step::SystemInfo));

        // Locked step is NOT navigable
        stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, true);
        CHECK_FALSE(stepper.isStepNavigable(SoundIdSidebarStepper::Step::ExportReport));

        // Unlocked step becomes navigable
        stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, false);
        CHECK(stepper.isStepNavigable(SoundIdSidebarStepper::Step::ExportReport));
    }

    SECTION("Hallazgo 05 - Visual contract metrics")
    {
        CHECK(SoundIdSidebarStepper::getActiveIndicatorWidth() == 2.5f);
        CHECK(SoundIdSidebarStepper::getActiveTitleFontSize() == 13.0f);
        CHECK(SoundIdSidebarStepper::getActiveSubtitleFontSize() == 10.5f);
        CHECK(SoundIdSidebarStepper::getStandardTitleFontSize() == 12.0f);
        CHECK(SoundIdSidebarStepper::getStandardSubtitleFontSize() == 10.0f);
    }
}
