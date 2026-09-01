#include "LabAudioReceiver.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::audio
{

LabAudioReceiver::LabAudioReceiver()
    : fifo(1)
{
    prepare(96000.0, 6.0);
}

void LabAudioReceiver::prepare(double newSampleRate, double maxBufferSeconds)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 96000.0;
    ringBufferSize = static_cast<int>(std::ceil(sampleRate * maxBufferSeconds)) + 4096;
    
    ringBuffer.assign(static_cast<size_t>(ringBufferSize), 0.0f);
    fifo.setTotalSize(ringBufferSize);
    
    reset();
}

void LabAudioReceiver::reset()
{
    state.store(ReceiverState::Idle, std::memory_order_relaxed);
    fifo.reset();
    recordedCount.store(0, std::memory_order_relaxed);
    targetSamples.store(0, std::memory_order_relaxed);
}

void LabAudioReceiver::armCapture(int numSamplesToRecord, float triggerThresholdLinear)
{
    reset();
    targetSamples.store(std::min(numSamplesToRecord, ringBufferSize - 1024), std::memory_order_relaxed);
    triggerThreshold = triggerThresholdLinear;
    state.store(ReceiverState::WaitingForTrigger, std::memory_order_release);
}

void LabAudioReceiver::armContinuousCapture(int numSamplesToRecord)
{
    reset();
    targetSamples.store(std::min(numSamplesToRecord, ringBufferSize - 1024), std::memory_order_relaxed);
    triggerThreshold = 0.0f; // Start immediately
    state.store(ReceiverState::Recording, std::memory_order_release);
}

void LabAudioReceiver::processBlock(const float* inputBuffer, int numSamples) noexcept
{
    auto currentState = state.load(std::memory_order_relaxed);
    if (currentState == ReceiverState::Idle || currentState == ReceiverState::Finished || inputBuffer == nullptr)
        return;

    int sampleOffset = 0;

    if (currentState == ReceiverState::WaitingForTrigger)
    {
        // Search for first sample above threshold
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::abs(inputBuffer[i]) >= triggerThreshold)
            {
                sampleOffset = i;
                currentState = ReceiverState::Recording;
                state.store(ReceiverState::Recording, std::memory_order_release);
                break;
            }
        }

        if (currentState == ReceiverState::WaitingForTrigger)
            return; // Still waiting
    }

    if (currentState == ReceiverState::Recording)
    {
        int remainingToRecord = targetSamples.load(std::memory_order_relaxed) - recordedCount.load(std::memory_order_relaxed);
        int availableSamples = numSamples - sampleOffset;
        int samplesToWrite = std::min(availableSamples, remainingToRecord);

        if (samplesToWrite > 0)
        {
            int start1, size1, start2, size2;
            fifo.prepareToWrite(samplesToWrite, start1, size1, start2, size2);

            if (size1 > 0)
            {
                std::copy_n(inputBuffer + sampleOffset, size1, ringBuffer.data() + start1);
            }
            if (size2 > 0)
            {
                std::copy_n(inputBuffer + sampleOffset + size1, size2, ringBuffer.data() + start2);
            }

            fifo.finishedWrite(size1 + size2);
            recordedCount.fetch_add(size1 + size2, std::memory_order_relaxed);
        }

        if (recordedCount.load(std::memory_order_relaxed) >= targetSamples.load(std::memory_order_relaxed))
        {
            state.store(ReceiverState::Finished, std::memory_order_release);
        }
    }
}

bool LabAudioReceiver::retrieveRecordedData(std::vector<float>& destination)
{
    if (state.load(std::memory_order_acquire) != ReceiverState::Finished)
        return false;

    int totalToRead = recordedCount.load(std::memory_order_relaxed);
    destination.resize(static_cast<size_t>(totalToRead));

    int start1, size1, start2, size2;
    fifo.prepareToRead(totalToRead, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::copy_n(ringBuffer.data() + start1, size1, destination.data());
    }
    if (size2 > 0)
    {
        std::copy_n(ringBuffer.data() + start2, size2, destination.data() + size1);
    }

    fifo.finishedRead(size1 + size2);
    return true;
}

} // namespace abdaudiolab::audio
