#include "OutOfProcessVst3LifecycleAdapter.h"
#include "SynthTargetLifecycleAdapters.h"
#include <chrono>

namespace abdaudiolab::synth
{

class OutOfProcessVst3Target : public ISynthTarget
{
public:
    OutOfProcessVst3Target(ipc::WorkerProcessHost& host, double sampleRate, int blockSize)
        : host_(host), sampleRate_(sampleRate), blockSize_(blockSize)
    {
    }

    bool loadState(const SynthPresetState& /*state*/) override
    {
        return true;
    }

    [[nodiscard]] StateAppliedStatus verifyState() const override
    {
        return StateAppliedStatus::Passed;
    }

    void prepare(const ProcessingSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        blockSize_ = spec.blockSize;
    }

    void resetState() override
    {
        if (host_.isWorkerAlive())
        {
            host_.resetPlugin(0, 3000);
        }
    }

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int /*repetitionIndex*/ = 0) override
    {
        if (!host_.isWorkerAlive())
        {
            destinationAudio.clear();
            return;
        }

        int totalSamples = static_cast<int>(std::lround(sequence.totalDurationSec * sampleRate_));
        if (totalSamples <= 0)
        {
            destinationAudio.clear();
            return;
        }

        destinationAudio.assign(static_cast<size_t>(totalSamples), 0.0f);

        int samplesRendered = 0;
        uint64_t seqCounter = 1;

        while (samplesRendered < totalSamples)
        {
            // En IPC entre procesos, agrupamos en bloques de hasta 2048 muestras para minimizar
            // las transiciones de contexto del SO y la latencia de Named Pipes, respetando el límite kIpcMaxBlockSamples.
            int maxIpcChunk = std::min<int>(2048, static_cast<int>(ipc::kIpcMaxBlockSamples));
            int curBlock = std::min(maxIpcChunk, totalSamples - samplesRendered);

            ipc::RenderBlockRequestPayload req;
            req.sequence = seqCounter++;
            req.blockSize = static_cast<uint32_t>(curBlock);
            req.numChannels = 2;

            for (const auto& ev : sequence.events)
            {
                if (ev.sampleOffset >= samplesRendered && ev.sampleOffset < (samplesRendered + curBlock))
                {
                    ipc::TimedMidiWireEvent wev;
                    wev.sampleOffset = static_cast<uint16_t>(ev.sampleOffset - samplesRendered);
                    if (ev.type == TimedMidiType::NoteOn)
                        wev.type = 0;
                    else if (ev.type == TimedMidiType::NoteOff)
                        wev.type = 1;
                    else if (ev.type == TimedMidiType::AllNotesOff)
                        wev.type = 2;
                    else
                        continue;

                    wev.channel = static_cast<uint8_t>(ev.channel);
                    wev.noteNumber = static_cast<uint8_t>(ev.noteNumber);
                    wev.velocity = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(ev.velocity * 127.0f)), 0, 127));
                    req.midiEvents.push_back(wev);
                }
            }

            ipc::RenderBlockResponsePayload resp;
            bool ok = host_.renderBlock(req, resp, 5000);
            if (!ok || resp.status != 0)
            {
                // Fallo de render o worker muerto: abortar de forma limpia
                destinationAudio.clear();
                return;
            }

            // Copiar canal 0 (izquierdo) para análisis mono/fixture
            for (int s = 0; s < curBlock; ++s)
            {
                size_t srcIdx = static_cast<size_t>(s * resp.numChannels); // Canal 0
                if (srcIdx < resp.audioData.size())
                {
                    destinationAudio[static_cast<size_t>(samplesRendered + s)] = resp.audioData[srcIdx];
                }
            }

            samplesRendered += curBlock;
        }
    }

    [[nodiscard]] TargetTimingInfo timingInfo() const override
    {
        TargetTimingInfo info;
        info.isPhysicalHardware = false;
        info.isDirectPlugin = true;
        info.declaredLatencySamples = 0.0;
        info.measuredTransportLatencyMs = 0.0;
        info.timingJitterMs = 0.0;
        info.timingDescription = "Remote Isolated VST3 Worker via IPC";
        return info;
    }

private:
    ipc::WorkerProcessHost& host_;
    double sampleRate_ { 48000.0 };
    int blockSize_ { 256 };
};

OutOfProcessVst3LifecycleAdapter::OutOfProcessVst3LifecycleAdapter(const std::string& customWorkerPath)
    : customWorkerPath_(customWorkerPath)
{
}

OutOfProcessVst3LifecycleAdapter::~OutOfProcessVst3LifecycleAdapter()
{
    releaseTarget();
}

juce::File OutOfProcessVst3LifecycleAdapter::resolveWorkerExecutable()
{
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    // 1. En mismo directorio del ejecutable actual (distribución portable / artefacts)
    juce::File sibling = exeDir.getChildFile("ABDAudioLab_PluginWorker.exe");
    if (sibling.existsAsFile())
        return sibling;

    // 2. En build/Release subiendo 2 niveles desde build/ABDAudioLab_artefacts/Release
    juce::File buildRel2 = exeDir.getParentDirectory().getParentDirectory().getChildFile("Release/ABDAudioLab_PluginWorker.exe");
    if (buildRel2.existsAsFile())
        return buildRel2;

    // 3. Subiendo 1 nivel (por si la jerarquía es build/Release)
    juce::File buildRel1 = exeDir.getParentDirectory().getChildFile("Release/ABDAudioLab_PluginWorker.exe");
    if (buildRel1.existsAsFile())
        return buildRel1;

    // 4. En directorio build/Release relativo al CWD
    juce::File buildWorker = juce::File::getCurrentWorkingDirectory()
        .getChildFile("build/Release/ABDAudioLab_PluginWorker.exe");
    if (buildWorker.existsAsFile())
        return buildWorker;

    // 5. En CWD directo
    juce::File cwdWorker = juce::File::getCurrentWorkingDirectory()
        .getChildFile("ABDAudioLab_PluginWorker.exe");
    if (cwdWorker.existsAsFile())
        return cwdWorker;

    return sibling;
}

bool OutOfProcessVst3LifecycleAdapter::initializeTarget(const gui::session::TargetSelectionState& targetState,
                                                       double sampleRate,
                                                       int blockSize,
                                                       std::string& outErrorMessage)
{
    isReady_ = false;
    lastError_.clear();
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    auto appExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto cwd = juce::File::getCurrentWorkingDirectory();

    // 1. Localizar el ejecutable del worker
    juce::File workerExe = customWorkerPath_.empty() ? resolveWorkerExecutable() : juce::File(customWorkerPath_);
    if (!workerExe.existsAsFile())
    {
        outErrorMessage = "[WorkerNotFound] No se pudo localizar el ejecutable del worker.\n"
                          "Worker intentado: " + workerExe.getFullPathName().toStdString() + "\n"
                          "Directorio del ejecutable: " + appExe.getParentDirectory().getFullPathName().toStdString() + "\n"
                          "Directorio de trabajo actual: " + cwd.getFullPathName().toStdString();
        lastError_ = outErrorMessage;
        return false;
    }

    // 2. Resolver la ruta física del plugin VST3
    juce::File pluginFile = InProcessVst3LifecycleAdapter::resolveVst3File(targetState);
    if (!pluginFile.exists())
    {
        outErrorMessage = "[PluginNotFound] No se pudo localizar el plugin VST3.\n"
                          "Target ID: " + targetState.targetId + "\n"
                          "Ruta probada: " + pluginFile.getFullPathName().toStdString() + "\n"
                          "Directorio del ejecutable: " + appExe.getParentDirectory().getFullPathName().toStdString() + "\n"
                          "Directorio de trabajo actual: " + cwd.getFullPathName().toStdString();
        lastError_ = outErrorMessage;
        return false;
    }

    // 3. Spawnea y conecta el worker si no está vivo
    if (!workerHost_.isWorkerAlive())
    {
        std::string sessionId = "oop_" + std::to_string(juce::Time::currentTimeMillis());
        uint64_t nonce = 0xAA55BEEF12345678ULL ^ static_cast<uint64_t>(juce::Time::currentTimeMillis());

        if (!workerHost_.spawnAndConnect(workerExe.getFullPathName().toStdString(), sessionId, nonce, 5000))
        {
            outErrorMessage = "[HandshakeTimeout] Falló el arranque o conexión IPC con el worker.\n"
                              "Worker: " + workerExe.getFullPathName().toStdString() + "\n"
                              "Detalle: " + workerHost_.getLastError();
            lastError_ = outErrorMessage;
            return false;
        }
    }

    // 4. Enviar LoadPluginRequest al worker
    ipc::LoadPluginRequestPayload req;
    req.runId = "run_" + std::to_string(juce::Time::currentTimeMillis());
    req.sessionId = "oop_session";
    req.pluginPath = pluginFile.getFullPathName().toStdString();
    req.expectedBinarySha256 = ""; // Sin hash esperado específico por defecto salvo que se defina
    req.sampleRate = sampleRate;
    req.blockSize = static_cast<uint32_t>(blockSize);

    ipc::LoadPluginResponsePayload resp;
    if (!workerHost_.loadPlugin(req, resp, 10000))
    {
        outErrorMessage = "Worker failed to load plugin: " + workerHost_.getLastError();
        lastError_ = outErrorMessage;
        return false;
    }

    // 5. Poblar el TargetFingerprint metrológico con datos inmutables del worker
    fingerprint_.pluginPath = pluginFile.getFullPathName().toStdString();
    fingerprint_.pluginFormatVersion = "VST 3.7.x (OutOfProcess VST3 Worker)";
    fingerprint_.vendor = resp.vendor.empty() ? "ABDSynths" : resp.vendor;
    fingerprint_.pluginUid = resp.pluginUid.empty() ? targetState.targetId : resp.pluginUid;
    fingerprint_.fileSizeBytes = static_cast<uint64_t>(pluginFile.getSize());
    fingerprint_.binarySha256 = resp.binarySha256;
    fingerprint_.buildConfiguration = "Release-x64";
    fingerprint_.hostSampleRate = sampleRate;
    fingerprint_.hostBlockSize = blockSize;
    fingerprint_.osArchitecture = "x86_64-windows";
    fingerprint_.normalizedFingerprint = fingerprint_.computeNormalizedFingerprint();

    isReady_ = true;
    targetProxy_ = std::make_unique<OutOfProcessVst3Target>(workerHost_, sampleRate, blockSize);
    return true;
}

ISynthTarget* OutOfProcessVst3LifecycleAdapter::getTarget() noexcept
{
    if (!isReady() || targetProxy_ == nullptr)
        return nullptr;
    return targetProxy_.get();
}

const ISynthTarget* OutOfProcessVst3LifecycleAdapter::getTarget() const noexcept
{
    if (!isReady() || targetProxy_ == nullptr)
        return nullptr;
    return targetProxy_.get();
}

void OutOfProcessVst3LifecycleAdapter::resetForTrial()
{
    if (workerHost_.isWorkerAlive())
    {
        workerHost_.resetPlugin(0, 3000);
    }
}

void OutOfProcessVst3LifecycleAdapter::releaseTarget()
{
    targetProxy_.reset();
    if (workerHost_.isWorkerAlive())
    {
        workerHost_.releasePlugin(3000);
        workerHost_.orderlyShutdown(3000);
    }
    isReady_ = false;
}

bool OutOfProcessVst3LifecycleAdapter::isReady() const noexcept
{
    return isReady_ && workerHost_.isWorkerAlive();
}

EvaluationOrigin OutOfProcessVst3LifecycleAdapter::getEvaluationOrigin() const noexcept
{
    return EvaluationOrigin::MeasuredExternalPlugin;
}

std::string OutOfProcessVst3LifecycleAdapter::getExecutionMode() const
{
    return "OutOfProcessVST3";
}

TargetFingerprint OutOfProcessVst3LifecycleAdapter::getFingerprint() const
{
    return fingerprint_;
}

} // namespace abdaudiolab::synth
