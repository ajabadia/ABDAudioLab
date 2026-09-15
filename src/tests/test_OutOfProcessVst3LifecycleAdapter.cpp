#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ipc/IpcProtocol.h"
#include "ipc/Win32NamedPipe.h"
#include "ipc/WorkerProcessHost.h"
#include "synth/OutOfProcessVst3LifecycleAdapter.h"
#include "synth/SynthTargetLifecycleAdapters.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionCoordinator.h"

#include <juce_core/juce_core.h>
#include <chrono>
#include <thread>

using namespace abdaudiolab::ipc;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::gui::session;

namespace
{

#if JUCE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

void pumpTestUiMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
#else
void pumpTestUiMessages() {}
#endif

juce::File getTestReferenceVst3File()
{
    TargetSelectionState targetState;
    targetState.targetId = "ReferenceSynth";
    return InProcessVst3LifecycleAdapter::resolveVst3File(targetState);
}

} // namespace

// ==============================================================================
// Pruebas Unitarias de Protocolo Binario para el Slice 2
// ==============================================================================
TEST_CASE("IPC Protocol Slice 2: Serialización y Deserialización Little-Endian", "[worker][ipc][slice2]")
{
    SECTION("LoadPluginRequestPayload Round-trip")
    {
        LoadPluginRequestPayload req;
        req.runId = "run_12345";
        req.sessionId = "session_abcdef";
        req.pluginPath = "C:/Plugins/TestSynth.vst3";
        req.expectedBinarySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        req.sampleRate = 96000.0;
        req.blockSize = 512;

        auto bytes = req.serialize();
        auto opt = LoadPluginRequestPayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->runId == "run_12345");
        CHECK(opt->sessionId == "session_abcdef");
        CHECK(opt->pluginPath == "C:/Plugins/TestSynth.vst3");
        CHECK(opt->expectedBinarySha256 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        CHECK(opt->sampleRate == 96000.0);
        CHECK(opt->blockSize == 512);
    }

    SECTION("LoadPluginResponsePayload Round-trip")
    {
        LoadPluginResponsePayload resp;
        resp.status = 0;
        resp.errorCode = 0;
        resp.errorMessage = "";
        resp.pluginFormat = "VST3";
        resp.pluginUid = "UID_TEST_999";
        resp.vendor = "ABDSynths";
        resp.name = "ReferenceSynth";
        resp.version = "1.0.0";
        resp.binarySha256 = "feedfacecafe";
        resp.capabilities = 0x0001;
        resp.executionMode = "OutOfProcessVST3";

        auto bytes = resp.serialize();
        auto opt = LoadPluginResponsePayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->status == 0);
        CHECK(opt->pluginFormat == "VST3");
        CHECK(opt->name == "ReferenceSynth");
        CHECK(opt->binarySha256 == "feedfacecafe");
        CHECK(opt->executionMode == "OutOfProcessVST3");
    }

    SECTION("QueryContractPayloads Round-trip")
    {
        QueryContractResponsePayload resp;
        resp.targetId = "ReferenceSynth";
        resp.inputChannels = 0;
        resp.outputChannels = 2;
        resp.numParameters = 14;
        resp.supportsMidi = 1;
        resp.supportsNativeGui = 1;
        resp.requiresResetBetweenTrials = 1;
        resp.settlingTimeMs = 60;
        resp.determinism = "DeterministicAfterReset";
        resp.latencySamples = 0;

        auto bytes = resp.serialize();
        auto opt = QueryContractResponsePayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->targetId == "ReferenceSynth");
        CHECK(opt->outputChannels == 2);
        CHECK(opt->numParameters == 14);
        CHECK(opt->requiresResetBetweenTrials == 1);
        CHECK(opt->settlingTimeMs == 60);
        CHECK(opt->determinism == "DeterministicAfterReset");
    }

    SECTION("RenderBlockRequestPayload Round-trip y límites defensivos")
    {
        RenderBlockRequestPayload req;
        req.sequence = 1001;
        req.blockSize = 256;
        req.numChannels = 2;
        req.midiEvents.push_back({ 0, 0, 1, 60, 100 }); // NoteOn C4
        req.midiEvents.push_back({ 128, 1, 1, 60, 0 });  // NoteOff C4

        auto bytes = req.serialize();
        auto opt = RenderBlockRequestPayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->sequence == 1001);
        CHECK(opt->blockSize == 256);
        CHECK(opt->numChannels == 2);
        REQUIRE(opt->midiEvents.size() == 2);
        CHECK(opt->midiEvents[0].sampleOffset == 0);
        CHECK(opt->midiEvents[0].type == 0);
        CHECK(opt->midiEvents[0].noteNumber == 60);
        CHECK(opt->midiEvents[0].velocity == 100);
        CHECK(opt->midiEvents[1].sampleOffset == 128);

        // Rechazo defensivo: blockSize = 0 o blockSize excesivo
        RenderBlockRequestPayload badReq = req;
        badReq.blockSize = 0;
        auto badBytes = badReq.serialize();
        CHECK_FALSE(RenderBlockRequestPayload::deserialize(badBytes.data(), badBytes.size()).has_value());

        badReq.blockSize = 999999;
        badBytes = badReq.serialize();
        CHECK_FALSE(RenderBlockRequestPayload::deserialize(badBytes.data(), badBytes.size()).has_value());
    }

    SECTION("RenderBlockResponsePayload Round-trip con audio float32")
    {
        RenderBlockResponsePayload resp;
        resp.sequence = 1001;
        resp.status = 0;
        resp.errorCode = 0;
        resp.errorMessage = "";
        resp.samplesRendered = 256;
        resp.numChannels = 2;
        resp.durationUs = 1450;
        resp.audioData.resize(512, 0.5f); // 256 muestras x 2 canales

        auto bytes = resp.serialize();
        auto opt = RenderBlockResponsePayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->sequence == 1001);
        CHECK(opt->status == 0);
        CHECK(opt->samplesRendered == 256);
        CHECK(opt->numChannels == 2);
        CHECK(opt->durationUs == 1450);
        REQUIRE(opt->audioData.size() == 512);
        CHECK(opt->audioData[0] == Catch::Approx(0.5f));
        CHECK(opt->audioData[511] == Catch::Approx(0.5f));
    }
}

// ==============================================================================
// Pruebas de Integración con el Worker Real: Carga, Contrato, Reset y Liberación
// ==============================================================================
TEST_CASE("OutOfProcessVst3: Carga Nominal de ReferenceSynth en Worker Aislado", "[worker][vst3][slice2]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    auto vst3File = getTestReferenceVst3File();
    if (!vst3File.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en disco.");
    }

    OutOfProcessVst3LifecycleAdapter adapter;
    TargetSelectionState targetState;
    targetState.targetId = "ReferenceSynth";

    std::string errMsg;
    bool ok = adapter.initializeTarget(targetState, 48000.0, 256, errMsg);
    REQUIRE(ok);
    REQUIRE(adapter.isReady());
    REQUIRE(adapter.isWorkerAlive());

    // 1. Verificar Fingerprint Metrológico inmutable devuelto por el worker
    auto fp = adapter.getFingerprint();
    CHECK_FALSE(fp.vendor.empty());
    CHECK(fp.pluginFormatVersion.find("VST 3.7.x") != std::string::npos);
    CHECK(adapter.getExecutionMode() == "OutOfProcessVST3");
    CHECK_FALSE(fp.binarySha256.empty());
    CHECK(adapter.getEvaluationOrigin() == EvaluationOrigin::MeasuredExternalPlugin);

    // 2. Consulta de contrato y capacidades a través de IPC
    QueryContractResponsePayload contract;
    bool queried = adapter.getWorkerHost().queryContract(contract, 3000);
    REQUIRE(queried);
    CHECK(contract.outputChannels == 2);
    CHECK(contract.supportsMidi == 1);
    CHECK(contract.requiresResetBetweenTrials == 1);

    // 3. Reset de estado remoto para ensayos sucesivos
    adapter.resetForTrial();
    CHECK(adapter.isReady());

    // 4. Liberación limpia del target y worker
    adapter.releaseTarget();
    CHECK_FALSE(adapter.isReady());
    CHECK_FALSE(adapter.isWorkerAlive());
}

TEST_CASE("OutOfProcessVst3: Rechazo de Hash Divergente (BinaryChanged)", "[worker][vst3][slice2][safety]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    auto vst3File = getTestReferenceVst3File();
    if (!vst3File.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en disco.");
    }

    WorkerProcessHost host;
    std::string sessionId = "hash_test_" + std::to_string(juce::Time::currentTimeMillis());
    uint64_t nonce = 0x55AA1234ULL;

    REQUIRE(host.spawnAndConnect(workerExe.getFullPathName().toStdString(), sessionId, nonce, 5000));
    REQUIRE(host.isWorkerAlive());

    LoadPluginRequestPayload req;
    req.runId = "test_run";
    req.sessionId = sessionId;
    req.pluginPath = vst3File.getFullPathName().toStdString();
    // Hash fraudulento o antiguo para simular binario modificado
    req.expectedBinarySha256 = "0000111122223333444455556666777788889999aaaabbbbccccddddeeeeffff";
    req.sampleRate = 48000.0;
    req.blockSize = 256;

    LoadPluginResponsePayload resp;
    bool loaded = host.loadPlugin(req, resp, 5000);

    // DEBE RECHAZAR LA CARGA CATEGÓRICAMENTE
    CHECK_FALSE(loaded);
    CHECK(host.getLastError().find("Binary SHA-256 mismatch") != std::string::npos);

    host.orderlyShutdown(3000);
}

TEST_CASE("OutOfProcessVst3: Ruta Inexistente y Llamadas Fuera de Secuencia", "[worker][vst3][slice2][safety]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    SECTION("Ruta Inexistente reporta error sin crashear")
    {
        OutOfProcessVst3LifecycleAdapter adapter;
        TargetSelectionState targetState;
        targetState.targetId = "C:/Ruta/Completamente/Falsa/NoExiste.vst3";

        std::string errMsg;
        bool ok = adapter.initializeTarget(targetState, 48000.0, 256, errMsg);
        CHECK_FALSE(ok);
        CHECK_FALSE(adapter.isReady());
        CHECK_FALSE(errMsg.empty());
    }

    SECTION("Query o Reset antes de cargar plugin es rechazado con error estructurado")
    {
        WorkerProcessHost host;
        std::string sessionId = "seq_test_" + std::to_string(juce::Time::currentTimeMillis());
        uint64_t nonce = 0x12345678ULL;

        REQUIRE(host.spawnAndConnect(workerExe.getFullPathName().toStdString(), sessionId, nonce, 5000));
        REQUIRE(host.isWorkerAlive());

        QueryContractResponsePayload contract;
        CHECK_FALSE(host.queryContract(contract, 2000));
        CHECK(host.getLastError().find("no plugin loaded") != std::string::npos);

        CHECK_FALSE(host.resetPlugin(0, 2000));
        CHECK(host.getLastError().find("no plugin loaded") != std::string::npos);

        // Release repetido es idempotente y no falla
        CHECK(host.releasePlugin(2000));
        CHECK(host.releasePlugin(2000));

        host.orderlyShutdown(3000);
    }
}

// ==============================================================================
// Slice 3: Renderizado de Bloque Remoto y Comparación In-Process vs Out-of-Process
// ==============================================================================
TEST_CASE("OutOfProcessVst3 Slice 3: Render de Bloque Remoto en ReferenceSynth", "[worker][vst3][slice3]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    auto vst3File = getTestReferenceVst3File();
    if (!vst3File.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en disco.");
    }

    TargetSelectionState targetState;
    targetState.targetId = "ReferenceSynth";

    const double sampleRate = 48000.0;
    const int blockSize = 256;

    // 1. Inicializar adaptador remoto (Out-of-Process)
    OutOfProcessVst3LifecycleAdapter remoteAdapter;
    std::string errMsg;
    bool okRemote = remoteAdapter.initializeTarget(targetState, sampleRate, blockSize, errMsg);
    REQUIRE(okRemote);
    REQUIRE(remoteAdapter.isReady());
    auto* remoteTarget = remoteAdapter.getTarget();
    REQUIRE(remoteTarget != nullptr);

    // 2. Inicializar adaptador local de referencia (In-Process)
    InProcessVst3LifecycleAdapter localAdapter;
    bool okLocal = localAdapter.initializeTarget(targetState, sampleRate, blockSize, errMsg);
    REQUIRE(okLocal);
    REQUIRE(localAdapter.isReady());
    auto* localTarget = localAdapter.getTarget();
    REQUIRE(localTarget != nullptr);

    // 3. Crear secuencia idéntica de excitación (1 bloque: NoteOn C4 vel 100)
    MidiExcitationSequence seq;
    seq.channel = 1;
    seq.noteNumber = 60;
    seq.totalDurationSec = static_cast<double>(blockSize) / sampleRate;
    seq.events.push_back({
        TimedMidiType::NoteOn,
        1,
        60,
        100.0f / 127.0f,
        0, // sampleOffset = 0
        0.0
    });

    // Resetear ambos targets para garantizar mismo punto de partida
    localTarget->resetState();
    remoteTarget->resetState();

    // 4. Renderizar ambos
    std::vector<float> localAudio;
    localTarget->render(seq, localAudio, 0);

    std::vector<float> remoteAudio;
    remoteTarget->render(seq, remoteAudio, 0);

    // 5. Validaciones de tamaño y presencia de señal
    REQUIRE(remoteAudio.size() == static_cast<size_t>(blockSize));
    REQUIRE(localAudio.size() == static_cast<size_t>(blockSize));

    // Calcular RMS de la señal remota producida por el worker
    double sumSquaresRemote = 0.0;
    for (float sample : remoteAudio)
    {
        sumSquaresRemote += static_cast<double>(sample * sample);
    }
    double rmsRemote = std::sqrt(sumSquaresRemote / static_cast<double>(blockSize));
    CHECK(rmsRemote > 0.0001); // Comprobar que el worker produjo audio real (no silencio)

    // 6. Comparación contractual estricta (tolerancia 1e-5 y reporte si es bit-exacta)
    bool bitExact = true;
    float maxAbsDiff = 0.0f;
    for (size_t i = 0; i < static_cast<size_t>(blockSize); ++i)
    {
        float diff = std::abs(remoteAudio[i] - localAudio[i]);
        if (remoteAudio[i] != localAudio[i])
        {
            bitExact = false;
        }
        if (diff > maxAbsDiff)
        {
            maxAbsDiff = diff;
        }
    }

    CHECK(maxAbsDiff <= 1e-5f);
    INFO("Comparación Slice 3 In-Process vs Worker: Bit-Exact = " << (bitExact ? "YES" : "NO")
         << ", Max Abs Diff = " << maxAbsDiff << ", Remote RMS = " << rmsRemote);

    // 7. Liberación limpia de ambos targets
    remoteAdapter.releaseTarget();
    localAdapter.releaseTarget();
    CHECK_FALSE(remoteAdapter.isReady());
}

TEST_CASE("OutOfProcessVst3 Slice 3: Validación Defensiva de Render (Precondiciones y Tamaños)", "[worker][vst3][slice3][safety]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    WorkerProcessHost host;
    std::string sessionId = "render_defensive_" + std::to_string(juce::Time::currentTimeMillis());
    uint64_t nonce = 0xAABBCCDDULL;

    REQUIRE(host.spawnAndConnect(workerExe.getFullPathName().toStdString(), sessionId, nonce, 5000));
    REQUIRE(host.isWorkerAlive());

    // 1. RenderBlock sin plugin cargado debe devolver error estructurado 412
    RenderBlockRequestPayload req;
    req.sequence = 42;
    req.blockSize = 256;
    req.numChannels = 2;

    RenderBlockResponsePayload resp;
    bool ok = host.renderBlock(req, resp, 2000);
    CHECK_FALSE(ok);
    CHECK(resp.status != 0);
    CHECK(resp.errorCode == 412);
    CHECK(host.getLastError().find("no plugin loaded") != std::string::npos);

    host.orderlyShutdown(3000);
}

// ==============================================================================
// Slice 4: Integración End-to-End del Modo Guiado con ProfilingSessionCoordinator
// ==============================================================================
namespace
{

class TestCoordinatorListenerProbe : public ICoordinatorListener
{
public:
    std::mutex mtx;
    std::vector<CoordinatorSnapshot> snapshots;
    int completedCount { 0 };
    int failedCount { 0 };
    std::string lastError;
    std::optional<ModelEvaluation> lastCandidate;

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

    void onCoordinatorCancelled(uint64_t, uint64_t) override {}

    void onCoordinatorFailed(uint64_t, uint64_t, const std::string& error) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        failedCount++;
        lastError = error;
    }
};

} // namespace

TEST_CASE("Modo Guiado End-to-End: Orquestación completa con ReferenceSynth en Worker Aislado", "[coordinator][guided][mvp]")
{
    auto workerExe = OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable();
    if (!workerExe.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún.");
    }

    auto vst3File = getTestReferenceVst3File();
    if (!vst3File.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en disco.");
    }

    TestCoordinatorListenerProbe listener;
    ProfilingSessionCoordinator coordinator(&listener);

    TargetSelectionState target;
    target.targetId = "ReferenceSynth";
    target.targetName = "ReferenceSynth VST3 (Worker Aislado IPC)";
    target.manufacturer = "ABDSynths";
    target.kind = TargetKind::PluginVST3;
    target.useIsolatedProcess = true; // Forzar modo esclavo en worker
    target.isConnected = true;
    target.isDeterministic = true;

    // Configurar watchdog adaptado al transporte IPC entre procesos
    coordinator.setWatchdogBlockTimeoutMs(2500.0);

    // Iniciar corrida guiada de 3 ensayos rápidos
    bool started = coordinator.start(target, 1, 3, 20);
    REQUIRE(started);

    // Esperar a que el coordinador termine su secuencia
    auto tStart = std::chrono::steady_clock::now();
    while (coordinator.isRunning())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        if (elapsed > 10000) break;
    }
    coordinator.waitForWorkerToStop(2000);

    std::lock_guard<std::mutex> lock(listener.mtx);
    INFO("Coordinator failedCount: " << listener.failedCount << ", lastError: " << listener.lastError);
    REQUIRE(listener.completedCount == 1);
    REQUIRE(listener.failedCount == 0);
    REQUIRE(listener.lastCandidate.has_value());

    // Verificar que el informe metrológico RFC 8785 registra procedencia aislada
    const auto& cand = *listener.lastCandidate;
    CHECK(cand.origin == EvaluationOrigin::MeasuredExternalPlugin);
    CHECK(cand.executionMode == "OutOfProcessVST3");
    CHECK_FALSE(cand.pluginBinarySha256.empty());
    CHECK(cand.hashVerified);
    CHECK_FALSE(cand.canonicalEvaluationHash.empty());
}

TEST_CASE("No Regresión: Target con useIsolatedProcess=false preserva InProcessVST3", "[coordinator][safety][legacy]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto vst3File = getTestReferenceVst3File();
    if (!vst3File.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en disco.");
    }

    TestCoordinatorListenerProbe listener;
    ProfilingSessionCoordinator coordinator(&listener);

    TargetSelectionState legacyTarget;
    legacyTarget.targetId = "ReferenceSynth";
    legacyTarget.kind = TargetKind::PluginVST3;
    legacyTarget.useIsolatedProcess = false; // Ruta no guiada / in-process tradicional

    bool started = coordinator.start(legacyTarget, 1, 2, 10);
    REQUIRE(started);

    auto tStart = std::chrono::steady_clock::now();
    while (coordinator.isRunning())
    {
        pumpTestUiMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        if (elapsed > 10000) break;
    }
    pumpTestUiMessages();
    coordinator.waitForWorkerToStop(2000);
    pumpTestUiMessages();

    std::lock_guard<std::mutex> lock(listener.mtx);
    INFO("Legacy InProcess failedCount: " << listener.failedCount << ", lastError: " << listener.lastError);
    REQUIRE(listener.completedCount == 1);
    REQUIRE(listener.lastCandidate.has_value());
    CHECK(listener.lastCandidate->executionMode == "InProcessVST3");
}

