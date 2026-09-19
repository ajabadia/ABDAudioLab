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

core::SessionManifest makeManifest(int totalPoints = 0, int testCount = 0)
{
    core::SessionManifest m;
    m.formatVersion       = "1.0";
    m.hardwareId          = "hw_test";
    m.hardwareDisplayName = "Test Hardware";
    m.activeFunctionId    = "func_a";
    m.activeFunctionName  = "Function A";
    m.targetModule        = "MODULE_TEST";
    m.totalMeasuredPoints = totalPoints;

    for (int i = 0; i < testCount; ++i)
    {
        gui::TestConfiguration tc;
        tc.testName         = "Test " + std::to_string(i + 1);
        tc.stimulusType     = audio::StimulusType::LogFarinaSweep;
        tc.burstDurationSec = 1.0f;
        tc.captureMode      = "DirectAudio";
        m.tests.push_back(tc);
    }
    return m;
}

std::vector<exporting::MeasuredPoint> makePoints(int n)
{
    std::vector<exporting::MeasuredPoint> pts;
    for (int i = 0; i < n; ++i)
    {
        exporting::MeasuredPoint p;
        p.pointId = "P_" + std::to_string(i + 1);
        pts.push_back(p);
    }
    return pts;
}
} // namespace

// ============================================================
// Original characterization suite
// ============================================================

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

        std::vector<std::string> expectedOrder = {
            "setSessionData",
            "clearPlotterAndAddPoints",
            "updateDrawerAndEnvironment",
            "updateHardwarePanels",
            "rebuildTestSuiteQueue",
            "updateWorkflowAndNavigation"
        };
        REQUIRE(target.callOrder == expectedOrder);

        REQUIRE(target.lastDrawerData.hardwareDisplayName == "Korg MS-20 Mini");
        REQUIRE(target.lastDrawerData.operatorNotes == "Calibrated at 23C");
        REQUIRE(target.lastDrawerData.ambientTemperatureC == Catch::Approx(23.0f));
        REQUIRE(target.lastDrawerData.warmupTimeMinutes == 20);

        REQUIRE(target.lastHardwareData.hardwareId == "hw_korg_ms20");
        REQUIRE(target.lastHardwareData.activeFunctionId == "vcf_hpf_lpf");
        REQUIRE(target.lastHardwareData.hasValidHardwareContract == true);

        REQUIRE(target.lastReconstructedQueue.size() == 2);
        REQUIRE(target.lastReconstructedQueue[0].title == "LPF Sweep");
        REQUIRE(target.lastReconstructedQueue[0].badgeText == "FLT");
        REQUIRE(target.lastReconstructedQueue[0].status == gui::QueueItemStatus::Completed);
        REQUIRE(target.lastReconstructedQueue[1].title == "Saturation Step");
        REQUIRE(target.lastReconstructedQueue[1].badgeText == "SAT");
        REQUIRE(target.lastReconstructedQueue[1].status == gui::QueueItemStatus::Completed);

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
        core::SessionManifest badManifest;

        auto res = LoadedSessionApplier::apply(badManifest, points, false, cleanTarget);
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == SessionApplicationStatus::InvalidManifest);
        REQUIRE(cleanTarget.callOrder.empty());
    }
}

// ============================================================
// Contract completeness: idempotency, zero points, edge cases
// ============================================================

TEST_CASE("LoadedSessionApplier: Contract Completeness", "[LoadedSessionApplier]")
{
    SECTION("Valid manifest with zero measured points -> all 6 phases run, queue is Queued, workflow is Current")
    {
        auto manifest = makeManifest(3, 2);
        std::vector<exporting::MeasuredPoint> noPoints;

        MockLoadedSessionTarget target;
        auto res = LoadedSessionApplier::apply(manifest, noPoints, true, target);

        REQUIRE(res.succeeded());
        REQUIRE(res.pointsApplied == 0);
        REQUIRE(res.testsRestored == 2);
        REQUIRE(target.callOrder.size() == 6);

        REQUIRE(target.lastReconstructedQueue.size() == 2);
        REQUIRE(target.lastReconstructedQueue[0].status == gui::QueueItemStatus::Queued);
        REQUIRE(target.lastReconstructedQueue[1].status == gui::QueueItemStatus::Queued);

        REQUIRE(target.lastWorkflowState.isSessionComplete == false);
        REQUIRE(target.lastWorkflowState.targetStepperStep == WorkflowStepperBar::Step::RunSession);
        REQUIRE(target.lastWorkflowState.runSessionStatus == WorkflowStepperBar::StepStatus::Current);
    }

    SECTION("Multiple points are projected to plotter in input order exactly once")
    {
        auto manifest = makeManifest(5, 1);
        auto points   = makePoints(5);

        MockLoadedSessionTarget target;
        auto res = LoadedSessionApplier::apply(manifest, points, false, target);

        REQUIRE(res.succeeded());
        REQUIRE(res.pointsApplied == 5);
        REQUIRE(target.plottedPoints.size() == 5);

        for (int i = 0; i < 5; ++i)
            REQUIRE(target.plottedPoints[i].pointId == points[i].pointId);
    }

    SECTION("Empty test queue produces empty rebuilt queue without failure")
    {
        auto manifest = makeManifest(0, 0);
        std::vector<exporting::MeasuredPoint> noPoints;

        MockLoadedSessionTarget target;
        auto res = LoadedSessionApplier::apply(manifest, noPoints, false, target);

        REQUIRE(res.succeeded());
        REQUIRE(res.testsRestored == 0);
        REQUIRE(target.lastReconstructedQueue.empty());
        REQUIRE(target.callOrder.size() == 6);
    }

    SECTION("Applying the same manifest twice does not duplicate plotter or queue (idempotency)")
    {
        auto manifest = makeManifest(2, 2);
        auto points   = makePoints(2);

        MockLoadedSessionTarget target;

        auto res1 = LoadedSessionApplier::apply(manifest, points, false, target);
        auto res2 = LoadedSessionApplier::apply(manifest, points, false, target);

        REQUIRE(res1.succeeded());
        REQUIRE(res2.succeeded());

        // Each apply replaces, never appends
        REQUIRE(target.plottedPoints.size() == 2);
        REQUIRE(target.lastReconstructedQueue.size() == 2);

        // 6 calls per apply, 2 applies -> 12 total
        REQUIRE(target.callOrder.size() == 12);
    }

    SECTION("computeWorkflowState: totalMeasuredPoints=0 never marks session complete even with actual points")
    {
        auto state = LoadedSessionApplier::computeWorkflowState(0, 3);
        REQUIRE(state.isSessionComplete == false);
    }

    SECTION("Manifest with optional fields absent applies cleanly with empty strings")
    {
        core::SessionManifest partial;
        partial.formatVersion       = "1.0";
        partial.hardwareId          = "hw_basic";
        partial.hardwareDisplayName = "Basic Device";
        partial.activeFunctionId    = "func_default";
        partial.totalMeasuredPoints = 1;

        gui::TestConfiguration tc;
        tc.testName         = "Basic Test";
        tc.stimulusType     = audio::StimulusType::PinkNoise;
        tc.burstDurationSec = 0.5f;
        tc.captureMode      = "DirectAudio";
        partial.tests.push_back(tc);

        auto points = makePoints(1);

        MockLoadedSessionTarget target;
        auto res = LoadedSessionApplier::apply(partial, points, false, target);

        REQUIRE(res.succeeded());
        REQUIRE(res.pointsApplied == 1);
        REQUIRE(target.lastDrawerData.operatorNotes == "");
        REQUIRE(target.lastDrawerData.targetModule   == "");
        REQUIRE(target.callOrder.size() == 6);
    }

    SECTION("hasHardwareContract=false propagates to hardware phase data")
    {
        auto manifest = makeManifest(1, 1);
        auto points   = makePoints(1);

        MockLoadedSessionTarget target;
        auto res = LoadedSessionApplier::apply(manifest, points, false, target);

        REQUIRE(res.succeeded());
        REQUIRE(target.lastHardwareData.hasValidHardwareContract == false);
    }

    SECTION("reconstructQueueItems produces deterministic counter-based IDs across calls")
    {
        auto manifest = makeManifest(2, 3);

        auto items1 = LoadedSessionApplier::reconstructQueueItems(manifest.tests, true);
        auto items2 = LoadedSessionApplier::reconstructQueueItems(manifest.tests, true);

        REQUIRE(items1.size() == 3);
        REQUIRE(items2.size() == 3);

        for (size_t i = 0; i < items1.size(); ++i)
            REQUIRE(items1[i].id == items2[i].id);
    }

    SECTION("computeWorkflowState: exact point match marks session complete")
    {
        auto state = LoadedSessionApplier::computeWorkflowState(3, 3);
        REQUIRE(state.isSessionComplete == true);
        REQUIRE(state.targetStepperStep == WorkflowStepperBar::Step::ExportReport);
        REQUIRE(state.runSessionStatus  == WorkflowStepperBar::StepStatus::Completed);
    }

    SECTION("computeWorkflowState: surplus points also marks session complete")
    {
        auto state = LoadedSessionApplier::computeWorkflowState(2, 5);
        REQUIRE(state.isSessionComplete == true);
    }
}
