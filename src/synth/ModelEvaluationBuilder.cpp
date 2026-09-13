#include "ModelEvaluationBuilder.h"
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace abdaudiolab::synth
{

ModelEvaluationBuilder& ModelEvaluationBuilder::withTargetAudit(const TargetAuditReport& auditReport)
{
    auditReport_ = &auditReport;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withExcitationReport(const ExcitationExperimentReport& excitationReport)
{
    excitationReport_ = &excitationReport;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withModelArtifact(const ModelArtifactDescriptor& modelArtifact)
{
    modelArtifact_ = &modelArtifact;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withHoldoutDataset(const HoldoutDataset* holdoutDataset)
{
    holdoutDataset_ = holdoutDataset;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withCandidateEvaluator(IModelCandidateEvaluator* evaluator)
{
    candidateEvaluator_ = evaluator;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withAcceptancePolicy(const ModelAcceptancePolicy& policy)
{
    policy_ = policy;
    return *this;
}

ModelEvaluationBuilder& ModelEvaluationBuilder::withResourceFootprint(const ResourceFootprint& footprint)
{
    resourceCost_ = footprint;
    return *this;
}

ModelEvaluation ModelEvaluationBuilder::build()
{
    ModelEvaluation eval;
    eval.evaluationId = "eval_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    eval.evaluationProtocolVersion = "1.0.0";
    eval.evaluationProtocolHash = policy_.policyId + ":" + std::to_string(policy_.maxAcceptedEsrDb) + ":" + std::to_string(policy_.maxWarningEsrDb);
    eval.resourceCost = resourceCost_;

    // 1. Procedencia del Audit Report
    if (auditReport_ != nullptr)
    {
        eval.targetAuditSummary = std::make_shared<TargetAuditReport>(*auditReport_);
        std::string auditBlob = auditReport_->auditProtocolId + ":"
                              + auditReport_->auditProtocolVersion + ":"
                              + approvalStatusToString(auditReport_->approvalStatus) + ":"
                              + std::to_string(auditReport_->sampleRate) + ":"
                              + std::to_string(auditReport_->blockSize);
        eval.sourceAuditReportHash = Sha256::computeHex(auditBlob);
        eval.sampleRate = auditReport_->sampleRate;
        eval.blockSize = auditReport_->blockSize;

        if (auditReport_->approvalStatus == ApprovalStatus::Approved)
            eval.auditEvidenceStatus = EvidenceStatus::Verified;
        else if (auditReport_->approvalStatus == ApprovalStatus::ApprovedWithWarnings)
            eval.auditEvidenceStatus = EvidenceStatus::InheritedWithWarnings;
        else
            eval.auditEvidenceStatus = EvidenceStatus::Rejected;
    }
    else
    {
        eval.sourceAuditReportHash = "MISSING_AUDIT_REPORT";
        eval.auditEvidenceStatus = EvidenceStatus::Missing;
    }

    // 2. Procedencia del Excitation Report
    if (excitationReport_ != nullptr)
    {
        eval.excitationSummary = std::make_shared<ExcitationExperimentReport>(*excitationReport_);
        eval.sourceExcitationReportHash = excitationReport_->reportHash.empty()
                                        ? "unhashed_excitation_report"
                                        : excitationReport_->reportHash;
        eval.validatedParameters = excitationReport_->excitedParameters;
        eval.excitationEvidenceStatus = EvidenceStatus::Verified;
    }
    else
    {
        eval.sourceExcitationReportHash = "MISSING_EXCITATION_REPORT";
        eval.excitationEvidenceStatus = EvidenceStatus::Missing;
    }

    // 3. Procedencia del Modelo Candidato
    if (modelArtifact_ != nullptr)
    {
        eval.evaluatedModel = *modelArtifact_;
        eval.modelArtifactHash = modelArtifact_->artifactHash;
        eval.modelArtifactStatus = EvidenceStatus::Verified;
    }
    else
    {
        eval.modelArtifactHash = "MISSING_MODEL_ARTIFACT";
        eval.modelArtifactStatus = EvidenceStatus::Missing;
    }

    // 4. Procedencia del Holdout
    if (holdoutDataset_ != nullptr && !holdoutDataset_->empty())
    {
        eval.sourceHoldoutHash = holdoutDataset_->getHoldoutHash();
        eval.holdoutEvidenceStatus = EvidenceStatus::Verified;
    }
    else
    {
        eval.sourceHoldoutHash = "NONE_OR_EMPTY_HOLDOUT";
        eval.holdoutEvidenceStatus = EvidenceStatus::Missing;
    }

    // 5. Evaluación acústica fuera de muestra si existe holdout y evaluador
    if (holdoutDataset_ != nullptr && !holdoutDataset_->empty() && candidateEvaluator_ != nullptr)
    {
        evaluateHoldout(eval);
    }

    // 6. Aplicar reglas de decisión e invariantes metrológicos
    applyDecisionRules(eval);

    // 7. Sello canónico final
    eval.computeCanonicalHash();

    return eval;
}

void ModelEvaluationBuilder::evaluateHoldout(ModelEvaluation& eval)
{
    const auto& points = holdoutDataset_->accessForEvaluation();
    double totalSse = 0.0;
    double totalSignalEnergy = 0.0;
    size_t totalSamples = 0;
    double maxAbsError = 0.0;
    double sumResidualLag1 = 0.0;
    double sumResidualLag0 = 0.0;

    for (const auto& pt : points)
    {
        std::vector<float> predicted = candidateEvaluator_->predictResponse(pt);
        const auto& truth = pt.targetGroundTruthAudio;

        size_t n = std::min(predicted.size(), truth.size());
        if (n == 0)
            continue;

        totalSamples += n;
        for (size_t i = 0; i < n; ++i)
        {
            double e = static_cast<double>(truth[i]) - static_cast<double>(predicted[i]);
            double sig = static_cast<double>(truth[i]);

            totalSse += e * e;
            totalSignalEnergy += sig * sig;
            double absE = std::abs(e);
            if (absE > maxAbsError)
                maxAbsError = absE;

            sumResidualLag0 += e * e;
            if (i > 0)
            {
                double prevE = static_cast<double>(truth[i - 1]) - static_cast<double>(predicted[i - 1]);
                sumResidualLag1 += e * prevE;
            }
        }
    }

    if (totalSamples > 0 && totalSignalEnergy > 1e-12)
    {
        double esr = totalSse / totalSignalEnergy;
        eval.metrics.errorToSignalRatioDb = (esr > 1e-12) ? (10.0 * std::log10(esr)) : -120.0;
        eval.metrics.rootMeanSquareError = std::sqrt(totalSse / static_cast<double>(totalSamples));
        eval.metrics.peakError = maxAbsError;
        eval.metrics.rSquaredScore = std::max(0.0, 1.0 - (totalSse / totalSignalEnergy));

        if (sumResidualLag0 > 1e-12)
            eval.diagnostics.peakAutocorrelation = std::abs(sumResidualLag1 / sumResidualLag0);
        else
            eval.diagnostics.peakAutocorrelation = 0.0;

        eval.diagnostics.residualCharacterization = (eval.diagnostics.peakAutocorrelation < 0.15)
                                                    ? "UnstructuredResidual"
                                                    : "CorrelatedDynamicResidual";
    }
}

void ModelEvaluationBuilder::applyDecisionRules(ModelEvaluation& eval)
{
    // A. Validación de componentes obligatorios
    if (auditReport_ == nullptr)
    {
        eval.decision.status = SelectionStatus::InvalidMeasurement;
        eval.decision.rationale = "TargetAuditReport is mandatory for model evaluation";
        eval.warnings.push_back("AuditReportMissing");
        return;
    }

    if (excitationReport_ == nullptr)
    {
        eval.decision.status = SelectionStatus::InvalidMeasurement;
        eval.decision.rationale = "ExcitationExperimentReport is mandatory for model evaluation";
        eval.warnings.push_back("ExcitationReportMissing");
        return;
    }

    if (modelArtifact_ == nullptr)
    {
        eval.decision.status = SelectionStatus::InvalidMeasurement;
        eval.decision.rationale = "ModelArtifactDescriptor is mandatory for model evaluation";
        eval.warnings.push_back("ModelArtifactMissing");
        return;
    }

    // B. Regla: Target rechazado propaga rechazo inmediato
    if (auditReport_->approvalStatus == ApprovalStatus::Rejected)
    {
        eval.decision.status = SelectionStatus::Rejected;
        eval.decision.rationale = "Target was rejected during preliminary audit";
        eval.limitations.push_back("TargetUnstableOrNonDeterministic");
        return;
    }

    if (auditReport_->approvalStatus == ApprovalStatus::Unsupported)
    {
        eval.decision.status = SelectionStatus::Rejected;
        eval.decision.rationale = "Target does not support excitation recipe";
        eval.limitations.push_back("TargetUnsupportedForRecipe");
        return;
    }

    // C. Regla: Propagación de advertencias del target (ApprovedWithWarnings nunca se disuelve)
    if (auditReport_->approvalStatus == ApprovalStatus::ApprovedWithWarnings)
    {
        std::string warnMsg = "TargetAuditWarning: Target was approved with warnings: " + auditReport_->summaryMessage;
        eval.warnings.push_back(warnMsg);
        for (const auto& tw : auditReport_->warnings)
            eval.warnings.push_back("TargetWarning: " + tw);
    }

    // D. Regla: Holdout obligatorio para Accepted
    if (holdoutDataset_ == nullptr || holdoutDataset_->empty())
    {
        eval.decision.status = SelectionStatus::Inconclusive;
        eval.decision.rationale = "Out-of-sample holdout dataset is mandatory to accept a model";
        eval.warnings.push_back("HoldoutValidationMissing: Out-of-sample validation not performed");
        return;
    }

    // E. Reglas de métricas frente a la política de aceptación
    eval.decision.recommendedModelId = eval.evaluatedModel.modelId;
    double esr = eval.metrics.errorToSignalRatioDb;

    if (esr > policy_.maxWarningEsrDb)
    {
        eval.decision.status = SelectionStatus::Rejected;
        eval.decision.rationale = "ESR (" + std::to_string(esr) + " dB) exceeds max warning threshold ("
                                + std::to_string(policy_.maxWarningEsrDb) + " dB)";
        eval.limitations.push_back("InsufficientFidelity");
    }
    else if (esr > policy_.maxAcceptedEsrDb)
    {
        eval.decision.status = SelectionStatus::AcceptedWithWarnings;
        eval.decision.rationale = "ESR meets warning threshold but is above pure acceptance threshold";
        eval.warnings.push_back("ModerateFidelityWarning");
    }
    else
    {
        // Cumple la fidelidad estricta. ¿Hereda advertencias del target?
        if (auditReport_->approvalStatus == ApprovalStatus::ApprovedWithWarnings)
        {
            eval.decision.status = SelectionStatus::AcceptedWithWarnings;
            eval.decision.rationale = "Model passes out-of-sample fidelity but inherits target operational warnings";
        }
        else
        {
            eval.decision.status = SelectionStatus::Accepted;
            eval.decision.rationale = "Model passed all out-of-sample fidelity and target criteria";
        }
    }
}

std::string ModelEvaluationBuilder::toJsonString(const ModelEvaluation& eval)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"evaluationId\": \"" << eval.evaluationId << "\",\n";
    ss << "  \"protocolVersion\": \"" << eval.evaluationProtocolVersion << "\",\n";
    ss << "  \"canonicalEvaluationHash\": \"" << eval.canonicalEvaluationHash << "\",\n";
    ss << "  \"provenance\": {\n";
    ss << "    \"sourceAuditReportHash\": \"" << eval.sourceAuditReportHash << "\",\n";
    ss << "    \"sourceExcitationReportHash\": \"" << eval.sourceExcitationReportHash << "\",\n";
    ss << "    \"sourceHoldoutHash\": \"" << eval.sourceHoldoutHash << "\",\n";
    ss << "    \"modelArtifactHash\": \"" << eval.modelArtifactHash << "\"\n";
    ss << "  },\n";
    ss << "  \"model\": {\n";
    ss << "    \"modelId\": \"" << eval.evaluatedModel.modelId << "\",\n";
    ss << "    \"architecture\": \"" << eval.evaluatedModel.modelArchitecture << "\",\n";
    ss << "    \"format\": \"" << eval.evaluatedModel.format << "\"\n";
    ss << "  },\n";
    ss << "  \"metrics\": {\n";
    ss << "    \"esrDb\": " << eval.metrics.errorToSignalRatioDb << ",\n";
    ss << "    \"rmse\": " << eval.metrics.rootMeanSquareError << ",\n";
    ss << "    \"peakError\": " << eval.metrics.peakError << ",\n";
    ss << "    \"rSquared\": " << eval.metrics.rSquaredScore << "\n";
    ss << "  },\n";
    ss << "  \"diagnostics\": {\n";
    ss << "    \"peakAutocorrelation\": " << eval.diagnostics.peakAutocorrelation << ",\n";
    ss << "    \"residualCharacterization\": \"" << eval.diagnostics.residualCharacterization << "\"\n";
    ss << "  },\n";
    ss << "  \"decision\": {\n";
    ss << "    \"status\": \"" << selectionStatusToString(eval.decision.status) << "\",\n";
    ss << "    \"recommendedModelId\": \"" << eval.decision.recommendedModelId << "\",\n";
    ss << "    \"rationale\": \"" << eval.decision.rationale << "\"\n";
    ss << "  },\n";
    ss << "  \"warnings\": [\n";
    for (size_t i = 0; i < eval.warnings.size(); ++i)
    {
        ss << "    \"" << eval.warnings[i] << "\"" << (i + 1 < eval.warnings.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

} // namespace abdaudiolab::synth
