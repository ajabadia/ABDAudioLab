#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "synth/ModelEvaluationTypes.h"
#include "synth/ModelEvaluationBuilder.h"
#include "synth/CandidateEvaluators.h"

namespace abdaudiolab::synth
{

/**
 * @enum OptimizationPreset
 * @brief Perfiles canónicos de selección multiobjetivo.
 */
enum class OptimizationPreset
{
    StudioMaster,     /**< Máxima fidelidad: lambda = 0.05, mu = 0.15 (prioriza fidelidad pura). */
    LivePerformance,  /**< Máximo rendimiento: lambda = 0.60, mu = 0.20 (prioriza bajo coste y latencia nula). */
    Balanced,         /**< Compromiso nominal: lambda = 0.25, mu = 0.20. */
    Custom            /**< Ponderaciones personalizadas por el usuario. */
};

[[nodiscard]] inline std::string optimizationPresetToString(OptimizationPreset preset)
{
    switch (preset)
    {
        case OptimizationPreset::StudioMaster:    return "StudioMaster";
        case OptimizationPreset::LivePerformance: return "LivePerformance";
        case OptimizationPreset::Balanced:        return "Balanced";
        case OptimizationPreset::Custom:          return "Custom";
        default:                                  return "Unknown";
    }
}

/**
 * @struct SelectionWeights
 * @brief Parámetros y pesos efectivos de la función de coste multiobjetivo.
 * J(m) = E_val(m) + lambda * C(m) + mu * U(m)
 */
struct SelectionWeights
{
    double lambda { 0.25 };            /**< Ponderación del coste computacional C(m). */
    double mu { 0.20 };                /**< Ponderación de la incertidumbre U(m). */
    double uncertaintyMargin { 0.03 }; /**< Margen sigma: si |J(m1) - J(m2)| < sigma => Inconclusive. */
};

/**
 * @struct CandidateBenchmarkResult
 * @brief Registro exhaustivo de evaluación y puntuación de un modelo candidato.
 */
struct CandidateBenchmarkResult
{
    std::string modelId;
    std::string architecture;
    ValidationMetrics metrics;
    ResidualDiagnostics diagnostics;
    ResourceFootprint footprint;

    double validationErrorScore { 0.0 };   /**< E_val(m) in [0, 1] */
    double computationalCostScore { 0.0 }; /**< C(m) in [0, 1] */
    double uncertaintyScore { 0.0 };       /**< U(m) in [0, 1] */
    double totalCompositeScore { 0.0 };    /**< J(m) = E + lambda*C + mu*U */

    bool meetsStrictFidelity { false };
    bool isRawFidelityLeader { false };
};

/**
 * @struct CandidateBenchmarkingReport
 * @brief Informe completo del proceso de evaluación multiobjetivo y comparación.
 */
struct CandidateBenchmarkingReport
{
    OptimizationPreset preset { OptimizationPreset::Balanced };
    SelectionWeights weights;
    std::vector<CandidateBenchmarkResult> candidates;
    std::string bestRawFidelityModelId;
    std::string recommendedTargetModelId;
    std::string runnerUpModelId;
    double runnerUpGap { 0.0 };
    bool isInconclusive { false };
    std::string decisionRationale;
    std::string residualDiagnosis;
};

/**
 * @class MultiObjectiveModelSelector
 * @brief Motor de identificación de sistemas y selección multiobjetivo de modelos emulables.
 */
class MultiObjectiveModelSelector
{
public:
    MultiObjectiveModelSelector();
    explicit MultiObjectiveModelSelector(OptimizationPreset preset);
    MultiObjectiveModelSelector(double lambda, double mu, double sigma = 0.03);

    void setPreset(OptimizationPreset preset) noexcept;
    [[nodiscard]] OptimizationPreset getPreset() const noexcept { return preset_; }

    void setWeights(const SelectionWeights& weights) noexcept
    {
        weights_ = weights;
        preset_ = OptimizationPreset::Custom;
    }
    [[nodiscard]] const SelectionWeights& getWeights() const noexcept { return weights_; }

    void addCandidate(std::shared_ptr<ICandidateModelEvaluator> candidate);
    void clearCandidates() noexcept;
    [[nodiscard]] size_t getCandidateCount() const noexcept { return candidates_.size(); }

    /**
     * @brief Ejecuta el benchmarking exhaustivo sobre el HoldoutDataset y produce la evaluación canónica.
     */
    [[nodiscard]] ModelEvaluation evaluateAndSelect(const TargetAuditReport& audit,
                                                   const ExcitationSessionReport& excitation,
                                                   const HoldoutDataset& holdout,
                                                   CandidateBenchmarkingReport* outReport = nullptr);

    /**
     * @brief Calcula la función de coste normalizada para una terna (error, coste, incertidumbre).
     */
    [[nodiscard]] static double computeCompositeCost(double errorVal, double compCost, double uncertainty,
                                                     double lambda, double mu) noexcept;

private:
    void applyPresetWeights() noexcept;

    OptimizationPreset preset_ { OptimizationPreset::Balanced };
    SelectionWeights weights_;
    std::vector<std::shared_ptr<ICandidateModelEvaluator>> candidates_;
};

} // namespace abdaudiolab::synth
