#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/session/ProfilingSessionCoordinator.h"
#include "gui/session/ProfilingSessionController.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "synth/ModelEvaluationBuilder.h"
#include <chrono>
#include <thread>

using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::synth;

namespace
{

TargetSelectionState makeSyntheticTestTarget()
{
    TargetSelectionState t;
    t.targetId = "synthetic_test_target";
    t.targetName = "SyntheticFixture_DemoMode";
    t.manufacturer = "ABDAudioLab";
    t.version = "1.0.0";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    t.availableDomainDescription = "MIDI C1-C6, Vel 1-127";
    t.parameterCount = 4;
    return t;
}

class TestCoordinatorListener : public ICoordinatorListener
{
public:
    std::mutex mtx;
    std::vector<CoordinatorSnapshot> snapshots;
    int completedCount { 0 };
    int cancelledCount { 0 };
    int failedCount { 0 };
    std::optional<ModelEvaluation> lastCandidate;
    std::string lastErrorMessage;
    uint64_t lastCompletedRunId { 0 };

    void onCoordinatorSnapshotUpdated(const CoordinatorSnapshot& snap) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        snapshots.push_back(snap);
    }

    void onCoordinatorCompleted(uint64_t runId, uint64_t, const ModelEvaluation& candidate) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        completedCount++;
        lastCompletedRunId = runId;
        lastCandidate = candidate;
    }

    void onCoordinatorCancelled(uint64_t, uint64_t) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        cancelledCount++;
    }

    void onCoordinatorFailed(uint64_t, uint64_t, const std::string& error) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        failedCount++;
        lastErrorMessage = error;
    }
};

class TestSessionEventListener : public IProfilingSessionEventListener
{
public:
    std::mutex mtx;
    std::vector<ProfilingSessionSnapshot> snapshots;
    std::vector<UiAlert> alerts;

    void onSessionSnapshotUpdated(const ProfilingSessionSnapshot& snap) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        snapshots.push_back(snap);
    }

    void onAlertRaised(const UiAlert& alert) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        alerts.push_back(alert);
    }

    void onWorkflowStageChanged(ProfilingWorkflowStage) override {}
    void onSessionStatusChanged(ProfilingSessionStatus) override {}
};

} // namespace

TEST_CASE("Coordinator: Transicion Preparing -> Running -> Completed", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);

    TargetSelectionState target = makeSyntheticTestTarget();
    bool started = coordinator.start(target, 1, 5, 10);
    REQUIRE(started);

    coordinator.waitForWorkerToStop(3000);

    CHECK_FALSE(coordinator.isThreadRunning());
    CHECK(coordinator.getState() == CoordinatorState::Completed);

    std::lock_guard<std::mutex> lock(listener.mtx);
    CHECK(listener.completedCount == 1);
    CHECK(listener.cancelledCount == 0);
    CHECK(listener.failedCount == 0);
    REQUIRE(listener.lastCandidate.has_value());
    CHECK(listener.lastCandidate->origin == EvaluationOrigin::SyntheticDemo);
    CHECK(listener.lastCandidate->hashVerified == true);
    CHECK_FALSE(listener.lastCandidate->canonicalEvaluationHash.empty());

    // Verificar que se emitieron snapshots de Preparing y Running
    bool sawPreparing = false;
    bool sawRunning = false;
    bool sawCompleted = false;

    for (const auto& s : listener.snapshots)
    {
        if (s.state == CoordinatorState::Preparing) sawPreparing = true;
        if (s.state == CoordinatorState::Running) sawRunning = true;
        if (s.state == CoordinatorState::Completed && s.terminal) sawCompleted = true;
    }

    CHECK(sawPreparing);
    CHECK(sawRunning);
    CHECK(sawCompleted);
}

TEST_CASE("Coordinator: runId distinto y estrictamente creciente en ejecuciones consecutivas", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 3, 5));
    coordinator.waitForWorkerToStop(3000);
    uint64_t runId1 = coordinator.getCurrentRunId();

    REQUIRE(coordinator.start(target, 1, 3, 5));
    coordinator.waitForWorkerToStop(3000);
    uint64_t runId2 = coordinator.getCurrentRunId();

    REQUIRE(coordinator.start(target, 1, 3, 5));
    coordinator.waitForWorkerToStop(3000);
    uint64_t runId3 = coordinator.getCurrentRunId();

    CHECK(runId1 > 0);
    CHECK(runId2 > runId1);
    CHECK(runId3 > runId2);
}

TEST_CASE("Coordinator: Progreso no retrocede dentro del mismo runId (monotonicidad estricta)", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 6, 8));
    coordinator.waitForWorkerToStop(3000);

    std::lock_guard<std::mutex> lock(listener.mtx);
    REQUIRE(listener.snapshots.size() >= 6);

    double prevProgress = 0.0;
    uint64_t targetRunId = coordinator.getCurrentRunId();

    for (const auto& s : listener.snapshots)
    {
        if (s.runId == targetRunId)
        {
            CHECK(s.progress >= prevProgress);
            CHECK(s.progress >= 0.0);
            CHECK(s.progress <= 100.0);
            prevProgress = s.progress;
        }
    }
    CHECK(prevProgress == 100.0);
}

TEST_CASE("Coordinator: Pausa congela el progreso y no avanza ensayos mientras esta pausado", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    // 10 ensayos de 40ms cada uno
    REQUIRE(coordinator.start(target, 1, 10, 40));

    // Esperar a que alcance al menos ensayo 2
    for (int i = 0; i < 50; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (coordinator.getSnapshot().currentTrial >= 2)
            break;
    }

    coordinator.pause();
    auto pausedSnap = coordinator.getSnapshot();
    CHECK(pausedSnap.state == CoordinatorState::Paused);
    int frozenTrial = pausedSnap.currentTrial;
    double frozenProgress = pausedSnap.progress;

    // Dejar pasar tiempo: no debe avanzar
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    auto snapAfterWait = coordinator.getSnapshot();
    CHECK(snapAfterWait.currentTrial == frozenTrial);
    CHECK(snapAfterWait.progress == frozenProgress);

    // Cancelar para limpiar
    coordinator.requestCancel();
    coordinator.waitForWorkerToStop(3000);
}

TEST_CASE("Coordinator: Reanudacion continua sin duplicar ni omitir ensayos", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 6, 30));

    // Esperar al ensayo 2
    for (int i = 0; i < 50; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (coordinator.getSnapshot().currentTrial >= 2)
            break;
    }

    coordinator.pause();
    CHECK(coordinator.getSnapshot().state == CoordinatorState::Paused);

    coordinator.resume();
    CHECK(coordinator.getState() == CoordinatorState::Running);

    coordinator.waitForWorkerToStop(3000);
    CHECK(coordinator.getState() == CoordinatorState::Completed);

    std::lock_guard<std::mutex> lock(listener.mtx);
    CHECK(listener.completedCount == 1);

    // Verificar que los ensayos observados abarcan hasta 6
    int maxTrial = 0;
    for (const auto& s : listener.snapshots)
    {
        if (s.currentTrial > maxTrial) maxTrial = s.currentTrial;
    }
    CHECK(maxTrial == 6);
}

TEST_CASE("Coordinator: Cancelacion durante Preparing termina limpiamente en Cancelled", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 10, 50));
    // Cancelación inmediata durante Preparing
    coordinator.requestCancel();

    coordinator.waitForWorkerToStop(3000);
    CHECK(coordinator.getState() == CoordinatorState::Cancelled);

    std::lock_guard<std::mutex> lock(listener.mtx);
    CHECK(listener.cancelledCount == 1);
    CHECK(listener.completedCount == 0);
    CHECK_FALSE(listener.lastCandidate.has_value());
}

TEST_CASE("Coordinator: Cancelacion durante Running termina limpiamente en Cancelled", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 10, 30));

    // Esperar a entrar en Running
    for (int i = 0; i < 50; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (coordinator.getSnapshot().currentTrial >= 2)
            break;
    }

    coordinator.requestCancel();
    coordinator.waitForWorkerToStop(3000);

    CHECK(coordinator.getState() == CoordinatorState::Cancelled);

    std::lock_guard<std::mutex> lock(listener.mtx);
    CHECK(listener.cancelledCount == 1);
    CHECK(listener.completedCount == 0);
}

TEST_CASE("Coordinator: Cancelacion idempotente (multiples requestCancel disparan un solo evento)", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 10, 30));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Llamadas concurrentes/repetidas
    coordinator.requestCancel();
    coordinator.requestCancel();
    coordinator.requestCancel();

    coordinator.waitForWorkerToStop(3000);

    CHECK(coordinator.getState() == CoordinatorState::Cancelled);

    std::lock_guard<std::mutex> lock(listener.mtx);
    CHECK(listener.cancelledCount == 1);
}

TEST_CASE("Coordinator: Despertar cooperativo inmediato del hilo al cancelar mientras esta pausado", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    ProfilingSessionCoordinator coordinator(&listener);
    TargetSelectionState target = makeSyntheticTestTarget();

    REQUIRE(coordinator.start(target, 1, 10, 100));
    for (int i = 0; i < 50; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (coordinator.getState() == CoordinatorState::Running)
            break;
    }

    coordinator.pause();
    CHECK(coordinator.getSnapshot().state == CoordinatorState::Paused);

    auto tStart = std::chrono::steady_clock::now();
    coordinator.requestCancel();
    coordinator.waitForWorkerToStop(3000);
    auto tElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();

    // Debe despertar inmediatamente y no esperar timeouts largos (< 500 ms)
    CHECK(tElapsedMs < 500);
    CHECK(coordinator.getState() == CoordinatorState::Cancelled);
}

TEST_CASE("Coordinator: Destructor seguro espera al worker sin fugas ni dangling pointers", "[coordinator][session]")
{
    TestCoordinatorListener listener;
    {
        ProfilingSessionCoordinator coordinator(&listener);
        TargetSelectionState target = makeSyntheticTestTarget();
        REQUIRE(coordinator.start(target, 1, 20, 50));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // Destructor se ejecuta aquí con el hilo en pleno vuelo
    }
    // Si llegamos aquí sin assert ni crash, la parada segura funcionó
    SUCCEED("ProfilingSessionCoordinator destroyed safely while worker was running");
}

TEST_CASE("Controller + Coordinator: Transaccionalidad (fallo restaura evaluacion previa)", "[gui][session][transaction]")
{
    ProfilingSessionController controller;
    TestSessionEventListener listener;
    controller.addListener(&listener);

    controller.selectTarget(makeSyntheticTestTarget());

    // 1. Cargar una evaluación previa aprobada (compromiso inicial)
    juce::File fixturesDir = juce::File::getCurrentWorkingDirectory().getChildFile("fixtures").getChildFile("evaluations");
    juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
    REQUIRE(approvedFile.existsAsFile());
    REQUIRE(controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString()));

    auto baselineSnap = controller.getCurrentSnapshot();
    REQUIRE(baselineSnap.evaluation.hasEvaluation == true);
    std::string baselineHash = baselineSnap.evaluation.canonicalEvaluationHash;
    REQUIRE_FALSE(baselineHash.empty());

    // 2. Iniciar perfilado inyectando fallo de validación
    REQUIRE(controller.getCoordinator() != nullptr);
    controller.getCoordinator()->setSimulateValidationFailure(true);
    REQUIRE(controller.startProfiling());

    // Esperar a que el worker termine
    controller.getCoordinator()->waitForWorkerToStop(3000);

    auto snapAfterFailure = controller.getCurrentSnapshot();
    CHECK(snapAfterFailure.sessionStatus == ProfilingSessionStatus::Failed);

    // INVARIANTE TRANSACCIONAL: La evaluación previa debe haber sido restaurada
    CHECK(snapAfterFailure.evaluation.hasEvaluation == true);
    CHECK(snapAfterFailure.evaluation.canonicalEvaluationHash == baselineHash);

    // Debe existir alerta de error registrada
    CHECK_FALSE(snapAfterFailure.activeAlerts.empty());
}

TEST_CASE("Controller + Coordinator: Cancelacion preserva evaluacion previa sin sobrescribirla", "[gui][session][transaction]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeSyntheticTestTarget());

    juce::File fixturesDir = juce::File::getCurrentWorkingDirectory().getChildFile("fixtures").getChildFile("evaluations");
    juce::File approvedFile = fixturesDir.getChildFile("fixture_approved.json");
    REQUIRE(approvedFile.existsAsFile());
    REQUIRE(controller.loadEvaluationFromFile(approvedFile.getFullPathName().toStdString()));

    auto baselineSnap = controller.getCurrentSnapshot();
    std::string baselineHash = baselineSnap.evaluation.canonicalEvaluationHash;

    // Iniciar y cancelar
    REQUIRE(controller.startProfiling());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    controller.cancelProfiling();

    REQUIRE(controller.getCoordinator() != nullptr);
    controller.getCoordinator()->waitForWorkerToStop(3000);

    auto snapAfterCancel = controller.getCurrentSnapshot();
    CHECK(snapAfterCancel.sessionStatus == ProfilingSessionStatus::Cancelled);
    // Preservada
    CHECK(snapAfterCancel.evaluation.hasEvaluation == true);
    CHECK(snapAfterCancel.evaluation.canonicalEvaluationHash == baselineHash);
}

TEST_CASE("Controller + Coordinator: Exito publica nuevo ModelEvaluation verificado con RFC 8785", "[gui][session][rfc8785]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeSyntheticTestTarget());

    REQUIRE(controller.startProfiling());
    REQUIRE(controller.getCoordinator() != nullptr);
    controller.getCoordinator()->waitForWorkerToStop(3000);

    auto finalSnap = controller.getCurrentSnapshot();
    CHECK(finalSnap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(finalSnap.workflowStage == ProfilingWorkflowStage::ReviewResults);

    REQUIRE(finalSnap.evaluation.hasEvaluation == true);
    CHECK(finalSnap.evaluation.evaluationOrigin == EvaluationOrigin::SyntheticDemo);
    CHECK(finalSnap.evaluation.hashVerified == true);
    CHECK_FALSE(finalSnap.evaluation.canonicalEvaluationHash.empty());
}

TEST_CASE("Controller + Coordinator: Descarte de callback tardio con generacion o runId obsoleto", "[gui][session][callbacks]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeSyntheticTestTarget());

    auto snapBefore = controller.getCurrentSnapshot();
    uint64_t currentGen = snapBefore.controllerGeneration;

    // Simular inyección directa de callback de coordinator con sessionGeneration u obsoleta
    CoordinatorSnapshot staleSnap;
    staleSnap.state = CoordinatorState::Running;
    staleSnap.sessionGeneration = currentGen - 1; // Obsoleto
    staleSnap.runId = 999;
    staleSnap.progress = 50.0;
    staleSnap.currentTrial = 5;

    // Se invoca el listener del coordinador directamente
    controller.onCoordinatorSnapshotUpdated(staleSnap);

    // El snapshot no debe haber mutado el estado del controlador a Running
    auto snapAfter = controller.getCurrentSnapshot();
    CHECK(snapAfter.sessionStatus != ProfilingSessionStatus::Profiling);
    CHECK(snapAfter.progress.currentTrial == 0);
}

TEST_CASE("Controller: Rechazo de segundo startProfiling() concurrente (proteccion single-worker)", "[gui][session][concurrency]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeSyntheticTestTarget());

    REQUIRE(controller.startProfiling());
    REQUIRE(controller.getCoordinator() != nullptr);
    uint64_t firstRunId = controller.getCoordinator()->getCurrentRunId();

    // Intentar segundo start mientras corre el primero
    bool secondAccepted = controller.startProfiling();
    CHECK_FALSE(secondAccepted);

    // Verificar que el runId sigue siendo el primero y el coordinador sigue corriendo
    CHECK(controller.getCoordinator()->getCurrentRunId() == firstRunId);

    controller.cancelProfiling();
    controller.getCoordinator()->waitForWorkerToStop(3000);
}

TEST_CASE("Controller: Destruccion de listener/vista durante medicion no causa acceso invalido", "[gui][session][lifecycle]")
{
    ProfilingSessionController controller;
    controller.selectTarget(makeSyntheticTestTarget());

    {
        TestSessionEventListener scopedListener;
        controller.addListener(&scopedListener);
        REQUIRE(controller.startProfiling());
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        controller.removeListener(&scopedListener);
        // scopedListener sale de alcance aquí
    }

    // El worker sigue corriendo y emitiendo snapshots, no debe haber crash ni memory corruption
    REQUIRE(controller.getCoordinator() != nullptr);
    controller.getCoordinator()->waitForWorkerToStop(3000);
    SUCCEED("Worker completed cleanly after listener was removed and destroyed");
}
