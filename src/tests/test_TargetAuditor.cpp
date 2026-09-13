#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "synth/TargetAuditor.h"
#include "synth/SyntheticSynthFixture.h"
#include "synth/SyntheticSynthTarget.h"

using namespace abdaudiolab::synth;

TEST_CASE("TargetAuditor: Nominal Deterministic Target", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::None);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::GenerativeAudioObserved);
    CHECK(report.generationEvidence.noteResponsive == true);
    CHECK(report.determinism == DeterminismClass::Deterministic);
    CHECK(report.resetCapability == ResetCapability::Resettable);
    CHECK(report.interNoteState == InterNoteState::Stateless);
    CHECK(report.stateRoundTrip == StateRoundTripStatus::Passed);
    CHECK(report.roundTripEvidence.binaryIdentical == true);
    CHECK(report.roundTripEvidence.behaviorIdentical == true);
    CHECK(report.approvalStatus == ApprovalStatus::Approved);
    CHECK(report.isApprovedForParameterExcitation == true);
    CHECK(report.warnings.empty());
}

TEST_CASE("TargetAuditor: Free-Running Phase with Effective Reset", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::FreeRunningPhase);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::GenerativeAudioObserved);
    CHECK(report.determinism == DeterminismClass::DeterministicAfterReset);
    CHECK(report.resetCapability == ResetCapability::Resettable);
    CHECK(report.determinismEvidence.resetEliminatesDrift == true);
    CHECK(report.operationalInstructions.resetBeforeEachTrial == true);
    CHECK(report.approvalStatus == ApprovalStatus::ApprovedWithWarnings);
    CHECK(report.isApprovedForParameterExcitation == true);
    CHECK_FALSE(report.warnings.empty());
}

TEST_CASE("TargetAuditor: Unseeded Stochastic Noise Target", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::UnseededStochasticNoise);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::GenerativeAudioObserved);
    CHECK(report.determinism == DeterminismClass::StochasticUnseeded);
    CHECK(report.determinismEvidence.byteIdentical == false);
    CHECK(report.operationalInstructions.useStatisticalAveraging == true);
    CHECK(report.operationalInstructions.exactHashComparisonPermitted == false);
    CHECK(report.approvalStatus == ApprovalStatus::ApprovedWithWarnings);
    CHECK(report.isApprovedForParameterExcitation == true);
}

TEST_CASE("TargetAuditor: Inter-Note Residual Effect Tail", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::InterNoteResidualTail);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::GenerativeAudioObserved);
    CHECK(report.interNoteState == InterNoteState::StatefulBehaviorDetected);
    CHECK(report.statefulnessEvidence.detectedCause == ResidualCause::EffectTail);
    CHECK(report.operationalInstructions.recommendedSettlingTimeMs >= 1000.0);
    CHECK(report.approvalStatus == ApprovalStatus::ApprovedWithWarnings);
}

TEST_CASE("TargetAuditor: State Round-Trip Behavioral Discrepancy", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::StateCorruptionOnRestore);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.stateRoundTrip == StateRoundTripStatus::StateRoundTripWarning);
    CHECK(report.roundTripEvidence.failureCause == RoundTripFailureCause::BehavioralMismatch);
    CHECK(report.approvalStatus == ApprovalStatus::ApprovedWithWarnings);
}

TEST_CASE("TargetAuditor: Silent Plugin / Pure Effect Unsupported", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::SilentEffectPlugin);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::SilentOutput);
    CHECK(report.approvalStatus == ApprovalStatus::Unsupported);
    CHECK(report.isApprovedForParameterExcitation == false);
    CHECK_FALSE(report.limitations.empty());
}

TEST_CASE("TargetAuditor: Audio Present But Not Note Responsive", "[synth][auditor]")
{
    SyntheticSynthFixture fixture(96000.0, 42);
    fixture.setFaultMode(FixtureFaultMode::AudioWithoutNoteResponse);
    SyntheticSynthTarget target(fixture);

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };

    auto report = auditor.auditTarget(target, spec);

    CHECK(report.generation == GenerationClass::AudioObservedButNotNoteResponsive);
    CHECK(report.approvalStatus == ApprovalStatus::Unsupported);
    CHECK(report.isApprovedForParameterExcitation == false);
    CHECK_FALSE(report.limitations.empty());
}
