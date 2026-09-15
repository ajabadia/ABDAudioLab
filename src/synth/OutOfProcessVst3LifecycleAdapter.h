#pragma once

#include "ISynthTargetLifecycleAdapter.h"
#include "ipc/WorkerProcessHost.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <string>

namespace abdaudiolab::synth
{

/**
 * @class OutOfProcessVst3LifecycleAdapter
 * @brief Adaptador ISynthTargetLifecycleAdapter que ejecuta y supervisa el VST3
 *        dentro de un proceso auxiliar esclavo aislado (ABDAudioLab_PluginWorker.exe).
 *
 * Características del Slice 2:
 * 1. Spawnea y gestiona el proceso worker esclavo con canal Named Pipe y ACL exclusiva.
 * 2. Carga física del VST3 dentro del worker con verificación estricta de hash SHA-256 (BinaryChanged).
 * 3. Consulta y reporta TargetFingerprint y capacidades remotas hacia el host.
 * 4. Gestiona Reset y Release de forma idempotente.
 * 5. Protege al host principal: cualquier crash en el worker deja intacta la sesión del host.
 */
class OutOfProcessVst3LifecycleAdapter : public ISynthTargetLifecycleAdapter
{
public:
    explicit OutOfProcessVst3LifecycleAdapter(const std::string& customWorkerPath = "");
    ~OutOfProcessVst3LifecycleAdapter() override;

    bool initializeTarget(const gui::session::TargetSelectionState& targetState,
                          double sampleRate,
                          int blockSize,
                          std::string& outErrorMessage) override;

    [[nodiscard]] ISynthTarget* getTarget() noexcept override;
    [[nodiscard]] const ISynthTarget* getTarget() const noexcept override;

    void resetForTrial() override;
    void releaseTarget() override;

    [[nodiscard]] bool isReady() const noexcept override;
    [[nodiscard]] EvaluationOrigin getEvaluationOrigin() const noexcept override;
    [[nodiscard]] std::string getExecutionMode() const override;
    [[nodiscard]] TargetFingerprint getFingerprint() const override;
    [[nodiscard]] bool requiresUnblockedMessageThread() const noexcept override { return false; }

    [[nodiscard]] ipc::WorkerProcessHost& getWorkerHost() noexcept { return workerHost_; }
    [[nodiscard]] bool isWorkerAlive() const noexcept { return workerHost_.isWorkerAlive(); }

    static juce::File resolveWorkerExecutable();

private:
    std::string customWorkerPath_;
    ipc::WorkerProcessHost workerHost_;
    TargetFingerprint fingerprint_;
    std::unique_ptr<ISynthTarget> targetProxy_;

    bool isReady_ { false };
    double sampleRate_ { 48000.0 };
    int blockSize_ { 256 };
    std::string lastError_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutOfProcessVst3LifecycleAdapter)
};

} // namespace abdaudiolab::synth
