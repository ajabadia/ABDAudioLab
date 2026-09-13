#pragma once

#include "ISynthTarget.h"
#include "TargetContract.h"
#include "TargetAuditor.h"
#include "ExperimentPlan.h"
#include "ExperimentRecipe.h"
#include "TargetEventDispatcher.h"
#include "AcousticObserver.h"
#include <string>
#include <vector>
#include <memory>

namespace abdaudiolab::synth
{

/**
 * @brief Informe completo y auditable del experimento de excitación de parámetros.
 */
struct ExcitationExperimentReport
{
    std::string schemaVersion { "1.0.0" };
    std::string recipeId;
    std::string planId;
    std::string planHash;
    std::string targetId;
    bool executionPermitted { true };
    std::string rejectionReason;

    ExcitationUncertaintyBudget uncertaintyBudget;
    TargetExecutionTrace trace;
    std::vector<WindowAcousticFeatures> observations;

    // Resultados especializados según la receta ejecutada
    std::vector<StepObservation> stepResponses;
    RampProcessingAnalysis rampAnalysis;
    JacobianEstimate jacobian;
    PairwiseDifferentialAnalysis differential;

    std::string summaryNotes;
};

/**
 * @brief Motor de excitación de parámetros gobernado por la auditoría metrológica.
 *
 * Implementa las garantías de la Fase 20.3:
 * 1. Gatekeeper de auditoría: Rechaza o adapta la ejecución según TargetAuditReport.
 * 2. Desacoplamiento total: Las recetas solo generan planes; el dispatcher ejecuta;
 *    el AcousticObserver analiza y evalúa observabilidad acústica.
 * 3. Trazabilidad de 4 estados con subestados de confirmación y clasificaciones robustas
 *    de observabilidad acústica (Observed, NotObservedInCurrentCondition, Inconclusive, Rejected).
 */
class ParameterExcitationEngine
{
public:
    ParameterExcitationEngine(ISynthTarget& target,
                              const TargetContract& contract,
                              const TargetAuditReport& auditReport,
                              ProcessingSpec spec = { 96000.0, 512, 2 });

    /**
     * @brief Comprueba si la auditoría permite ejecutar una receta dada.
     */
    [[nodiscard]] bool canExecuteRecipe(const IExperimentRecipe& recipe, std::string& diagnosticReason) const;

    /**
     * @brief Ejecuta una receta de excitación, respetando las políticas de la auditoría
     * y computando las observaciones acústicas y el presupuesto de incertidumbre.
     */
    ExcitationExperimentReport executeRecipe(const IExperimentRecipe& recipe,
                                            const SynthPresetState& baseState);

    /**
     * @brief Acceso al presupuesto combinado de incertidumbre configurado.
     */
    [[nodiscard]] ExcitationUncertaintyBudget computeBudget() const;

private:
    ISynthTarget& target_;
    const TargetContract& contract_;
    const TargetAuditReport& auditReport_;
    ProcessingSpec spec_;
    TargetEventDispatcher dispatcher_;
};

} // namespace abdaudiolab::synth
