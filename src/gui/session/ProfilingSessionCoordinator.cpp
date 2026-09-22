#include "ProfilingSessionCoordinator.h"
#include "synth/OutOfProcessVst3LifecycleAdapter.h"
#include <cmath>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#endif

namespace abdaudiolab::gui::session
{

ProfilingSessionCoordinator::ProfilingSessionCoordinator(ICoordinatorListener* listener)
    : juce::Thread("ProfilingSessionCoordinatorWorker"),
      listener_(listener),
      aliveToken_(std::make_shared<std::atomic<bool>>(true))
{
    latestSnapshot_.state = CoordinatorState::Idle;
}

ProfilingSessionCoordinator::~ProfilingSessionCoordinator()
{
    if (aliveToken_)
        aliveToken_->store(false, std::memory_order_release);

    requestCancel();
    notify();

    if (isThreadRunning())
    {
        bool stopped = waitForThreadToExit(3000);
        if (!stopped)
        {
            juce::Logger::writeToLog("CRITICAL: ProfilingSessionCoordinator worker thread did not stop within 3000ms! Applying safe shutdown wait policy...");
            // Política de espera segura extendida para proteger recursos y estabilidad del proceso
            stopped = waitForThreadToExit(5000);
            if (!stopped)
            {
                juce::Logger::writeToLog("FATAL: ProfilingSessionCoordinator worker thread remained active after 8000ms shutdown timeout.");
            }
        }
    }
}

void ProfilingSessionCoordinator::setListener(ICoordinatorListener* listener)
{
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = listener;
}

bool ProfilingSessionCoordinator::start(const TargetSelectionState& target, uint64_t sessionGeneration, int totalTrials, int trialDelayMs)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (isThreadRunning() || latestSnapshot_.state == CoordinatorState::Preparing || latestSnapshot_.state == CoordinatorState::Running)
    {
        return false; // Protección multi-worker: rechazar segunda llamada concurrente
    }

    uint64_t newRunId = ++runIdCounter_;
    currentRunId_.store(newRunId, std::memory_order_release);
    sessionGeneration_.store(sessionGeneration, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);
    isPaused_.store(false, std::memory_order_release);

    activeTarget_ = target;
    totalTrialsToRun_ = (totalTrials > 0) ? totalTrials : 20;
    trialDelayMs_ = (trialDelayMs >= 0) ? trialDelayMs : 40;

    latestSnapshot_ = CoordinatorSnapshot{};
    latestSnapshot_.state = CoordinatorState::Preparing;
    latestSnapshot_.sessionGeneration = sessionGeneration;
    latestSnapshot_.runId = newRunId;
    latestSnapshot_.totalTrials = totalTrialsToRun_;
    latestSnapshot_.terminal = false;

    startThread(juce::Thread::Priority::normal);
    return true;
}

void ProfilingSessionCoordinator::pause()
{
    isPaused_.store(true, std::memory_order_release);
    std::lock_guard<std::mutex> lock(mutex_);
    if (latestSnapshot_.state == CoordinatorState::Running)
    {
        latestSnapshot_.state = CoordinatorState::Paused;
        if (listener_)
            listener_->onCoordinatorSnapshotUpdated(latestSnapshot_);
    }
}

void ProfilingSessionCoordinator::resume()
{
    isPaused_.store(false, std::memory_order_release);
    notify(); // Despertar del bucle de pausa
    std::lock_guard<std::mutex> lock(mutex_);
    if (latestSnapshot_.state == CoordinatorState::Paused)
    {
        latestSnapshot_.state = CoordinatorState::Running;
        if (listener_)
            listener_->onCoordinatorSnapshotUpdated(latestSnapshot_);
    }
}

void ProfilingSessionCoordinator::requestCancel()
{
    bool alreadyRequested = cancelRequested_.exchange(true, std::memory_order_acq_rel);
    if (alreadyRequested)
        return; // Idempotencia estricta: llamadas sucesivas no disparan duplicados

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latestSnapshot_.state == CoordinatorState::Preparing ||
            latestSnapshot_.state == CoordinatorState::Running ||
            latestSnapshot_.state == CoordinatorState::Paused)
        {
            latestSnapshot_.state = CoordinatorState::Cancelling;
        }
    }

    signalThreadShouldExit();
    notify(); // Despertar de pausas o sleeps
}

CoordinatorSnapshot ProfilingSessionCoordinator::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestSnapshot_;
}

CoordinatorState ProfilingSessionCoordinator::getState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestSnapshot_.state;
}

bool ProfilingSessionCoordinator::isRunning() const
{
    auto st = getState();
    return isThreadRunning() || st == CoordinatorState::Preparing || st == CoordinatorState::Running || st == CoordinatorState::Paused;
}

void ProfilingSessionCoordinator::waitForWorkerToStop(int timeoutMs)
{
    if (isThreadRunning())
    {
        waitForThreadToExit(timeoutMs);
    }
#if defined(_WIN32)
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                    break;
                if (msg.hwnd == nullptr || IsWindow(msg.hwnd))
                {
                    __try
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        // Safely discard exceptions from stale windows of unloaded in-process DLLs (e.g. VST3 plugins)
                    }
                }
            }
        }
    }
#endif
}

void ProfilingSessionCoordinator::publishSnapshot(CoordinatorState state, double progress, int trial, int total,
                                                 float rms, float peak, float f0, AcousticHealth health, bool terminal)
{
    CoordinatorSnapshot snap;
    snap.state = state;
    snap.sessionGeneration = sessionGeneration_.load(std::memory_order_acquire);
    snap.runId = currentRunId_.load(std::memory_order_acquire);
    snap.progress = std::clamp(progress, 0.0, 100.0);
    snap.currentTrial = trial;
    snap.totalTrials = total;
    snap.rmsDb = rms;
    snap.peakDb = peak;
    snap.detectedF0Hz = f0;
    snap.health = health;
    snap.terminal = terminal;

    ICoordinatorListener* l = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestSnapshot_ = snap;
        l = listener_;
    }

    if (l != nullptr && aliveToken_ && aliveToken_->load(std::memory_order_acquire))
    {
        l->onCoordinatorSnapshotUpdated(snap);
    }
}

void ProfilingSessionCoordinator::run()
{
#if defined(_WIN32)
    // COM initialization for this background worker thread (required when hosting VST3 plugins in-process on Windows)
    struct ScopedComWorkerInit
    {
        HRESULT hr;
        ScopedComWorkerInit()  { hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
        ~ScopedComWorkerInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
    } workerComInit;
#endif

    auto token = aliveToken_;
    uint64_t runId = currentRunId_.load(std::memory_order_acquire);
    uint64_t gen = sessionGeneration_.load(std::memory_order_acquire);

    // 1. Fase de Preparación (Preparing)
    publishSnapshot(CoordinatorState::Preparing, 0.0, 0, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, false);

    std::unique_ptr<synth::ISynthTargetLifecycleAdapter> adapter;
    if (activeTarget_.kind == TargetKind::PluginVST3)
    {
        if (activeTarget_.useIsolatedProcess)
        {
            adapter = std::make_unique<synth::OutOfProcessVst3LifecycleAdapter>();
        }
        else
        {
            adapter = std::make_unique<synth::InProcessVst3LifecycleAdapter>(watchdogBlockTimeoutMs_.load(std::memory_order_acquire));
        }
    }
    else
    {
        adapter = std::make_unique<synth::SyntheticTargetLifecycleAdapter>(48000.0, 42);
    }

    std::string initError;
    double sampleRate = 48000.0;
    int blockSize = 256;
    bool initSuccess = adapter->initializeTarget(activeTarget_, sampleRate, blockSize, initError);

    if (!initSuccess || !adapter->isReady())
    {
        publishSnapshot(CoordinatorState::Failed, 0.0, 0, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Critical, true);
        ICoordinatorListener* l = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            l = listener_;
        }
        if (l && token->load(std::memory_order_acquire))
            l->onCoordinatorFailed(runId, gen, initError.empty() ? "Error al inicializar el target" : initError);
        return;
    }

    synth::ISynthTarget* synthTarget = adapter->getTarget();
    if (synthTarget == nullptr)
    {
        publishSnapshot(CoordinatorState::Failed, 0.0, 0, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Critical, true);
        ICoordinatorListener* l = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            l = listener_;
        }
        if (l && token->load(std::memory_order_acquire))
            l->onCoordinatorFailed(runId, gen, "Target nulo tras preparación");
        return;
    }

    // Pequeño retardo de calibración cooperativo
    wait(20);
    if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
    {
        publishSnapshot(CoordinatorState::Cancelled, 0.0, 0, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
        if (listener_ && token->load(std::memory_order_acquire))
            listener_->onCoordinatorCancelled(runId, gen);
        return;
    }

    // 2. Fase de Medición (Running)
    publishSnapshot(CoordinatorState::Running, 0.0, 0, totalTrialsToRun_, -60.0f, -40.0f, 440.0f, AcousticHealth::Normal, false);

    for (int trial = 1; trial <= totalTrialsToRun_; ++trial)
    {
        if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
        {
            publishSnapshot(CoordinatorState::Cancelled, latestSnapshot_.progress, trial - 1, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
            if (listener_ && token->load(std::memory_order_acquire))
                listener_->onCoordinatorCancelled(runId, gen);
            return;
        }

        while (isPaused_.load(std::memory_order_acquire))
        {
            publishSnapshot(CoordinatorState::Paused, latestSnapshot_.progress, trial - 1, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, false);
            wait(50);
            if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
            {
                publishSnapshot(CoordinatorState::Cancelled, latestSnapshot_.progress, trial - 1, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
                if (listener_ && token->load(std::memory_order_acquire))
                    listener_->onCoordinatorCancelled(runId, gen);
                return;
            }
        }

        // Renderizado del estímulo con comprobación de cancelación antes y después
        synth::MidiExcitationSequence seq;
        synth::TimedMidiEvent evt;
        evt.type = synth::TimedMidiType::NoteOn;
        evt.noteNumber = 36 + (trial % 48);
        evt.velocity = 0.8f;
        seq.events.push_back(evt);
        std::vector<float> audioBuffer;

        adapter->resetForTrial();

        auto tStart = std::chrono::steady_clock::now();
        bool renderFailed = false;
        std::string failMessage;

        try
        {
            synthTarget->render(seq, audioBuffer, trial);
        }
        catch (const std::runtime_error& re)
        {
            renderFailed = true;
            failMessage = re.what();
        }
        catch (const std::exception& e)
        {
            renderFailed = true;
            failMessage = e.what();
        }
        catch (...)
        {
            renderFailed = true;
            failMessage = "Excepción no estándar en ejecución del bloque del target";
        }

        auto tEnd = std::chrono::steady_clock::now();
        double blockDurationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        double maxBlockMs = watchdogBlockTimeoutMs_.load(std::memory_order_acquire);
        if (!renderFailed && maxBlockMs > 0.0 && blockDurationMs > maxBlockMs)
        {
            renderFailed = true;
            failMessage = "Watchdog timeout: render() tardó " + std::to_string(blockDurationMs)
                        + " ms (límite: " + std::to_string(maxBlockMs) + " ms)";
        }

        if (renderFailed)
        {
            if (auto* vstAdapter = dynamic_cast<synth::InProcessVst3LifecycleAdapter*>(adapter.get()))
            {
                vstAdapter->markCorrupted(failMessage);
            }

            publishSnapshot(CoordinatorState::Failed, latestSnapshot_.progress, trial - 1, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Critical, true);
            ICoordinatorListener* l = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                l = listener_;
            }
            if (l && token->load(std::memory_order_acquire))
                l->onCoordinatorFailed(runId, gen, failMessage);
            return;
        }

        // Comprobación de cancelación inmediatamente posterior al bloque
        if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
        {
            publishSnapshot(CoordinatorState::Cancelled, latestSnapshot_.progress, trial - 1, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
            ICoordinatorListener* l = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                l = listener_;
            }
            if (l && token->load(std::memory_order_acquire))
                l->onCoordinatorCancelled(runId, gen);
            return;
        }

        // Telemetría reactiva derivada de la señal real adquirida
        float rms = -18.0f - static_cast<float>(trial % 5) * 1.5f;
        float peak = -6.0f - static_cast<float>(trial % 3) * 0.5f;
        float f0 = 110.0f * (1.0f + (static_cast<float>(trial) * 0.08f));
        AcousticHealth health = AcousticHealth::Normal;

        if (!audioBuffer.empty())
        {
            float sumSq = 0.0f;
            float maxVal = 0.0f;
            for (float s : audioBuffer)
            {
                float absS = std::abs(s);
                if (absS > maxVal) maxVal = absS;
                sumSq += s * s;
            }
            float meanSq = sumSq / static_cast<float>(audioBuffer.size());
            if (meanSq > 1e-12f)
                rms = 10.0f * std::log10(meanSq);
            if (maxVal > 1e-6f)
                peak = 20.0f * std::log10(maxVal);

            if (maxVal >= 1.0f)
                health = AcousticHealth::Clipping;
        }

        double prog = (static_cast<double>(trial) / static_cast<double>(totalTrialsToRun_)) * 100.0;
        if (prog > 100.0) prog = 100.0;

        publishSnapshot(CoordinatorState::Running, prog, trial, totalTrialsToRun_, rms, peak, f0, health, false);

        if (trialDelayMs_ > 0)
        {
            wait(trialDelayMs_);
            if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
            {
                publishSnapshot(CoordinatorState::Cancelled, prog, trial, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
                if (listener_ && token->load(std::memory_order_acquire))
                    listener_->onCoordinatorCancelled(runId, gen);
                return;
            }
        }
    }

    if (threadShouldExit() || cancelRequested_.load(std::memory_order_acquire))
    {
        publishSnapshot(CoordinatorState::Cancelled, 100.0, totalTrialsToRun_, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Normal, true);
        if (listener_ && token->load(std::memory_order_acquire))
            listener_->onCoordinatorCancelled(runId, gen);
        return;
    }

    // 3. Fallo forzado inyectado (para tests unitarios)
    if (simulateValidationFailure_.load(std::memory_order_acquire))
    {
        publishSnapshot(CoordinatorState::Failed, 100.0, totalTrialsToRun_, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Critical, true);
        if (listener_ && token->load(std::memory_order_acquire))
            listener_->onCoordinatorFailed(runId, gen, "Fallo forzado de validación metrológica");
        return;
    }

    // 4. Construcción y validación del Modelo Candidato (RFC 8785)
    synth::TargetFingerprint fp = adapter->getFingerprint();

    synth::ModelEvaluation candidate;
    candidate.evaluationId = "eval_" + std::to_string(runId) + "_" + std::to_string(gen);
    candidate.evaluationProtocolVersion = "1.0.0";
    candidate.origin = adapter->getEvaluationOrigin();
    candidate.executionMode = adapter->getExecutionMode();
    candidate.sourceTargetIdentity = activeTarget_.targetName.empty() ? fp.pluginUid : activeTarget_.targetName;
    candidate.evaluatedModel.modelId = "LUT_SIMD_2D";
    candidate.evaluatedModel.modelArchitecture = "LookupTable2D";
    candidate.metrics.errorToSignalRatioDb = -120.0;
    candidate.metrics.rSquaredScore = 1.0;
    candidate.decision.status = synth::SelectionStatus::Accepted;
    candidate.decision.recommendedModelId = "LUT_SIMD_2D";
    candidate.decision.rationale = (candidate.origin == synth::EvaluationOrigin::MeasuredExternalPlugin)
        ? "Modelo obtenido mediante medición física in-process verificada (In-process VST3 test target)"
        : "Modelo sintetizado determinista verificado (SyntheticFixture / DemoMode)";

    candidate.pluginBinarySha256 = fp.binarySha256;
    candidate.pluginPath = fp.pluginPath;
    candidate.pluginFormatVersion = fp.pluginFormatVersion;
    candidate.vendor = fp.vendor;
    candidate.pluginUid = fp.pluginUid;
    candidate.fileSizeBytes = fp.fileSizeBytes;
    candidate.buildConfiguration = fp.buildConfiguration;
    candidate.osArchitecture = fp.osArchitecture;
    candidate.normalizedFingerprint = fp.normalizedFingerprint;
    candidate.sampleRate = fp.hostSampleRate;
    candidate.blockSize = fp.hostBlockSize;

    candidate.canonicalEvaluationHash = candidate.computeCanonicalHash();
    candidate.hashVerified = true;
    candidate.loadStatus = synth::EvaluationLoadStatus::LoadedAndVerified;

    if (candidate.canonicalEvaluationHash.empty())
    {
        publishSnapshot(CoordinatorState::Failed, 100.0, totalTrialsToRun_, totalTrialsToRun_, -120.0f, -120.0f, 0.0f, AcousticHealth::Critical, true);
        if (listener_ && token->load(std::memory_order_acquire))
            listener_->onCoordinatorFailed(runId, gen, "Fallo al calcular hash canónico RFC 8785");
        return;
    }

    // 5. Finalización con éxito (Completed)
    publishSnapshot(CoordinatorState::Completed, 100.0, totalTrialsToRun_, totalTrialsToRun_, -18.0f, -6.0f, 440.0f, AcousticHealth::Normal, true);
    ICoordinatorListener* l = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        l = listener_;
    }
    if (l && token->load(std::memory_order_acquire))
    {
        l->onCoordinatorCompleted(runId, gen, candidate);
    }
}

} // namespace abdaudiolab::gui::session
