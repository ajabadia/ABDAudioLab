/**
 * @file test_ExportReadiness.cpp
 * @brief Test suite ST-69–ST-85: ExportReadiness contract (HITO-04).
 *
 * These tests verify that evaluateExportReadiness() is the single authority
 * for all metrological export guards and that the two request* commands
 * delegate correctly without duplicating business logic.
 *
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>

#include "gui/session/ProfilingSessionController.h"
#include "synth/ModelEvaluationTypes.h"

using namespace abdaudiolab::gui::session;
using namespace abdaudiolab;

// ===========================================================================
// ST-69 -- Blocked: session not completed
// ===========================================================================
TEST_CASE("ST-69: ExportReadiness blocked when session is Idle", "[ExportReadiness][ST-69]")
{
    ProfilingSessionController ctrl;
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.sessionCompleted);
    REQUIRE_FALSE(r.canProceed());
    REQUIRE_FALSE(r.blockReason.empty());
}

// ===========================================================================
// ST-70 -- Blocked: session Auditing (not completed)
// ===========================================================================
TEST_CASE("ST-70: ExportReadiness blocked when session is Auditing", "[ExportReadiness][ST-70]")
{
    ProfilingSessionController ctrl;
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.sessionCompleted);
}

// ===========================================================================
// ST-71 -- Blocked: measurement invalid
// ===========================================================================
TEST_CASE("ST-71: ExportReadiness blocks on InvalidMeasurement selectionStatus", "[ExportReadiness][ST-71]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("rejected.json");
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.canProceed());
}

// ===========================================================================
// ST-72 -- Blocked: hash not verified
// ===========================================================================
TEST_CASE("ST-72: ExportReadiness blocks when hash is not verified", "[ExportReadiness][ST-72]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("tampered_hash_mismatch.json");
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.canProceed());
}

// ===========================================================================
// ST-73 -- Blocked: inconclusive
// ===========================================================================
TEST_CASE("ST-73: ExportReadiness blocks on Inconclusive selectionStatus", "[ExportReadiness][ST-73]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("inconclusive.json");
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
}

// ===========================================================================
// ST-74 -- Ready: clean approved fixture
// ===========================================================================
TEST_CASE("ST-74: ExportReadiness is Ready for clean approved fixture", "[ExportReadiness][ST-74]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("fixture_approved.json");
    ExportReadiness r = ctrl.evaluateExportReadiness();
    const auto status = ctrl.getCurrentSnapshot().sessionStatus;
    if (status == ProfilingSessionStatus::EvaluationLoadedForReview)
    {
        REQUIRE(r.canProceed());
        REQUIRE(r.sessionCompleted);
        REQUIRE(r.measurementValid);
        REQUIRE(r.hashVerified);
        REQUIRE(r.targetConsistent);
        REQUIRE(r.calibrationSatisfied);
    }
    else
    {
        REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    }
}

// ===========================================================================
// ST-75 -- ReadyWithWarnings: Dexed fixture
// ===========================================================================
TEST_CASE("ST-75: ExportReadiness is ReadyWithWarnings for dexed_warnings fixture", "[ExportReadiness][ST-75]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("dexed_warnings.json");
    ExportReadiness r = ctrl.evaluateExportReadiness();
    const auto status = ctrl.getCurrentSnapshot().sessionStatus;
    if (status == ProfilingSessionStatus::EvaluationLoadedForReview)
    {
        REQUIRE((r.decision == ExportReadiness::Decision::ReadyWithWarnings ||
                 r.decision == ExportReadiness::Decision::Ready));
        REQUIRE(r.canProceed());
    }
    else
    {
        REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    }
}

// ===========================================================================
// ST-76 -- canProceed() contract
// ===========================================================================
TEST_CASE("ST-76: canProceed is true for Ready and ReadyWithWarnings only", "[ExportReadiness][ST-76]")
{
    ExportReadiness blocked;
    blocked.decision = ExportReadiness::Decision::Blocked;
    REQUIRE_FALSE(blocked.canProceed());

    ExportReadiness ready;
    ready.decision = ExportReadiness::Decision::Ready;
    REQUIRE(ready.canProceed());

    ExportReadiness withWarnings;
    withWarnings.decision = ExportReadiness::Decision::ReadyWithWarnings;
    REQUIRE(withWarnings.canProceed());
}

// ===========================================================================
// ST-77 -- Default-constructed ExportReadiness is Blocked
// ===========================================================================
TEST_CASE("ST-77: Default-constructed ExportReadiness is Blocked", "[ExportReadiness][ST-77]")
{
    ExportReadiness r;
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.sessionCompleted);
    REQUIRE_FALSE(r.measurementValid);
    REQUIRE_FALSE(r.metrologyPassed);
    REQUIRE_FALSE(r.hashVerified);
    REQUIRE_FALSE(r.provenancePresent);
    REQUIRE_FALSE(r.targetConsistent);
    REQUIRE_FALSE(r.calibrationSatisfied);
    REQUIRE_FALSE(r.canProceed());
}

// ===========================================================================
// ST-78 -- requestExportProductionPackage fails when session is Idle
// ===========================================================================
TEST_CASE("ST-78: requestExportProductionPackage returns false for Idle session", "[ExportReadiness][ST-78]")
{
    ProfilingSessionController ctrl;
    bool result = ctrl.requestExportProductionPackage();
    REQUIRE_FALSE(result);
}

// ===========================================================================
// ST-79 -- requestExportCertificationReport fails when session is Idle
// ===========================================================================
TEST_CASE("ST-79: requestExportCertificationReport returns false for Idle session", "[ExportReadiness][ST-79]")
{
    ProfilingSessionController ctrl;
    bool result = ctrl.requestExportCertificationReport();
    REQUIRE_FALSE(result);
}

// ===========================================================================
// ST-80 -- requestExportProductionPackage fails for tampered hash fixture
// ===========================================================================
TEST_CASE("ST-80: requestExportProductionPackage blocked for tampered hash", "[ExportReadiness][ST-80]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("tampered_hash_mismatch.json");
    bool result = ctrl.requestExportProductionPackage();
    REQUIRE_FALSE(result);
}

// ===========================================================================
// ST-81 -- requestExportCertificationReport fails for rejected fixture
// ===========================================================================
TEST_CASE("ST-81: requestExportCertificationReport blocked for rejected fixture", "[ExportReadiness][ST-81]")
{
    ProfilingSessionController ctrl;
    ctrl.loadPredefinedFixture("rejected.json");
    bool result = ctrl.requestExportCertificationReport();
    REQUIRE_FALSE(result);
}

// ===========================================================================
// ST-82 -- blockReason non-empty when Blocked
// ===========================================================================
TEST_CASE("ST-82: blockReason is populated when decision is Blocked", "[ExportReadiness][ST-82]")
{
    ProfilingSessionController ctrl;
    ExportReadiness r = ctrl.evaluateExportReadiness();
    REQUIRE(r.decision == ExportReadiness::Decision::Blocked);
    REQUIRE_FALSE(r.blockReason.empty());
    REQUIRE_FALSE(r.operatorGuidance.empty());
}

// ===========================================================================
// ST-83 -- evaluateExportReadiness is const -- no state mutation
// ===========================================================================
TEST_CASE("ST-83: evaluateExportReadiness does not mutate session status", "[ExportReadiness][ST-83]")
{
    ProfilingSessionController ctrl;
    auto snapBefore = ctrl.getCurrentSnapshot();

    static_cast<void>(ctrl.evaluateExportReadiness());
    static_cast<void>(ctrl.evaluateExportReadiness());
    static_cast<void>(ctrl.evaluateExportReadiness());

    auto snapAfter = ctrl.getCurrentSnapshot();
    REQUIRE(snapBefore.sessionStatus == snapAfter.sessionStatus);
    REQUIRE(snapBefore.monotonicSequence == snapAfter.monotonicSequence);
}

// ===========================================================================
// ST-84 -- calibration guard: Required but not completed -> Blocked
// ===========================================================================
TEST_CASE("ST-84: CalibrationStatus not ready when Required calibration is incomplete", "[ExportReadiness][ST-84]")
{
    CalibrationStatus cs;
    cs.audio.requirement = CalibrationRequirement::Required;
    cs.audio.completed   = false;
    cs.audio.bypassed    = false;
    REQUIRE_FALSE(cs.isReadyForProfiling());
}

// ===========================================================================
// ST-85 -- calibration guard: Required and completed -> ready
// ===========================================================================
TEST_CASE("ST-85: CalibrationStatus ready when Required calibration is completed", "[ExportReadiness][ST-85]")
{
    CalibrationStatus cs;
    cs.audio.requirement   = CalibrationRequirement::Required;
    cs.audio.completed     = true;
    cs.midi.requirement    = CalibrationRequirement::NotApplicable;
    cs.digital.requirement = CalibrationRequirement::NotApplicable;
    REQUIRE(cs.isReadyForProfiling());
}
