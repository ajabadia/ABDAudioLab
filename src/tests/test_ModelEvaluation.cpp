#include <catch2/catch_test_macros.hpp>
#include "synth/ModelEvaluationTypes.h"
#include "synth/ModelEvaluationBuilder.h"

using namespace abdaudiolab::synth;

namespace
{

/**
 * @brief Evaluador simulado de un modelo candidato que aplica un factor de escala de fidelidad.
 */
class MockCandidateEvaluator : public IModelCandidateEvaluator
{
public:
    explicit MockCandidateEvaluator(float noiseLevel = 0.0f)
        : noiseLevel_(noiseLevel) {}

    std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        std::vector<float> out = point.targetGroundTruthAudio;
        for (size_t i = 0; i < out.size(); ++i)
        {
            // Inyectar pequeña perturbación controlada
            out[i] += noiseLevel_ * std::sin(static_cast<float>(i) * 0.1f);
        }
        return out;
    }

private:
    float noiseLevel_ { 0.0f };
};

/**
 * @brief Helper para fabricar un TargetAuditReport válido básico.
 */
TargetAuditReport makeValidAuditReport(ApprovalStatus status = ApprovalStatus::Approved)
{
    TargetAuditReport report;
    report.auditProtocolId = "audit_canonical";
    report.auditProtocolVersion = "1.0.0";
    report.sampleRate = 96000.0;
    report.blockSize = 256;
    report.approvalStatus = status;
    report.isApprovedForParameterExcitation = (status == ApprovalStatus::Approved || status == ApprovalStatus::ApprovedWithWarnings);
    if (status == ApprovalStatus::ApprovedWithWarnings)
    {
        report.warnings.push_back("OscillatorFreeRunningPhase");
        report.summaryMessage = "Requires state reset before each trial";
    }
    return report;
}

/**
 * @brief Helper para fabricar un ExcitationExperimentReport válido básico.
 */
ExcitationExperimentReport makeValidExcitationReport()
{
    ExcitationExperimentReport exp;
    exp.experimentId = "exp_001";
    exp.targetIdentityHash = "target_hash_123456";
    exp.recipeType = "DifferentialRamp";
    exp.trialCount = 10;
    exp.sampleRate = 96000.0;
    exp.blockSize = 256;
    exp.excitedParameters = { "Cutoff", "Resonance" };
    exp.observedSensitivities["Cutoff"] = 0.85;
    exp.observedSensitivities["Resonance"] = 0.60;
    exp.experimentPlanHash = "plan_hash_abc";
    exp.executionTraceHash = "trace_hash_def";
    exp.computeHash();
    return exp;
}

/**
 * @brief Helper para fabricar un ModelArtifactDescriptor básico.
 */
ModelArtifactDescriptor makeValidModelArtifact()
{
    ModelArtifactDescriptor model;
    model.modelId = "LUT_SIMD_2D";
    model.modelArchitecture = "LookupTable2D";
    model.format = "binary_lut";
    model.parameterCount = 2048;
    model.artifactHash = "model_hash_987654";
    return model;
}

/**
 * @brief Helper para fabricar un HoldoutDataset congelado con 2 puntos sintéticos.
 */
HoldoutDataset makeValidHoldoutDataset()
{
    std::vector<HoldoutValidationPoint> pts;

    HoldoutValidationPoint p1;
    p1.pointId = "holdout_pt_1";
    p1.coordinates = {
        { "Cutoff", DimensionKind::Continuous, 0.33, 0, "" },
        { "Note", DimensionKind::Note, 60.0, 60, "" }
    };
    p1.targetGroundTruthAudio.assign(256, 0.5f);
    p1.acousticHash = "audio_hash_p1";
    pts.push_back(p1);

    HoldoutValidationPoint p2;
    p2.pointId = "holdout_pt_2";
    p2.coordinates = {
        { "Cutoff", DimensionKind::Continuous, 0.77, 0, "" },
        { "Note", DimensionKind::Note, 72.0, 72, "" }
    };
    p2.targetGroundTruthAudio.assign(256, -0.4f);
    p2.acousticHash = "audio_hash_p2";
    pts.push_back(p2);

    return HoldoutDataset("holdout_canonical_v1", pts, "EvaluationPhaseOnly");
}

} // namespace

TEST_CASE("ModelEvaluation: Procedencia inmutable de hashes y artefactos", "[synth][evaluation]")
{
    auto audit = makeValidAuditReport();
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();
    auto holdout = makeValidHoldoutDataset();
    MockCandidateEvaluator evaluator(0.0f); // Réplica exacta (ESR -> -inf)

    ModelEvaluation eval = ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    // 1. Procedencia conservada
    REQUIRE_FALSE(eval.sourceAuditReportHash.empty());
    REQUIRE_FALSE(eval.sourceExcitationReportHash.empty());
    REQUIRE_FALSE(eval.sourceHoldoutHash.empty());
    REQUIRE_FALSE(eval.modelArtifactHash.empty());
    REQUIRE_FALSE(eval.canonicalEvaluationHash.empty());

    // 2. Coincidencia exacta con hashes de origen
    REQUIRE(eval.sourceExcitationReportHash == exp.reportHash);
    REQUIRE(eval.sourceHoldoutHash == holdout.getHoldoutHash());
    REQUIRE(eval.modelArtifactHash == model.artifactHash);

    // 3. Veredicto formal
    REQUIRE(eval.decision.status == SelectionStatus::Accepted);
    REQUIRE(eval.decision.recommendedModelId == "LUT_SIMD_2D");
}

TEST_CASE("ModelEvaluation: Rechazo ante componentes obligatorios ausentes", "[synth][evaluation]")
{
    auto audit = makeValidAuditReport();
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();

    SECTION("Falta TargetAuditReport")
    {
        ModelEvaluation eval = ModelEvaluationBuilder()
            .withExcitationReport(exp)
            .withModelArtifact(model)
            .build();

        REQUIRE(eval.decision.status == SelectionStatus::InvalidMeasurement);
        REQUIRE(eval.sourceAuditReportHash == "MISSING_AUDIT_REPORT");
    }

    SECTION("Falta ExcitationExperimentReport")
    {
        ModelEvaluation eval = ModelEvaluationBuilder()
            .withTargetAudit(audit)
            .withModelArtifact(model)
            .build();

        REQUIRE(eval.decision.status == SelectionStatus::InvalidMeasurement);
        REQUIRE(eval.sourceExcitationReportHash == "MISSING_EXCITATION_REPORT");
    }

    SECTION("Falta ModelArtifactDescriptor")
    {
        ModelEvaluation eval = ModelEvaluationBuilder()
            .withTargetAudit(audit)
            .withExcitationReport(exp)
            .build();

        REQUIRE(eval.decision.status == SelectionStatus::InvalidMeasurement);
        REQUIRE(eval.modelArtifactHash == "MISSING_MODEL_ARTIFACT");
    }
}

TEST_CASE("ModelEvaluation: Propagacion estricta de rechazo del target", "[synth][evaluation]")
{
    auto rejectedAudit = makeValidAuditReport(ApprovalStatus::Rejected);
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();
    auto holdout = makeValidHoldoutDataset();
    MockCandidateEvaluator evaluator(0.0f);

    ModelEvaluation eval = ModelEvaluationBuilder()
        .withTargetAudit(rejectedAudit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    REQUIRE(eval.decision.status == SelectionStatus::Rejected);
    REQUIRE_FALSE(eval.limitations.empty());
}

TEST_CASE("ModelEvaluation: Propagacion obligatoria de advertencias (ApprovedWithWarnings)", "[synth][evaluation]")
{
    // Como en Dexed (fase libre / reset necesario)
    auto warnAudit = makeValidAuditReport(ApprovalStatus::ApprovedWithWarnings);
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();
    auto holdout = makeValidHoldoutDataset();
    MockCandidateEvaluator evaluator(0.0f); // Aunque el modelo sea 100% perfecto

    ModelEvaluation eval = ModelEvaluationBuilder()
        .withTargetAudit(warnAudit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    // No debe disolver la advertencia convirtiéndolo en Accepted puro
    REQUIRE(eval.decision.status == SelectionStatus::AcceptedWithWarnings);
    REQUIRE_FALSE(eval.warnings.empty());

    // Debe contener explícitamente la traza de la advertencia del target
    bool hasTargetWarning = false;
    for (const auto& w : eval.warnings)
    {
        if (w.find("TargetAuditWarning") != std::string::npos || w.find("OscillatorFreeRunningPhase") != std::string::npos)
            hasTargetWarning = true;
    }
    REQUIRE(hasTargetWarning);
}

TEST_CASE("ModelEvaluation: Holdout ausente fuerza estado Inconclusive", "[synth][evaluation]")
{
    auto audit = makeValidAuditReport();
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();

    // Sin Holdout
    ModelEvaluation eval = ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .build();

    REQUIRE(eval.decision.status == SelectionStatus::Inconclusive);
    bool hasHoldoutMissing = false;
    for (const auto& w : eval.warnings)
    {
        if (w.find("HoldoutValidationMissing") != std::string::npos)
            hasHoldoutMissing = true;
    }
    REQUIRE(hasHoldoutMissing);
}

TEST_CASE("ModelEvaluation: Proteccion, inmutabilidad y deteccion de colision en HoldoutDataset", "[synth][evaluation]")
{
    auto holdout = makeValidHoldoutDataset();
    REQUIRE(holdout.size() == 2);
    REQUIRE(holdout.getAccessCount() == 0);
    REQUIRE_FALSE(holdout.getHoldoutHash().empty());

    DistancePolicy policy;

    // Coordenada que colisiona con el punto 1 del holdout
    std::vector<TypedCoordinate> collisionCoord = {
        { "Cutoff", DimensionKind::Continuous, 0.33, 0, "" },
        { "Note", DimensionKind::Note, 60.0, 60, "" }
    };
    REQUIRE(holdout.containsCoordinate(collisionCoord, policy, 1e-4));

    // Coordenada libre (no colisiona)
    std::vector<TypedCoordinate> freeCoord = {
        { "Cutoff", DimensionKind::Continuous, 0.50, 0, "" },
        { "Note", DimensionKind::Note, 60.0, 60, "" }
    };
    REQUIRE_FALSE(holdout.containsCoordinate(freeCoord, policy, 1e-4));

    // Al ser consultado formalmente para evaluación, se audita el acceso
    const auto& pts = holdout.accessForEvaluation();
    REQUIRE(pts.size() == 2);
    REQUIRE(holdout.getAccessCount() == 1);
}

TEST_CASE("ModelEvaluation: Serializacion JSON canonica", "[synth][evaluation]")
{
    auto audit = makeValidAuditReport();
    auto exp = makeValidExcitationReport();
    auto model = makeValidModelArtifact();
    auto holdout = makeValidHoldoutDataset();
    MockCandidateEvaluator evaluator(0.001f); // Pequeño error

    ModelEvaluation eval = ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    std::string json = ModelEvaluationBuilder::toJsonString(eval);
    REQUIRE_FALSE(json.empty());
    REQUIRE(json.find("\"evaluationId\"") != std::string::npos);
    REQUIRE(json.find("\"sourceAuditReportHash\"") != std::string::npos);
    REQUIRE(json.find("\"sourceExcitationReportHash\"") != std::string::npos);
    REQUIRE(json.find("\"sourceHoldoutHash\"") != std::string::npos);
    REQUIRE(json.find("\"modelArtifactHash\"") != std::string::npos);
    REQUIRE(json.find("\"status\": \"Accepted\"") != std::string::npos);
}
