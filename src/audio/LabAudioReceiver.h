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
    Finished
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

    [[nodiscard]] ReceiverState getState() const noexcept { return state.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isFinished() const noexcept { return state.load(std::memory_order_relaxed) == ReceiverState::Finished; }
    void forceFinish() noexcept { state.store(ReceiverState::Finished, std::memory_order_release); }
    [[nodiscard]] int getRecordedSampleCount() const noexcept { return recordedCount.load(std::memory_order_relaxed); }

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
    }

    /**
     * @brief Early stopping indicator (dynamic silence cutoff < -80 dBfs).
     */
    [[nodiscard]] bool isEarlyStopTriggered() const noexcept { return earlyStopTriggered.load(std::memory_order_acquire); }
    void setEarlyStoppingEnabled(bool enabled) noexcept { earlyStoppingEnabled.store(enabled, std::memory_order_release); }
    [[nodiscard]] bool isEarlyStoppingEnabled() const noexcept { return earlyStoppingEnabled.load(std::memory_order_acquire); }

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
};

} // namespace abdaudiolab::audio
