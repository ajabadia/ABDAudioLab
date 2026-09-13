#include "AdaptiveExperimentPlanner.h"
#include <iomanip>
#include <sstream>
#include <limits>

namespace abdaudiolab::synth
{

AdaptiveExperimentPlanner::AdaptiveExperimentPlanner(TargetContract contract,
                                                     TargetAuditReport auditReport,
                                                     AdaptivePlannerPolicy policy)
    : contract_(std::move(contract)),
      auditReport_(std::move(auditReport)),
      policy_(std::move(policy))
{
    // Sincronizar pesos por defecto si no están seteados
    if (policy_.batchDiversityPenalty <= 0.0)
        policy_.batchDiversityPenalty = 0.5;
    if (policy_.minimumCandidateSeparation <= 0.0)
        policy_.minimumCandidateSeparation = 0.05;
}

void AdaptiveExperimentPlanner::setHoldoutGuard(const IHoldoutProtection* holdoutGuard) noexcept
{
    holdoutGuard_ = holdoutGuard;
}

void AdaptiveExperimentPlanner::registerMeasuredPoint(std::vector<TypedCoordinate> point, double observedVariance)
{
    measuredPoints_.push_back(std::move(point));
    measuredVariances_.push_back(observedVariance);
    executedTrialsCount_++;
}

std::vector<CandidateAcquisitionBreakdown> AdaptiveExperimentPlanner::generateCandidates(int gridResolution)
{
    std::vector<CandidateAcquisitionBreakdown> candidates;
    if (gridResolution < 2)
        gridResolution = 2;

    // Buscar parámetros relevantes en el contrato
    std::vector<const TargetParameterDescriptor*> activeParams;
    for (const auto& p : contract_.parameters)
    {
        if (p.isAutomatable || p.role == ParameterRole::AudioControl)
        {
            activeParams.push_back(&p);
        }
    }

    if (activeParams.empty() && !contract_.parameters.empty())
    {
        // Fallback al primer parámetro
        activeParams.push_back(&contract_.parameters.front());
    }

    if (activeParams.empty())
    {
        // Sintetizador sin parámetros declarados: candidato unitario fijo
        CandidateAcquisitionBreakdown cand;
        cand.candidateId = "cand_0";
        TypedCoordinate coord;
        coord.dimensionId = "default_param";
        coord.kind = DimensionKind::Continuous;
        coord.continuousValue = 0.5;
        cand.coordinates.push_back(coord);
        candidates.push_back(cand);
        return candidates;
    }

    // Si hay 1 solo parámetro, generamos rejilla lineal 1D exacta
    if (activeParams.size() == 1)
    {
        const auto* p = activeParams[0];
        for (int i = 0; i < gridResolution; ++i)
        {
            double step = static_cast<double>(i) / (gridResolution - 1);
            double val = p->minValue + step * (p->maxValue - p->minValue);

            CandidateAcquisitionBreakdown cand;
            cand.candidateId = "cand_" + std::to_string(i);

            TypedCoordinate coord;
            coord.dimensionId = p->normalizedId;
            coord.kind = p->isDiscrete ? DimensionKind::Discrete : DimensionKind::Continuous;
            coord.continuousValue = val;
            coord.discreteValue = static_cast<int>(std::round(val));
            cand.coordinates.push_back(coord);

            candidates.push_back(cand);
        }
    }
    else
    {
        // Para múltiples parámetros (ej. 2 o más), generamos una rejilla o muestreo determinista
        // Si el producto cartesiano es razonable (< 200), hacemos producto cartesiano.
        int totalCombinations = 1;
        for (size_t i = 0; i < activeParams.size(); ++i)
        {
            totalCombinations *= gridResolution;
            if (totalCombinations > 100)
                break;
        }

        if (totalCombinations <= 100 && activeParams.size() <= 2)
        {
            int candId = 0;
            for (int i = 0; i < gridResolution; ++i)
            {
                double stepI = static_cast<double>(i) / (gridResolution - 1);
                double valI = activeParams[0]->minValue + stepI * (activeParams[0]->maxValue - activeParams[0]->minValue);

                for (int j = 0; j < gridResolution; ++j)
                {
                    double stepJ = static_cast<double>(j) / (gridResolution - 1);
                    double valJ = activeParams[1]->minValue + stepJ * (activeParams[1]->maxValue - activeParams[1]->minValue);

                    CandidateAcquisitionBreakdown cand;
                    cand.candidateId = "cand_" + std::to_string(candId++);

                    TypedCoordinate c1;
                    c1.dimensionId = activeParams[0]->normalizedId;
                    c1.kind = activeParams[0]->isDiscrete ? DimensionKind::Discrete : DimensionKind::Continuous;
                    c1.continuousValue = valI;
                    c1.discreteValue = static_cast<int>(std::round(valI));
                    cand.coordinates.push_back(c1);

                    TypedCoordinate c2;
                    c2.dimensionId = activeParams[1]->normalizedId;
                    c2.kind = activeParams[1]->isDiscrete ? DimensionKind::Discrete : DimensionKind::Continuous;
                    c2.continuousValue = valJ;
                    c2.discreteValue = static_cast<int>(std::round(valJ));
                    cand.coordinates.push_back(c2);

                    candidates.push_back(cand);
                }
            }
        }
        else
        {
            // Muestreo determinista pseudo-aleatorio con semilla fija
            std::mt19937 rng(policy_.randomSeed);
            std::uniform_real_distribution<double> dist01(0.0, 1.0);

            const int maxPoints = std::min(64, gridResolution * 4);
            for (int i = 0; i < maxPoints; ++i)
            {
                CandidateAcquisitionBreakdown cand;
                cand.candidateId = "cand_" + std::to_string(i);

                for (const auto* p : activeParams)
                {
                    double r = dist01(rng);
                    double val = p->minValue + r * (p->maxValue - p->minValue);

                    TypedCoordinate coord;
                    coord.dimensionId = p->normalizedId;
                    coord.kind = p->isDiscrete ? DimensionKind::Discrete : DimensionKind::Continuous;
                    coord.continuousValue = val;
                    coord.discreteValue = static_cast<int>(std::round(val));
                    cand.coordinates.push_back(coord);
                }
                candidates.push_back(cand);
            }
        }
    }

    return candidates;
}

void AdaptiveExperimentPlanner::validateContractAndSafety(CandidateAcquisitionBreakdown& candidate)
{
    // 1. Verificación del estado de aprobación del target
    if (auditReport_.approvalStatus == ApprovalStatus::Rejected ||
        auditReport_.approvalStatus == ApprovalStatus::Unsupported)
    {
        candidate.passedSafety = false;
        candidate.rejectionReason = "Target rejected or unsupported by audit (" +
                                    approvalStatusToString(auditReport_.approvalStatus) + ")";
        return;
    }

    // 2. Verificación de límites de parámetros contra el contrato
    for (const auto& coord : candidate.coordinates)
    {
        const auto* paramDesc = contract_.findParameter(coord.dimensionId);
        if (paramDesc == nullptr)
        {
            candidate.passedContract = false;
            candidate.rejectionReason = "Coordinate refers to undeclared parameter: " + coord.dimensionId;
            return;
        }

        if (coord.kind == DimensionKind::Continuous || coord.kind == DimensionKind::Note || coord.kind == DimensionKind::Velocity)
        {
            if (coord.continuousValue < (paramDesc->minValue - 1e-6) ||
                coord.continuousValue > (paramDesc->maxValue + 1e-6))
            {
                candidate.passedContract = false;
                candidate.rejectionReason = "Value " + std::to_string(coord.continuousValue) +
                                            " exceeds bounds [" + std::to_string(paramDesc->minValue) +
                                            ", " + std::to_string(paramDesc->maxValue) + "]";
                return;
            }
        }
    }

    candidate.passedContract = true;
    candidate.passedSafety = true;
}

void AdaptiveExperimentPlanner::checkHoldoutCollision(CandidateAcquisitionBreakdown& candidate)
{
    if (holdoutGuard_ != nullptr)
    {
        if (holdoutGuard_->containsCoordinate(candidate.coordinates, distancePolicy_, policy_.minimumCandidateSeparation))
        {
            candidate.excludedByHoldout = true;
            candidate.rejectionReason = "Excluded by holdout guard: collision with protected validation set";
        }
    }
}

double AdaptiveExperimentPlanner::estimateMeasurementCost(const CandidateAcquisitionBreakdown& candidate)
{
    double cost = 1.0; // Coste base normalizado

    // Considerar instrucciones operativas del informe de auditoría
    const auto& ops = auditReport_.operationalInstructions;
    if (ops.resetBeforeEachTrial)
    {
        cost += 0.5; // Sobrecoste por llamada a resetState() y vaciado de buffers
    }

    // Tiempo de reposo (settling time) recomendado
    if (ops.recommendedSettlingTimeMs > 0.0)
    {
        cost += ops.recommendedSettlingTimeMs / 200.0;
    }

    // Promedio estadístico necesario si el target es estocástico o de fase libre
    if (ops.useStatisticalAveraging)
    {
        cost *= 2.0;
    }

    // Distancia al último punto medido (salto paramétrico)
    if (!measuredPoints_.empty())
    {
        double jumpDistance = distancePolicy_.computeDistance(measuredPoints_.back(), candidate.coordinates);
        cost += 0.3 * jumpDistance;
    }

    return std::max(0.1, cost);
}

void AdaptiveExperimentPlanner::scoreCandidate(CandidateAcquisitionBreakdown& candidate)
{
    // Si no superó contrato, seguridad o colisionó con holdout, alpha queda anulado
    if (!candidate.passedContract || !candidate.passedSafety || candidate.excludedByHoldout)
    {
        candidate.alpha = -1.0;
        return;
    }

    // 1. Cobertura espacial C(x)
    double cScore = 1.0;
    double minMeasuredDist = 1.0;
    if (!measuredPoints_.empty())
    {
        for (const auto& measured : measuredPoints_)
        {
            double d = distancePolicy_.computeDistance(measured, candidate.coordinates);
            if (d < minMeasuredDist)
                minMeasuredDist = d;
        }
        // Cuanto más lejos de cualquier punto medido, mayor puntuación de cobertura (hasta 1.0)
        cScore = std::clamp(minMeasuredDist, 0.0, 1.0);
    }
    candidate.cScore = cScore;

    // 2. Incertidumbre predictiva U(x)
    // En las regiones no exploradas, U(x) = C(x). Si hay puntos cercanos con alta varianza, U(x) aumenta.
    double uScore = cScore;
    if (!measuredPoints_.empty() && minMeasuredDist < 0.1)
    {
        // Encontramos la varianza del punto más cercano
        double nearVariance = 0.0;
        for (size_t i = 0; i < measuredPoints_.size(); ++i)
        {
            double d = distancePolicy_.computeDistance(measuredPoints_[i], candidate.coordinates);
            if (d <= minMeasuredDist + 1e-6)
            {
                nearVariance = measuredVariances_[i];
                break;
            }
        }
        uScore = std::clamp(minMeasuredDist + nearVariance, 0.0, 1.0);
    }
    candidate.uScore = uScore;

    // 3. Desacuerdo entre modelos competidores D(x)
    // Calculamos el desacuerdo entre hipótesis analíticas en base a la coordenada normalizada
    double normVal = 0.5;
    if (!candidate.coordinates.empty())
    {
        const auto& c0 = candidate.coordinates[0];
        const auto* p0 = contract_.findParameter(c0.dimensionId);
        if (p0 != nullptr && (p0->maxValue > p0->minValue))
        {
            normVal = (c0.continuousValue - p0->minValue) / (p0->maxValue - p0->minValue);
        }
        else
        {
            normVal = c0.continuousValue;
        }
    }
    candidate.dScore = competitor_models::computeModelDisagreement(normVal);

    // 4. Coste de medición K(x)
    candidate.kScore = estimateMeasurementCost(candidate);

    // 5. Configurar ponderaciones
    candidate.weights.wU = policy_.weightUncertainty;
    candidate.weights.wD = policy_.weightDisagreement;
    candidate.weights.wC = policy_.weightCoverage;
    candidate.weights.wK = policy_.weightCost;

    // 6. Función de adquisición global: alpha(x) = wU*U + wD*D + wC*C - wK*K
    candidate.alpha = candidate.weights.wU * candidate.uScore
                    + candidate.weights.wD * candidate.dScore
                    + candidate.weights.wC * candidate.cScore
                    - candidate.weights.wK * candidate.kScore;
}

std::vector<CandidateAcquisitionBreakdown> AdaptiveExperimentPlanner::selectBatchWithDiversity(
    std::vector<CandidateAcquisitionBreakdown>& pool, int batchSize)
{
    std::vector<CandidateAcquisitionBreakdown> selectedBatch;
    if (batchSize <= 0 || pool.empty())
        return selectedBatch;

    // Filtramos los candidatos viables
    std::vector<CandidateAcquisitionBreakdown*> viable;
    for (auto& cand : pool)
    {
        if (cand.passedContract && cand.passedSafety && !cand.excludedByHoldout && cand.alpha > -0.9)
        {
            viable.push_back(&cand);
        }
    }

    if (viable.empty())
        return selectedBatch;

    // Puntuaciones dinámicas ajustadas durante la selección por lote
    std::vector<double> dynamicAlphas;
    dynamicAlphas.reserve(viable.size());
    for (const auto* c : viable)
        dynamicAlphas.push_back(c->alpha);

    int countToSelect = std::min(batchSize, static_cast<int>(viable.size()));
    std::vector<bool> wasPicked(viable.size(), false);

    for (int step = 0; step < countToSelect; ++step)
    {
        // Encontrar el candidato con mayor dynamicAlpha que no haya sido seleccionado
        int bestIdx = -1;
        double bestAlpha = -1e9;

        for (size_t i = 0; i < viable.size(); ++i)
        {
            if (!wasPicked[i] && dynamicAlphas[i] > bestAlpha)
            {
                bestAlpha = dynamicAlphas[i];
                bestIdx = static_cast<int>(i);
            }
        }

        if (bestIdx < 0 || bestAlpha < -100.0)
            break;

        wasPicked[bestIdx] = true;
        auto* pickedCand = viable[bestIdx];
        pickedCand->selected = true;
        pickedCand->selectionReason = "Selected in round " + std::to_string(step + 1) +
                                      " with effective acquisition score " + std::to_string(bestAlpha);
        selectedBatch.push_back(*pickedCand);

        // Aplicar penalización por diversidad a los candidatos restantes cercanos al recién elegido
        for (size_t j = 0; j < viable.size(); ++j)
        {
            if (!wasPicked[j])
            {
                double dist = distancePolicy_.computeDistance(pickedCand->coordinates, viable[j]->coordinates);
                if (dist < policy_.minimumCandidateSeparation)
                {
                    double proximity = 1.0 - (dist / policy_.minimumCandidateSeparation);
                    dynamicAlphas[j] -= policy_.batchDiversityPenalty * proximity;
                }
            }
        }
    }

    return selectedBatch;
}

PlannerTerminationReason AdaptiveExperimentPlanner::evaluateStoppingPolicy(
    const std::vector<CandidateAcquisitionBreakdown>& selected,
    const std::vector<CandidateAcquisitionBreakdown>& pool,
    std::string& outRationale)
{
    // 1. Target inestable o rechazado
    if (auditReport_.approvalStatus == ApprovalStatus::Rejected ||
        auditReport_.approvalStatus == ApprovalStatus::Unsupported)
    {
        outRationale = "Target audit report rejected or unsupported (" +
                       approvalStatusToString(auditReport_.approvalStatus) + ")";
        return PlannerTerminationReason::TargetUnstable;
    }

    // 2. Presupuesto agotado
    if (executedTrialsCount_ >= policy_.maxTrialsBudget)
    {
        outRationale = "Trials budget exhausted: executed " + std::to_string(executedTrialsCount_) +
                       " of " + std::to_string(policy_.maxTrialsBudget) + " trials.";
        return PlannerTerminationReason::BudgetExhausted;
    }

    // 3. Colisión masiva de holdout
    bool allExcludedByHoldout = !pool.empty();
    for (const auto& c : pool)
    {
        if (!c.excludedByHoldout)
        {
            allExcludedByHoldout = false;
            break;
        }
    }
    if (allExcludedByHoldout)
    {
        outRationale = "All candidate points collided with protected holdout set.";
        return PlannerTerminationReason::HoldoutProtected;
    }

    // 4. Sin candidatos informativos
    if (selected.empty())
    {
        outRationale = "No viable or informative candidates remaining in search space.";
        return PlannerTerminationReason::NoInformativeCandidates;
    }

    // 5. Convergencia por ganancia mínima esperada
    double maxAlpha = -1e9;
    for (const auto& s : selected)
    {
        if (s.alpha > maxAlpha)
            maxAlpha = s.alpha;
    }

    if (maxAlpha < policy_.minExpectedGainEpsilon)
    {
        outRationale = "Expected acquisition score alpha (" + std::to_string(maxAlpha) +
                       ") is below convergence threshold epsilon (" +
                       std::to_string(policy_.minExpectedGainEpsilon) + ").";
        return PlannerTerminationReason::Converged;
    }

    // En curso, no terminado
    outRationale = "Optimization in progress. Informative candidates selected.";
    return PlannerTerminationReason::Converged; // No se marca isTerminated salvo cuando se cumplan condiciones finales
}

AdaptivePlanResult AdaptiveExperimentPlanner::planNextBatch(int batchSize, int gridResolution)
{
    AdaptivePlanResult result;
    result.planId = "plan_" + std::to_string(executedTrialsCount_ + 1);
    result.plannerPolicyId = policy_.policyId;
    result.plannerPolicyVersion = policy_.policyVersion;
    result.randomSeed = policy_.randomSeed;
    result.trialsExecutedSoFar = executedTrialsCount_;

    // 1. Generar candidatos
    auto pool = generateCandidates(gridResolution);

    // 2. Validar contrato y seguridad
    for (auto& cand : pool)
    {
        validateContractAndSafety(cand);
        if (cand.passedContract && cand.passedSafety)
        {
            checkHoldoutCollision(cand);
        }
        scoreCandidate(cand);
    }

    // 3. Seleccionar lote con diversidad
    auto selected = selectBatchWithDiversity(pool, batchSize);

    // 4. Estimar duración del lote
    double batchDuration = 0.0;
    for (const auto& s : selected)
    {
        batchDuration += s.kScore;
    }
    result.estimatedBatchDurationSec = batchDuration;

    // 5. Evaluar política de parada
    result.terminationReason = evaluateStoppingPolicy(selected, pool, result.terminationRationale);

    // Determinar isTerminated según razón
    if (auditReport_.approvalStatus == ApprovalStatus::Rejected ||
        auditReport_.approvalStatus == ApprovalStatus::Unsupported)
    {
        result.isTerminated = true;
    }
    else if (executedTrialsCount_ + static_cast<int>(selected.size()) >= policy_.maxTrialsBudget)
    {
        result.isTerminated = true;
        result.terminationReason = PlannerTerminationReason::BudgetExhausted;
        result.terminationRationale = "Trials budget reached: executed=" + std::to_string(executedTrialsCount_) +
                                      ", selected=" + std::to_string(selected.size()) +
                                      ", max=" + std::to_string(policy_.maxTrialsBudget);
    }
    else if (selected.empty())
    {
        result.isTerminated = true;
    }
    else
    {
        // Si la ganancia máxima es menor que epsilon, declaramos convergencia
        double maxAlpha = -1e9;
        for (const auto& s : selected)
        {
            if (s.alpha > maxAlpha)
                maxAlpha = s.alpha;
        }
        if (maxAlpha < policy_.minExpectedGainEpsilon)
        {
            result.isTerminated = true;
            result.terminationReason = PlannerTerminationReason::Converged;
        }
        else
        {
            result.isTerminated = false;
        }
    }

    result.trialsRemainingInBudget = std::max(0, policy_.maxTrialsBudget - (executedTrialsCount_ + static_cast<int>(selected.size())));
    result.evaluatedCandidates = std::move(pool);
    result.selectedBatch = std::move(selected);

    // 6. Hashes de reproducibilidad
    result.computeHashes();

    return result;
}

} // namespace abdaudiolab::synth
