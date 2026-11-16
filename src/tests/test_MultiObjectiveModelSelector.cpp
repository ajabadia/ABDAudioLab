#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "synth/MultiObjectiveModelSelector.h"
#include "synth/CandidateEvaluators.h"
#include "synth/SyntheticGroundTruthFixtures.h"
#include "synth/ModelEvaluationTypes.h"
#include "synth/ModelEvaluationBuilder.h"
#include "math/WienerHammersteinFitter.h"
#include <vector>
#include <cmath>
#include <numbers>
#include <functional>
#include <algorithm>

using namespace abdaudiolab;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::synth::ground_truth;
using namespace abdaudiolab::math;
using Catch::Matchers::WithinAbs;

namespace
{

/**
 * @brief Helper para fabricar un TargetAuditReport nominal válido.
 */
TargetAuditReport makeAudit(ApprovalStatus status = ApprovalStatus::Approved)
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
        report.warnings.push_back("NonLinearPhaseResponse");
        report.summaryMessage = "Phase distortion observed in high frequencies";
    }
    return report;
}

/**
 * @brief Helper para fabricar un ExcitationSessionReport nominal válido.
 */
ExcitationSessionReport makeExcitation()
{
    ExcitationSessionReport exp;
    exp.experimentId = "exp_bench_001";
    exp.targetIdentityHash = "target_hash_bench";
    exp.recipeType = "DifferentialSweep";
    exp.trialCount = 10;
    exp.sampleRate = 96000.0;
    exp.blockSize = 256;
    exp.excitedParameters = { "Cutoff", "Drive" };
    exp.observedSensitivities["Cutoff"] = 0.90;
    exp.observedSensitivities["Drive"] = 0.75;
    exp.computeHash();
    return exp;
}

/**
 * @brief Helper para generar un HoldoutDataset a partir de una señal de estímulo y un fixture ground truth.
 */
HoldoutDataset createHoldoutFromFixture(const std::string& datasetId,
                                        const std::vector<float>& stimulus,
                                        std::function<std::vector<float>(const std::vector<float>&)> processFn)
{
    std::vector<HoldoutValidationPoint> points;
    HoldoutValidationPoint pt;
    pt.pointId = "holdout_pt_1";
    pt.inputStimulusAudio = stimulus;
    pt.targetGroundTruthAudio = processFn(stimulus);

    TypedCoordinate c1;
    c1.dimensionId = "Drive";
    c1.kind = DimensionKind::Continuous;
    c1.continuousValue = 0.75;
    pt.coordinates.push_back(c1);

    points.push_back(pt);
    return HoldoutDataset(datasetId, points);
}

} // namespace

// ==============================================================================
// 1. Benchmarking vs 4 Synthetic Ground Truth Fixtures
// ==============================================================================

TEST_CASE("MultiObjectiveModelSelector - Benchmark vs 4 Fixtures", "[selector][benchmark]")
{
    const double sampleRate = 96000.0;
    TargetAuditReport audit = makeAudit();
    ExcitationSessionReport excitation = makeExcitation();

    // Generar estímulo senoidal denso de prueba (500 Hz, 1000 muestras)
    std::vector<float> stimulus(1000);
    for (size_t i = 0; i < stimulus.size(); ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        stimulus[i] = static_cast<float>(0.8 * std::sin(2.0 * std::numbers::pi * 500.0 * t));
    }

    SECTION("StaticPolynomialSaturatorFixture: LUT 1D and LNL correctly fitted beat generic models")
    {
        StaticPolynomialSaturatorFixture saturator(0.15, 0.0, PolynomialSaturatorMode::CubicOnly);
        saturator.prepare(sampleRate);

        HoldoutDataset holdout = createHoldoutFromFixture("holdout_saturator", stimulus, [&](const std::vector<float>& in) {
            return saturator.process(in);
        });

        MultiObjectiveModelSelector selector(OptimizationPreset::Balanced);

        // Candidato 1: LUT 1D ajustado exactamente con la función cúbica
        auto lut1d = std::make_shared<Lut1DCandidateEvaluator>("LUT_1D_EXACT", 1024);
        lut1d->setPolynomialCubic(0.15);

        // Candidato 2: LUT SIMD 2D genérica
        auto lut2d = std::make_shared<LutSimd2DCandidateEvaluator>("LUT_SIMD_2D", 32);

        // Candidato 3: LNL Wiener-Hammerstein
        abdaudiolab::math::WienerHammersteinModel lnlModel;
        lnlModel.h1Taps = { 0.0f, 1.0f, 0.0f };
        lnlModel.h2Taps = { 0.0f, 1.0f, 0.0f };
        lnlModel.nonLinearCoeffA = -0.15f; // y = x - 0.15 x^3
        auto lnl = std::make_shared<WienerHammersteinCandidateEvaluator>("LNL_WH_EXACT", lnlModel);

        selector.addCandidate(lut1d);
        selector.addCandidate(lut2d);
        selector.addCandidate(lnl);

        CandidateBenchmarkingReport report;
        ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

        // Verificación de reporte
        REQUIRE(report.candidates.size() == 3);
        REQUIRE_FALSE(report.bestRawFidelityModelId.empty());
        REQUIRE(eval.hashVerified);
        REQUIRE_FALSE(eval.canonicalEvaluationHash.empty());

        // Ambos modelos ajustados (LUT 1D y LNL) deben lograr ESR muy bajo (< -30 dB)
        auto itWinner = std::find_if(report.candidates.begin(), report.candidates.end(), [&](const CandidateBenchmarkResult& r) {
            return r.modelId == eval.decision.recommendedModelId;
        });
        REQUIRE(itWinner != report.candidates.end());
        REQUIRE(itWinner->metrics.errorToSignalRatioDb < -30.0);
        REQUIRE(itWinner->metrics.rSquaredScore > 0.999);
    }
}

// ==============================================================================
// 2. Preset Inversion (LivePerformance vs StudioMaster)
// ==============================================================================

TEST_CASE("MultiObjectiveModelSelector - Preset Inversion (LivePerformance vs StudioMaster)", "[selector][presets]")
{
    const double sampleRate = 96000.0;
    TargetAuditReport audit = makeAudit();
    ExcitationSessionReport excitation = makeExcitation();

    std::vector<float> stimulus(800);
    for (size_t i = 0; i < stimulus.size(); ++i)
        stimulus[i] = static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * 440.0 * (static_cast<double>(i) / sampleRate)));

    // Fixture ground truth con saturación suave (tanh)
    StaticPolynomialSaturatorFixture saturator(0.15, 0.0, PolynomialSaturatorMode::NormalizedTanh);
    saturator.prepare(sampleRate);
    HoldoutDataset holdout = createHoldoutFromFixture("holdout_tanh", stimulus, [&](const std::vector<float>& in) {
        return saturator.process(in);
    });

    // Candidato 1: LUT 1D aproximada (error moderado ~0.05, CPU ultra bajo 0.15%, 0 latencia)
    auto lightCandidate = std::make_shared<Lut1DCandidateEvaluator>("LIGHT_LUT_1D", 64);
    // Aproximación lineal simple (error moderado frente a tanh)
    lightCandidate->setTransferFunction([](float x) { return x * 0.90f; });

    // Candidato 2: Modelo de alta fidelidad pero computacionalmente pesado (CPU alto 8.5%, latencia 64)
    class HeavyModelEvaluator : public Lut1DCandidateEvaluator
    {
    public:
        using Lut1DCandidateEvaluator::Lut1DCandidateEvaluator;
        [[nodiscard]] ResourceFootprint getResourceFootprint() const override
        {
            ResourceFootprint r;
            r.cpuUsagePercentPerVoice = 8.5; // Fuerte coste CPU
            r.memoryBytes = 2 * 1024 * 1024;
            r.introducedLatencySamples = 64;
            return r;
        }
    };

    auto heavyCandidate = std::make_shared<HeavyModelEvaluator>("HEAVY_HIGH_FIDELITY", 1024);
    heavyCandidate->setTransferFunction([&](float x) { return saturator.evaluateTransfer(x); });

    // 1. Bajo Preset LivePerformance (lambda = 0.60, penaliza fuertemente el consumo de recursos)
    {
        MultiObjectiveModelSelector selector(OptimizationPreset::LivePerformance);
        selector.addCandidate(lightCandidate);
        selector.addCandidate(heavyCandidate);

        CandidateBenchmarkingReport report;
        ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

        // En LivePerformance gana el ligero (LIGHT_LUT_1D) por menor coste total
        REQUIRE(eval.decision.recommendedModelId == "LIGHT_LUT_1D");
        REQUIRE(report.preset == OptimizationPreset::LivePerformance);
    }

    // 2. Bajo Preset StudioMaster (lambda = 0.05, prima la fidelidad pura minimizando penalización de CPU)
    {
        MultiObjectiveModelSelector selector(OptimizationPreset::StudioMaster);
        selector.addCandidate(lightCandidate);
        selector.addCandidate(heavyCandidate);

        CandidateBenchmarkingReport report;
        ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

        // El candidato de alta fidelidad gana en Studio Master
        REQUIRE(eval.decision.recommendedModelId == "HEAVY_HIGH_FIDELITY");
        REQUIRE(eval.decision.bestRawFidelityModelId == "HEAVY_HIGH_FIDELITY");
        REQUIRE(report.preset == OptimizationPreset::StudioMaster);
        REQUIRE(report.weights.lambda == 0.05);
    }
}

// ==============================================================================
// 3. Runner-up gap < sigma marks Inconclusive
// ==============================================================================

TEST_CASE("MultiObjectiveModelSelector - Runner-up gap < sigma marks Inconclusive", "[selector][inconclusive]")
{
    TargetAuditReport audit = makeAudit();
    ExcitationSessionReport excitation = makeExcitation();

    std::vector<float> stimulus(500, 0.5f);
    HoldoutValidationPoint pt;
    pt.pointId = "pt_identical";
    pt.inputStimulusAudio = stimulus;
    pt.targetGroundTruthAudio = stimulus; // Identidad
    HoldoutDataset holdout("holdout_identical", { pt });

    // Dos evaluadores idénticos de la misma arquitectura
    auto candidateA = std::make_shared<Lut1DCandidateEvaluator>("LUT_CANDIDATE_A", 256);
    auto candidateB = std::make_shared<Lut1DCandidateEvaluator>("LUT_CANDIDATE_B", 256);

    // Margen de incertidumbre sigma = 0.05
    MultiObjectiveModelSelector selector(0.20, 0.20, 0.05);
    selector.addCandidate(candidateA);
    selector.addCandidate(candidateB);

    CandidateBenchmarkingReport report;
    ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

    // Como ambos candidatos tienen puntuación idéntica (gap = 0.0 < 0.05), la decisión debe ser Inconclusive
    REQUIRE(report.isInconclusive);
    REQUIRE(eval.decision.status == SelectionStatus::Inconclusive);
    REQUIRE_THAT(report.runnerUpGap, WithinAbs(0.0, 1e-6));
    REQUIRE(eval.decision.rationale.find("estadísticamente equivalentes") != std::string::npos);
    REQUIRE_FALSE(eval.warnings.empty());
}

// ==============================================================================
// 4. Dynamic residual detection
// ==============================================================================

TEST_CASE("MultiObjectiveModelSelector - Dynamic residual detection on DynamicMemoryFixture", "[selector][diagnostics]")
{
    const double sampleRate = 96000.0;
    TargetAuditReport audit = makeAudit();
    ExcitationSessionReport excitation = makeExcitation();

    // Sistema con memoria dinámica fuerte (beta = 2.0)
    DynamicMemorySystemFixture dynamicSystem(0.005, 0.050, 2.0, 0.20);
    dynamicSystem.prepare(sampleRate);

    std::vector<float> stimulus(2000);
    for (size_t i = 0; i < stimulus.size(); ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        stimulus[i] = static_cast<float>(0.85 * std::sin(2.0 * std::numbers::pi * 100.0 * t));
    }

    HoldoutDataset holdout = createHoldoutFromFixture("holdout_dynamic", stimulus, [&](const std::vector<float>& in) {
        return dynamicSystem.process(in);
    });

    // Se evalúa un saturador estático 1D (sin memoria)
    auto staticCandidate = std::make_shared<Lut1DCandidateEvaluator>("STATIC_LUT_1D", 512);

    MultiObjectiveModelSelector selector(OptimizationPreset::Balanced);
    selector.addCandidate(staticCandidate);

    CandidateBenchmarkingReport report;
    ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

    // El residuo frente al saturador estático debe ser dinámico correlacionado (lag-1 autocorrelación >= 0.15)
    REQUIRE(eval.diagnostics.peakAutocorrelation >= 0.15);
    REQUIRE(eval.diagnostics.residualCharacterization == "CorrelatedDynamicResidual");
    REQUIRE(report.residualDiagnosis.find("recomienda considerar arquitectura recurrente") != std::string::npos);
}
