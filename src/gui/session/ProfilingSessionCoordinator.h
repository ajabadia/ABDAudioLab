#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include "ProfilingSessionContracts.h"
#include "synth/SyntheticSynthFixture.h"
#include "synth/SyntheticSynthTarget.h"
#include "synth/ISynthTargetLifecycleAdapter.h"
#include "synth/SynthTargetLifecycleAdapters.h"
#include "synth/ModelEvaluationTypes.h"
#include "synth/ModelEvaluationBuilder.h"

namespace abdaudiolab::gui::session
{

enum class CoordinatorState
{
    Idle,
    Preparing,
    Running,
    Paused,
    Cancelling,
    Completed,
    Cancelled,
    Failed
};

[[nodiscard]] inline std::string coordinatorStateToString(CoordinatorState state)
{
    switch (state)
    {
        case CoordinatorState::Idle:       return "Idle";
        case CoordinatorState::Preparing:  return "Preparing";
        case CoordinatorState::Running:    return "Running";
        case CoordinatorState::Paused:     return "Paused";
        case CoordinatorState::Cancelling: return "Cancelling";
        case CoordinatorState::Completed:  return "Completed";
        case CoordinatorState::Cancelled:  return "Cancelled";
        case CoordinatorState::Failed:     return "Failed";
        default:                           return "Unknown";
    }
}

enum class AcousticHealth
{
    Normal,
    Warning,
    Clipping,
    Critical
};

[[nodiscard]] inline std::string acousticHealthToString(AcousticHealth health)
{
    switch (health)
    {
        case AcousticHealth::Normal:   return "Normal (OK)";
        case AcousticHealth::Warning:  return "Warning (Nivel Bajo)";
        case AcousticHealth::Clipping: return "Clipping (Sobrecarga)";
        case AcousticHealth::Critical: return "Critical (Fallo de Audio)";
        default:                       return "Unknown";
    }
}

struct CoordinatorSnapshot
{
    CoordinatorState state { CoordinatorState::Idle };
    uint64_t sessionGeneration { 0 };
    uint64_t runId { 0 };
    double progress { 0.0 };      // Acotado a [0.0, 100.0] y monótono dentro del runId
    int currentTrial { 0 };
    int totalTrials { 0 };
    float rmsDb { -120.0f };
    float peakDb { -120.0f };
    float detectedF0Hz { 0.0f };
    AcousticHealth health { AcousticHealth::Normal };
    bool terminal { false };       // True únicamente para Completed, Cancelled, Failed
};

class ICoordinatorListener
{
public:
    virtual ~ICoordinatorListener() = default;
    virtual void onCoordinatorSnapshotUpdated(const CoordinatorSnapshot& snapshot) = 0;
    virtual void onCoordinatorCompleted(uint64_t runId, uint64_t sessionGeneration, const synth::ModelEvaluation& candidate) = 0;
    virtual void onCoordinatorCancelled(uint64_t runId, uint64_t sessionGeneration) = 0;
    virtual void onCoordinatorFailed(uint64_t runId, uint64_t sessionGeneration, const std::string& error) = 0;
};

/**
 * @brief Orquestador desacoplado en segundo plano (worker) para la ejecución del perfilado.
 * 
 * Corre en un juce::Thread independiente del MessageThread y del hilo de audio.
 * Diseñado con cancelación cooperativa idempotente y comprobación de tokens de vida / generación.
 */
class ProfilingSessionCoordinator : public juce::Thread
{
public:
    explicit ProfilingSessionCoordinator(ICoordinatorListener* listener = nullptr);
    ~ProfilingSessionCoordinator() override;

    void setListener(ICoordinatorListener* listener);

    [[nodiscard]] bool start(const TargetSelectionState& target, uint64_t sessionGeneration, int totalTrials = 20, int trialDelayMs = 40);
    void pause();
    void resume();
    void requestCancel();

    [[nodiscard]] CoordinatorSnapshot getSnapshot() const;
    [[nodiscard]] CoordinatorState getState() const;
    [[nodiscard]] uint64_t getCurrentRunId() const noexcept { return currentRunId_.load(std::memory_order_acquire); }
    [[nodiscard]] bool isRunning() const;

    void waitForWorkerToStop(int timeoutMs = 3000);

    // Permite inyectar fallo para pruebas unitarias
    void setSimulateValidationFailure(bool fail) noexcept { simulateValidationFailure_ = fail; }
    void setWatchdogBlockTimeoutMs(double ms) noexcept { watchdogBlockTimeoutMs_.store(ms, std::memory_order_release); }

private:
    void run() override;
    void publishSnapshot(CoordinatorState state, double progress, int trial, int total, float rms, float peak, float f0, AcousticHealth health, bool terminal);

    mutable std::mutex mutex_;
    ICoordinatorListener* listener_ { nullptr };

    std::atomic<uint64_t> runIdCounter_ { 0 };
    std::atomic<uint64_t> currentRunId_ { 0 };
    std::atomic<uint64_t> sessionGeneration_ { 0 };

    std::atomic<bool> cancelRequested_ { false };
    std::atomic<bool> isPaused_ { false };
    std::atomic<bool> simulateValidationFailure_ { false };
    std::atomic<double> watchdogBlockTimeoutMs_ { 500.0 };

    int totalTrialsToRun_ { 20 };
    int trialDelayMs_ { 40 };

    TargetSelectionState activeTarget_;
    CoordinatorSnapshot latestSnapshot_;

    std::shared_ptr<std::atomic<bool>> aliveToken_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfilingSessionCoordinator)
};

} // namespace abdaudiolab::gui::session
