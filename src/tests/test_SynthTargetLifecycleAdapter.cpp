#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "synth/ISynthTargetLifecycleAdapter.h"
#include "synth/SynthTargetLifecycleAdapters.h"
#include "synth/ExternalPluginFixture.h"
#include "synth/ModelEvaluationBuilder.h"
#include "gui/session/ProfilingSessionCoordinator.h"
#include "gui/session/ProfilingSessionController.h"
#include "gui/session/ProfilingSessionContracts.h"

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <thread>

#if JUCE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace abdaudiolab::synth;
using namespace abdaudiolab::gui::session;

namespace
{

void pumpUiMessages()
{
#if JUCE_WINDOWS
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

juce::File getReferenceSynthFileForTests()
{
    TargetSelectionState state;
    state.targetId = "ReferenceSynth";
    return InProcessVst3LifecycleAdapter::resolveVst3File(state);
}

class TestCoordinatorProbeListener : public ICoordinatorListener
{
public:
    std::mutex mtx;
    std::vector<CoordinatorSnapshot> snapshots;
    int completedCount { 0 };
    int cancelledCount { 0 };
    int failedCount { 0 };
    std::optional<ModelEvaluation> lastCandidate;
    std::string lastErrorMessage;

    void onCoordinatorSnapshotUpdated(const CoordinatorSnapshot& snap) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        snapshots.push_back(snap);
    }

    void onCoordinatorCompleted(uint64_t, uint64_t, const ModelEvaluation& candidate) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        completedCount++;
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

} // namespace

TEST_CASE("LifecycleAdapter: SyntheticTargetLifecycleAdapter contrato completo", "[lifecycle][synthetic]")
{
    SyntheticTargetLifecycleAdapter adapter(48000.0, 42);
    REQUIRE_FALSE(adapter.isReady());
    REQUIRE(adapter.getTarget() == nullptr);

    TargetSelectionState state;
    state.targetId = "synthetic_fixture_demo";
    state.targetName = "Sintetizador Virtual de Prueba";
    state.manufacturer = "ABDAudioLab";

    std::string err;
    bool ok = adapter.initializeTarget(state, 48000.0, 256, err);
    REQUIRE(ok);
    REQUIRE(adapter.isReady());
    REQUIRE(adapter.getTarget() != nullptr);
    REQUIRE(adapter.getEvaluationOrigin() == EvaluationOrigin::SyntheticDemo);
    REQUIRE(adapter.getExecutionMode() == "DemoMode");

    auto fp = adapter.getFingerprint();
    REQUIRE_FALSE(fp.normalizedFingerprint.empty());
    REQUIRE(fp.vendor == "ABDAudioLab");

    adapter.releaseTarget();
    REQUIRE_FALSE(adapter.isReady());
    REQUIRE(adapter.getTarget() == nullptr);
}

TEST_CASE("LifecycleAdapter: Resolucion y diagnostico controlado de artefacto VST3", "[lifecycle][vst3]")
{
    TargetSelectionState invalidState;
    invalidState.targetId = "Ruta_Inexistente_Para_Prueba/PluginQueNoExiste.vst3";

    InProcessVst3LifecycleAdapter adapter;
    std::string err;
    bool ok = adapter.initializeTarget(invalidState, 48000.0, 256, err);

    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(adapter.isReady());
    REQUIRE(adapter.getTarget() == nullptr);
    REQUIRE(err.find("does not exist") != std::string::npos);
}

TEST_CASE("LifecycleAdapter: Huella normalizada determinista ante variaciones de ruta", "[lifecycle][provenance]")
{
    TargetFingerprint fp1;
    fp1.pluginPath = "D:/desarrollos/ABDSynths/ABDAudioLab/build/ReferenceSynth.vst3";
    fp1.pluginFormatVersion = "VST 3.7.x";
    fp1.vendor = "ABDSynths";
    fp1.pluginUid = "RefSynth_UID";
    fp1.fileSizeBytes = 1048576;
    fp1.binarySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    fp1.buildConfiguration = "Release-x64";
    fp1.hostSampleRate = 96000.0;
    fp1.hostBlockSize = 256;
    fp1.osArchitecture = "x86_64-windows";

    TargetFingerprint fp2 = fp1;
    // Misma ruta con barras invertidas y variacion de mayusculas/minusculas
    fp2.pluginPath = "d:\\desarrollos\\abdsynths\\abdaudiolab\\build\\referencesynth.vst3";

    std::string hash1 = fp1.computeNormalizedFingerprint();
    std::string hash2 = fp2.computeNormalizedFingerprint();

    REQUIRE_FALSE(hash1.empty());
    REQUIRE(hash1 == hash2);

    // Si cambia el hash binario o la configuracion, la huella cambia obligatoriamente
    fp2.binarySha256 = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    REQUIRE(fp1.computeNormalizedFingerprint() != fp2.computeNormalizedFingerprint());
}

TEST_CASE("LifecycleAdapter: Integridad de hash binario ante modificacion de archivo", "[lifecycle][provenance]")
{
    auto tempFile = juce::File::createTempFile("vst3_test_hash");
    tempFile.replaceWithText("Original Content 1");
    juce::MemoryBlock mb1;
    tempFile.loadFileAsData(mb1);
    std::string hash1 = Sha256::computeHex(static_cast<const uint8_t*>(mb1.getData()), mb1.getSize());

    tempFile.replaceWithText("Modified Content 2");
    juce::MemoryBlock mb2;
    tempFile.loadFileAsData(mb2);
    std::string hash2 = Sha256::computeHex(static_cast<const uint8_t*>(mb2.getData()), mb2.getSize());

    REQUIRE_FALSE(hash1.empty());
    REQUIRE_FALSE(hash2.empty());
    REQUIRE(hash1 != hash2);

    tempFile.deleteFile();
}

TEST_CASE("LifecycleAdapter: ReleaseTarget invalida el target de forma irreversible", "[lifecycle][vst3]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = getReferenceSynthFileForTests();
    if (!pluginFile.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en build/ReferenceSynth_artefacts; compile ReferenceSynth primero.");
    }

    InProcessVst3LifecycleAdapter adapter;
    TargetSelectionState state;
    state.targetId = pluginFile.getFullPathName().toStdString();
    state.kind = TargetKind::PluginVST3;

    std::string err;
    bool ok = adapter.initializeTarget(state, 48000.0, 256, err);
    REQUIRE(ok);
    REQUIRE(adapter.isReady());
    REQUIRE(adapter.getTarget() != nullptr);

    adapter.releaseTarget();
    REQUIRE_FALSE(adapter.isReady());
    REQUIRE(adapter.getTarget() == nullptr);
}

TEST_CASE("LifecycleAdapter: Excepcion o corrupcion marca el adaptador no reutilizable", "[lifecycle][safety]")
{
    InProcessVst3LifecycleAdapter adapter;
    adapter.markCorrupted("Fallo simulado critico de memoria");

    REQUIRE_FALSE(adapter.isReady());
    REQUIRE(adapter.isCorrupted());
    REQUIRE(adapter.getTarget() == nullptr);

    TargetSelectionState state;
    state.targetId = "dummy";
    std::string err;
    bool ok = adapter.initializeTarget(state, 48000.0, 256, err);
    REQUIRE_FALSE(ok);
    REQUIRE(err.find("corrupted") != std::string::npos);
}

TEST_CASE("LifecycleAdapter: Watchdog de timeout por bloque aborta de forma segura", "[lifecycle][watchdog]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = getReferenceSynthFileForTests();
    if (!pluginFile.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en build; compile ReferenceSynth primero.");
    }

    // Adaptador con timeout ultrabajo para provocar intencionalmente disparo de watchdog
    InProcessVst3LifecycleAdapter adapter(0.000001); // 1 nanosegundo
    TargetSelectionState state;
    state.targetId = pluginFile.getFullPathName().toStdString();
    state.kind = TargetKind::PluginVST3;

    std::string err;
    bool ok = adapter.initializeTarget(state, 48000.0, 256, err);
    REQUIRE(ok);

    auto* target = adapter.getTarget();
    REQUIRE(target != nullptr);

    MidiExcitationSequence seq;
    seq.totalDurationSec = 0.05; // ~2400 muestras
    TimedMidiEvent ev;
    ev.type = TimedMidiType::NoteOn;
    ev.noteNumber = 60;
    ev.velocity = 0.8f;
    seq.events.push_back(ev);

    std::vector<float> audio;
    bool caughtTimeout = false;
    try
    {
        target->render(seq, audio, 1);
    }
    catch (const std::exception& e)
    {
        caughtTimeout = true;
        std::string msg = e.what();
        REQUIRE(msg.find("Watchdog timeout") != std::string::npos);
    }

    REQUIRE(caughtTimeout);
}

TEST_CASE("Exportador: Rechazo de MeasuredExternalPlugin si falta procedencia binaria", "[controller][export]")
{
    ProfilingSessionController controller;
    TargetSelectionState target;
    target.targetId = "test_plugin";
    target.targetName = "External Plugin Test";
    target.kind = TargetKind::PluginVST3;
    controller.selectTarget(target);

    // Simular resultado de evaluacion MeasuredExternalPlugin sin SHA-256 binario
    ModelEvaluation eval;
    eval.evaluationId = "eval_external_test_missing_hash";
    eval.origin = EvaluationOrigin::MeasuredExternalPlugin;
    eval.sourceTargetIdentity = "External Plugin Test";
    eval.decision.status = SelectionStatus::Accepted;
    eval.decision.recommendedModelId = "LUT_SIMD_2D";
    eval.pluginBinarySha256 = ""; // FALTANTE deliberadamente
    eval.pluginPath = "";
    eval.computeCanonicalHash();

    controller.updateModelEvaluation(eval);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE(snap.evaluation.hasEvaluation);
    // Debe estar bloqueado por la politica metrologica de procedencia
    REQUIRE_FALSE(snap.exportOptions.canExportCpp);
    REQUIRE(snap.evaluation.exportBlockReason.find("MeasuredExternalPlugin") != std::string::npos);

    // Intento de exportacion debe ser rechazado
    bool exported = controller.exportModel("cpp_header", "dummy_dest.h");
    REQUIRE_FALSE(exported);
}

TEST_CASE("Coordinator: Medicion in-process end-to-end con ReferenceSynth.vst3", "[coordinator][vst3][e2e]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = getReferenceSynthFileForTests();
    if (!pluginFile.exists())
    {
        SKIP("ReferenceSynth.vst3 no disponible en build; se omite test E2E");
    }

    TestCoordinatorProbeListener listener;
    ProfilingSessionCoordinator coordinator(&listener);

    TargetSelectionState target;
    target.targetId = pluginFile.getFullPathName().toStdString();
    target.targetName = "ReferenceSynth VST3";
    target.manufacturer = "ABDSynths";
    target.kind = TargetKind::PluginVST3;
    target.isConnected = true;
    target.isDeterministic = true;

    bool started = coordinator.start(target, 1, 5, 5); // 5 ensayos rapidos
    REQUIRE(started);

    // Esperar hasta 10000ms a que termine el worker en segundo plano despachando mensajes de Windows/JUCE
    auto tStart = std::chrono::steady_clock::now();
    while (coordinator.isRunning())
    {
        pumpUiMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        if (elapsed > 10000)
            break;
    }
    // Drenar mensajes pendientes finales antes de esperar a la detención del hilo
    pumpUiMessages();
    coordinator.waitForWorkerToStop(2000);
    pumpUiMessages();

    auto snap = coordinator.getSnapshot();
    std::lock_guard<std::mutex> lock(listener.mtx);
    INFO("Coordinator state: " << static_cast<int>(snap.state) << " isThreadRunning: " << coordinator.isThreadRunning());
    INFO("Snapshots count: " << listener.snapshots.size());
    INFO("Coordinator lastErrorMessage: " << listener.lastErrorMessage);
    INFO("Coordinator failedCount: " << listener.failedCount);
    INFO("Coordinator cancelledCount: " << listener.cancelledCount);
    INFO("Coordinator completedCount: " << listener.completedCount);
    REQUIRE(listener.completedCount == 1);
    REQUIRE(listener.failedCount == 0);
    REQUIRE(listener.lastCandidate.has_value());

    const auto& cand = *listener.lastCandidate;
    REQUIRE(cand.origin == EvaluationOrigin::MeasuredExternalPlugin);
    REQUIRE(cand.executionMode == "InProcessVST3");
    REQUIRE_FALSE(cand.pluginBinarySha256.empty());
    REQUIRE_FALSE(cand.normalizedFingerprint.empty());
    REQUIRE(cand.hashVerified);
    REQUIRE_FALSE(cand.canonicalEvaluationHash.empty());
}
