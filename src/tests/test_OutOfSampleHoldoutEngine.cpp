#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "synth/OutOfSampleHoldoutEngine.h"
#include "synth/HoldoutStimulusGenerator.h"
#include "synth/MultiObjectiveModelSelector.h"
#include "synth/CandidateEvaluators.h"
#include "synth/SyntheticGroundTruthFixtures.h"
#include "synth/ModelEvaluationTypes.h"
#include <vector>
#include <cmath>
#include <numbers>
#include <functional>
#include <algorithm>

using namespace abdaudiolab;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::synth::ground_truth;
using Catch::Matchers::WithinAbs;

namespace
{

TargetAuditReport makeAudit()
{
    TargetAuditReport report;
    report.auditProtocolId = "audit_canonical";
    report.auditProtocolVersion = "1.0.0";
    report.sampleRate = 96000.0;
    report.blockSize = 256;
    report.approvalStatus = ApprovalStatus::Approved;
    report.isApprovedForParameterExcitation = true;
    return report;
}

ExcitationSessionReport makeExcitation()
{
    ExcitationSessionReport exp;
    exp.experimentId = "exp_holdout_001";
    exp.targetIdentityHash = "target_hash_holdout";
    exp.recipeType = "DifferentialSweep";
    exp.trialCount = 10;
    exp.sampleRate = 96000.0;
    exp.blockSize = 256;
    exp.excitedParameters = { "Drive", "Cutoff" };
    exp.observedSensitivities["Drive"] = 0.85;
    exp.observedSensitivities["Cutoff"] = 0.90;
    exp.computeHash();
    return exp;
}

} // namespace

// ==============================================================================
// 1. Verificación Acústica de las 4 Sub-Baterías de Estímulos Ciegos
// ==============================================================================

TEST_CASE("OutOfSampleHoldoutEngine - 4 Blind Stimulus Sub-Batteries Synthesis", "[holdout][stimulus]")
{
    const double sampleRate = 96000.0;

    SECTION("Sub-batería 1: Escalas dinámicas de amplitud (-30 a -3 dBFS)")
    {
        auto steps = HoldoutStimulusGenerator::generateDynamicAmplitudeSteps(sampleRate, 440.0, 0.05, 0.005);
        REQUIRE_FALSE(steps.empty());

        auto props = HoldoutStimulusGenerator::analyzeAcousticProperties(steps, sampleRate);
        REQUIRE_THAT(props.peakDbfs, WithinAbs(-3.0, 0.3));
        REQUIRE(props.rmsDbfs < -3.0);
        REQUIRE(props.rmsDbfs > -20.0);
        REQUIRE(props.peakAmplitude <= 1.0);
    }

    SECTION("Sub-batería 2: Ráfagas de transitorios y silencios (Burst & Ringing)")
    {
        auto bursts = HoldoutStimulusGenerator::generateTransientBursts(sampleRate, 3, 0.02, 0.05, 880.0);
        REQUIRE_FALSE(bursts.empty());

        auto props = HoldoutStimulusGenerator::analyzeAcousticProperties(bursts, sampleRate);
        REQUIRE_THAT(props.peakDbfs, WithinAbs(-3.0, 0.3));

        // Verificar que existen zonas de reposo (silencio absoluto entre ráfagas)
        size_t zeroCount = 0;
        for (float s : bursts)
        {
            if (std::abs(s) < 1e-6f)
                zeroCount++;
        }
        REQUIRE(zeroCount > (bursts.size() / 3)); // Al menos 33% de muestras en reposo
    }

    SECTION("Sub-batería 3: Multitono armónicamente denso (bajo factor de cresta Schroeder)")
    {
        auto multitone = HoldoutStimulusGenerator::generateDenseMultitone(sampleRate, 0.15, { 250.0, 550.0, 1200.0, 3100.0 }, -3.0);
        REQUIRE_FALSE(multitone.empty());

        auto props = HoldoutStimulusGenerator::analyzeAcousticProperties(multitone, sampleRate);
        REQUIRE_THAT(props.peakDbfs, WithinAbs(-3.0, 0.2));
        // Gracias a las fases de Schroeder, el factor de cresta se mantiene controlado (< 9.0 dB considerando ventanas)
        REQUIRE(props.crestFactorDb < 9.0);
        REQUIRE(props.crestFactorDb > 1.5);
    }

    SECTION("Sub-batería 4: Señal pseudo-musical representativa")
    {
        auto musical = HoldoutStimulusGenerator::generatePseudoMusicalSequence(sampleRate, 0.25, 220.0);
        REQUIRE_FALSE(musical.empty());

        auto props = HoldoutStimulusGenerator::analyzeAcousticProperties(musical, sampleRate);
        REQUIRE_THAT(props.peakDbfs, WithinAbs(-3.0, 0.3));
        REQUIRE(props.durationSeconds > 0.24);
    }
}

// ==============================================================================
// 2. Integridad Criptográfica e Inmutabilidad del HoldoutDataset
// ==============================================================================

TEST_CASE("OutOfSampleHoldoutEngine - HoldoutDataset Cryptographic Integrity & Access Policy", "[holdout][dataset]")
{
    const double sampleRate = 96000.0;
    HoldoutEngineConfig cfg;
    cfg.datasetId = "holdout_audit_2026";
    cfg.sampleRate = sampleRate;

    OutOfSampleHoldoutEngine engine(cfg);

    StaticPolynomialSaturatorFixture saturator(0.15, 0.0, PolynomialSaturatorMode::CubicOnly);
    saturator.prepare(sampleRate);

    HoldoutDataset dataset = engine.buildHoldoutDataset([&](const std::vector<float>& in, const std::vector<TypedCoordinate>&) {
        return saturator.process(in);
    });

    REQUIRE(dataset.getDatasetId() == "holdout_audit_2026");
    REQUIRE(dataset.size() == 4);
    REQUIRE_FALSE(dataset.empty());
    REQUIRE(dataset.getAccessPolicy() == "EvaluationPhaseOnly");
    REQUIRE_FALSE(dataset.getHoldoutHash().empty());
    REQUIRE(dataset.getAccessCount() == 0);

    // Acceso formal para evaluación incrementa el contador de auditoría
    const auto& points = dataset.accessForEvaluation();
    REQUIRE(dataset.getAccessCount() == 1);
    REQUIRE(points.size() == 4);

    for (const auto& pt : points)
    {
        REQUIRE_FALSE(pt.pointId.empty());
        REQUIRE_FALSE(pt.inputStimulusAudio.empty());
        REQUIRE_FALSE(pt.targetGroundTruthAudio.empty());
        REQUIRE(pt.inputStimulusAudio.size() == pt.targetGroundTruthAudio.size());
        REQUIRE_FALSE(pt.acousticHash.empty());
        REQUIRE(pt.coordinates.size() == 3);
    }
}

// ==============================================================================
// 3. Auditoría de Fuga de Datos (Data Leakage Protection)
// ==============================================================================

TEST_CASE("OutOfSampleHoldoutEngine - Data Leakage Detection & Isolation", "[holdout][leakage]")
{
    OutOfSampleHoldoutEngine engine;
    HoldoutDataset holdout = engine.buildHoldoutDataset(nullptr);

    // 1. Puntos de entrenamiento disjuntos (lejanos a las coordenadas reservadas)
    std::vector<std::vector<TypedCoordinate>> disjointTrainingPoints;
    {
        std::vector<TypedCoordinate> p1;
        TypedCoordinate c1; c1.dimensionId = "Drive"; c1.continuousValue = 0.05; p1.push_back(c1);
        TypedCoordinate c2; c2.dimensionId = "Cutoff"; c2.continuousValue = 0.95; p1.push_back(c2);
        TypedCoordinate c3; c3.dimensionId = "Resonance"; c3.continuousValue = 0.05; p1.push_back(c3);
        disjointTrainingPoints.push_back(p1);
    }

    bool hasLeakageDisjoint = engine.checkDataLeakage(holdout, disjointTrainingPoints);
    REQUIRE_FALSE(hasLeakageDisjoint);

    // 2. Punto de entrenamiento idéntico / colisionante con Sub 1 (Drive = 0.25, Cutoff = 0.80, Resonance = 0.15)
    std::vector<std::vector<TypedCoordinate>> leakingTrainingPoints;
    {
        std::vector<TypedCoordinate> pLeak;
        TypedCoordinate c1; c1.dimensionId = "Drive"; c1.continuousValue = 0.25; pLeak.push_back(c1);
        TypedCoordinate c2; c2.dimensionId = "Cutoff"; c2.continuousValue = 0.80; pLeak.push_back(c2);
        TypedCoordinate c3; c3.dimensionId = "Resonance"; c3.continuousValue = 0.15; pLeak.push_back(c3);
        leakingTrainingPoints.push_back(pLeak);
    }

    bool hasLeakage = engine.checkDataLeakage(holdout, leakingTrainingPoints);
    REQUIRE(hasLeakage); // El motor detecta la colisión y previene fuga de datos
}

// ==============================================================================
// 4. Descomposición de Incertidumbre e Integración con MultiObjectiveModelSelector
// ==============================================================================

TEST_CASE("OutOfSampleHoldoutEngine - Uncertainty Decomposition & End-to-End Selector Integration", "[holdout][integration]")
{
    const double sampleRate = 96000.0;
    TargetAuditReport audit = makeAudit();
    ExcitationSessionReport excitation = makeExcitation();

    // 1. Verificación de Descomposición de Incertidumbre
    std::vector<float> baseSignal(500, 0.5f);
    std::vector<std::vector<float>> repeatedPasses(3, baseSignal);
    // Simular perturbación analógica estocástica menor
    for (size_t i = 0; i < repeatedPasses[1].size(); ++i)
        repeatedPasses[1][i] += 0.001f * static_cast<float>(std::sin(static_cast<double>(i)));

    std::vector<float> modelPred(500, 0.495f); // Pequeña discrepancia estructural

    auto decomp = OutOfSampleHoldoutEngine::estimateUncertainty(repeatedPasses, modelPred, -96.0);
    REQUIRE(decomp.instrumentalUncertaintyDb > 0.0);
    REQUIRE(decomp.randomDispersalDb > 0.0);
    REQUIRE(decomp.structuralModelErrorDb > 0.0);
    REQUIRE(decomp.combinedUncertaintyDb > decomp.instrumentalUncertaintyDb);

    // 2. Integración End-to-End con MultiObjectiveModelSelector
    HoldoutEngineConfig cfg;
    cfg.datasetId = "holdout_full_e2e";
    cfg.sampleRate = sampleRate;
    OutOfSampleHoldoutEngine engine(cfg);

    StaticPolynomialSaturatorFixture saturator(0.15, 0.0, PolynomialSaturatorMode::CubicOnly);
    saturator.prepare(sampleRate);

    HoldoutDataset holdout = engine.buildHoldoutDataset([&](const std::vector<float>& in, const std::vector<TypedCoordinate>&) {
        return saturator.process(in);
    });

    MultiObjectiveModelSelector selector(OptimizationPreset::Balanced);

    auto lut1d = std::make_shared<Lut1DCandidateEvaluator>("LUT_1D_EXACT", 1024);
    lut1d->setPolynomialCubic(0.15);

    auto lut2d = std::make_shared<LutSimd2DCandidateEvaluator>("LUT_SIMD_2D", 32);

    selector.addCandidate(lut1d);
    selector.addCandidate(lut2d);

    CandidateBenchmarkingReport report;
    ModelEvaluation eval = selector.evaluateAndSelect(audit, excitation, holdout, &report);

    REQUIRE(eval.hashVerified);
    REQUIRE_FALSE(eval.canonicalEvaluationHash.empty());
    REQUIRE(eval.decision.recommendedModelId == "LUT_1D_EXACT");
    REQUIRE(eval.decision.bestRawFidelityModelId == "LUT_1D_EXACT");
    REQUIRE(report.candidates.size() == 2);
    REQUIRE(eval.metrics.errorToSignalRatioDb < -30.0);
}
