#include "ModelEvaluationBuilder.h"
#include <nlohmann/json.hpp>
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

ModelEvaluationBuilder& ModelEvaluationBuilder::withExcitationReport(const ExcitationSessionReport& excitationReport)
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
        eval.excitationSummary = std::make_shared<ExcitationSessionReport>(*excitationReport_);
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

namespace {

nlohmann::json toCanonicalJsonObject(const ModelEvaluation& eval)
{
    nlohmann::json j;
    nlohmann::json dec;
    dec["rationale"] = eval.decision.rationale;
    dec["recommendedModelId"] = eval.decision.recommendedModelId;
    dec["status"] = selectionStatusToString(eval.decision.status);
    j["decision"] = dec;

    nlohmann::json diag;
    diag["peakAutocorrelation"] = eval.diagnostics.peakAutocorrelation;
    diag["residualCharacterization"] = eval.diagnostics.residualCharacterization;
    j["diagnostics"] = diag;

    j["evaluationId"] = eval.evaluationId;
    j["evaluationOrigin"] = evaluationOriginToString(eval.origin);

    nlohmann::json met;
    met["esrDb"] = eval.metrics.errorToSignalRatioDb;
    met["peakError"] = eval.metrics.peakError;
    met["rSquared"] = eval.metrics.rSquaredScore;
    met["rmse"] = eval.metrics.rootMeanSquareError;
    j["metrics"] = met;

    nlohmann::json model;
    model["architecture"] = eval.evaluatedModel.modelArchitecture;
    model["format"] = eval.evaluatedModel.format;
    model["modelId"] = eval.evaluatedModel.modelId;
    j["model"] = model;

    j["protocolVersion"] = eval.evaluationProtocolVersion;

    nlohmann::json prov;
    prov["modelArtifactHash"] = eval.modelArtifactHash;
    prov["sourceAuditReportHash"] = eval.sourceAuditReportHash;
    prov["sourceExcitationReportHash"] = eval.sourceExcitationReportHash;
    prov["sourceHoldoutHash"] = eval.sourceHoldoutHash;

    if (eval.origin == EvaluationOrigin::MeasuredExternalPlugin || !eval.pluginBinarySha256.empty())
    {
        nlohmann::json binProv;
        binProv["binarySha256"] = eval.pluginBinarySha256;
        binProv["buildConfiguration"] = eval.buildConfiguration;
        binProv["executionMode"] = eval.executionMode;
        binProv["fileSizeBytes"] = eval.fileSizeBytes;
        binProv["hostBlockSize"] = eval.blockSize;
        binProv["hostSampleRate"] = eval.sampleRate;
        binProv["normalizedFingerprint"] = eval.normalizedFingerprint;
        binProv["osArchitecture"] = eval.osArchitecture;
        binProv["pluginFormatVersion"] = eval.pluginFormatVersion;
        binProv["pluginPath"] = eval.pluginPath;
        binProv["pluginUid"] = eval.pluginUid;
        binProv["vendor"] = eval.vendor;
        prov["binaryProvenance"] = binProv;
    }
    j["provenance"] = prov;

    j["limitations"] = eval.limitations;
    j["sourceTargetIdentity"] = eval.sourceTargetIdentity;
    j["warnings"] = eval.warnings;

    return j;
}

} // namespace

std::string ModelEvaluationBuilder::computeCanonicalHash(const ModelEvaluation& eval)
{
    nlohmann::json j = toCanonicalJsonObject(eval);
    std::string canonicalJson = j.dump();
    return Sha256::computeHex(canonicalJson);
}

std::string ModelEvaluation::computeCanonicalHash()
{
    canonicalEvaluationHash = ModelEvaluationBuilder::computeCanonicalHash(*this);
    hashVerified = true;
    return canonicalEvaluationHash;
}

std::string ModelEvaluationBuilder::toJsonString(const ModelEvaluation& eval)
{
    nlohmann::json j = toCanonicalJsonObject(eval);
    j["canonicalEvaluationHash"] = eval.canonicalEvaluationHash;
    return j.dump(2);
}

EvaluationLoadStatus ModelEvaluationBuilder::fromJsonString(const std::string& jsonString,
                                                            ModelEvaluation& outEval,
                                                            std::string& outError)
{
    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(jsonString);
    }
    catch (const std::exception& e)
    {
        outError = "Invalid JSON syntax: " + std::string(e.what());
        outEval.loadStatus = EvaluationLoadStatus::InvalidJson;
        outEval.hashVerified = false;
        return EvaluationLoadStatus::InvalidJson;
    }

    // 1. Validar campos obligatorios del esquema canónico
    if (!j.contains("evaluationId") || !j.contains("protocolVersion") ||
        !j.contains("canonicalEvaluationHash") || !j.contains("model") ||
        !j.contains("metrics") || !j.contains("decision"))
    {
        outError = "Schema mismatch: missing mandatory fields in ModelEvaluation JSON";
        outEval.loadStatus = EvaluationLoadStatus::SchemaMismatch;
        outEval.hashVerified = false;
        return EvaluationLoadStatus::SchemaMismatch;
    }

    // 2. Validar versión de protocolo
    std::string proto = j["protocolVersion"].is_string() ? j["protocolVersion"].get<std::string>() : "";
    if (proto != "1.0.0")
    {
        outError = "Unsupported protocol version: " + proto;
        outEval.loadStatus = EvaluationLoadStatus::UnsupportedProtocol;
        outEval.hashVerified = false;
        return EvaluationLoadStatus::UnsupportedProtocol;
    }

    // 3. Extraer hash declarado
    std::string declaredHash = j["canonicalEvaluationHash"].is_string() ? j["canonicalEvaluationHash"].get<std::string>() : "";

    // 4. Eliminar o excluir el campo hash para serialización canónica
    j.erase("canonicalEvaluationHash");

    // 5. Serializar canónicamente y recalcular SHA-256 (RFC 8785)
    std::string canonicalJson = j.dump();
    std::string recomputedHash = Sha256::computeHex(canonicalJson);

    // 6. Extraer metadatos y contenido a outEval
    outEval.evaluationId = j.value("evaluationId", "");
    outEval.evaluationProtocolVersion = proto;
    outEval.sourceTargetIdentity = j.value("sourceTargetIdentity", "");
    outEval.origin = evaluationOriginFromString(j.value("evaluationOrigin", "ImportedArtifact"));

    if (j.contains("provenance") && j["provenance"].is_object())
    {
        const auto& prov = j["provenance"];
        outEval.sourceAuditReportHash = prov.value("sourceAuditReportHash", "");
        outEval.sourceExcitationReportHash = prov.value("sourceExcitationReportHash", "");
        outEval.sourceHoldoutHash = prov.value("sourceHoldoutHash", "");
        outEval.modelArtifactHash = prov.value("modelArtifactHash", "");

        if (prov.contains("binaryProvenance") && prov["binaryProvenance"].is_object())
        {
            const auto& binProv = prov["binaryProvenance"];
            outEval.pluginBinarySha256 = binProv.value("binarySha256", "");
            outEval.buildConfiguration = binProv.value("buildConfiguration", "");
            outEval.executionMode = binProv.value("executionMode", "");
            outEval.fileSizeBytes = binProv.value("fileSizeBytes", static_cast<uint64_t>(0));
            outEval.normalizedFingerprint = binProv.value("normalizedFingerprint", "");
            outEval.osArchitecture = binProv.value("osArchitecture", "");
            outEval.pluginFormatVersion = binProv.value("pluginFormatVersion", "");
            outEval.pluginPath = binProv.value("pluginPath", "");
            outEval.pluginUid = binProv.value("pluginUid", "");
            outEval.vendor = binProv.value("vendor", "");
        }
    }

    const auto& m = j["model"];
    outEval.evaluatedModel.modelId = m.value("modelId", "");
    outEval.evaluatedModel.modelArchitecture = m.value("architecture", "");
    outEval.evaluatedModel.format = m.value("format", "");

    const auto& met = j["metrics"];
    outEval.metrics.errorToSignalRatioDb = met.value("esrDb", -120.0);
    outEval.metrics.rootMeanSquareError = met.value("rmse", 0.0);
    outEval.metrics.peakError = met.value("peakError", 0.0);
    outEval.metrics.rSquaredScore = met.value("rSquared", 1.0);

    if (j.contains("diagnostics") && j["diagnostics"].is_object())
    {
        const auto& diag = j["diagnostics"];
        outEval.diagnostics.peakAutocorrelation = diag.value("peakAutocorrelation", 0.0);
        outEval.diagnostics.residualCharacterization = diag.value("residualCharacterization", "");
    }

    const auto& dec = j["decision"];
    outEval.decision.status = selectionStatusFromString(dec.value("status", "Inconclusive"));
    outEval.decision.recommendedModelId = dec.value("recommendedModelId", "");
    outEval.decision.rationale = dec.value("rationale", "");

    outEval.warnings.clear();
    if (j.contains("warnings") && j["warnings"].is_array())
    {
        for (const auto& w : j["warnings"])
        {
            if (w.is_string())
                outEval.warnings.push_back(w.get<std::string>());
        }
    }

    outEval.limitations.clear();
    if (j.contains("limitations") && j["limitations"].is_array())
    {
        for (const auto& l : j["limitations"])
        {
            if (l.is_string())
                outEval.limitations.push_back(l.get<std::string>());
        }
    }

    // 7. Comparar estrictamente contra el declarado
    if (declaredHash.empty() || declaredHash != recomputedHash)
    {
        outEval.canonicalEvaluationHash = declaredHash; // Preservar declarado para auditoría en GUI
        outEval.loadStatus = EvaluationLoadStatus::HashMismatch;
        outEval.hashVerified = false;
        outError = "Cryptographic integrity failure: declared hash '" + declaredHash
                 + "' does not match recomputed SHA-256 '" + recomputedHash + "'";
        return EvaluationLoadStatus::HashMismatch;
    }

    // 8. Hash verificado exitosamente
    outEval.hashVerified = true;
    outEval.canonicalEvaluationHash = recomputedHash;
    if (!outEval.warnings.empty() || !outEval.limitations.empty() ||
        outEval.decision.status == SelectionStatus::AcceptedWithWarnings ||
        outEval.decision.status == SelectionStatus::Rejected ||
        outEval.decision.status == SelectionStatus::Inconclusive)
    {
        outEval.loadStatus = EvaluationLoadStatus::LoadedWithWarnings;
    }
    else
    {
        outEval.loadStatus = EvaluationLoadStatus::LoadedAndVerified;
    }
    return outEval.loadStatus;
}

} // namespace abdaudiolab::synth
