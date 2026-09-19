/**
 * @file test_LoadedSessionApplier.cpp
 * @brief Characterization and regression test suite for LoadedSessionApplier.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/controllers/LoadedSessionApplier.h"
#include <vector>
#include <string>

using namespace abdaudiolab;
using namespace abdaudiolab::gui;

namespace
{
class MockLoadedSessionTarget : public ILoadedSessionTarget
{
public:
    std::vector<std::string> callOrder;

    core::SessionManifest lastManifest;
    std::vector<exporting::MeasuredPoint> lastPoints;
    std::vector<exporting::MeasuredPoint> plottedPoints;
    SessionUiPresentationData lastDrawerData;
    SessionUiPresentationData lastHardwareData;
    std::vector<gui::QueueItem> lastReconstructedQueue;
    WorkflowStepState lastWorkflowState;

    void setSessionData(const core::SessionManifest& manifest,
                        const std::vector<exporting::MeasuredPoint>& points) override
    {
        callOrder.push_back("setSessionData");
        lastManifest = manifest;
        lastPoints = points;
    }

    void clearPlotterAndAddPoints(const std::vector<exporting::MeasuredPoint>& points) override
    {
        callOrder.push_back("clearPlotterAndAddPoints");
        plottedPoints = points;
    }

    void updateDrawerAndEnvironment(const SessionUiPresentationData& data) override
    {
        callOrder.push_back("updateDrawerAndEnvironment");
        lastDrawerData = data;
    }

    void updateHardwarePanels(const SessionUiPresentationData& data) override
    {
        callOrder.push_back("updateHardwarePanels");
        lastHardwareData = data;
    }

    virtual void rebuildTestSuiteQueue(const std::vector<core::SessionManifest>&,
                                       const std::vector<gui::QueueItem>& items) override
    {
        callOrder.push_back("rebuildTestSuiteQueue");
        lastReconstructedQueue = items;
    }

    void updateWorkflowAndNavigation(const WorkflowStepState& workflowState) override
    {
        callOrder.push_back("updateWorkflowAndNavigation");
        lastWorkflowState = workflowState;
    }
};
} // namespace

TEST_CASE("LoadedSessionApplier: Target Order and Data Projection", "[LoadedSessionApplier]")
{
    MockLoadedSessionTarget target;

    core::SessionManifest manifest;
    manifest.formatVersion = "1.0";
    manifest.hardwareId = "hw_korg_ms20";
    manifest.hardwareDisplayName = "Korg MS-20 Mini";
    manifest.activeFunctionId = "vcf_hpf_lpf";
    manifest.activeFunctionName = "Dual Filter";
    manifest.targetModule = "ANALOG_FILTER";
    manifest.operatorNotes = "Calibrated at 23C";
    manifest.ambientTemperatureC = 23.0f;
    manifest.warmupTimeMinutes = 20;

    gui::TestConfiguration tc1;
    tc1.testName = "LPF Sweep";
    tc1.stimulusType = audio::StimulusType::LogFarinaSweep;
    tc1.burstDurationSec = 1.0f;
    tc1.captureMode = "DirectAudio";
    manifest.tests.push_back(tc1);

    gui::TestConfiguration tc2;
    tc2.testName = "Saturation Step";
    tc2.stimulusType = audio::StimulusType::AmplitudeRamp;
    tc2.burstDurationSec = 0.5f;
    tc2.captureMode = "DirectAudio";
    manifest.tests.push_back(tc2);

    manifest.totalMeasuredPoints = 2;

    std::vector<exporting::MeasuredPoint> points;
    exporting::MeasuredPoint p1;
    p1.pointId = "P_001";
    p1.testId = "LPF Sweep";
    points.push_back(p1);

    exporting::MeasuredPoint p2;
    p2.pointId = "P_002";
    p2.testId = "Saturation Step";
    points.push_back(p2);

    SECTION("Applies all components in strict deterministic order without partial mutation on failure")
    {
        auto res = LoadedSessionApplier::apply(manifest, points, true, target);
        REQUIRE(res.succeeded());
        REQUIRE(res.pointsApplied == 2);
        REQUIRE(res.testsRestored == 2);

        // Verify exact call order
        std::vector<std::string> expectedOrder = {
            "setSessionData",
            "clearPlotterAndAddPoints",
            "updateDrawerAndEnvironment",
            "updateHardwarePanels",
            "rebuildTestSuiteQueue",
            "updateWorkflowAndNavigation"
        };
        REQUIRE(target.callOrder == expectedOrder);

        // Verify Drawer projection
        REQUIRE(target.lastDrawerData.hardwareDisplayName == "Korg MS-20 Mini");
        REQUIRE(target.lastDrawerData.operatorNotes == "Calibrated at 23C");
        REQUIRE(target.lastDrawerData.ambientTemperatureC == Catch::Approx(23.0f));
        REQUIRE(target.lastDrawerData.warmupTimeMinutes == 20);

        // Verify Hardware Panels
        REQUIRE(target.lastHardwareData.hardwareId == "hw_korg_ms20");
        REQUIRE(target.lastHardwareData.activeFunctionId == "vcf_hpf_lpf");
        REQUIRE(target.lastHardwareData.hasValidHardwareContract == true);

        // Verify Reconstructed Queue
        REQUIRE(target.lastReconstructedQueue.size() == 2);
        REQUIRE(target.lastReconstructedQueue[0].title == "LPF Sweep");
        REQUIRE(target.lastReconstructedQueue[0].badgeText == "FLT");
        REQUIRE(target.lastReconstructedQueue[0].status == gui::QueueItemStatus::Completed);
        REQUIRE(target.lastReconstructedQueue[1].title == "Saturation Step");
        REQUIRE(target.lastReconstructedQueue[1].badgeText == "SAT");
        REQUIRE(target.lastReconstructedQueue[1].status == gui::QueueItemStatus::Completed);

        // Verify Workflow state (Session complete -> ExportReport)
        REQUIRE(target.lastWorkflowState.isSessionComplete == true);
        REQUIRE(target.lastWorkflowState.targetStepperStep == WorkflowStepperBar::Step::ExportReport);
        REQUIRE(target.lastWorkflowState.targetSidebarStep == SoundIdSidebarStepper::Step::ExportReport);
    }

    SECTION("Incomplete session transitions to RunSession step")
    {
        std::vector<exporting::MeasuredPoint> singlePoint = { p1 };
        auto res = LoadedSessionApplier::apply(manifest, singlePoint, true, target);
        REQUIRE(res.succeeded());

        REQUIRE(target.lastWorkflowState.isSessionComplete == false);
        REQUIRE(target.lastWorkflowState.targetStepperStep == WorkflowStepperBar::Step::RunSession);
        REQUIRE(target.lastWorkflowState.targetSidebarStep == SoundIdSidebarStepper::Step::RunSession);
        REQUIRE(target.lastWorkflowState.runSessionStatus == WorkflowStepperBar::StepStatus::Completed);
    }

    SECTION("Invalid manifest rejects immediately without mutating targets")
    {
        MockLoadedSessionTarget cleanTarget;
        core::SessionManifest badManifest; // Empty formatVersion and displayName

        auto res = LoadedSessionApplier::apply(badManifest, points, false, cleanTarget);
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == SessionApplicationStatus::InvalidManifest);
        REQUIRE(cleanTarget.callOrder.empty()); // Zero calls made!
    }
}
