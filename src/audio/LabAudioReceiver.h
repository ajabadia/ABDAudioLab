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

private:
    double sampleRate { 96000.0 };
    std::atomic<ReceiverState> state { ReceiverState::Idle };

    std::vector<float> ringBuffer;
    std::atomic<int> ringBufferSize { 0 };

    std::atomic<int> targetSamples { 0 };
    std::atomic<int> recordedCount { 0 };
    std::atomic<float> triggerThreshold { 0.01f };

    juce::AbstractFifo fifo;
};

} // namespace abdaudiolab::audio
