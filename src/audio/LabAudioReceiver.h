#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

namespace abdaudiolab::audio
{

enum class ReceiverState
{
    Idle,
    WaitingForTrigger,
    Recording,
    FinalizeRequested,
    Finalized,
    Finished
};

enum class CaptureResult
{
    Complete,                   ///< Ventana objetivo completa alcanzada (N >= requiredSamples).
    TimedOutWaitingForTrigger,  ///< Exclusivo de modo trigger: expiró sin superar el umbral.
    TimedOutIncomplete,         ///< Venció el plazo con N < requiredSamples (fallo explícito, vector vacío).
    Aborted,                    ///< Captura abortada (ver abortReason).
    Invalid                     ///< Error de configuración o buffers.
};

enum class CaptureAbortReason
{
    None,
    SustainedClipping,          ///< Sobrecarga por clipping sostenido (> 700 samples).
    DeviceStopped,              ///< El dispositivo de audio se detuvo.
    UserCancelled,              ///< Cancelado por la UI o el usuario.
    BufferOverrun,              ///< Desbordamiento de la FIFO circular.
    PossibleFeedbackLoop        ///< Posible bucle de retroalimentación analógica detectado en baseline (> -6 dBFS).
};

struct CaptureRequirements
{
    int requiredSamples { 0 };      ///< Total requerido (sweep + margen latencia + cola).
    int sweepSamples { 0 };         ///< Duración nominal del sweep.
    int latencyMarginSamples { 0 }; ///< Margen de latencia máxima esperable.
    int decayTailSamples { 0 };     ///< Margen de cola de decaimiento.
    bool requireTrigger { false };  ///< false: Loopback continuo; true: Modo trigger.

    [[nodiscard]] static CaptureRequirements makeLoopbackRequirements(
        double sampleRate,
        double sweepDurationSeconds = 1.0,
        double latencyMarginSeconds = 0.2,
        double decayTailSeconds = 0.1) noexcept
    {
        CaptureRequirements req;
        if (sampleRate <= 0.0 || sweepDurationSeconds <= 0.0 || latencyMarginSeconds < 0.0 || decayTailSeconds < 0.0)
            return req;

        req.sweepSamples = static_cast<int>(std::lround(sampleRate * sweepDurationSeconds));
        req.latencyMarginSamples = static_cast<int>(std::lround(sampleRate * latencyMarginSeconds));
        req.decayTailSamples = static_cast<int>(std::lround(sampleRate * decayTailSeconds));
        req.requiredSamples = req.sweepSamples + req.latencyMarginSamples + req.decayTailSamples;
        req.requireTrigger = false;
        return req;
    }

    [[nodiscard]] static int computeUiDeadlineMs(
        const CaptureRequirements& req,
        double sampleRate,
        int schedulerGraceMs = 250) noexcept
    {
        if (req.requiredSamples <= 0 || sampleRate <= 0.0)
            return 0;
        int durationMs = static_cast<int>(std::ceil((static_cast<double>(req.requiredSamples) / sampleRate) * 1000.0));
        return durationMs + std::max(0, schedulerGraceMs);
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return requiredSamples > 0 && sweepSamples > 0;
    }
};

struct CaptureStatus
{
    CaptureResult result { CaptureResult::Invalid };
    CaptureAbortReason abortReason { CaptureAbortReason::None };
    int samplesCaptured { 0 };
    int requiredSamples { 0 };
    float peakDetectedLinear { 0.0f };
    bool triggerReached { false };
};

/**
 * @brief Real-time lock-free audio capture engine.
 * 
 * Captures hardware return signals triggered by amplitude threshold
 * to eliminate system round-trip latency. Transmits data via lock-free FIFO.
 */
class LabAudioReceiver
{
public:
    LabAudioReceiver();
    ~LabAudioReceiver() = default;

    void prepare(double newSampleRate, double maxBufferSeconds = 6.0);
    void reset();

    void armCapture(int numSamplesToRecord, float triggerThresholdLinear = 0.01f);
    void armContinuousCapture(int numSamplesToRecord);

    /**
     * @brief Arms receiver for physical noise baseline measurement (300ms - 500ms).
     * Enforces immediate per-block emergency abort upon dangerous feedback (> -6 dBFS) or clipping.
     */
    void armBaselineCapture(int numSamplesToRecord);
    [[nodiscard]] bool isBaselineMode() const noexcept { return baselineMode.load(std::memory_order_relaxed); }

    /**
     * @brief Arms capture using explicit temporal requirements.
     * @return true if armed successfully; false if requirements are invalid or exceed buffer capacity.
     */
    bool armWithRequirements(const CaptureRequirements& requirements, float triggerThresholdLinear = 0.0f);

    /**
     * @brief Requests audio thread to finalize capture at next block boundary.
     */
    void requestFinalizeCapture() noexcept;

    /**
     * @brief Checks if audio thread acknowledged finalization and snapshot is frozen.
     */
    [[nodiscard]] bool isSnapshotReady() const noexcept;

    /**
     * @brief Extracts finalized snapshot. If N < requiredSamples, clears destination and reports TimedOutIncomplete.
     */
    CaptureStatus retrieveFinalizedSnapshot(std::vector<float>& destination);

    [[nodiscard]] ReceiverState getState() const noexcept { return state.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isFinished() const noexcept
    {
        auto s = state.load(std::memory_order_relaxed);
        return s == ReceiverState::Finished || s == ReceiverState::Finalized;
    }
    void forceFinish() noexcept
    {
        state.store(ReceiverState::Finalized, std::memory_order_release);
        snapshotReady.store(true, std::memory_order_release);
    }
    [[nodiscard]] int getRecordedSampleCount() const noexcept { return recordedCount.load(std::memory_order_relaxed); }
    [[nodiscard]] int getRingBufferSize() const noexcept { return ringBufferSize.load(std::memory_order_relaxed); }

    void processBlock(const float* inputBuffer, int numSamples) noexcept;

    /**
     * @brief Pulls recorded audio buffer for analysis (called from background/analysis thread).
     */
    bool retrieveRecordedData(std::vector<float>& destination);

    /**
     * @brief Performs sample-accurate alignment between a recorded signal and an ideal
     *        reference pattern (e.g. SyncPulses3 or 1kHz chirp) via spectral cross-correlation.
     * @param recordedBuffer The captured audio signal.
     * @param referencePattern The known reference stimulus pattern.
     * @param sampleRate The operating sample rate.
     * @return Sample index (offset) corresponding to exact t0 alignment peak.
     */
    static int findSampleAccurateSyncOffset(const std::vector<float>& recordedBuffer,
                                           const std::vector<float>& referencePattern,
                                           double sampleRate);

    /**
     * @brief Pulls recorded audio buffer and aligns it sample-accurately to referencePattern.
     * @param destination Output vector containing aligned audio.
     * @param referencePattern Ideal pre-roll pattern.
     * @param outSyncOffset Optional pointer to receive the detected sample delay t0.
     * @return true if data was retrieved successfully.
     */
    bool retrieveRecordedDataAligned(std::vector<float>& destination,
                                     const std::vector<float>& referencePattern,
                                     int* outSyncOffset = nullptr);

    /**
     * @brief Real-time overload guard (sustained digital clipping detector).
     */
    [[nodiscard]] bool isOverloadTriggered() const noexcept { return overloadTriggered.load(std::memory_order_acquire); }
    void resetOverloadGuard() noexcept
    {
        overloadTriggered.store(false, std::memory_order_release);
        consecutiveClippingSamples.store(0, std::memory_order_release);
        abortReason.store(CaptureAbortReason::None, std::memory_order_release);
    }
    void triggerOverloadForTesting() noexcept
    {
        overloadTriggered.store(true, std::memory_order_release);
        abortReason.store(CaptureAbortReason::SustainedClipping, std::memory_order_release);
        forceFinish();
    }

    /**
     * @brief Early stopping indicator (dynamic silence cutoff < -80 dBfs).
     */
    [[nodiscard]] bool isEarlyStopTriggered() const noexcept { return earlyStopTriggered.load(std::memory_order_acquire); }
    void setEarlyStoppingEnabled(bool enabled) noexcept { earlyStoppingEnabled.store(enabled, std::memory_order_release); }
    [[nodiscard]] bool isEarlyStoppingEnabled() const noexcept { return earlyStoppingEnabled.load(std::memory_order_acquire); }

    /**
     * @brief Configures internal plugin/algorithmic latency compensation in samples.
     * When > 0, capture starts at t=0 and the initial latency samples are trimmed upon retrieval.
     */
    void setLatencyCompensationSamples(int samples) noexcept
    {
        latencyCompensation.store(std::max(0, samples), std::memory_order_release);
    }
    [[nodiscard]] int getLatencyCompensationSamples() const noexcept
    {
        return latencyCompensation.load(std::memory_order_relaxed);
    }

private:
    double sampleRate { 96000.0 };
    std::atomic<ReceiverState> state { ReceiverState::Idle };

    std::vector<float> ringBuffer;
    std::atomic<int> ringBufferSize { 0 };

    std::atomic<int> targetSamples { 0 };
    std::atomic<int> recordedCount { 0 };
    std::atomic<float> triggerThreshold { 0.01f };

    juce::AbstractFifo fifo;

    // Safety Guard: Sustained clipping detector
    std::atomic<bool> overloadTriggered { false };
    std::atomic<int> consecutiveClippingSamples { 0 };

    // Usability Optimization: Dynamic early stopping detector
    std::atomic<bool> earlyStopTriggered { false };
    std::atomic<int> consecutiveSilenceSamples { 0 };
    std::atomic<bool> earlyStoppingEnabled { true };

    // Algorithmic / Plugin Latency Compensation (lookahead, oversampling, linear phase)
    std::atomic<int> latencyCompensation { 0 };

    // Baseline Mode with Immediate Feedback Abort
    std::atomic<bool> baselineMode { false };

    // R5-AUDIO2 Lifecycle and Handshake
    std::atomic<bool> finalizeRequested { false };
    std::atomic<bool> snapshotReady { false };
    std::atomic<CaptureAbortReason> abortReason { CaptureAbortReason::None };
    CaptureRequirements activeRequirements_;
};

} // namespace abdaudiolab::audio
