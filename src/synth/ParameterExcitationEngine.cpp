#include "ParameterExcitationEngine.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::synth
{

ParameterExcitationEngine::ParameterExcitationEngine(ISynthTarget& target,
                                                     const TargetContract& contract,
                                                     const TargetAuditReport& auditReport,
                                                     ProcessingSpec spec)
    : target_(target)
    , contract_(contract)
    , auditReport_(auditReport)
    , spec_(spec)
    , dispatcher_()
{
}

bool ParameterExcitationEngine::canExecuteRecipe(const IExperimentRecipe& recipe, std::string& diagnosticReason) const
{
    if (auditReport_.approvalStatus == ApprovalStatus::Rejected)
    {
        diagnosticReason = "Target rejected by audit: " + auditReport_.summaryMessage;
        return false;
    }

    if (auditReport_.approvalStatus == ApprovalStatus::Unsupported)
    {
        diagnosticReason = "Target unsupported by contract: required capabilities are missing.";
        return false;
    }

    // Regla metrológica: Si la receta exige resta diferencial por pares y el target es estocástico no semillado,
    // la cancelación de fase fallará y amplificará ruido
    if (recipe.recipeType() == "PairwiseDifferential")
    {
        if (auditReport_.determinism == DeterminismClass::StochasticUnseeded)
        {
            diagnosticReason = "Pairwise differential subtraction requires deterministic or reset-paired phase; target is StochasticUnseeded.";
            return false;
        }
    }

    return true;
}

ExcitationUncertaintyBudget ParameterExcitationEngine::computeBudget() const
{
    ExcitationUncertaintyBudget budget;
    budget.auditUncertainty.spectralCentroidHz = 15.0;
    budget.auditUncertainty.rmsDb = 0.5;
    budget.auditUncertainty.pitchCents = 5.0;
    budget.auditUncertainty.attackMs = 1.0;

    auto timing = target_.timingInfo();
    budget.excitationUncertainty.jitterMs = timing.timingJitterMs;
    budget.excitationUncertainty.parameterResolution = 1e-4;
    budget.excitationUncertainty.settlingErrorPercent = 0.05;
    budget.excitationUncertainty.measurementNoiseFloorDb = auditReport_.generationEvidence.restRmsDb;

    budget.computeCombined();
    return budget;
}

ExcitationExperimentReport ParameterExcitationEngine::executeRecipe(const IExperimentRecipe& recipe,
                                                                    const SynthPresetState& baseState)
{
    ExcitationExperimentReport report;
    report.recipeId = recipe.recipeId();
    report.targetId = contract_.name;
    report.uncertaintyBudget = computeBudget();

    std::string diagnosticReason;
    if (!canExecuteRecipe(recipe, diagnosticReason))
    {
        report.executionPermitted = false;
        report.rejectionReason = diagnosticReason;
        report.summaryNotes = "Execution aborted by audit gatekeeper: " + diagnosticReason;
        return report;
    }

    report.executionPermitted = true;

    // Políticas iniciales de asentamiento y aleatoriedad
    SettlingPolicy settling;
    RandomizationPolicy randomization;

    // Adaptar las políticas según las instrucciones operativas de TargetAuditor
    if (auditReport_.operationalInstructions.resetBeforeEachTrial)
    {
        settling.enforceResetBeforeTrial = true;
    }

    if (auditReport_.operationalInstructions.recommendedSettlingTimeMs > 0.0)
    {
        double recommendedSec = auditReport_.operationalInstructions.recommendedSettlingTimeMs / 1000.0;
        settling.interStepSettlingSec = std::max(settling.interStepSettlingSec, recommendedSec);
    }

    if (auditReport_.operationalInstructions.useStatisticalAveraging)
    {
        randomization.useStatisticalAveraging = true;
        randomization.repetitionsPerCondition = std::max(3, randomization.repetitionsPerCondition);
    }

    // 1. Generar plan declarativo a partir de la receta
    ExperimentPlan plan = recipe.generatePlan(spec_.sampleRate, settling, randomization);
    report.planId = plan.planId;
    report.planHash = plan.planHash;

    // 2. Cargar estado base en el target
    target_.loadState(baseState);

    // 3. Despachar el plan sobre el target produciendo la traza de ejecución
    report.trace = dispatcher_.dispatchPlan(target_, plan, 0);

    // 4. Analizar acústicamente las ventanas observadas
    AcousticObserver observer(spec_.sampleRate);
    report.observations = observer.analyzeWindows(plan, report.trace, report.uncertaintyBudget);

    // 5. Análisis especializado según tipo de receta
    if (recipe.recipeType() == "ParameterStep")
    {
        report.stepResponses = observer.analyzeStepResponses(plan, report.trace, report.uncertaintyBudget);
    }
    else if (recipe.recipeType() == "ParameterRamp")
    {
        report.rampAnalysis = observer.analyzeRampResponse(plan, report.trace);
    }
    else if (recipe.recipeType() == "LocalPerturbation")
    {
        report.jacobian = observer.estimateJacobian(plan, report.trace, report.uncertaintyBudget);
    }
    else if (recipe.recipeType() == "PairwiseDifferential")
    {
        report.differential = observer.analyzePairwiseDifferential(plan, report.trace, report.uncertaintyBudget);
    }

    // 6. Generar resumen diagnóstico
    std::ostringstream ss;
    ss << "Recipe '" << recipe.recipeId() << "' executed on target '" << contract_.name << "'. "
       << "Observed windows: " << report.observations.size() << ". "
       << "Combined uncertainty: centroid=" << std::fixed << std::setprecision(2)
       << report.uncertaintyBudget.combinedUncertainty.spectralCentroidHz << " Hz, rms="
       << report.uncertaintyBudget.combinedUncertainty.rmsDb << " dB. ";

    int observedCount = 0;
    int unobservedCount = 0;
    for (const auto& obs : report.observations)
    {
        if (obs.outcome == ObservationOutcome::Observed)
            observedCount++;
        else if (obs.outcome == ObservationOutcome::NotObservedInCurrentCondition)
            unobservedCount++;
    }
    ss << "Outcomes: " << observedCount << " Observed, " << unobservedCount << " NotObservedInCurrentCondition.";

    report.summaryNotes = ss.str();
    return report;
}

} // namespace abdaudiolab::synth
