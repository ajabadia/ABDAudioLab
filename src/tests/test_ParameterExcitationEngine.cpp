#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "synth/SyntheticSynthFixture.h"
#include "synth/SyntheticSynthTarget.h"
#include "synth/TargetAuditor.h"
#include "synth/TargetContract.h"
#include "synth/ParameterExcitationEngine.h"
#include "synth/ExperimentRecipe.h"
#include <cmath>
#include <iostream>

using namespace abdaudiolab::synth;

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: Gatekeeper de Auditoria", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "TestGatekeeperSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    SECTION("Auditoria con ApprovalStatus::Rejected aborta ejecucion con mensaje diagnostico")
    {
        TargetAuditReport rejectedReport;
        rejectedReport.approvalStatus = ApprovalStatus::Rejected;
        rejectedReport.summaryMessage = "Excessive thermal drift and unrepeatable state persistence.";

        ParameterExcitationEngine engine(target, contract, rejectedReport, spec);
        ParameterStepRecipe recipe("cutoff_step", "filter_cutoff", { 0.2, 0.5, 0.8 }, 0.2);

        SynthPresetState baseState;
        auto result = engine.executeRecipe(recipe, baseState);

        REQUIRE_FALSE(result.executionPermitted);
        REQUIRE(result.rejectionReason.find("Target rejected by audit") != std::string::npos);
        REQUIRE(result.observations.empty());
    }

    SECTION("Auditoria con ApprovalStatus::ApprovedWithWarnings adapta politica de asentamiento")
    {
        TargetAuditReport warningReport;
        warningReport.approvalStatus = ApprovalStatus::ApprovedWithWarnings;
        warningReport.operationalInstructions.resetBeforeEachTrial = true;
        warningReport.operationalInstructions.recommendedSettlingTimeMs = 150.0;
        warningReport.operationalInstructions.useStatisticalAveraging = true;

        ParameterExcitationEngine engine(target, contract, warningReport, spec);
        ParameterStepRecipe recipe("cutoff_step", "filter_cutoff", { 0.3, 0.7 }, 0.2);

        SynthPresetState baseState;
        auto result = engine.executeRecipe(recipe, baseState);

        REQUIRE(result.executionPermitted);
        REQUIRE(result.trace.resetExecutedBeforeTrial);
        REQUIRE(result.stepResponses.size() == 2);
    }
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: Trazabilidad de 4 Estados y Subestados de Evidencia", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "TraceFixtureSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });
    contract.parameters.push_back({ "dummy_param", "dummy_native", 0.0, 1.0, 0.5, "", ParameterCategory::Custom, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;
    approvedReport.determinism = DeterminismClass::Deterministic;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);

    SECTION("Parametro reactivo (filter_cutoff) progresa de Requested a ObservedInAudio con ConfirmedByAPI")
    {
        ParameterStepRecipe recipe("cutoff_step", "filter_cutoff", { 0.2, 0.9 }, 0.2);
        SynthPresetState baseState;
        auto result = engine.executeRecipe(recipe, baseState);

        REQUIRE(result.executionPermitted);
        REQUIRE(result.stepResponses.size() == 2);

        // Verificar trazabilidad en traza
        REQUIRE_FALSE(result.trace.executedParameterEvents.empty());
        for (const auto& ev : result.trace.executedParameterEvents)
        {
            REQUIRE(ev.status == ParameterEventStatus::AppliedByTarget);
            REQUIRE(ev.appliedConfirmation == AppliedConfirmation::ConfirmedByAPI);
            REQUIRE(ev.transportAccuracy == TransportAccuracy::SampleAccurate);
            REQUIRE(ev.absoluteSample >= 0);
        }

        // Ambas ventanas deben ser clasificadas como Observed porque el corte varía los armónicos notablemente
        for (const auto& obs : result.observations)
        {
            REQUIRE(obs.outcome == ObservationOutcome::Observed);
            REQUIRE(obs.hasAudibleEffect);
            REQUIRE_FALSE(obs.clippingDetected);
        }
    }

    SECTION("Parametro no acoplado al audio en la condicion actual resulta en NotObservedInCurrentCondition")
    {
        // 'dummy_param' es aceptado por contrato y aplicado por API, pero la sintesis no lo usa
        ParameterStepRecipe recipe("dummy_step", "dummy_param", { 0.1, 0.8 }, 0.2);
        SynthPresetState baseState;
        auto result = engine.executeRecipe(recipe, baseState);

        REQUIRE(result.executionPermitted);
        REQUIRE(result.observations.size() == 2);

        for (const auto& obs : result.observations)
        {
            // El parámetro fue aplicado pero no produce diferencia por encima de incertidumbre metrológica
            REQUIRE(obs.outcome == ObservationOutcome::NotObservedInCurrentCondition);
            REQUIRE_FALSE(obs.hasAudibleEffect);
        }
    }
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: ParameterStepRecipe Monotonicidad", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "StepSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);
    std::vector<double> steps = { 0.2, 0.4, 0.6, 0.8, 1.0 };
    ParameterStepRecipe recipe("cutoff_sweep", "filter_cutoff", steps, 0.2);

    SynthPresetState baseState;
    auto result = engine.executeRecipe(recipe, baseState);

    REQUIRE(result.executionPermitted);
    REQUIRE(result.stepResponses.size() == steps.size());

    // Verificar que a mayor cutoff, el centroide espectral medido aumenta monotónicamente
    for (size_t i = 1; i < result.stepResponses.size(); ++i)
    {
        double prevCentroid = result.stepResponses[i - 1].features.spectralCentroidHz;
        double currCentroid = result.stepResponses[i].features.spectralCentroidHz;
        REQUIRE(currCentroid >= prevCentroid);
    }
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: ParameterRampRecipe Resolucion y Suavizado", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "RampSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);
    ParameterRampRecipe recipe("cutoff_ramp", "filter_cutoff", 0.1, 1.0, 0.5, 32);

    SynthPresetState baseState;
    auto result = engine.executeRecipe(recipe, baseState);

    REQUIRE(result.executionPermitted);
    REQUIRE(result.rampAnalysis.requestedCurve == "LinearRamp");
    REQUIRE(result.rampAnalysis.transportResolution == "TransportedSampleAccurate");
    REQUIRE(result.rampAnalysis.effectiveLatencyMs >= 0.0);
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: LocalPerturbationRecipe Jacobiano y Varianza", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "JacobianSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);
    // Perturbación local en punto de operación 0.5 con delta = 0.05
    LocalPerturbationRecipe recipe("cutoff_jac", "filter_cutoff", 0.50, 0.05, 0.25);

    SynthPresetState baseState;
    auto result = engine.executeRecipe(recipe, baseState);

    REQUIRE(result.executionPermitted);
    REQUIRE(result.jacobian.parameterId == "filter_cutoff");
    REQUIRE(result.jacobian.delta == Catch::Approx(0.05));
    REQUIRE(result.jacobian.dCentroid_dParam > 0.0); // La sensibilidad espectral debe ser positiva
    REQUIRE(result.jacobian.estimatorVarianceCentroid >= 0.0);
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: PairwiseDifferentialRecipe Cancelacion y Fiabilidad", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "DifferentialSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;
    approvedReport.determinism = DeterminismClass::Deterministic;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);
    PairwiseDifferentialRecipe recipe("cutoff_diff", "filter_cutoff", 0.30, 0.70, 0.3);

    SynthPresetState baseState;
    auto result = engine.executeRecipe(recipe, baseState);

    REQUIRE(result.executionPermitted);
    REQUIRE(result.differential.reliability == PairwiseSubtractionReliability::PairedDeterministic);
    REQUIRE(result.differential.deltaRmsDb > -150.0); // Existe energía residual al restar dos condiciones distintas de corte
}

TEST_CASE("Fase 20.3 - ParameterExcitationEngine: PRBSExcitationRecipe APRBS Multinivel", "[synth][excitation]")
{
    SyntheticSynthFixture fixture(96000.0);
    SyntheticSynthTarget target(fixture);
    ProcessingSpec spec { 96000.0, 512, 2 };
    target.prepare(spec);

    TargetContract contract;
    contract.name = "PrbsSynth";
    contract.declaredDeterministic = true;
    contract.supportsReset = true;
    contract.parameters.push_back({ "filter_cutoff", "fc_native", 0.0, 1.0, 1.0, "Hz", ParameterCategory::Filter, false, false });

    TargetAuditReport approvedReport;
    approvedReport.approvalStatus = ApprovalStatus::Approved;

    ParameterExcitationEngine engine(target, contract, approvedReport, spec);
    // APRBS multinivel de 5 niveles: 0.25 -> 0.75 -> 0.40 -> 0.90 -> 0.10
    PRBSExcitationRecipe recipe("aprbs_cutoff", "filter_cutoff", { 0.25, 0.75, 0.40, 0.90, 0.10 }, 0.10);

    SynthPresetState baseState;
    auto result = engine.executeRecipe(recipe, baseState);

    REQUIRE(result.executionPermitted);
    REQUIRE(result.observations.size() == 5);
    REQUIRE(result.trace.executedParameterEvents.size() == 5);
}
