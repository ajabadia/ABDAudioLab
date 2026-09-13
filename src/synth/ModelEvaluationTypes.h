#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "TargetAuditor.h"
#include "ExperimentPlan.h"
#include "TargetContract.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Estado formal del veredicto de evaluación de un modelo exportable.
 */
enum class SelectionStatus
{
    Accepted,               /**< Supera con solvencia todos los criterios de error, estabilidad y CPU. */
    AcceptedWithWarnings,   /**< Aceptado pero con advertencias documentadas (extrapolación, target con advertencias). */
    Inconclusive,           /**< Indeciso: diferencia entre candidatos dentro del margen de incertidumbre, o falta holdout. */
    Rejected,               /**< Incumple restricciones de fidelidad, estabilidad o target rechazado. */
    InvalidMeasurement      /**< Medición abortada o descartada por clipping, inestabilidad o desincronización. */
};

[[nodiscard]] inline std::string selectionStatusToString(SelectionStatus status)
{
    switch (status)
    {
        case SelectionStatus::Accepted:             return "Accepted";
        case SelectionStatus::AcceptedWithWarnings: return "AcceptedWithWarnings";
        case SelectionStatus::Inconclusive:         return "Inconclusive";
        case SelectionStatus::Rejected:             return "Rejected";
        case SelectionStatus::InvalidMeasurement:   return "InvalidMeasurement";
        default:                                    return "Unknown";
    }
}

/**
 * @brief Causa formal de terminación del planificador adaptativo.
 * BudgetExhausted no equivale a Converged.
 */
enum class PlannerTerminationReason
{
    Converged,                /**< Reducción esperada de incertidumbre por unidad de coste inferior a epsilon. */
    BudgetExhausted,          /**< Agotado el presupuesto de ensayos o tiempo asignado. */
    NoInformativeCandidates,  /**< Todos los candidatos explorables aportan información nula o redundante. */
    TargetUnstable,           /**< Target presentó derivas, clipping recurrente o fallos de determinismo. */
    HoldoutProtected,         /**< Detención para evitar invasión de puntos reservados para validación ciega. */
    UserCancelled             /**< Operación cancelada por el operador. */
};

[[nodiscard]] inline std::string plannerTerminationReasonToString(PlannerTerminationReason reason)
{
    switch (reason)
    {
        case PlannerTerminationReason::Converged:               return "Converged";
        case PlannerTerminationReason::BudgetExhausted:         return "BudgetExhausted";
        case PlannerTerminationReason::NoInformativeCandidates: return "NoInformativeCandidates";
        case PlannerTerminationReason::TargetUnstable:          return "TargetUnstable";
        case PlannerTerminationReason::HoldoutProtected:        return "HoldoutProtected";
        case PlannerTerminationReason::UserCancelled:           return "UserCancelled";
        default:                                                return "Unknown";
    }
}

/**
 * @brief Tipología formal de dimensiones en el espacio de parámetros de un sintetizador.
 * Evita la falacia de tratar cambios categóricos o de algoritmo como pequeños deltas flotantes.
 */
enum class DimensionKind
{
    Continuous,   /**< Parámetro analógico continuo (ej. Cutoff, Resonance, Volume). */
    Discrete,     /**< Parámetro entero cuantizado o con pasos finitos (ej. Octava, Polifonía). */
    Categorical,  /**< Parámetro nominal no ordinal (ej. Algoritmo FM 1..32, Tipo de Filtro). */
    Note,         /**< Nota MIDI (0..127, con semántica tonal logarítmica). */
    Velocity,     /**< Dinámica/Velocidad MIDI (1..127). */
    State         /**< Estado discreto o modo operacional (ej. Mono/Poly, Unison). */
};

[[nodiscard]] inline std::string dimensionKindToString(DimensionKind kind)
{
    switch (kind)
    {
        case DimensionKind::Continuous:   return "Continuous";
        case DimensionKind::Discrete:     return "Discrete";
        case DimensionKind::Categorical:  return "Categorical";
        case DimensionKind::Note:         return "Note";
        case DimensionKind::Velocity:     return "Velocity";
        case DimensionKind::State:        return "State";
        default:                          return "Unknown";
    }
}

/**
 * @brief Estado de la evidencia aportada por cada dependencia de entrada.
 */
enum class EvidenceStatus
{
    Verified,
    Inferred,
    Missing,
    Rejected,
    InheritedWithWarnings
};

[[nodiscard]] inline std::string evidenceStatusToString(EvidenceStatus status)
{
    switch (status)
    {
        case EvidenceStatus::Verified:              return "Verified";
        case EvidenceStatus::Inferred:              return "Inferred";
        case EvidenceStatus::Missing:               return "Missing";
        case EvidenceStatus::Rejected:              return "Rejected";
        case EvidenceStatus::InheritedWithWarnings: return "InheritedWithWarnings";
        default:                                    return "Unknown";
    }
}

/**
 * @brief Descriptor de una dimensión tipada en el espacio de parámetros.
 */
struct ParameterDimension
{
    std::string dimensionId;
    DimensionKind kind { DimensionKind::Continuous };
    double minValue { 0.0 };
    double maxValue { 1.0 };
    int stepCount { 0 };
    std::vector<std::string> categoryLabels;
};

/**
 * @brief Coordenada tipada para un punto en el espacio de parámetros.
 */
struct TypedCoordinate
{
    std::string dimensionId;
    DimensionKind kind { DimensionKind::Continuous };
    double continuousValue { 0.0 };
    int discreteValue { 0 };
    std::string categoryValue;
};

/**
 * @brief Política y función de distancia estructurada asimétrica entre coordenadas de parámetros.
 */
struct DistancePolicy
{
    std::string distancePolicyId { "structured_typed_v1" };
    double categoricalMismatchCost { 1.0 }; /**< Penalización por algoritmo o categoría diferente. */
    double missingDimensionCost { 0.5 };

    [[nodiscard]] double computeDistance(const std::vector<TypedCoordinate>& a,
                                         const std::vector<TypedCoordinate>& b) const
    {
        if (a.empty() || b.empty())
            return 1.0;

        double totalDistSq = 0.0;
        int matchedDims = 0;

        for (const auto& ca : a)
        {
            auto it = std::find_if(b.begin(), b.end(), [&](const TypedCoordinate& cb) {
                return cb.dimensionId == ca.dimensionId;
            });

            if (it != b.end())
            {
                matchedDims++;
                const auto& cb = *it;
                if (ca.kind == DimensionKind::Categorical)
                {
                    double diff = (ca.categoryValue == cb.categoryValue) ? 0.0 : categoricalMismatchCost;
                    totalDistSq += diff * diff;
                }
                else if (ca.kind == DimensionKind::State)
                {
                    double diff = (ca.discreteValue == cb.discreteValue) ? 0.0 : 1.0;
                    totalDistSq += diff * diff;
                }
                else if (ca.kind == DimensionKind::Discrete)
                {
                    double diff = (ca.discreteValue == cb.discreteValue) ? 0.0 : 0.5;
                    totalDistSq += diff * diff;
                }
                else if (ca.kind == DimensionKind::Note)
                {
                    double diff = (ca.continuousValue - cb.continuousValue) / 127.0;
                    totalDistSq += diff * diff;
                }
                else if (ca.kind == DimensionKind::Velocity)
                {
                    double diff = (ca.continuousValue - cb.continuousValue) / 127.0;
                    totalDistSq += diff * diff;
                }
                else // Continuous
                {
                    double diff = ca.continuousValue - cb.continuousValue;
                    totalDistSq += diff * diff;
                }
            }
            else
            {
                totalDistSq += missingDimensionCost * missingDimensionCost;
            }
        }

        if (matchedDims == 0)
            return 1.0;

        return std::sqrt(totalDistSq);
    }
};

/**
 * @brief Interfaz restringida de solo lectura geométrica para el planificador adaptativo.
 * Garantiza aislamiento estructural estricto: el planificador NO puede acceder a las señales acústicas del holdout.
 */
class IHoldoutProtection
{
public:
    virtual ~IHoldoutProtection() = default;

    [[nodiscard]] virtual bool containsCoordinate(const std::vector<TypedCoordinate>& coords,
                                                  const DistancePolicy& distPolicy,
                                                  double threshold = 1e-4) const = 0;

    [[nodiscard]] virtual const std::string& getHoldoutHash() const noexcept = 0;
};

/**
 * @brief Desglose explicable de adquisición para cada candidato evaluado por el planificador.
 * Conserva la justificación granular y no solo el valor escalar alpha(x).
 */
struct CandidateAcquisitionBreakdown
{
    std::string candidateId;
    std::vector<TypedCoordinate> coordinates;

    double uScore { 0.0 }; /**< Incertidumbre predictiva U(x). */
    double dScore { 0.0 }; /**< Desacuerdo entre hipótesis de modelos D(x). */
    double cScore { 0.0 }; /**< Cobertura tipada en el espacio de parámetros C(x). */
    double kScore { 0.0 }; /**< Coste estimado de medición (settling, reset, transitorio) K(x). */

    struct {
        double wU { 0.4 };
        double wD { 0.3 };
        double wC { 0.3 };
        double wK { 0.1 };
    } weights;

    double alpha { 0.0 };               /**< alpha(x) = wU*U + wD*D + wC*C - wK*K */
    bool passedContract { true };
    bool passedSafety { true };
    bool excludedByHoldout { false };
    bool selected { false };
    std::string selectionReason;
    std::string rejectionReason;
};

/**
 * @brief Punto de validación reservado fuera de muestra (Holdout Point).
 */
struct HoldoutValidationPoint
{
    std::string pointId;
    std::vector<TypedCoordinate> coordinates;
    std::vector<float> targetGroundTruthAudio;
    double measuredRmsDb { -120.0 };
    double measuredSpectralCentroidHz { 0.0 };
    std::string acousticHash;
};

/**
 * @brief Conjunto inmutable de validación fuera de muestra (Holdout Dataset).
 * Congelado antes de la fase de planificación y estrictamente protegido contra fugas.
 */
class HoldoutDataset : public IHoldoutProtection
{
public:
    HoldoutDataset() = default;

    explicit HoldoutDataset(std::string datasetId,
                            std::vector<HoldoutValidationPoint> points,
                            std::string accessPolicy = "EvaluationPhaseOnly")
        : datasetId_(std::move(datasetId)),
          points_(std::move(points)),
          accessPolicy_(std::move(accessPolicy)),
          createdTimestamp_("2026-09-13T12:00:00Z")
    {
        recomputeHash();
    }

    [[nodiscard]] const std::string& getDatasetId() const noexcept { return datasetId_; }
    [[nodiscard]] const std::string& getHoldoutHash() const noexcept override { return holdoutHash_; }
    [[nodiscard]] const std::string& getAccessPolicy() const noexcept { return accessPolicy_; }
    [[nodiscard]] const std::string& getCreatedTimestamp() const noexcept { return createdTimestamp_; }
    [[nodiscard]] size_t size() const noexcept { return points_.size(); }
    [[nodiscard]] bool empty() const noexcept { return points_.empty(); }
    [[nodiscard]] int getAccessCount() const noexcept { return accessCount_; }
    [[nodiscard]] bool isRetired() const noexcept { return isRetired_; }

    /**
     * @brief Acceso controlado exclusivo para la fase de evaluación formal.
     * Incrementa la auditoría de accesos. NO forma parte de IHoldoutProtection.
     */
    [[nodiscard]] const std::vector<HoldoutValidationPoint>& accessForEvaluation() const
    {
        accessCount_++;
        return points_;
    }

    /**
     * @brief Comprueba si una coordenada colisiona con el conjunto reservado de holdout.
     * Utilizado por el planificador para exclusión física sin revelar las señales acústicas.
     */
    [[nodiscard]] bool containsCoordinate(const std::vector<TypedCoordinate>& coords,
                                          const DistancePolicy& distPolicy,
                                          double threshold = 1e-4) const override
    {
        for (const auto& pt : points_)
        {
            if (distPolicy.computeDistance(pt.coordinates, coords) < threshold)
                return true;
        }
        return false;
    }

    void retire() noexcept { isRetired_ = true; }

private:
    void recomputeHash()
    {
        std::string blob = datasetId_ + ":" + accessPolicy_ + ":" + std::to_string(points_.size()) + "\n";
        for (const auto& pt : points_)
        {
            blob += pt.pointId + ";";
            for (const auto& c : pt.coordinates)
                blob += c.dimensionId + "=" + std::to_string(c.continuousValue) + ",";
            blob += pt.acousticHash + "\n";
        }
        holdoutHash_ = Sha256::computeHex(blob);
    }

    std::string datasetId_;
    std::vector<HoldoutValidationPoint> points_;
    std::string accessPolicy_ { "EvaluationPhaseOnly" };
    std::string createdTimestamp_;
    std::string holdoutHash_;
    mutable int accessCount_ { 0 };
    bool isRetired_ { false };
};

/**
 * @brief Informe empírico consolidado de una sesión de excitación (Fase 20).
 */
struct ExcitationExperimentReport
{
    std::string experimentId;
    std::string targetIdentityHash;
    std::string recipeType;
    int trialCount { 0 };
    double sampleRate { 96000.0 };
    int blockSize { 256 };

    std::vector<std::string> excitedParameters;
    std::map<std::string, double> observedSensitivities;
    bool monotonicityObserved { true };
    PairwiseSubtractionReliability pairwiseReliability { PairwiseSubtractionReliability::PairedDeterministic };

    std::string experimentPlanHash;
    std::string executionTraceHash;
    std::string reportHash;
    std::vector<std::string> warnings;

    void computeHash()
    {
        std::string blob = experimentId + ":" + targetIdentityHash + ":" + recipeType + "\n"
                         + std::to_string(trialCount) + ":" + std::to_string(sampleRate) + "\n"
                         + experimentPlanHash + ":" + executionTraceHash + "\n";
        reportHash = Sha256::computeHex(blob);
    }
};

/**
 * @brief Descriptor del artefacto del modelo exportable candidato (LUT, LNL, GrayBox, NAM).
 */
struct ModelArtifactDescriptor
{
    std::string modelId;             /**< ej. "LUT_SIMD_2D", "LNL_WienerHammerstein", "NAM_WaveNet" */
    std::string modelArchitecture;   /**< ej. "StaticLookupTable", "PolynomialHammerstein", "WaveNet" */
    std::string format;              /**< ej. "cpp_header", "json_weights", "binary_lut" */
    size_t parameterCount { 0 };
    std::string artifactHash;
};

/**
 * @brief Métricas objetivas de validación fuera de muestra (*out-of-sample*).
 */
struct ValidationMetrics
{
    double errorToSignalRatioDb { -120.0 }; /**< ESR = 10*log10(sum(e^2) / sum(y^2)) dB */
    double rootMeanSquareError { 0.0 };
    double peakError { 0.0 };

    struct {
        double subBandDeltaDb { 0.0 };  /**< < 100 Hz */
        double lowBandDeltaDb { 0.0 };  /**< 100 - 1000 Hz */
        double midBandDeltaDb { 0.0 };  /**< 1 kHz - 5 kHz */
        double highBandDeltaDb { 0.0 }; /**< > 5 kHz */
    } spectralBandErrors;

    double phaseErrorRadians { 0.0 };
    double groupDelayErrorSamples { 0.0 };
    double rSquaredScore { 1.0 };
};

/**
 * @brief Diagnósticos de la estructura del residuo e=y_target - y_model.
 */
struct ResidualDiagnostics
{
    double peakAutocorrelation { 0.0 };     /**< Pico de R_ee(tau) para tau != 0. Alto => dinámica no modelada. */
    double inputResidualCoherence { 0.0 };   /**< gamma_xe^2 promedio. Alto => no-linealidad omitida. */
    double cycleAsymmetry { 0.0 };           /**< Asimetría de semiciclos. */
    std::string residualCharacterization;   /**< "UnstructuredNoiseFloor", "DynamicMemoryResidual", etc. */
};

/**
 * @brief Descomposición tripartita de la incertidumbre metrológica.
 */
struct UncertaintyDecomposition
{
    double instrumentalUncertaintyDb { 0.1 }; /**< Ruido de loopback, cuantización de audio. */
    double randomDispersalDb { 0.2 };         /**< Deriva térmica, jitter o variación entre tomas. */
    double structuralModelErrorDb { 0.5 };    /**< Error propio de la arquitectura matemática elegida. */
    double combinedUncertaintyDb { 0.6 };     /**< sqrt(inst^2 + rand^2 + struct^2). */
};

/**
 * @brief Huella de consumo de recursos computacionales.
 */
struct ResourceFootprint
{
    double cpuUsagePercentPerVoice { 0.5 };
    size_t memoryBytes { 4096 };
    int introducedLatencySamples { 0 };
};

/**
 * @brief Veredicto formal y trazabilidad de decisión multiobjetivo.
 */
struct SelectionDecision
{
    SelectionStatus status { SelectionStatus::Inconclusive };
    std::string recommendedModelId;
    std::string runnerUpModelId;
    double selectionScore { 0.0 };
    double runnerUpGap { 0.0 };
    double uncertaintyMargin { 0.0 };
    std::string rationale;
};

/**
 * @brief Objeto unificado canónico ModelEvaluation (Fuente Única de Verdad).
 * Integra los resultados de auditoría, excitación empírica y validación fuera de muestra.
 */
struct ModelEvaluation
{
    ModelEvaluation() = default;

    std::string evaluationId;
    std::string evaluationProtocolVersion { "1.0.0" };
    std::string evaluationProtocolHash { "protocol_canonical_v1" };

    // --- Estados explícitos de evidencia ---
    EvidenceStatus auditEvidenceStatus { EvidenceStatus::Missing };
    EvidenceStatus excitationEvidenceStatus { EvidenceStatus::Missing };
    EvidenceStatus holdoutEvidenceStatus { EvidenceStatus::Missing };
    EvidenceStatus modelArtifactStatus { EvidenceStatus::Missing };

    // --- Procedencia inmutable de entrada ---
    std::string sourceAuditReportHash;
    std::string sourceExcitationReportHash;
    std::string sourceHoldoutHash;
    std::string modelArtifactHash;

    ModelArtifactDescriptor evaluatedModel;
    std::shared_ptr<const TargetAuditReport> targetAuditSummary;
    std::shared_ptr<const ExcitationExperimentReport> excitationSummary;

    // --- Dominios validados ---
    double sampleRate { 96000.0 };
    int blockSize { 256 };
    std::vector<std::string> validatedParameters;
    int validatedNotesMin { 21 };
    int validatedNotesMax { 108 };
    int validatedVelocityMin { 1 };
    int validatedVelocityMax { 127 };

    // --- Metrología y diagnósticos ---
    ValidationMetrics metrics;
    ResidualDiagnostics diagnostics;
    UncertaintyDecomposition uncertainty;
    ResourceFootprint resourceCost;
    SelectionDecision decision;

    std::vector<std::string> warnings;
    std::vector<std::string> limitations;
    std::string canonicalEvaluationHash;

    void computeCanonicalHash()
    {
        std::string blob = evaluationId + ":" + evaluationProtocolVersion + ":" + evaluationProtocolHash + "\n"
                         + sourceAuditReportHash + "\n"
                         + sourceExcitationReportHash + "\n"
                         + sourceHoldoutHash + "\n"
                         + modelArtifactHash + "\n"
                         + selectionStatusToString(decision.status) + "\n"
                         + std::to_string(metrics.errorToSignalRatioDb) + "\n"
                         + std::to_string(metrics.rootMeanSquareError) + "\n"
                         + std::to_string(uncertainty.combinedUncertaintyDb) + "\n";
        canonicalEvaluationHash = Sha256::computeHex(blob);
    }
};

} // namespace abdaudiolab::synth
