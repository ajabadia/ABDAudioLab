#include "LabAudioReceiver.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>
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
    int newSize = static_cast<int>(std::ceil(sampleRate * maxBufferSeconds)) + 4096;
    
    ringBuffer.assign(static_cast<size_t>(newSize), 0.0f);
    fifo.setTotalSize(newSize);
    ringBufferSize.store(newSize, std::memory_order_release);
    
    reset();
}

void LabAudioReceiver::reset()
{
    state.store(ReceiverState::Idle, std::memory_order_relaxed);
    fifo.reset();
    recordedCount.store(0, std::memory_order_relaxed);
    targetSamples.store(0, std::memory_order_relaxed);
    resetOverloadGuard();
    earlyStopTriggered.store(false, std::memory_order_release);
    consecutiveSilenceSamples.store(0, std::memory_order_relaxed);
}

void LabAudioReceiver::armCapture(int numSamplesToRecord, float triggerThresholdLinear)
{
    reset();
    int bufSize = ringBufferSize.load(std::memory_order_acquire);
    targetSamples.store(std::min(numSamplesToRecord, bufSize - 1024), std::memory_order_relaxed);
    triggerThreshold.store(triggerThresholdLinear, std::memory_order_relaxed);
    state.store(ReceiverState::WaitingForTrigger, std::memory_order_release);
}

void LabAudioReceiver::armContinuousCapture(int numSamplesToRecord)
{
    reset();
    int bufSize = ringBufferSize.load(std::memory_order_acquire);
    targetSamples.store(std::min(numSamplesToRecord, bufSize - 1024), std::memory_order_relaxed);
    triggerThreshold.store(0.0f, std::memory_order_relaxed); // Start immediately
    state.store(ReceiverState::Recording, std::memory_order_release);
}

void LabAudioReceiver::processBlock(const float* inputBuffer, int numSamples) noexcept
{
    if (overloadTriggered.load(std::memory_order_acquire))
        return;

    if (inputBuffer == nullptr || numSamples <= 0)
        return;

    // 1. Safety Guard: Sustained clipping detector (auto-abort on feedback/overload)
    const float clippingThreshold = 0.99f; // -0.1 dBfs approx
    int localClipCounter = consecutiveClippingSamples.load(std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        if (std::abs(inputBuffer[i]) >= clippingThreshold)
        {
            localClipCounter++;
            if (localClipCounter > 700) // >700 samples (~15ms @ 48kHz, ~7.3ms @ 96kHz)
            {
                overloadTriggered.store(true, std::memory_order_release);
                forceFinish();
                break;
            }
        }
        else
        {
            if (localClipCounter > 0)
                localClipCounter--;
        }
    }
    consecutiveClippingSamples.store(localClipCounter, std::memory_order_relaxed);

    if (overloadTriggered.load(std::memory_order_relaxed))
        return;

    auto currentState = state.load(std::memory_order_acquire);
    if (currentState == ReceiverState::Idle || currentState == ReceiverState::Finished)
        return;

    int sampleOffset = 0;

    if (currentState == ReceiverState::WaitingForTrigger)
    {
        float threshold = triggerThreshold.load(std::memory_order_relaxed);
        // Search for first sample above threshold
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::abs(inputBuffer[i]) >= threshold)
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

        // 2. Usability: Dynamic Early Stopping (Parada Temprana por Silencio < -80 dBfs)
        if (earlyStoppingEnabled.load(std::memory_order_relaxed) &&
            recordedCount.load(std::memory_order_relaxed) > static_cast<int>(0.25 * sampleRate)) // 250ms grace period
        {
            float maxBlockAmp = 0.0f;
            for (int i = sampleOffset; i < numSamples; ++i)
            {
                maxBlockAmp = std::max(maxBlockAmp, std::abs(inputBuffer[i]));
            }

            constexpr float silenceThresholdLinear = 0.0001f; // -80 dBfs
            if (maxBlockAmp < silenceThresholdLinear)
            {
                int silenceSamples = consecutiveSilenceSamples.load(std::memory_order_relaxed) + (numSamples - sampleOffset);
                if (silenceSamples > static_cast<int>(sampleRate * 0.10)) // >100ms sustained silence
                {
                    earlyStopTriggered.store(true, std::memory_order_release);
                    forceFinish();
                    return;
                }
                consecutiveSilenceSamples.store(silenceSamples, std::memory_order_relaxed);
            }
            else
            {
                consecutiveSilenceSamples.store(0, std::memory_order_relaxed);
            }
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

int LabAudioReceiver::findSampleAccurateSyncOffset(const std::vector<float>& recordedBuffer,
                                                  const std::vector<float>& referencePattern,
                                                  double sampleRate)
{
    if (recordedBuffer.empty() || referencePattern.empty() || sampleRate <= 0.0)
        return 0;

    const size_t nRef = referencePattern.size();

    // Limit cross-correlation search window to avoid giant FFTs on long multi-second recordings.
    // 1.0 second of audio at sampleRate is plenty to find pre-roll (300ms) with round-trip latency.
    const size_t maxSearchSamples = static_cast<size_t>(std::lround(sampleRate * 1.0));
    const size_t nRec = std::min(recordedBuffer.size(), std::max(nRef + 1024, maxSearchSamples));

    const size_t convLen = nRec + nRef - 1;

    // Dynamic power of 2 for FFT
    int fftOrder = 1;
    while ((1ULL << fftOrder) < convLen)
        fftOrder++;

    const size_t fftSize = 1ULL << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    // Buffers for zero-padded forward transforms
    std::vector<std::complex<float>> inRec(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> inRef(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> specRec(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> specRef(fftSize, { 0.0f, 0.0f });

    for (size_t i = 0; i < nRec; ++i)
        inRec[i] = { recordedBuffer[i], 0.0f };

    for (size_t i = 0; i < nRef; ++i)
        inRef[i] = { referencePattern[i], 0.0f };

    fft.perform(inRec.data(), specRec.data(), false);
    fft.perform(inRef.data(), specRef.data(), false);

    // Spectral cross-correlation: R[k] = Rec[k] * conj(Ref[k])
    std::vector<std::complex<float>> mult(fftSize, { 0.0f, 0.0f });
    for (size_t k = 0; k < fftSize; ++k)
    {
        mult[k] = specRec[k] * std::conj(specRef[k]);
    }

    // Inverse transform to obtain temporal cross-correlation
    std::vector<std::complex<float>> timeCorr(fftSize, { 0.0f, 0.0f });
    fft.perform(mult.data(), timeCorr.data(), true);

    // Locate peak within the valid search region.
    // Compare squared real magnitude (r * r) to avoid expensive sqrt in inner loop.
    float maxEnergy = -1.0f;
    size_t peakOffset = 0;

    const size_t searchLimit = (nRec > nRef / 2) ? (nRec - nRef / 2) : nRec;
    for (size_t i = 0; i < searchLimit; ++i)
    {
        float realPart = timeCorr[i].real();
        float energy = realPart * realPart;
        if (energy > maxEnergy)
        {
            maxEnergy = energy;
            peakOffset = i;
        }
    }

    return static_cast<int>(peakOffset);
}

bool LabAudioReceiver::retrieveRecordedDataAligned(std::vector<float>& destination,
                                                   const std::vector<float>& referencePattern,
                                                   int* outSyncOffset)
{
    std::vector<float> raw;
    if (!retrieveRecordedData(raw))
        return false;

    if (raw.empty() || referencePattern.empty())
    {
        destination = std::move(raw);
        if (outSyncOffset != nullptr)
            *outSyncOffset = 0;
        return true;
    }

    int syncOffset = findSampleAccurateSyncOffset(raw, referencePattern, sampleRate);
    if (outSyncOffset != nullptr)
        *outSyncOffset = syncOffset;

    if (syncOffset > 0 && static_cast<size_t>(syncOffset) < raw.size())
    {
        destination.assign(raw.begin() + syncOffset, raw.end());
    }
    else
    {
        destination = std::move(raw);
    }

    return true;
}

} // namespace abdaudiolab::audio
