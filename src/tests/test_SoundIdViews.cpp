#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/session/ProfilingSessionController.h"
#include "gui/soundid/SoundIdTopHeaderStrip.h"
#include "gui/soundid/SoundIdTargetView.h"
#include "gui/soundid/SoundIdProfilingRunView.h"
#include "gui/soundid/SoundIdResultsSummaryView.h"
#include "gui/SoundIdMeterStrip.h"
#include "gui/controllers/WorkflowNavigationController.h"
#include "synth/ModelEvaluationBuilder.h"

using namespace abdaudiolab;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;

TEST_CASE("SoundIdTopHeaderStrip: ActualizaciÃ³n de contexto operativo y badges", "[gui][soundid]")
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

TEST_CASE("SoundIdTargetView: Reflejo del estado de conexiÃ³n y avance", "[gui][soundid]")
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

TEST_CASE("SoundIdProfilingRunView: Monitor de progreso en vivo y salud acÃºstica", "[gui][soundid]")
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

    // TelemetrÃ­a con clipping
    controller.updateObservation(-0.0, 0.5, 440.0, true, false, 40.0);
    snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
}

TEST_CASE("SoundIdResultsSummaryView: MÃ©tricas interpretables objetivas y botÃ³n de exportaciÃ³n", "[gui][soundid]")
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

TEST_CASE("SoundIdResultsSummaryView: Advertencias crÃ­ticas visibles con AcceptedWithWarnings", "[gui][soundid]")
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
                                     { "Deriva tÃ©rmica en frecuencias agudas" },
                                     { "CalibraciÃ³n de oscilador recomendada tras 30 min" });
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(snap.exportOptions.canExportCpp == true);
}

TEST_CASE("SoundIdResultsSummaryView: DeshabilitaciÃ³n estricta de exportaciÃ³n ante InvalidMeasurement", "[gui][soundid]")
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
                                     { "SaturaciÃ³n continua en conversor ADC" },
                                     { "Reducir trim de entrada" });
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::InvalidMeasurement);
    CHECK(snap.exportOptions.canExportCpp == false);
    CHECK(resView.isExportEnabled() == false);
}

TEST_CASE("SoundIdResultsSummaryView: Holdout validation, metrology metrics and audition controls (Phase 20.8.6 T2)", "[gui][soundid]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(900, 700);

    ProfilingSessionSnapshot snap;
    snap.workflowMode = UiWorkflowMode::Guided;
    snap.workflowStage = ProfilingWorkflowStage::ReviewResults;
    snap.evaluation.hasEvaluation = true;
    snap.evaluation.recommendedModelType = "AnalogLutFilterModule";
    snap.evaluation.selectionStatus = synth::SelectionStatus::Accepted;
    snap.evaluation.canonicalEvaluationHash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    snap.evaluation.hashVerified = true;
    snap.exportOptions.canExportCpp = true;

    // Poblar ValidationUiSummary tipado
    snap.validationSummary.status = core::ValidationUiSummary::Status::completed;
    snap.validationSummary.verdict = core::ValidationUiSummary::Verdict::pass;
    snap.validationSummary.policy = "audio-ab-v1";
    snap.validationSummary.reason = "WITHIN_TOLERANCE";
    snap.validationSummary.esrDb = -38.5;
    snap.validationSummary.correlation = 0.9980;
    snap.validationSummary.sampleOffset = 42;
    snap.validationSummary.integrityVerified = true;
    snap.validationSummary.targetAvailable = true;
    snap.validationSummary.modelAvailable = true;
    snap.validationSummary.residualAvailable = true;
    snap.validationSummary.htmlReportAvailable = true;

    resView.updateFromSnapshot(snap);

    CHECK(resView.getValidationStatus() == core::ValidationUiSummary::Status::completed);
    CHECK(resView.getValidationVerdict() == core::ValidationUiSummary::Verdict::pass);
    CHECK(resView.getEsrDb() == Catch::Approx(-38.5));
    CHECK(resView.getCorrelation() == Catch::Approx(0.9980));
    CHECK(resView.getSampleOffset() == 42);
    CHECK(resView.isTargetAudioAvailable() == true);
    CHECK(resView.isModelAudioAvailable() == true);
    CHECK(resView.isResidualAudioAvailable() == true);
    CHECK(resView.isHtmlReportAvailable() == true);
    CHECK(resView.isExportEnabled() == true);

    // Caso de advertencias metrolÃ³gicas
    snap.validationSummary.verdict = core::ValidationUiSummary::Verdict::passWithLimitations;
    snap.validationSummary.reason = "RESIDUAL_ELEVATED";
    snap.validationSummary.esrDb = -22.1;
    resView.updateFromSnapshot(snap);
    CHECK(resView.getValidationVerdict() == core::ValidationUiSummary::Verdict::passWithLimitations);
    CHECK(resView.getEsrDb() == Catch::Approx(-22.1));

    // Caso de fallo / corrupto
    snap.validationSummary.status = core::ValidationUiSummary::Status::corrupt;
    snap.validationSummary.verdict = core::ValidationUiSummary::Verdict::notAvailable;
    snap.validationSummary.integrityVerified = false;
    snap.evaluation.hashVerified = false;
    snap.exportOptions.canExportCpp = false;
    resView.updateFromSnapshot(snap);
    CHECK(resView.getValidationStatus() == core::ValidationUiSummary::Status::corrupt);
    CHECK(resView.getValidationVerdict() == core::ValidationUiSummary::Verdict::notAvailable);
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

    // Fabricar ModelEvaluation canÃ³nico
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

    // Capa 2: La exportaciÃ³n formal en controller es permitida
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

    // La exportaciÃ³n formal sigue permitida en AcceptedWithWarnings
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

    // Simular un fallo de integridad criptogrÃ¡fica (HashMismatch)
    eval.computeCanonicalHash();
    eval.loadStatus = synth::EvaluationLoadStatus::HashMismatch;
    eval.hashVerified = false;

    controller.updateModelEvaluation(eval);
    controller.completeProfiling();

    resView.updateFromSnapshot(controller.getCurrentSnapshot());

    // Capa 1: UI deshabilita exportaciÃ³n
    CHECK(resView.isExportEnabled() == false);
    CHECK(resView.isHashVerified() == false);

    // Capa 2: Controller rechaza la exportaciÃ³n
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

    // El hash canÃ³nico expuesto en la GUI debe ser idÃ©ntico al del objeto y al del snapshot
    REQUIRE(resView.getFullCanonicalHash() == eval.canonicalEvaluationHash);
    REQUIRE(snap.evaluation.canonicalEvaluationHash == eval.canonicalEvaluationHash);
    REQUIRE(resView.getFullCanonicalHash().size() == 64); // SHA-256 hex string
}

TEST_CASE("Step 1 Integration: SoundIdTargetView Snapshot and Feature Flag Contracts", "[soundid][step1][integration]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    // 1. Feature flag contract verification
    REQUIRE(static_cast<int>(TargetViewIntegrationMode::Disabled) == 0);
    REQUIRE(static_cast<int>(TargetViewIntegrationMode::ClassicStep1) == 1);

    ProfilingSessionController controller;
    soundid::SoundIdTargetView targetView(controller);
    targetView.setSize(480, 600);

    // 2. Initial state
    auto initialSnap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(targetView.updateFromSnapshot(initialSnap));

    // 3. VST3 Dexed Target Snapshot (ST-01 contract)
    TargetSelectionState dexedTarget;
    dexedTarget.targetId = "dexed_vst3";
    dexedTarget.targetName = "Dexed FM Synthesizer";
    dexedTarget.manufacturer = "Digital Suburban";
    dexedTarget.version = "1.0.1";
    dexedTarget.kind = TargetKind::PluginVST3;
    dexedTarget.isConnected = true;
    dexedTarget.isDeterministic = true;
    dexedTarget.availableDomainDescription = "6 Operadores FM, Algoritmos 1-32, Pitch Env, LFO";
    dexedTarget.parameterCount = 155;

    controller.selectTarget(dexedTarget);
    controller.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (Digital Host VST3)",
        "Reset de ciclo instantaneo",
        0.0,
        false,
        {},
        "Plugin cargado y validado en bus digital interno");

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(targetView.updateFromSnapshot(snap));
    REQUIRE(snap.target.targetName == "Dexed FM Synthesizer");
    REQUIRE(snap.target.parameterCount == 155);
    REQUIRE(snap.target.isConnected);
    REQUIRE(snap.audit.isAudited);
    REQUIRE(snap.audit.approvalStatus == synth::ApprovalStatus::Approved);

    // 4. Physical Hardware Target Snapshot
    TargetSelectionState hwTarget;
    hwTarget.targetId = "hw_korg_ms20";
    hwTarget.targetName = "Korg MS-20";
    hwTarget.manufacturer = "Korg";
    hwTarget.version = "1.0";
    hwTarget.kind = TargetKind::HardwareAnalogue;
    hwTarget.isConnected = true;
    hwTarget.isDeterministic = false;
    hwTarget.availableDomainDescription = "Audio In/Out Loopback ASIO";
    hwTarget.parameterCount = 12;

    controller.selectTarget(hwTarget);
    controller.updateAuditResult(
        synth::ApprovalStatus::ApprovedWithWarnings,
        "Repetibilidad analogica / audio loopback",
        "Reset de compuerta requerido",
        100.0,
        true,
        { "Latencia y calibracion analogica requerida" },
        "Hardware conectado y validado para ruteo");

    auto hwSnap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(targetView.updateFromSnapshot(hwSnap));
    REQUIRE(hwSnap.target.targetName == "Korg MS-20");
    REQUIRE(hwSnap.target.kind == TargetKind::HardwareAnalogue);
    REQUIRE(hwSnap.audit.approvalStatus == synth::ApprovalStatus::ApprovedWithWarnings);
}


// =============================================================================
// PERF-02: Dirty check — snapshot identico NO actualiza la UI
// PERF-03: Snapshot con cambio real → actualiza exactamente una vez
// =============================================================================

TEST_CASE("PERF-02: SoundIdProfilingRunView dirty check omite snapshot identico",
          "[gui][soundid][perf]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    ProfilingSessionController controller;
    soundid::SoundIdProfilingRunView runView(controller);
    runView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId    = "perf02_target";
    target.kind        = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();
    controller.updateProgress(5, 100, 2.0, 95.0, "Sweep A");
    controller.updateObservation(-18.0, -6.0, 220.0, false, false, 90.0);

    // Primera actualizacion: hasPresentationState_ = false -> siempre pinta
    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
#ifdef ABD_TESTING
    REQUIRE(runView.getTestUpdateExecutedCount() == 1);
    REQUIRE(runView.getTestRepaintCount() == 1);
    REQUIRE(runView.getTestSetTextCount() > 0);
#endif

    SECTION("PERF-02a: snapshot identico repetido omite setText y repaint")
    {
#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
        REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
        REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
#ifdef ABD_TESTING
        // Dirty check: snapshot identico -> ZERO updates ejecutados, ZERO setText, ZERO repaint
        REQUIRE(runView.getTestUpdateExecutedCount() == 0);
        REQUIRE(runView.getTestRepaintCount() == 0);
        REQUIRE(runView.getTestSetTextCount() == 0);
#endif
    }

    SECTION("PERF-02b: RMS dentro del umbral (delta < 0.5 dBFS) no activa actualizacion")
    {
#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        // -18.0 -> -17.7: delta 0.3 dBFS < umbral 0.5 -> debe retornar early
        controller.updateObservation(-17.7, -6.0, 220.0, false, false, 90.0);
        auto snapSubThreshold = controller.getCurrentSnapshot();
        REQUIRE_NOTHROW(runView.updateFromSnapshot(snapSubThreshold));
#ifdef ABD_TESTING
        REQUIRE(runView.getTestUpdateExecutedCount() == 0);
        REQUIRE(runView.getTestRepaintCount() == 0);
        REQUIRE(runView.getTestSetTextCount() == 0);
#endif
    }
}

TEST_CASE("PERF-03: SoundIdProfilingRunView dirty check actualiza en cambio semantico real",
          "[gui][soundid][perf]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    ProfilingSessionController controller;
    soundid::SoundIdProfilingRunView runView(controller);
    runView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId    = "perf03_target";
    target.kind        = TargetKind::SyntheticFixture;
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();

    SECTION("PERF-03a: primera actualizacion siempre pinta (hasPresentationState_ = false)")
    {
#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        controller.updateProgress(1, 50, 0.5, 49.5, "Stimulus Init");
        controller.updateObservation(-24.0, -12.0, 110.0, false, false, 95.0);
        auto snap1 = controller.getCurrentSnapshot();
        REQUIRE_NOTHROW(runView.updateFromSnapshot(snap1));
#ifdef ABD_TESTING
        REQUIRE(runView.getTestUpdateExecutedCount() == 1);
        REQUIRE(runView.getTestRepaintCount() == 1);
        REQUIRE(runView.getTestSetTextCount() > 0);
#endif
    }

    SECTION("PERF-03b: avance de trial index activa actualizacion")
    {
        controller.updateProgress(1, 50, 0.5, 49.5, "Stimulus A");
        controller.updateObservation(-24.0, -12.0, 110.0, false, false, 95.0);
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));

#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        controller.updateProgress(2, 50, 1.0, 49.0, "Stimulus B");
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));
#ifdef ABD_TESTING
        REQUIRE(runView.getTestUpdateExecutedCount() == 1);
        REQUIRE(runView.getTestRepaintCount() == 1);
#endif
    }

    SECTION("PERF-03c: clipping detectado activa actualizacion independientemente del RMS")
    {
        controller.updateProgress(3, 50, 2.0, 48.0, "Sweep Clipping");
        controller.updateObservation(-1.0, 0.5, 440.0, false, false, 60.0);
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));

#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        // Mismo RMS, pero clipping = true -> campo semantico -> DEBE actualizar
        controller.updateObservation(-1.0, 0.5, 440.0, true, false, 60.0);
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));
#ifdef ABD_TESTING
        REQUIRE(runView.getTestUpdateExecutedCount() == 1);
        REQUIRE(runView.getTestRepaintCount() == 1);
#endif
    }

    SECTION("PERF-03d: transicion a Completed activa actualizacion")
    {
        controller.updateProgress(50, 50, 100.0, 0.0, "Final Stimulus");
        controller.updateObservation(-18.0, -6.0, 220.0, false, false, 90.0);
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));

#ifdef ABD_TESTING
        runView.resetTestCounters();
#endif
        controller.completeProfiling();
        // sessionStatus Profiling -> Completed -> DEBE actualizar
        REQUIRE_NOTHROW(runView.updateFromSnapshot(controller.getCurrentSnapshot()));
#ifdef ABD_TESTING
        REQUIRE(runView.getTestUpdateExecutedCount() == 1);
        REQUIRE(runView.getTestRepaintCount() == 1);
#endif
    }
}

// =============================================================================
// PERF-04: SoundIdMeterStrip — Reposo/Silencio omite repaint incondicional
// PERF-05: SoundIdMeterStrip — Audio activo actualiza con dirty check visual
// =============================================================================

TEST_CASE("PERF-04: SoundIdMeterStrip dirty check omite repaint en silencio u omision de cambio",
          "[gui][soundid][perf]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    SoundIdMeterStrip meter;
    meter.setSize(60, 400);

    // En reposo (silencio absoluto 0.0f)
    meter.setLevels(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

#ifdef ABD_TESTING
    meter.resetTestCounters();
    // Primer tick: render inicial en reposo
    meter.testTriggerTimerCallback();
    REQUIRE(meter.getTestTimerTickCount() == 1);
    REQUIRE(meter.getTestRepaintExecutedCount() == 1);
    REQUIRE(meter.getTestRepaintSkippedCount() == 0);

    // Ticks sucesivos en silencio: el dirty check DEBE omitir repaint
    for (int i = 0; i < 30; ++i)
    {
        meter.testTriggerTimerCallback();
    }

    REQUIRE(meter.getTestTimerTickCount() == 31);
    REQUIRE(meter.getTestRepaintExecutedCount() == 1); // Exactamente 1 repintado inicial
    REQUIRE(meter.getTestRepaintSkippedCount() == 30); // 30 repintados omitidos (100% ahorrados a 30 Hz)
#endif
}

TEST_CASE("PERF-05: SoundIdMeterStrip dirty check responde a cambios de audio y estado",
          "[gui][soundid][perf]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    SoundIdMeterStrip meter;
    meter.setSize(60, 400);

#ifdef ABD_TESTING
    // 1. Tick inicial
    meter.testTriggerTimerCallback();
    REQUIRE(meter.getTestRepaintExecutedCount() == 1);

    meter.resetTestCounters();

    // 2. Llegada de señal de audio (ej: señal a -18 dBFS = 0.1259 linear)
    meter.setLevels(0.1259f, 0.1259f, 0.08f, 0.1259f, 0.1259f, 0.08f);
    meter.testTriggerTimerCallback();

    REQUIRE(meter.getTestRepaintExecutedCount() == 1);
    REQUIRE(meter.getTestRepaintSkippedCount() == 0);

    // 3. Cambio de estado de profiling (Profiling Active)
    meter.resetTestCounters();
    meter.setProfilingActive(true);
    meter.testTriggerTimerCallback();
    REQUIRE(meter.getTestRepaintExecutedCount() >= 1);

    // 4. Cambio de estado de pausa
    meter.resetTestCounters();
    meter.setSessionPaused(true);
    meter.testTriggerTimerCallback();
    REQUIRE(meter.getTestRepaintExecutedCount() >= 1);
#endif
}
