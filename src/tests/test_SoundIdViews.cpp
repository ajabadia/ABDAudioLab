#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/session/ProfilingSessionController.h"
#include "gui/soundid/SoundIdTopHeaderStrip.h"
#include "gui/soundid/SoundIdTargetView.h"
#include "gui/soundid/SoundIdProfilingRunView.h"
#include "gui/soundid/SoundIdResultsSummaryView.h"
#include "synth/ModelEvaluationBuilder.h"

using namespace abdaudiolab;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;

TEST_CASE("SoundIdTopHeaderStrip: Actualización de contexto operativo y badges", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    soundid::SoundIdTopHeaderStrip strip;
    strip.setSize(1000, 40);

    ProfilingSessionSnapshot snapshot;
    snapshot.target.targetName = "Minimoog Model D";
    snapshot.workflowStage = ProfilingWorkflowStage::ProfilingActive;
    snapshot.sessionStatus = ProfilingSessionStatus::Profiling;
    snapshot.evaluation.hasEvaluation = true;
    snapshot.evaluation.recommendedModelType = "Ladder Filter TPT";

    UiAlert alert;
    alert.severity = UiAlert::Severity::Warning;
    alert.title = "Aviso";
    snapshot.activeAlerts.push_back(alert);

    // No debe lanzar excepciones al pintar ni al actualizar
    REQUIRE_NOTHROW(strip.updateFromSnapshot(snapshot));
    REQUIRE_NOTHROW(strip.setHardwareTelemetry(96000.0, 256, 12.5));
}

TEST_CASE("SoundIdTargetView: Reflejo del estado de conexión y avance", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdTargetView view(controller);
    view.setSize(800, 600);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(view.updateFromSnapshot(snap));

    // Conectar target
    TargetSelectionState target;
    target.targetId = "ms20_target";
    target.targetName = "Korg MS-20";
    target.manufacturer = "Korg";
    target.kind = TargetKind::HardwareAnalogue;
    target.isConnected = true;
    target.availableDomainDescription = "MIDI Note 24-84";
    target.parameterCount = 12;

    controller.selectTarget(target);
    snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(view.updateFromSnapshot(snap));
}

TEST_CASE("SoundIdProfilingRunView: Monitor de progreso en vivo y salud acústica", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdProfilingRunView runView(controller);
    runView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "vst3_ref";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "Target ready");
    controller.startProfiling();

    controller.updateProgress(10, 50, 15.0, 60.0, "VCF Sweep Cutoff 0.5");
    controller.updateObservation(-12.0, -1.5, 440.0, false, false, 80.0);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));

    // Telemetría con clipping
    controller.updateObservation(-0.0, 0.5, 440.0, true, false, 40.0);
    snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
}

TEST_CASE("SoundIdResultsSummaryView: Métricas interpretables objetivas y botón de exportación", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_x";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();

    // Actualizar con modelo aceptado
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::Accepted,
                                     "Wiener-Hammerstein Grey-Box",
                                     -48.2, 0.9998, 97.5,
                                     "C1-C6, Vel 1-127", 1.05, {}, {});
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.exportOptions.canExportCpp == true);
}

TEST_CASE("SoundIdResultsSummaryView: Advertencias críticas visibles con AcceptedWithWarnings", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_analogue_drift";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::ApprovedWithWarnings,
                                 "QuasiDeterministic", "RequiresReset", 250.0, true,
                                 { "Requiere reset de fase antes de cada ensayo" },
                                 "Settling prolongado necesario");
    controller.startProfiling();

    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::AcceptedWithWarnings,
                                     "Wiener-Hammerstein Grey-Box",
                                     -35.4, 0.9950, 91.2,
                                     "C2-C5, Vel 40-120", 1.25,
                                     { "Deriva térmica en frecuencias agudas" },
                                     { "Calibración de oscilador recomendada tras 30 min" });
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(snap.exportOptions.canExportCpp == true);
}

TEST_CASE("SoundIdResultsSummaryView: Deshabilitación estricta de exportación ante InvalidMeasurement", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_clipped";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();

    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::InvalidMeasurement,
                                     "None", 0.0, 0.0, 0.0, "None", 1.0,
                                     { "Saturación continua en conversor ADC" },
                                     { "Reducir trim de entrada" });
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::InvalidMeasurement);
    CHECK(snap.exportOptions.canExportCpp == false);
    CHECK(resView.isExportEnabled() == false);
}

namespace
{
class LocalMockEvaluator : public synth::IModelCandidateEvaluator
{
public:
    std::vector<float> predictResponse(const synth::HoldoutValidationPoint& point) override
    {
        return point.targetGroundTruthAudio;
    }
};

synth::TargetAuditReport makeTestAuditReport(synth::ApprovalStatus status)
{
    synth::TargetAuditReport rep;
    rep.auditProtocolId = "audit_soundid_test";
    rep.approvalStatus = status;
    rep.isApprovedForParameterExcitation = (status == synth::ApprovalStatus::Approved || status == synth::ApprovalStatus::ApprovedWithWarnings);
    if (status == synth::ApprovalStatus::ApprovedWithWarnings)
    {
        rep.operationalInstructions.resetBeforeEachTrial = true;
        rep.operationalInstructions.recommendedSettlingTimeMs = 300.0;
        rep.warnings.push_back("OscillatorFreeRunningPhase");
        rep.summaryMessage = "Free running oscillator requires state reset before each trial";
    }
    return rep;
}

synth::ExcitationSessionReport makeTestExcitationReport()
{
    synth::ExcitationSessionReport rep;
    rep.experimentId = "exp_gui_001";
    rep.targetIdentityHash = "target_hash_gui";
    rep.recipeType = "DifferentialRamp";
    rep.trialCount = 12;
    rep.computeHash();
    return rep;
}

synth::ModelArtifactDescriptor makeTestModelArtifact(std::string id = "TPT_ZDF_Filter")
{
    synth::ModelArtifactDescriptor desc;
    desc.modelId = std::move(id);
    desc.modelArchitecture = "StateVariableFilter";
    desc.format = "cpp_header";
    desc.artifactHash = "artifact_hash_abc";
    return desc;
}

synth::HoldoutDataset makeTestHoldoutDataset()
{
    std::vector<synth::HoldoutValidationPoint> pts;
    synth::HoldoutValidationPoint p;
    p.pointId = "pt_1";
    p.coordinates = { { "Cutoff", synth::DimensionKind::Continuous, 0.5, 0, "" } };
    p.targetGroundTruthAudio = { 0.1f, 0.2f, 0.3f, 0.4f };
    pts.push_back(p);
    return synth::HoldoutDataset("holdout_gui", pts);
}
} // namespace

TEST_CASE("SoundIdResultsSummaryView: Presentacion de ModelEvaluation real de SyntheticSynthFixture", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "synthetic_fixture_v1";
    target.targetName = "SyntheticSynthFixture";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(synth::ApprovalStatus::Approved, "Deterministic", "Resettable", 50.0, false, {}, "OK");
    controller.startProfiling();

    // Fabricar ModelEvaluation canónico
    auto audit = makeTestAuditReport(synth::ApprovalStatus::Approved);
    auto exp = makeTestExcitationReport();
    auto model = makeTestModelArtifact("ZDF_Ladder_Synthetic");
    auto holdout = makeTestHoldoutDataset();
    LocalMockEvaluator evaluator;

    synth::ModelEvaluation eval = synth::ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    eval.origin = synth::EvaluationOrigin::MeasuredFixture;
    eval.sourceTargetIdentity = "SyntheticSynthFixture (Laboratory)";
    eval.computeCanonicalHash();

    controller.updateModelEvaluation(eval);
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));

    CHECK(resView.getCurrentVerdict() == synth::SelectionStatus::Accepted);
    CHECK(resView.isExportEnabled() == true);
    CHECK(resView.isHashVerified() == true);
    CHECK(resView.getFullCanonicalHash() == eval.canonicalEvaluationHash);
    CHECK(resView.getHashAuditText().contains("SHA-256:"));
    CHECK(resView.getWarningsText().contains("tolerancias"));

    // Capa 2: La exportación formal en controller es permitida
    CHECK(controller.exportModel("cpp", "build/test_export.cpp") == true);
}

TEST_CASE("SoundIdResultsSummaryView: Presentacion de ModelEvaluation de Dexed (AcceptedWithWarnings + reset + settling visible)", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "dexed_vst3";
    target.targetName = "Dexed FM Synth";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(synth::ApprovalStatus::ApprovedWithWarnings, "Deterministic", "RequiresReset", 300.0, true,
                                 { "OscillatorFreeRunningPhase" }, "Requires reset before each trial");
    controller.startProfiling();

    // Fabricar ModelEvaluation con advertencias reales de Dexed
    auto audit = makeTestAuditReport(synth::ApprovalStatus::ApprovedWithWarnings);
    auto exp = makeTestExcitationReport();
    auto model = makeTestModelArtifact("Dexed_FM_Operator_Model");
    auto holdout = makeTestHoldoutDataset();
    LocalMockEvaluator evaluator;

    synth::ModelEvaluation eval = synth::ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    eval.origin = synth::EvaluationOrigin::MeasuredExternalPlugin;
    eval.sourceTargetIdentity = "Dexed.vst3 1.0.1";
    eval.pluginPath = "C:/Program Files/Common Files/VST3/Dexed.vst3";
    eval.pluginBinarySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    eval.normalizedFingerprint = "Dexed_1.0.1_x86_64";
    eval.computeCanonicalHash();

    controller.updateModelEvaluation(eval);
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));

    CHECK(resView.getCurrentVerdict() == synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(resView.isExportEnabled() == true);
    CHECK(resView.isHashVerified() == true);
    CHECK(resView.getWarningsText().contains("ADVERTENCIAS"));
    CHECK(resView.getWarningsText().contains("Requiere reset"));
    CHECK(resView.getWarningsText().contains("Settling prolongado"));

    // La exportación formal sigue permitida en AcceptedWithWarnings
    CHECK(controller.exportModel("cpp", "build/test_dexed_export.cpp") == true);
}

TEST_CASE("SoundIdResultsSummaryView: Bloqueo estricto ante Inconclusive y Rejected (UI disabled + Controller exportModel devuelve false)", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_test_reject";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(synth::ApprovalStatus::Rejected, "NonDeterministic", "Unstable", 0.0, false, {}, "Unstable");

    // 1. Probar Inconclusive
    controller.updateModelEvaluation(synth::SelectionStatus::Inconclusive, "InconclusiveModel", 0.0, 0.0, 0.0, "", 1.0, {}, {});
    controller.completeProfiling();

    resView.updateFromSnapshot(controller.getCurrentSnapshot());
    CHECK(resView.isExportEnabled() == false);
    CHECK(resView.getCurrentVerdict() == synth::SelectionStatus::Inconclusive);
    CHECK(controller.exportModel("cpp", "invalid.cpp") == false);

    // 2. Probar Rejected
    controller.updateModelEvaluation(synth::SelectionStatus::Rejected, "RejectedModel", -10.0, 0.80, 40.0, "", 1.0, {}, { "ESR tolerancias no alcanzadas" });
    resView.updateFromSnapshot(controller.getCurrentSnapshot());
    CHECK(resView.isExportEnabled() == false);
    CHECK(resView.getCurrentVerdict() == synth::SelectionStatus::Rejected);
    CHECK(controller.exportModel("cpp", "invalid.cpp") == false);
}

TEST_CASE("SoundIdResultsSummaryView: Bloqueo estricto ante HashMismatch (Integridad no verificada -> Export deshabilitado)", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_tampered";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(synth::ApprovalStatus::Approved, "Deterministic", "Resettable", 50.0, false, {}, "OK");
    controller.startProfiling();

    auto audit = makeTestAuditReport(synth::ApprovalStatus::Approved);
    auto exp = makeTestExcitationReport();
    auto model = makeTestModelArtifact("Tampered_Model");
    auto holdout = makeTestHoldoutDataset();
    LocalMockEvaluator evaluator;

    synth::ModelEvaluation eval = synth::ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    // Simular un fallo de integridad criptográfica (HashMismatch)
    eval.computeCanonicalHash();
    eval.loadStatus = synth::EvaluationLoadStatus::HashMismatch;
    eval.hashVerified = false;

    controller.updateModelEvaluation(eval);
    controller.completeProfiling();

    resView.updateFromSnapshot(controller.getCurrentSnapshot());

    // Capa 1: UI deshabilita exportación
    CHECK(resView.isExportEnabled() == false);
    CHECK(resView.isHashVerified() == false);

    // Capa 2: Controller rechaza la exportación
    CHECK(controller.exportModel("cpp", "tampered_export.cpp") == false);
}

TEST_CASE("SoundIdResultsSummaryView: Copia de hash canonico y consistencia exacta con exportacion", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_canonical_sync";
    target.kind = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(synth::ApprovalStatus::Approved, "Deterministic", "Resettable", 50.0, false, {}, "OK");
    controller.startProfiling();

    auto audit = makeTestAuditReport(synth::ApprovalStatus::Approved);
    auto exp = makeTestExcitationReport();
    auto model = makeTestModelArtifact("Canonical_Model_Sync");
    auto holdout = makeTestHoldoutDataset();
    LocalMockEvaluator evaluator;

    synth::ModelEvaluation eval = synth::ModelEvaluationBuilder()
        .withTargetAudit(audit)
        .withExcitationReport(exp)
        .withModelArtifact(model)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evaluator)
        .build();

    eval.origin = synth::EvaluationOrigin::ImportedArtifact;
    eval.sourceTargetIdentity = "CertifiedSynthTarget";
    eval.computeCanonicalHash();

    controller.updateModelEvaluation(eval);
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    resView.updateFromSnapshot(snap);

    // El hash canónico expuesto en la GUI debe ser idéntico al del objeto y al del snapshot
    REQUIRE(resView.getFullCanonicalHash() == eval.canonicalEvaluationHash);
    REQUIRE(snap.evaluation.canonicalEvaluationHash == eval.canonicalEvaluationHash);
    REQUIRE(resView.getFullCanonicalHash().size() == 64); // SHA-256 hex string
}

