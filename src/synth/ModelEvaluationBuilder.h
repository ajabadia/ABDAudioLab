#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "ModelEvaluationTypes.h"

namespace abdaudiolab::synth
{

/**
 * @brief Política de aceptación para evaluar un modelo frente al holdout.
 */
struct ModelAcceptancePolicy
{
    std::string policyId { "standard_production_v1" };
    double maxAcceptedEsrDb { -40.0 };         /**< ESR máximo para Accepted (ej. -40 dB). */
    double maxWarningEsrDb { -24.0 };          /**< ESR máximo tolerable para AcceptedWithWarnings. */
    double minRSquared { 0.95 };               /**< Coeficiente de determinación R^2 mínimo. */
    double maxCpuUsagePercentPerVoice { 5.0 }; /**< Límite de CPU por voz. */
    double maxIntroducedLatencySamples { 64 }; /**< Límite de latencia introducida. */
};

/**
 * @brief Evaluador abstracto de un modelo candidato fuera de muestra.
 */
class IModelCandidateEvaluator
{
public:
    virtual ~IModelCandidateEvaluator() = default;

    /**
     * @brief Genera o predice la respuesta de audio del modelo candidato para un punto de validación.
     */
    [[nodiscard]] virtual std::vector<float> predictResponse(const HoldoutValidationPoint& point) = 0;
};

/**
 * @brief Constructor y validador de invariantes del objeto unificado ModelEvaluation.
 * Garantiza que una auditoría del target, un experimento de excitación y una validación fuera de muestra
 * mantengan su procedencia inmutable sin falsas precisiones ni omisión de advertencias.
 */
class ModelEvaluationBuilder
{
public:
    ModelEvaluationBuilder() = default;
    ~ModelEvaluationBuilder() = default;

    /**
     * @brief Asigna el informe de auditoría previa del target. (Obligatorio)
     */
    ModelEvaluationBuilder& withTargetAudit(const TargetAuditReport& auditReport);

    /**
     * @brief Asigna el informe del experimento de excitación. (Obligatorio)
     */
    ModelEvaluationBuilder& withExcitationReport(const ExcitationExperimentReport& excitationReport);

    /**
     * @brief Asigna el descriptor del modelo candidato a evaluar. (Obligatorio)
     */
    ModelEvaluationBuilder& withModelArtifact(const ModelArtifactDescriptor& modelArtifact);

    /**
     * @brief Asigna el conjunto inmutable de validación ciega fuera de muestra.
     */
    ModelEvaluationBuilder& withHoldoutDataset(const HoldoutDataset* holdoutDataset);

    /**
     * @brief Asigna el evaluador del modelo candidato frente al holdout.
     */
    ModelEvaluationBuilder& withCandidateEvaluator(IModelCandidateEvaluator* evaluator);

    /**
     * @brief Asigna la política de aceptación metrológica.
     */
    ModelEvaluationBuilder& withAcceptancePolicy(const ModelAcceptancePolicy& policy);

    /**
     * @brief Asigna huella de recursos computacionales medida.
     */
    ModelEvaluationBuilder& withResourceFootprint(const ResourceFootprint& footprint);

    /**
     * @brief Construye y valida el objeto canónico ModelEvaluation.
     * Rechaza construcciones sin componentes obligatorios y propaga advertencias.
     */
    [[nodiscard]] ModelEvaluation build();

    /**
     * @brief Serializa el informe ModelEvaluation en formato JSON canónico.
     */
    [[nodiscard]] static std::string toJsonString(const ModelEvaluation& eval);

private:
    const TargetAuditReport* auditReport_ { nullptr };
    const ExcitationExperimentReport* excitationReport_ { nullptr };
    const ModelArtifactDescriptor* modelArtifact_ { nullptr };
    const HoldoutDataset* holdoutDataset_ { nullptr };
    IModelCandidateEvaluator* candidateEvaluator_ { nullptr };
    ModelAcceptancePolicy policy_;
    ResourceFootprint resourceCost_;

    void evaluateHoldout(ModelEvaluation& eval);
    void applyDecisionRules(ModelEvaluation& eval);
};

} // namespace abdaudiolab::synth
