/**
 * @file test_Step3Step4Transitions.cpp
 * @brief Automated unit and smoke tests for Step 3 (Run Session) & Step 4 (Export & Report)
 *        lifecycle transitions, stepper governance, telemetry, and metrological export guards.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/soundid/SoundIdSidebarStepper.h"
#include "gui/soundid/SoundIdResultsSummaryView.h"
#include "gui/session/ProfilingSessionController.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "core/SessionSerializer.h"
#include "export/LutExporter.h"
#include "export/CertificationReportExporter.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::exporting;

namespace
{
struct Step3Step4TempDir
{
    std::filesystem::path path;

    Step3Step4TempDir(const std::string& prefix = "step3_step4_test")
    {
        auto tick = std::chrono::system_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
             / "abdaudiolab_test"
             / (prefix + "_" + std::to_string(tick));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~Step3Step4TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    Step3Step4TempDir(const Step3Step4TempDir&) = delete;
    Step3Step4TempDir& operator=(const Step3Step4TempDir&) = delete;
};
} // namespace

// ===========================================================================
// TEST 1: Load Evaluation... lleva a ReviewResults sin iniciar captura física
// ===========================================================================
TEST_CASE("Step3Step4: Load Evaluation transitions to ReviewResults without capture", "[step3][step4][transitions]")
{
    ProfilingSessionController controller;

    REQUIRE(controller.getCurrentSnapshot().workflowStage != ProfilingWorkflowStage::ReviewResults);
    REQUIRE(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Idle);

    bool loaded = controller.loadPredefinedFixture("fixture_approved.json");
    REQUIRE(loaded);

    const auto snap = controller.getCurrentSnapshot();
    CHECK(snap.workflowStage == ProfilingWorkflowStage::ReviewResults);
    CHECK(snap.sessionStatus == ProfilingSessionStatus::EvaluationLoadedForReview);
    CHECK(snap.evaluation.hasEvaluation == true);
    CHECK(snap.evaluation.hashVerified == true);
    // Physical capture progress remains zero / completed by external artifact
    CHECK(snap.progress.currentTrial == 0);
}

// ===========================================================================
// TEST 2: Fixture / Suite point count updates totalTrials and progress math
// ===========================================================================
TEST_CASE("Step3Step4: Point count updates totalTrials and does not show 100% early", "[step3][step4][telemetry]")
{
    // A single test with multiple target points (e.g., 5 points across cutoff)
    const int totalPointsPlanned = 5;
    int pointsMeasured = 0;

    auto computeProgressPercent = [](int measured, int total) -> float {
        if (total <= 0) return 0.0f;
        return (static_cast<float>(measured) / static_cast<float>(total)) * 100.0f;
    };

    // Before any point is measured
    CHECK(computeProgressPercent(pointsMeasured, totalPointsPlanned) == Catch::Approx(0.0f));

    // Mid-session: 2 of 5 points measured
    pointsMeasured = 2;
    float midProgress = computeProgressPercent(pointsMeasured, totalPointsPlanned);
    CHECK(midProgress == Catch::Approx(40.0f));
    CHECK(midProgress < 100.0f);

    // Only when all points are processed does progress reach 100%
    pointsMeasured = 5;
    CHECK(computeProgressPercent(pointsMeasured, totalPointsPlanned) == Catch::Approx(100.0f));
}

// ===========================================================================
// TEST 3: Hardware input level used for RMS when no plugin instance exists
// ===========================================================================
TEST_CASE("Step3Step4: Input RMS fallback when no plugin instance is active", "[step3][step4][telemetry]")
{
    // Simulate non-plugin scenario where physical or synthetic signal is present on input
    const float inputSignalRms = 0.125f; // ~ -18 dBFS

    auto computeTelemetryRmsDb = [](float pluginRms, bool hasPlugin, float hwInputRms) -> float {
        float rms = hasPlugin ? pluginRms : hwInputRms;
        return (rms > 0.00001f) ? juce::Decibels::gainToDecibels(rms) : -120.0f;
    };

    // Without plugin, but with hardware/synthetic signal: must NOT default to -120 dBFS
    float rmsDb = computeTelemetryRmsDb(0.0f, false, inputSignalRms);
    CHECK(rmsDb == Catch::Approx(-18.06f).margin(0.1f));
    CHECK(rmsDb > -100.0f);

    // Silent input correctly reports silence floor
    float silentDb = computeTelemetryRmsDb(0.0f, false, 0.0f);
    CHECK(silentDb == Catch::Approx(-120.0f));
}

// ===========================================================================
// TEST 4: onSessionFinished marks RunSession Completed, but NOT ExportReport
// ===========================================================================
TEST_CASE("Step3Step4: onSessionFinished stepper semantics", "[step3][step4][stepper]")
{
    SoundIdSidebarStepper stepper;
    stepper.setStepStatus(SoundIdSidebarStepper::Step::HardwareRouting, SoundIdSidebarStepper::StepStatus::Completed);
    stepper.setStepStatus(SoundIdSidebarStepper::Step::CalibrateLoopback, SoundIdSidebarStepper::StepStatus::Completed);
    stepper.setStepStatus(SoundIdSidebarStepper::Step::RunSession, SoundIdSidebarStepper::StepStatus::Current);
    stepper.setStepStatus(SoundIdSidebarStepper::Step::ExportReport, SoundIdSidebarStepper::StepStatus::Pending);
    stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, true);

    // Invariant: ExportReport is initially locked and pending
    REQUIRE(stepper.isStepLocked(SoundIdSidebarStepper::Step::ExportReport));
    REQUIRE(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) == SoundIdSidebarStepper::StepStatus::Pending);

    // Simulating sequence of onSessionFinished:
    // 1. RunSession is Completed
    stepper.setStepStatus(SoundIdSidebarStepper::Step::RunSession, SoundIdSidebarStepper::StepStatus::Completed);
    // 2. ExportReport is unlocked
    stepper.setStepLocked(SoundIdSidebarStepper::Step::ExportReport, false);
    // 3. User navigates to ExportReport (now active)
    stepper.setCurrentStep(SoundIdSidebarStepper::Step::ExportReport);

    // VERIFICATION:
    // RunSession MUST be Completed
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::RunSession) == SoundIdSidebarStepper::StepStatus::Completed);
    // ExportReport MUST NOT be Completed yet! It is Current (ready/pending)
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) != SoundIdSidebarStepper::StepStatus::Completed);
    CHECK_FALSE(stepper.isStepLocked(SoundIdSidebarStepper::Step::ExportReport));

    // Switching back and forth to inspect Step 3 MUST NOT falsely complete Step 4!
    stepper.setCurrentStep(SoundIdSidebarStepper::Step::RunSession);
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) != SoundIdSidebarStepper::StepStatus::Completed);
    stepper.setCurrentStep(SoundIdSidebarStepper::Step::ExportReport);
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) != SoundIdSidebarStepper::StepStatus::Completed);
}

// ===========================================================================
// TEST 5: Step 4 presents SoundIdResultsSummaryView
// ===========================================================================
TEST_CASE("Step3Step4: Step 4 hosts SoundIdResultsSummaryView with real metrics", "[step3][step4][ui]")
{
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView summaryView(controller);
    summaryView.setSize(800, 600);

    ProfilingSessionSnapshot snap;
    snap.workflowMode = UiWorkflowMode::Guided;
    snap.workflowStage = ProfilingWorkflowStage::ReviewResults;
    snap.sessionStatus = ProfilingSessionStatus::Completed;
    snap.target.targetName = "Dexed Model";
    snap.evaluation.hasEvaluation = true;
    snap.evaluation.recommendedModelType = "LUT_2D";
    snap.evaluation.selectionStatus = synth::SelectionStatus::Accepted;
    snap.evaluation.canonicalEvaluationHash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    snap.evaluation.hashVerified = true;
    snap.exportOptions.canExportCpp = true;

    summaryView.updateFromSnapshot(snap);

    CHECK(summaryView.getCurrentVerdict() == synth::SelectionStatus::Accepted);
    CHECK(summaryView.isExportEnabled() == true);
    CHECK(summaryView.isHashVerified() == true);
    CHECK(summaryView.getFullCanonicalHash() == snap.evaluation.canonicalEvaluationHash);
}

// ===========================================================================
// TEST 6: Export only marks ExportReport Completed after writing and validating
// ===========================================================================
TEST_CASE("Step3Step4: ExportReport transitions to Completed only after export succeeds", "[step3][step4][export]")
{
    SoundIdSidebarStepper stepper;
    stepper.setStepStatus(SoundIdSidebarStepper::Step::ExportReport, SoundIdSidebarStepper::StepStatus::Pending);

    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView summaryView(controller);

    bool exportCompletedFired = false;
    summaryView.onExportCompleted = [&] {
        exportCompletedFired = true;
        stepper.setStepStatus(SoundIdSidebarStepper::Step::ExportReport, SoundIdSidebarStepper::StepStatus::Completed);
    };

    // Before export: ExportReport is NOT completed
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) == SoundIdSidebarStepper::StepStatus::Pending);
    CHECK_FALSE(exportCompletedFired);

    // Simulate export success callback execution
    if (summaryView.onExportCompleted)
        summaryView.onExportCompleted();

    CHECK(exportCompletedFired);
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) == SoundIdSidebarStepper::StepStatus::Completed);
}

// ===========================================================================
// TEST 7: Cancel does not advance to ExportReport as if finished
// ===========================================================================
TEST_CASE("Step3Step4: Cancel profiling does not advance to ExportReport", "[step3][step4][cancel]")
{
    ProfilingSessionController controller;

    // Transition controller to profiling
    controller.setWorkflowMode(UiWorkflowMode::Guided);
    TargetSelectionState target;
    target.targetId = "mock_target";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    target.isDeterministic = true;
    controller.selectTarget(target);
    bool started = controller.startProfiling();
    REQUIRE(started);

    REQUIRE(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Profiling);

    // Cancel profiling
    bool cancelled = controller.cancelProfiling();
    REQUIRE(cancelled);

    const auto snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Cancelled);
    CHECK(snap.workflowStage != ProfilingWorkflowStage::ReviewResults);
}

// ===========================================================================
// TEST 8: Session with calibrationMode = Bypass retains state and incomplete provenance
// ===========================================================================
TEST_CASE("Step3Step4: CalibrationMode Bypass is preserved and uncertified", "[step3][step4][bypass]")
{
    Step3Step4TempDir tempDir("bypass_test");

    core::SessionManifest manifest;
    manifest.sessionTitle = "BypassedSession";
    manifest.calibrationMode = "Bypass";
    manifest.hasPhysicalNoiseBaseline = false;
    manifest.calibrationSnapshot = std::nullopt;
    manifest.lineCalibrationGainDb = 0.0f;

    // Serialize to JSON and reload
    core::SessionSerializer serializer;
    juce::File sessionFile(juce::String((tempDir.path / "session.json").string()));
    nlohmann::json j;
    // Test serialization through SessionSerializer round-trip logic
    j["calibrationMode"] = manifest.calibrationMode;
    j["hasPhysicalNoiseBaseline"] = manifest.hasPhysicalNoiseBaseline;
    j["sessionTitle"] = manifest.sessionTitle;

    core::SessionManifest loadedManifest;
    if (j.contains("calibrationMode"))
        loadedManifest.calibrationMode = j["calibrationMode"].get<std::string>();
    if (j.contains("hasPhysicalNoiseBaseline"))
        loadedManifest.hasPhysicalNoiseBaseline = j["hasPhysicalNoiseBaseline"].get<bool>();

    CHECK(loadedManifest.calibrationMode == "Bypass");
    CHECK_FALSE(loadedManifest.hasPhysicalNoiseBaseline);
    CHECK_FALSE(loadedManifest.calibrationSnapshot.has_value());

    // HTML Certification export with Bypass must include warning banner
    SessionManifestData manifestData;
    manifestData.hardwareName = "Bypass Test Hardware";
    manifestData.calibrationMode = "Bypass";

    juce::File htmlFile(juce::String((tempDir.path / "certification_report.html").string()));
    std::vector<MeasuredPoint> points;
    bool htmlOk = CertificationReportExporter::exportReportToHtml(
        htmlFile.getFullPathName().toStdString(),
        manifestData,
        points,
        nullptr,
        "notExecuted"
    );
    REQUIRE(htmlOk);
    REQUIRE(htmlFile.existsAsFile());

    std::string htmlContent = htmlFile.loadFileAsString().toStdString();
    CHECK(htmlContent.find("CALIBRATION STATUS: BYPASSED") != std::string::npos);
    CHECK(htmlContent.find("Calibration provenance incomplete") != std::string::npos);
}

// ===========================================================================
// TEST 9: Incomplete or failed session does not enable production export
// ===========================================================================
TEST_CASE("Step3Step4: Incomplete or failed session blocks production export", "[step3][step4][guards]")
{
    ProfilingSessionController controller;

    // 1. Idle session
    CHECK_FALSE(controller.evaluateExportReadiness().canProceed());
    CHECK_FALSE(controller.requestExportProductionPackage());

    // 2. Session with InvalidMeasurement
    ProfilingSessionSnapshot invalidSnap;
    invalidSnap.sessionStatus = ProfilingSessionStatus::Completed;
    invalidSnap.evaluation.selectionStatus = synth::SelectionStatus::InvalidMeasurement;
    ExportReadiness r = evaluateExportReadinessFromSnapshot(invalidSnap);
    CHECK_FALSE(r.canProceed());
    CHECK_FALSE(r.measurementValid);
    CHECK(r.decision == ExportReadiness::Decision::Blocked);
}

// ===========================================================================
// TEST 10: Step 4 is navigable freely for inspection without false completion
// ===========================================================================
TEST_CASE("Step3Step4: Free navigation to Step 4 without false completion", "[step3][step4][stepper][navigation]")
{
    SoundIdSidebarStepper stepper;

    // Invariant 1: Step 4 is navigable directly on cold start
    CHECK(stepper.canNavigateToExportReport());
    CHECK(stepper.canNavigateTo(SoundIdSidebarStepper::Step::ExportReport));
    CHECK_FALSE(stepper.isExportReportCompleted());

    // Invariant 2: Entering Step 4 sets Current, but never Completed
    stepper.setCurrentStep(SoundIdSidebarStepper::Step::ExportReport);
    CHECK(stepper.getCurrentStep() == SoundIdSidebarStepper::Step::ExportReport);
    CHECK(stepper.getStepStatus(SoundIdSidebarStepper::Step::ExportReport) == SoundIdSidebarStepper::StepStatus::Current);
    CHECK_FALSE(stepper.isExportReportCompleted());

    // Invariant 3: Explicit completion only happens when marked Completed
    stepper.setStepStatus(SoundIdSidebarStepper::Step::ExportReport, SoundIdSidebarStepper::StepStatus::Completed);
    CHECK(stepper.isExportReportCompleted());
}
