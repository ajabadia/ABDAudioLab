#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/session/ProfilingSessionController.h"
#include "gui/session/ProfilingSessionContracts.h"

using namespace abdaudiolab::gui::session;

namespace
{

TargetSelectionState makeValidTarget(std::string id = "synthetic_fixture_mock")
{
    TargetSelectionState t;
    t.targetId = std::move(id);
    t.targetName = "Dexed FM Synth";
    t.manufacturer = "Digital Suburban";
    t.version = "1.0.1";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    t.availableDomainDescription = "MIDI C1-C6, Vel 1-127, 8 Parameters";
    t.parameterCount = 8;
    return t;
}

class TestSessionListener : public IProfilingSessionEventListener
{
public:
    std::vector<ProfilingSessionSnapshot> receivedSnapshots;
    std::vector<UiAlert> receivedAlerts;
    std::vector<ProfilingWorkflowStage> stageTransitions;
    std::vector<ProfilingSessionStatus> statusTransitions;
    uint64_t lastObservedGeneration { 0 };
    uint64_t lastObservedSequence { 0 };
    int ignoredOutdatedSnapshotsCount { 0 };
    ProfilingSessionController* boundController { nullptr };

    ~TestSessionListener() override
    {
        if (boundController != nullptr)
        {
            boundController->removeListener(this);
            boundController = nullptr;
        }
    }

    void attachTo(ProfilingSessionController& c)
    {
        if (boundController != nullptr)
            boundController->removeListener(this);
        boundController = &c;
        c.addListener(this);
    }

    void onSessionSnapshotUpdated(const ProfilingSessionSnapshot& snapshot) override
    {
        if (snapshot.controllerGeneration < lastObservedGeneration)
        {
            ignoredOutdatedSnapshotsCount++;
            return;
        }
        if (snapshot.controllerGeneration == lastObservedGeneration &&
            snapshot.monotonicSequence <= lastObservedSequence)
        {
            ignoredOutdatedSnapshotsCount++;
            return;
        }
        lastObservedGeneration = snapshot.controllerGeneration;
        lastObservedSequence = snapshot.monotonicSequence;
        receivedSnapshots.push_back(snapshot);
    }

    void onAlertRaised(const UiAlert& alert) override
    {
        receivedAlerts.push_back(alert);
    }

    void onWorkflowStageChanged(ProfilingWorkflowStage newStage) override
    {
        stageTransitions.push_back(newStage);
    }

    void onSessionStatusChanged(ProfilingSessionStatus newStatus) override
    {
        statusTransitions.push_back(newStatus);
    }
};

} // namespace

TEST_CASE("ProfilingSessionController: Ciclo de vida completo y transiciones válidas", "[gui][session]")
{
    TestSessionListener listener;
    ProfilingSessionController controller;
    listener.attachTo(controller);

    // 1. Estado inicial
    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Idle);
    CHECK(snap.workflowStage == ProfilingWorkflowStage::TargetSelection);

    // 2. Selección de target
    auto target = makeValidTarget();
    CHECK(controller.selectTarget(target) == true);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(snap.workflowStage == ProfilingWorkflowStage::ConfigureAndStart);
    CHECK(snap.target.targetName == "Dexed FM Synth");

    // 3. Petición de auditoría
    CHECK(controller.requestAudit() == true);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Auditing);

    // 4. Conclusión de auditoría con éxito
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "Target approved");
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::ReadyToProfile);
    CHECK(snap.audit.isAudited == true);

    // 5. Iniciar perfilado
    CHECK(controller.startProfiling() == true);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Profiling);
    CHECK(snap.workflowStage == ProfilingWorkflowStage::ProfilingActive);

    // 6. Actualizaciones de telemetría sin romper estado
    controller.updateProgress(5, 20, 10.5, 30.0, "NoteOn C3 Vel 100");
    controller.updateObservation(-18.5, -6.0, 440.0, false, false, 75.0);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.progress.currentTrial == 5);
    CHECK(snap.progress.totalTrials == 20);
    CHECK(snap.progress.progressPercent == Catch::Approx(25.0));

    // 7. Completar perfilado y evaluar modelo
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::Accepted,
                                     "Analytic Grey-Box", -42.5, 0.9995, 98.0,
                                     "C1-C6, vel 30-127", 1.1, {}, {});
    controller.completeProfiling();
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(snap.workflowStage == ProfilingWorkflowStage::ReviewResults);
    CHECK(snap.exportOptions.canExportCpp == true);

    // 8. Exportar modelo primera vez
    CHECK(controller.exportModel("cpp", "build/export/Model.cpp") == true);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Exported);
    CHECK(snap.exportOptions.lastExportedFilePath == "build/export/Model.cpp");

    // 9. Re-exportar modelo inmediatamente (estado Exported permite re-exportacion)
    CHECK(controller.exportModel("cpp", "build/export/Model_1.cpp") == true);
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Exported);
    CHECK(snap.exportOptions.lastExportedFilePath == "build/export/Model_1.cpp");

    controller.removeListener(&listener);
    if (auto* coord = controller.getCoordinator())
    {
        coord->requestCancel();
        coord->waitForWorkerToStop(1000);
    }
}

TEST_CASE("ProfilingSessionController: Protección contra snapshots antiguos desordenados", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    controller.selectTarget(makeValidTarget());
    controller.requestAudit();

    // Verificamos que los snapshots avanzan estrictamente en secuencia monotónica
    REQUIRE(listener.receivedSnapshots.size() >= 2);
    for (size_t i = 1; i < listener.receivedSnapshots.size(); ++i)
    {
        CHECK(listener.receivedSnapshots[i].monotonicSequence > listener.receivedSnapshots[i - 1].monotonicSequence);
    }
    CHECK(listener.ignoredOutdatedSnapshotsCount == 0);

    // Simulamos llegada de un snapshot obsoleto
    ProfilingSessionSnapshot staleSnapshot = listener.receivedSnapshots.front();
    listener.onSessionSnapshotUpdated(staleSnapshot);
    CHECK(listener.ignoredOutdatedSnapshotsCount == 1);
}

TEST_CASE("ProfilingSessionController: Target incompatible o no auditado bloquea startProfiling", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    // 1. Intentar iniciar sin target ni auditoría
    CHECK_FALSE(controller.startProfiling());
    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Idle);
    REQUIRE_FALSE(listener.receivedAlerts.empty());
    CHECK(listener.receivedAlerts.back().severity == UiAlert::Severity::Error);

    // 2. Seleccionar target pero sin auditar (un plugin externo requiere auditoria previa obligatoria)
    auto unauditedTarget = makeValidTarget();
    unauditedTarget.kind = TargetKind::PluginVST3;
    controller.selectTarget(unauditedTarget);
    CHECK_FALSE(controller.startProfiling());

    // 3. Auditar pero con resultado rechazado
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Rejected,
                                 "NonDeterministic", "NotResettable", 0.0, false,
                                 { "High drift detected" }, "Do not profile");
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::AuditRejected);

    // 4. Intentar iniciar con target rechazado
    CHECK_FALSE(controller.startProfiling());
    snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::AuditRejected);
}

TEST_CASE("ProfilingSessionController: Cancelación de sesión durante auditoría y perfilado", "[gui][session]")
{
    ProfilingSessionController controller;

    // A. Cancelar durante auditoría
    controller.selectTarget(makeValidTarget());
    controller.requestAudit();
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Auditing);

    CHECK(controller.cancelProfiling() == true);
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Cancelled);

    // B. Cancelar durante perfilado
    controller.selectTarget(makeValidTarget());
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Profiling);

    CHECK(controller.cancelProfiling() == true);
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Cancelled);
}

TEST_CASE("ProfilingSessionController: Cambio de target invalida resultados anteriores", "[gui][session]")
{
    ProfilingSessionController controller;

    // Completar una sesión con target A
    controller.selectTarget(makeValidTarget("synth_A"));
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::Accepted,
                                     "Model_A", -40.0, 0.999, 95.0, "C1-C5", 1.0, {}, {});
    controller.completeProfiling();

    CHECK(controller.getCurrentSnapshot().evaluation.hasEvaluation == true);
    CHECK(controller.getCurrentSnapshot().audit.isAudited == true);

    // Cambiar a target B
    controller.selectTarget(makeValidTarget("synth_B"));
    auto snap = controller.getCurrentSnapshot();

    CHECK(snap.target.targetId == "synth_B");
    CHECK(snap.audit.isAudited == false);
    CHECK(snap.evaluation.hasEvaluation == false);
    CHECK(snap.exportOptions.canExportCpp == false);
    CHECK(snap.sessionStatus == ProfilingSessionStatus::TargetSelected);
}

TEST_CASE("ProfilingSessionController: Bloqueo de exportación si la medición es inválida", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    controller.selectTarget(makeValidTarget());
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();

    // Modelo marcado como medición inválida (clipping severo)
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::InvalidMeasurement,
                                     "Corrupted", 0.0, 0.0, 0.0, "None", 1.0,
                                     { "Clipping occurred" }, { "Host overload" });
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(snap.exportOptions.canExportCpp == false);

    // Intentar exportar debe fallar con alerta explicativa
    CHECK_FALSE(controller.exportModel("cpp", "invalid_output.cpp"));
    REQUIRE_FALSE(listener.receivedAlerts.empty());
    CHECK(listener.receivedAlerts.back().cause.find("descartadas") != std::string::npos);
}

TEST_CASE("ProfilingSessionController: Alertas UX estructuradas con los cuatro campos obligatorios", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    controller.raiseAlert(UiAlert::Severity::Warning,
                          "Advertencia de Calibración",
                          "El target requiere un tiempo de reposo mayor a 100 ms.",
                          "Las envolventes largas pueden causar solapamiento entre notas consecutivas.",
                          "Aumente el parámetro de settling en opciones avanzadas.",
                          "Posible medición de colas residuales.");

    REQUIRE_FALSE(listener.receivedAlerts.empty());
    const auto& alert = listener.receivedAlerts.back();

    CHECK_FALSE(alert.title.empty());
    CHECK_FALSE(alert.cause.empty());
    CHECK_FALSE(alert.impact.empty());
    CHECK_FALSE(alert.recommendedAction.empty());
    CHECK_FALSE(alert.consequenceIfIgnored.empty());
    CHECK(alert.timestampMs > 0);
}

TEST_CASE("ProfilingSessionController: ClassicGuidedClassicPreservesSession", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    // 1. Configurar target y estado de sesión en modo clásico
    auto target = makeValidTarget("synth_moog_d");
    CHECK(controller.selectTarget(target));
    CHECK(controller.requestAudit());
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "Target approved");

    auto snapInitial = controller.getCurrentSnapshot();
    CHECK(snapInitial.workflowMode == UiWorkflowMode::Classic);
    CHECK(snapInitial.sessionStatus == ProfilingSessionStatus::ReadyToProfile);
    CHECK(snapInitial.target.targetId == "synth_moog_d");
    std::string originalSessionId = snapInitial.sessionId;
    uint64_t originalGeneration = snapInitial.controllerGeneration;

    // 2. Conmutar Classic -> Guided
    controller.setWorkflowMode(UiWorkflowMode::Guided);
    auto snapGuided = controller.getCurrentSnapshot();
    CHECK(snapGuided.workflowMode == UiWorkflowMode::Guided);
    CHECK(snapGuided.target.targetId == "synth_moog_d");
    CHECK(snapGuided.sessionId == originalSessionId);
    CHECK(snapGuided.controllerGeneration == originalGeneration);
    CHECK(snapGuided.sessionStatus == ProfilingSessionStatus::ReadyToProfile);

    // 3. Conmutar Guided -> Classic
    controller.setWorkflowMode(UiWorkflowMode::Classic);
    auto snapRestored = controller.getCurrentSnapshot();
    CHECK(snapRestored.workflowMode == UiWorkflowMode::Classic);
    CHECK(snapRestored.target.targetId == "synth_moog_d");
    CHECK(snapRestored.sessionId == originalSessionId);
    CHECK(snapRestored.controllerGeneration == originalGeneration);
    CHECK(snapRestored.sessionStatus == ProfilingSessionStatus::ReadyToProfile);
}

TEST_CASE("ProfilingSessionController: OldGenerationSnapshotIsIgnored", "[gui][session]")
{
    TestSessionListener listener;
    listener.lastObservedGeneration = 3;
    listener.lastObservedSequence = 10;

    // Snapshot con generación anterior (2 < 3) pero secuencia alta (999) DEBE descartarse
    ProfilingSessionSnapshot staleGenSnap;
    staleGenSnap.controllerGeneration = 2;
    staleGenSnap.monotonicSequence = 999;

    listener.onSessionSnapshotUpdated(staleGenSnap);
    CHECK(listener.ignoredOutdatedSnapshotsCount == 1);
    CHECK(listener.receivedSnapshots.empty());
}

TEST_CASE("ProfilingSessionController: OldSequenceSnapshotIsIgnored", "[gui][session]")
{
    TestSessionListener listener;
    listener.lastObservedGeneration = 3;
    listener.lastObservedSequence = 50;

    // Snapshot con misma generación pero secuencia anterior o igual (49 <= 50)
    ProfilingSessionSnapshot staleSeqSnap;
    staleSeqSnap.controllerGeneration = 3;
    staleSeqSnap.monotonicSequence = 49;

    listener.onSessionSnapshotUpdated(staleSeqSnap);
    CHECK(listener.ignoredOutdatedSnapshotsCount == 1);
    CHECK(listener.receivedSnapshots.empty());

    // Snapshot idéntica secuencia (50 == 50)
    staleSeqSnap.monotonicSequence = 50;
    listener.onSessionSnapshotUpdated(staleSeqSnap);
    CHECK(listener.ignoredOutdatedSnapshotsCount == 2);
    CHECK(listener.receivedSnapshots.empty());

    // Snapshot con secuencia mayor (51 > 50) es aceptada
    staleSeqSnap.monotonicSequence = 51;
    listener.onSessionSnapshotUpdated(staleSeqSnap);
    CHECK(listener.ignoredOutdatedSnapshotsCount == 2);
    REQUIRE(listener.receivedSnapshots.size() == 1);
    CHECK(listener.lastObservedSequence == 51);
}

TEST_CASE("ProfilingSessionController: ChangingTargetInvalidatesEvaluation", "[gui][session]")
{
    ProfilingSessionController controller;

    // Sesión con Target A completada
    controller.selectTarget(makeValidTarget("target_alpha"));
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved, "Det", "Reset", 50.0, true, {}, "OK");
    controller.startProfiling();
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::Accepted, "GreyBox", -40.0, 0.999, 95.0, "C1-C5", 1.0, {}, {});
    controller.completeProfiling();

    auto snapA = controller.getCurrentSnapshot();
    CHECK(snapA.evaluation.hasEvaluation == true);
    CHECK(snapA.exportOptions.canExportCpp == true);
    uint64_t genA = snapA.controllerGeneration;
    std::string idA = snapA.sessionId;

    // Cambio atómico a Target B
    controller.selectTarget(makeValidTarget("target_beta"));
    auto snapB = controller.getCurrentSnapshot();

    CHECK(snapB.target.targetId == "target_beta");
    CHECK(snapB.controllerGeneration > genA);
    CHECK(snapB.sessionId != idA);
    CHECK_FALSE(snapB.audit.isAudited);
    CHECK_FALSE(snapB.evaluation.hasEvaluation);
    CHECK_FALSE(snapB.exportOptions.canExportCpp);
    CHECK(snapB.sessionStatus == ProfilingSessionStatus::TargetSelected);
}

TEST_CASE("ProfilingSessionController: DestroyedViewReceivesNoCallbacks", "[gui][session]")
{
    ProfilingSessionController controller;
    auto listener = std::make_unique<TestSessionListener>();
    controller.addListener(listener.get());

    controller.selectTarget(makeValidTarget());
    size_t countBefore = listener->receivedSnapshots.size();
    REQUIRE(countBefore > 0);

    // Simular destrucción / desregistro de la vista
    controller.removeListener(listener.get());
    listener.reset(); // Objeto destruido

    // Emisión posterior de eventos no debe crashear ni acceder a puntero colgante
    REQUIRE_NOTHROW(controller.requestAudit());
    REQUIRE_NOTHROW(controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                                "Det", "Reset", 50.0, true, {}, "OK"));
}

TEST_CASE("ProfilingSessionController: RejectedTargetCannotStart", "[gui][session]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeValidTarget());
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Rejected,
                                 "NonDeterministic", "None", 0.0, false, {"Unstable"}, "Rejected");

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::AuditRejected);

    // Intento de inicio bloqueado
    CHECK_FALSE(controller.startProfiling());
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::AuditRejected);
}

TEST_CASE("ProfilingSessionController: InvalidMeasurementCannotExport", "[gui][session]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeValidTarget());
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved, "Det", "Reset", 50.0, true, {}, "OK");
    controller.startProfiling();

    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::InvalidMeasurement,
                                     "None", 0.0, 0.0, 0.0, "None", 1.0, {"Clipping"}, {"Noise"});
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    CHECK_FALSE(snap.exportOptions.canExportCpp);
    CHECK_FALSE(controller.exportModel("cpp", "out.cpp"));
}

TEST_CASE("ProfilingSessionController: UserOverridesAndCognitiveMetrics", "[gui][session]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    controller.selectTarget(makeValidTarget());
    auto initialSnap = controller.getCurrentSnapshot();
    CHECK(initialSnap.clickCount == 0);
    CHECK_FALSE(initialSnap.openedAdvancedMode);
    CHECK_FALSE(initialSnap.warningsAcknowledged);
    CHECK_FALSE(initialSnap.userOverrides);

    // 1. Clics de usuario
    controller.recordUserClick();
    controller.recordUserClick();
    CHECK(controller.getCurrentSnapshot().clickCount == 2);

    // 2. Modo avanzado
    controller.setOpenedAdvancedMode(true);
    CHECK(controller.getCurrentSnapshot().openedAdvancedMode == true);

    // 3. Reconocimiento de advertencias
    controller.acknowledgeWarnings();
    CHECK(controller.getCurrentSnapshot().warningsAcknowledged == true);

    // 4. Overrides de usuario
    controller.recordUserOverride();
    auto snapWithOverride = controller.getCurrentSnapshot();
    CHECK(snapWithOverride.userOverrides == true);
    CHECK(snapWithOverride.monotonicSequence > initialSnap.monotonicSequence);
}

TEST_CASE("ProfilingSessionController: ControllerDestroyedWithPendingCallbacksSafelyIgnored", "[gui][session]")
{
    auto controller = std::make_unique<ProfilingSessionController>();
    TestSessionListener listener;
    controller->addListener(&listener);

    controller->selectTarget(makeValidTarget());
    controller->requestAudit();

    // Destruir el controlador antes de que se despachen callbacks pendientes en el MessageManager
    controller.reset();

    // El token de ciclo de vida (aliveToken) evita accesos a memoria destruida
    SUCCEED("No crash when controller is destroyed with pending callbacks");
}

TEST_CASE("ProfilingSessionController: Invariantes y proteccion al importar evaluacion desde archivo", "[gui][session][import]")
{
    ProfilingSessionController controller;
    TestSessionListener listener;
    controller.addListener(&listener);

    controller.selectTarget(makeValidTarget());
    auto baselineSnap = controller.getCurrentSnapshot();
    std::string originalSessionId = baselineSnap.sessionId;
    uint64_t originalGeneration = baselineSnap.controllerGeneration;
    std::string originalTargetId = baselineSnap.target.targetId;

    juce::File fixturesDir = juce::File::getCurrentWorkingDirectory().getChildFile("fixtures").getChildFile("evaluations");

    SECTION("1. Importar evaluacion valida no cambia targetId, sessionId ni controllerGeneration")
    {
        juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
        REQUIRE(approvedFile.existsAsFile());

        bool ok = controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString());
        CHECK(ok);

        auto snap = controller.getCurrentSnapshot();
        // Invariantes críticas:
        CHECK(snap.target.targetId == originalTargetId);
        CHECK(snap.sessionId == originalSessionId);
        CHECK(snap.controllerGeneration == originalGeneration);
        CHECK(snap.sessionStatus == ProfilingSessionStatus::EvaluationLoadedForReview);
        CHECK(snap.workflowStage == ProfilingWorkflowStage::ReviewResults);
        CHECK(snap.evaluation.evaluationOrigin == abdaudiolab::synth::EvaluationOrigin::ImportedArtifact);
        CHECK(snap.evaluation.hashVerified == true);
        CHECK(snap.exportOptions.canExportCpp == true);
        CHECK_FALSE(snap.evaluation.sourceFileHash.empty());

        // Exportación segura atómica
        std::string exportDest = "build/export/TestPackage.cpp";
        CHECK(controller.exportModel("cpp", exportDest) == true);
        juce::File exported(exportDest);
        CHECK(exported.existsAsFile());
        std::string exportedText = exported.loadFileAsString().toStdString();
        CHECK(exportedText.find(snap.evaluation.canonicalEvaluationHash) != std::string::npos);
    }

    SECTION("2. Archivo inexistente -> alerta estructurada y return false")
    {
        bool ok = controller.loadEvaluationFromFile("non_existent_path_12345.json");
        CHECK_FALSE(ok);
        REQUIRE_FALSE(listener.receivedAlerts.empty());
        CHECK(listener.receivedAlerts.back().title == "Archivo no encontrado");
    }

    SECTION("3. JSON sintacticamente invalido -> alerta estructurada y return false")
    {
        bool ok = controller.loadEvaluationFromJsonString("{ this is not valid json }");
        CHECK_FALSE(ok);
        REQUIRE_FALSE(listener.receivedAlerts.empty());
        CHECK(listener.receivedAlerts.back().title == "Error de sintaxis JSON");
    }

    SECTION("4. Protocolo incompatible -> alerta estructurada y return false")
    {
        std::string protoMismatch = R"({
            "evaluationId": "eval_1",
            "protocolVersion": "9.9.9",
            "canonicalEvaluationHash": "hash",
            "model": {}, "metrics": {}, "decision": {}
        })";
        bool ok = controller.loadEvaluationFromJsonString(protoMismatch);
        CHECK_FALSE(ok);
        REQUIRE_FALSE(listener.receivedAlerts.empty());
        CHECK(listener.receivedAlerts.back().title == "Protocolo no compatible");
    }

    SECTION("5. Hash ausente / SchemaMismatch -> alerta estructurada y return false")
    {
        std::string brokenSchema = R"({
            "evaluationId": "eval_1",
            "protocolVersion": "1.0.0"
        })";
        bool ok = controller.loadEvaluationFromJsonString(brokenSchema);
        CHECK_FALSE(ok);
        REQUIRE_FALSE(listener.receivedAlerts.empty());
        CHECK(listener.receivedAlerts.back().title == "Esquema JSON incompatible");
    }

    SECTION("6. Hash manipulado -> HashMismatch, hashVerified = false, canExport = false")
    {
        juce::File tamperedFile = fixturesDir.getChildFile("tampered_hash_mismatch.json");
        REQUIRE(tamperedFile.existsAsFile());

        bool ok = controller.loadEvaluationFromFile(tamperedFile.getFullPathName().toStdString());
        CHECK_FALSE(ok); // HashMismatch returns false

        auto snap = controller.getCurrentSnapshot();
        CHECK(snap.evaluation.hasEvaluation == true);
        CHECK(snap.evaluation.hashVerified == false);
        CHECK(snap.evaluation.evaluationLoadStatus == abdaudiolab::synth::EvaluationLoadStatus::HashMismatch);
        CHECK(snap.exportOptions.canExportCpp == false);
        CHECK(snap.sessionStatus == ProfilingSessionStatus::EvaluationLoadedForReview);

        // Capa 2: exportModel debe fallar en el controlador
        CHECK(controller.exportModel("cpp", "build/export/ShouldNotExport.cpp") == false);
    }

    SECTION("7. Inconclusive y Rejected bloquean exportModel")
    {
        juce::File incFile = fixturesDir.getChildFile("inconclusive.json");
        REQUIRE(incFile.existsAsFile());
        controller.loadEvaluationFromFile(incFile.getFullPathName().toStdString());
        CHECK(controller.getCurrentSnapshot().exportOptions.canExportCpp == false);
        CHECK(controller.exportModel("cpp", "build/export/ShouldNotExport.cpp") == false);

        juce::File rejFile = fixturesDir.getChildFile("rejected.json");
        REQUIRE(rejFile.existsAsFile());
        controller.loadEvaluationFromFile(rejFile.getFullPathName().toStdString());
        CHECK(controller.getCurrentSnapshot().exportOptions.canExportCpp == false);
        CHECK(controller.exportModel("cpp", "build/export/ShouldNotExport.cpp") == false);
    }

    SECTION("8. Importar segunda evaluacion reemplaza solo la evaluacion anterior")
    {
        juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
        juce::File dexedFile = fixturesDir.getChildFile("dexed_warnings.json");
        REQUIRE(approvedFile.existsAsFile());
        REQUIRE(dexedFile.existsAsFile());

        controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString());
        auto snap1 = controller.getCurrentSnapshot();
        CHECK(snap1.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::Accepted);

        controller.loadEvaluationFromFile(dexedFile.getFullPathName().toStdString());
        auto snap2 = controller.getCurrentSnapshot();
        CHECK(snap2.evaluation.selectionStatus == abdaudiolab::synth::SelectionStatus::AcceptedWithWarnings);
        CHECK(snap2.sessionId == originalSessionId);
        CHECK(snap2.controllerGeneration == originalGeneration);
        CHECK(snap2.target.targetId == originalTargetId);
    }

    SECTION("9. Cambiar de target invalida la evaluacion importada")
    {
        juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
        controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString());
        CHECK(controller.getCurrentSnapshot().evaluation.hasEvaluation == true);

        TargetSelectionState newTarget;
        newTarget.targetId = "different_synth_id";
        newTarget.targetName = "New Synth Target";
        controller.selectTarget(newTarget);

        auto snap = controller.getCurrentSnapshot();
        CHECK(snap.evaluation.hasEvaluation == false);
        CHECK(snap.controllerGeneration > originalGeneration);
        CHECK(snap.sessionId != originalSessionId);
    }

    SECTION("10. saveExperimentRecord y loadExperimentRecord de extremo a extremo")
    {
        juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
        controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString());
        REQUIRE(controller.getCurrentSnapshot().evaluation.hasEvaluation == true);

        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("ABDAudioLab_CtrlExp_" + juce::String(juce::Random::getSystemRandom().nextInt()));
        tempDir.createDirectory();

        std::string expFolder;
        std::string expErr;
        bool ok = controller.saveExperimentRecord(tempDir.getFullPathName().toStdString(), expFolder, expErr);
        REQUIRE(ok);
        REQUIRE(!expFolder.empty());

        // Ahora creamos un segundo controller limpio y cargamos el experimento
        ProfilingSessionController controller2;
        std::string loadErr;
        bool loadOk = controller2.loadExperimentRecord(expFolder, loadErr);
        REQUIRE(loadOk);
        auto snap2 = controller2.getCurrentSnapshot();
        CHECK(snap2.evaluation.hasEvaluation == true);
        CHECK(snap2.evaluation.hashVerified == true);
        CHECK(snap2.evaluation.canonicalEvaluationHash == controller.getCurrentSnapshot().evaluation.canonicalEvaluationHash);
        CHECK(snap2.exportOptions.canExportCpp == true);

        tempDir.deleteRecursively();
    }
}





