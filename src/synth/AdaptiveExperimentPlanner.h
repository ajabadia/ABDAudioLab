#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <cmath>
#include <random>

#include "ModelEvaluationTypes.h"
#include "TargetContract.h"
#include "TargetAuditor.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Política de adquisición adaptativa configurable para el planificador.
 */
struct AdaptivePlannerPolicy
{
    std::string policyId { "adaptive_canonical_v1" };
    std::string policyVersion { "1.0.0" };

    double weightUncertainty { 0.40 };   /**< Peso de la incertidumbre predictiva U(x). */
    double weightDisagreement { 0.30 };  /**< Peso del desacuerdo entre hipótesis D(x). */
    double weightCoverage { 0.30 };      /**< Peso de la cobertura espacial C(x). */
    double weightCost { 0.10 };          /**< Peso de la penalización por coste temporal K(x). */

    double batchDiversityPenalty { 0.50 };      /**< Penalización a candidatos contiguos en el mismo lote. */
    double minimumCandidateSeparation { 0.05 }; /**< Separación mínima deseable dentro del lote. */

    double minExpectedGainEpsilon { 0.01 };     /**< Ganancia mínima alpha para considerar progreso. */
    int maxTrialsBudget { 20 };                  /**< Presupuesto máximo de ensayos. */
    double maxWallClockTimeSec { 300.0 };       /**< Presupuesto máximo de tiempo en segundos. */

    uint32_t randomSeed { 1984 };               /**< Semilla determinista para generación reproducible. */
};

/**
 * @brief Modelos de hipótesis analíticas competidoras para estimar el desacuerdo D(x).
 */
namespace competitor_models
{
    /**
     * @brief Modelo estático continuo no lineal (curva suave cuadrática/sigmoide).
     */
    [[nodiscard]] inline double predictStaticCurve(double x)
    {
        x = std::clamp(x, 0.0, 1.0);
        return x * x;
    }

    /**
     * @brief Modelo analógico logarítmico/exponencial (tipo ley de filtro V/Oct).
     */
    [[nodiscard]] inline double predictLogMapping(double x)
    {
        x = std::clamp(x, 0.0, 1.0);
        return (std::exp(3.0 * x) - 1.0) / (std::exp(3.0) - 1.0);
    }

    /**
     * @brief Modelo discretizado por pasos finitos.
     */
    [[nodiscard]] inline double predictQuantized(double x, int steps = 10)
    {
        x = std::clamp(x, 0.0, 1.0);
        return std::round(x * steps) / static_cast<double>(steps);
    }

    /**
     * @brief Modelo con inercia/memoria transitoria o saturación asimétrica.
     */
    [[nodiscard]] inline double predictStateful(double x)
    {
        x = std::clamp(x, 0.0, 1.0);
        return std::tanh(2.5 * x) / std::tanh(2.5);
    }

    /**
     * @brief Calcula la divergencia máxima entre las 4 hipótesis competidoras.
     * D(x) = max_{A, B} |y_A(x) - y_B(x)|
     */
    [[nodiscard]] inline double computeModelDisagreement(double x)
    {
        double y1 = predictStaticCurve(x);
        double y2 = predictLogMapping(x);
        double y3 = predictQuantized(x);
        double y4 = predictStateful(x);

        double minY = std::min({ y1, y2, y3, y4 });
        double maxY = std::max({ y1, y2, y3, y4 });
        return maxY - minY;
    }
} // namespace competitor_models

/**
 * @brief Resultado consolidado de una ronda de planificación adaptativa.
 */
struct AdaptivePlanResult
{
    std::string planId;
    std::string plannerPolicyId;
    std::string plannerPolicyVersion;
    uint32_t randomSeed { 0 };

    PlannerTerminationReason terminationReason { PlannerTerminationReason::Converged };
    bool isTerminated { false };
    std::string terminationRationale;

    std::vector<CandidateAcquisitionBreakdown> evaluatedCandidates;
    std::vector<CandidateAcquisitionBreakdown> selectedBatch;

    int trialsExecutedSoFar { 0 };
    int trialsRemainingInBudget { 0 };
    double estimatedBatchDurationSec { 0.0 };

    std::string candidatePoolHash;
    std::string selectedBatchHash;

    void computeHashes()
    {
        std::string poolBlob;
        for (const auto& c : evaluatedCandidates)
        {
            poolBlob += c.candidateId + ":" + std::to_string(c.alpha) + ";";
        }
        candidatePoolHash = Sha256::computeHex(poolBlob);

        std::string batchBlob;
        for (const auto& s : selectedBatch)
        {
            batchBlob += s.candidateId + ";";
        }
        selectedBatchHash = Sha256::computeHex(batchBlob);
    }
};

/**
 * @brief Planificador adaptativo explicable y determinista para diseño óptimo de experimentos (Fase 20.4).
 * Combina cobertura espacial tipada, incertidumbre predictiva, desacuerdo entre modelos,
 * penalización por coste y protección estricta del conjunto holdout.
 */
class AdaptiveExperimentPlanner
{
public:
    AdaptiveExperimentPlanner(TargetContract contract,
                              TargetAuditReport auditReport,
                              AdaptivePlannerPolicy policy = AdaptivePlannerPolicy());

    /**
     * @brief Conecta la protección de holdout. Aislamiento estructural: no puede leer señales acústicas.
     */
    void setHoldoutGuard(const IHoldoutProtection* holdoutGuard) noexcept;

    /**
     * @brief Registra un punto ya medido en ensayos anteriores para actualizar cobertura e incertidumbre.
     */
    void registerMeasuredPoint(std::vector<TypedCoordinate> point, double observedVariance = 0.0);

    /**
     * @brief Genera y selecciona el siguiente lote adaptativo de candidatos informativos.
     * @param batchSize Número de ensayos a seleccionar en este lote.
     * @param gridResolution Resolución de muestreo candidato por dimensión continua (ej. 11 = {0.0, 0.1, ..., 1.0}).
     */
    [[nodiscard]] AdaptivePlanResult planNextBatch(int batchSize = 1, int gridResolution = 11);

    /**
     * @brief Devuelve la política de planificación activa.
     */
    [[nodiscard]] const AdaptivePlannerPolicy& getPolicy() const noexcept { return policy_; }

    /**
     * @brief Devuelve la lista de puntos medidos acumulados.
     */
    [[nodiscard]] const std::vector<std::vector<TypedCoordinate>>& getMeasuredPoints() const noexcept { return measuredPoints_; }

private:
    TargetContract contract_;
    TargetAuditReport auditReport_;
    AdaptivePlannerPolicy policy_;
    const IHoldoutProtection* holdoutGuard_ { nullptr };
    DistancePolicy distancePolicy_;

    std::vector<std::vector<TypedCoordinate>> measuredPoints_;
    std::vector<double> measuredVariances_;
    int executedTrialsCount_ { 0 };

    // Pipeline estricto de etapas unidireccionales
    std::vector<CandidateAcquisitionBreakdown> generateCandidates(int gridResolution);
    void validateContractAndSafety(CandidateAcquisitionBreakdown& candidate);
    void checkHoldoutCollision(CandidateAcquisitionBreakdown& candidate);
    double estimateMeasurementCost(const CandidateAcquisitionBreakdown& candidate);
    void scoreCandidate(CandidateAcquisitionBreakdown& candidate);
    std::vector<CandidateAcquisitionBreakdown> selectBatchWithDiversity(
        std::vector<CandidateAcquisitionBreakdown>& pool, int batchSize);
    PlannerTerminationReason evaluateStoppingPolicy(
        const std::vector<CandidateAcquisitionBreakdown>& selected,
        const std::vector<CandidateAcquisitionBreakdown>& pool,
        std::string& outRationale);
};

} // namespace abdaudiolab::synth
