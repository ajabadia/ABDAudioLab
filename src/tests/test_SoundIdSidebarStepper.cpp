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
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::HardwareRouting) == SoundIdSidebarStepper::StepStatus::Current);
        CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::CalibrateLoopback) == SoundIdSidebarStepper::StepStatus::Pending);
        CHECK_FALSE(stepper.isCollapsed());
        CHECK(stepper.getDesiredWidth() == 240);
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
}
