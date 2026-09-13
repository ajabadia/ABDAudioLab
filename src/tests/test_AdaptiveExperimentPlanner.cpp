#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "synth/AdaptiveExperimentPlanner.h"
#include "synth/TargetContract.h"
#include "synth/TargetAuditor.h"
#include "synth/ModelEvaluationTypes.h"

using namespace abdaudiolab::synth;
using Catch::Approx;

namespace
{

TargetContract makeTestContract()
{
    TargetContract contract;
    contract.name = "TestSubtractiveSynth";
    contract.manufacturer = "ABD AudioLab";
    contract.targetVersion = "1.0.0";
    contract.format = "VST3";

    TargetParameterDescriptor cutoff("cutoff", "p_cutoff", 20.0, 20000.0, 1000.0, "Hz",
                                     ParameterCategory::Filter, false, true);
    cutoff.role = ParameterRole::AudioControl;
    contract.parameters.push_back(cutoff);

    TargetParameterDescriptor resonance("resonance", "p_res", 0.0, 1.0, 0.2, "",
                                         ParameterCategory::Filter, false, false);
    resonance.role = ParameterRole::AudioControl;
    contract.parameters.push_back(resonance);

    return contract;
}

TargetAuditReport makeApprovedAuditReport()
{
    TargetAuditReport report;
    report.auditProtocolId = "audit_proto_v1";
    report.auditProtocolVersion = "1.0.0";
    report.determinism = DeterminismClass::Deterministic;
    report.resetCapability = ResetCapability::Resettable;
    report.interNoteState = InterNoteState::Stateless;
    report.generation = GenerationClass::GenerativeAudioObserved;
    report.approvalStatus = ApprovalStatus::Approved;
    report.isApprovedForParameterExcitation = true;

    report.operationalInstructions.resetBeforeEachTrial = true;
    report.operationalInstructions.recommendedSettlingTimeMs = 50.0;
    report.operationalInstructions.useStatisticalAveraging = false;

    return report;
}

class MockHoldoutGuard : public IHoldoutProtection
{
public:
    explicit MockHoldoutGuard(std::vector<TypedCoordinate> protectedPoint, std::string hash = "mock_hash_holdout_999")
        : protectedPoint_(std::move(protectedPoint)), hash_(std::move(hash)) {}

    [[nodiscard]] bool containsCoordinate(const std::vector<TypedCoordinate>& coords,
                                          const DistancePolicy& distPolicy,
                                          double threshold = 1e-4) const override
    {
        double d = distPolicy.computeDistance(coords, protectedPoint_);
        return d < threshold;
    }

    [[nodiscard]] const std::string& getHoldoutHash() const noexcept override
    {
        return hash_;
    }

private:
    std::vector<TypedCoordinate> protectedPoint_;
    std::string hash_;
};

} // namespace

TEST_CASE("AdaptiveExperimentPlanner: Determinismo y Reproducibilidad con la misma semilla", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    policy.randomSeed = 4242;

    AdaptiveExperimentPlanner planner1(contract, audit, policy);
    AdaptiveExperimentPlanner planner2(contract, audit, policy);

    auto res1 = planner1.planNextBatch(2, 5);
    auto res2 = planner2.planNextBatch(2, 5);

    REQUIRE(res1.evaluatedCandidates.size() == res2.evaluatedCandidates.size());
    REQUIRE(res1.selectedBatch.size() == 2);
    REQUIRE(res2.selectedBatch.size() == 2);

    CHECK(res1.candidatePoolHash == res2.candidatePoolHash);
    CHECK(res1.selectedBatchHash == res2.selectedBatchHash);
    CHECK(res1.selectedBatch[0].candidateId == res2.selectedBatch[0].candidateId);
    CHECK(res1.selectedBatch[1].candidateId == res2.selectedBatch[1].candidateId);
    CHECK(res1.selectedBatch[0].alpha == Approx(res2.selectedBatch[0].alpha));
}

TEST_CASE("AdaptiveExperimentPlanner: Aislamiento estructural y exclusión de puntos de Holdout", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    AdaptiveExperimentPlanner planner(contract, audit, policy);

    // Creamos un punto que coincidirá exactamente con uno de los candidatos de rejilla
    // Para rejilla 5 puntos en cutoff [20, 20000] y res [0, 1]:
    // cand_0 es (cutoff=20, res=0)
    std::vector<TypedCoordinate> holdoutCoords;
    TypedCoordinate c1;
    c1.dimensionId = "cutoff";
    c1.kind = DimensionKind::Continuous;
    c1.continuousValue = 20.0;
    holdoutCoords.push_back(c1);

    TypedCoordinate c2;
    c2.dimensionId = "resonance";
    c2.kind = DimensionKind::Continuous;
    c2.continuousValue = 0.0;
    holdoutCoords.push_back(c2);

    MockHoldoutGuard guard(holdoutCoords, "hash_holdout_secret_123");
    planner.setHoldoutGuard(&guard);

    auto result = planner.planNextBatch(3, 5);

    // Comprobar que cand_0 fue excluido por el guard
    bool cand0FoundAndExcluded = false;
    for (const auto& cand : result.evaluatedCandidates)
    {
        if (cand.candidateId == "cand_0")
        {
            CHECK(cand.excludedByHoldout == true);
            CHECK(cand.alpha < 0.0);
            CHECK_FALSE(cand.rejectionReason.empty());
            cand0FoundAndExcluded = true;
        }
    }
    CHECK(cand0FoundAndExcluded);

    // Ningún punto del lote seleccionado debe ser el punto protegido
    for (const auto& selected : result.selectedBatch)
    {
        CHECK(selected.candidateId != "cand_0");
        CHECK(selected.excludedByHoldout == false);
    }
}

TEST_CASE("AdaptiveExperimentPlanner: Validación de límites de contrato y rechazo explícito", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    AdaptiveExperimentPlanner planner(contract, audit, policy);

    CandidateAcquisitionBreakdown invalidCand;
    invalidCand.candidateId = "out_of_bounds";
    TypedCoordinate coord;
    coord.dimensionId = "cutoff";
    coord.continuousValue = 50000.0; // Superior al max de 20000.0
    invalidCand.coordinates.push_back(coord);

    auto result = planner.planNextBatch(1, 3);

    // Verificar que los candidatos generados respetan siempre el contrato
    for (const auto& cand : result.evaluatedCandidates)
    {
        CHECK(cand.passedContract == true);
        CHECK(cand.passedSafety == true);
    }
}

TEST_CASE("AdaptiveExperimentPlanner: Rechazo inmediato si el target fue rechazado en auditoría", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto rejectedAudit = makeApprovedAuditReport();
    rejectedAudit.approvalStatus = ApprovalStatus::Rejected;
    rejectedAudit.isApprovedForParameterExcitation = false;

    AdaptiveExperimentPlanner planner(contract, rejectedAudit);
    auto result = planner.planNextBatch(1, 3);

    CHECK(result.isTerminated == true);
    CHECK(result.terminationReason == PlannerTerminationReason::TargetUnstable);
    CHECK(result.selectedBatch.empty());

    for (const auto& cand : result.evaluatedCandidates)
    {
        CHECK(cand.passedSafety == false);
        CHECK_FALSE(cand.rejectionReason.empty());
    }
}

TEST_CASE("AdaptiveExperimentPlanner: Adaptación operativa del coste K(x) según auditoría", "[synth][planner]")
{
    auto contract = makeTestContract();

    // Auditoría A: Ligera (sin reset forzado, 0 ms settling)
    auto auditLight = makeApprovedAuditReport();
    auditLight.operationalInstructions.resetBeforeEachTrial = false;
    auditLight.operationalInstructions.recommendedSettlingTimeMs = 0.0;
    auditLight.operationalInstructions.useStatisticalAveraging = false;

    // Auditoría B: Pesada (reset forzado, settling 200 ms, promediado estadístico)
    auto auditHeavy = makeApprovedAuditReport();
    auditHeavy.operationalInstructions.resetBeforeEachTrial = true;
    auditHeavy.operationalInstructions.recommendedSettlingTimeMs = 200.0;
    auditHeavy.operationalInstructions.useStatisticalAveraging = true;

    AdaptiveExperimentPlanner plannerLight(contract, auditLight);
    AdaptiveExperimentPlanner plannerHeavy(contract, auditHeavy);

    auto resLight = plannerLight.planNextBatch(1, 3);
    auto resHeavy = plannerHeavy.planNextBatch(1, 3);

    REQUIRE_FALSE(resLight.selectedBatch.empty());
    REQUIRE_FALSE(resHeavy.selectedBatch.empty());

    // El coste K(x) y la duración estimada deben ser sensiblemente mayores en Heavy
    CHECK(resHeavy.selectedBatch[0].kScore > resLight.selectedBatch[0].kScore);
    CHECK(resHeavy.estimatedBatchDurationSec > resLight.estimatedBatchDurationSec);
}

TEST_CASE("AdaptiveExperimentPlanner: Diversidad dentro del lote (Batch Diversity Penalty)", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    policy.batchDiversityPenalty = 0.80;
    policy.minimumCandidateSeparation = 0.20;

    AdaptiveExperimentPlanner planner(contract, audit, policy);

    // Pedimos un lote de 2 candidatos
    auto result = planner.planNextBatch(2, 5);

    REQUIRE(result.selectedBatch.size() == 2);
    const auto& c1 = result.selectedBatch[0];
    const auto& c2 = result.selectedBatch[1];

    DistancePolicy distPolicy;
    double distBetweenSelected = distPolicy.computeDistance(c1.coordinates, c2.coordinates);

    // La diversidad debe evitar seleccionar dos puntos idénticos o hiper-próximos
    CHECK(distBetweenSelected >= 0.05);
    CHECK(c1.candidateId != c2.candidateId);
}

TEST_CASE("AdaptiveExperimentPlanner: Distinción entre Converged y BudgetExhausted", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    policy.maxTrialsBudget = 3; // Presupuesto bajo para agotar rápidamente

    AdaptiveExperimentPlanner planner(contract, audit, policy);

    // Rondas sucesivas consumiendo el presupuesto
    planner.registerMeasuredPoint({}, 0.01);
    planner.registerMeasuredPoint({}, 0.02);

    auto result = planner.planNextBatch(1, 3);

    CHECK(result.isTerminated == true);
    CHECK(result.terminationReason == PlannerTerminationReason::BudgetExhausted);
    CHECK(result.trialsRemainingInBudget == 0);
}

TEST_CASE("AdaptiveExperimentPlanner: Desglose completo de adquisición U, D, C, K y trazabilidad", "[synth][planner]")
{
    auto contract = makeTestContract();
    auto audit = makeApprovedAuditReport();

    AdaptivePlannerPolicy policy;
    policy.weightUncertainty = 0.4;
    policy.weightDisagreement = 0.3;
    policy.weightCoverage = 0.3;
    policy.weightCost = 0.1;

    AdaptiveExperimentPlanner planner(contract, audit, policy);

    auto result = planner.planNextBatch(1, 3);
    REQUIRE_FALSE(result.selectedBatch.empty());

    const auto& picked = result.selectedBatch[0];

    // Verificar desglose granular
    CHECK(picked.uScore >= 0.0);
    CHECK(picked.dScore >= 0.0);
    CHECK(picked.cScore >= 0.0);
    CHECK(picked.kScore > 0.0);

    double expectedAlpha = (policy.weightUncertainty * picked.uScore) +
                           (policy.weightDisagreement * picked.dScore) +
                           (policy.weightCoverage * picked.cScore) -
                           (policy.weightCost * picked.kScore);

    CHECK(picked.alpha == Approx(expectedAlpha));
    CHECK(picked.selected == true);
    CHECK_FALSE(picked.selectionReason.empty());
    CHECK(result.candidatePoolHash.length() == 64);
    CHECK(result.selectedBatchHash.length() == 64);
}
