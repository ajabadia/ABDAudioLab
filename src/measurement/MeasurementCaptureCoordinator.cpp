/**
 * @file MeasurementCaptureCoordinator.cpp
 * @brief Implementation of MeasurementCaptureCoordinator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementCaptureCoordinator.h"
#include "../synth/Sha256.h"
#include <cmath>
#include <algorithm>

#include "../synth/ISynthTarget.h"

namespace abdaudiolab::measurement
{

CaptureResult MeasurementCaptureCoordinator::captureSynchronous(synth::ISynthTarget* target,
                                                               const MeasurementSpec& spec,
                                                               const synth::SynthPresetState* state)
{
    CaptureResult result;
    result.measurementDomain = "synthesizedSpectralResponse";
    result.sampleRateHz = spec.execution.sampleRateHz;

    // 1. Guard against null or uninitialized target
    if (target == nullptr)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "target_null";
        return result;
    }

    // 2. Load preset state if specified
    if (state != nullptr)
    {
        if (!target->loadState(*state))
        {
            result.status = MeasurementStatus::failed;
            result.reason = "state_load_failed";
            return result;
        }

        // Compute canonical SHA-256 for preset state
        if (!state->stateHash.empty())
        {
            result.presetStateSha256 = state->stateHash;
        }
        else
        {
            synth::Sha256 hasher;
            hasher.update("PRESET_STATE_V1");
            hasher.update(state->presetName);
            for (const auto& param : state->normalizedParameters)
            {
                hasher.update(param.id);
                hasher.update(&param.value, sizeof(param.value));
            }
            result.presetStateSha256 = hasher.finalHex();
        }
    }
    else
    {
        result.presetStateSha256 = "PRESET_DEFAULT_UNSPECIFIED";
    }

    // 3. Prepare target with execution specifications
    synth::ProcessingSpec procSpec;
    procSpec.sampleRate = spec.execution.sampleRateHz;
    procSpec.blockSize = spec.execution.blockSize > 0 ? spec.execution.blockSize : 512;
    procSpec.numChannels = spec.execution.numChannels > 0 ? spec.execution.numChannels : 2;
    target->prepare(procSpec);

    // 4. Generate deterministic stimulus
    synth::MidiExcitationSequence seq = MeasurementStimulusCoordinator::generateMidiStimulus(
        spec.stimulus, spec.execution.sampleRateHz);
    result.stimulusSha256 = seq.sequenceHash;

    size_t expectedSamples = static_cast<size_t>(std::lround(seq.totalDurationSec * spec.execution.sampleRateHz));
    if (expectedSamples == 0)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "invalid_sequence_duration";
        return result;
    }

    // 5. Execute synchronous render into preallocated destination buffer
    std::vector<float> destinationAudio(expectedSamples, 0.0f);
    target->render(seq, destinationAudio);

    // 6. Verify capture completion
    if (destinationAudio.empty())
    {
        result.status = MeasurementStatus::failed;
        result.reason = "capture_incomplete";
        return result;
    }

    // 7. Inspect audio integrity (reject NaN / Inf)
    float maxAmp = 0.0f;
    for (size_t i = 0; i < destinationAudio.size(); ++i)
    {
        float s = destinationAudio[i];
        if (std::isnan(s) || std::isinf(s))
        {
            result.status = MeasurementStatus::invalid;
            result.reason = "non_finite_audio_samples";
            result.capturedAudio.clear(); // Zero leakage of corrupt samples
            return result;
        }
        maxAmp = std::max(maxAmp, std::abs(s));
    }

    // 8. Detect silent or unobservable flat signal
    if (maxAmp < 1e-5f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "silent_or_flat_signal";
        result.capturedAudio = std::move(destinationAudio);
        result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
        result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
        result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));
        return result;
    }

    // 9. Successfully completed capture
    result.status = MeasurementStatus::completed;
    result.reason = "capture_completed";
    result.capturedAudio = std::move(destinationAudio);
    result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
    result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
    result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));

    return result;
}

CaptureResult MeasurementCaptureCoordinator::captureAudioThrough(juce::AudioProcessor* processor,
                                                                 const MeasurementSpec& spec,
                                                                 int blockSize)
{
    CaptureResult result;
    result.measurementDomain = "directTransferFunction";
    result.sampleRateHz = spec.execution.sampleRateHz > 0.0 ? spec.execution.sampleRateHz : 48000.0;

    // 1. Guard against null processor
    if (processor == nullptr)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "target_null";
        return result;
    }

    // 2. Determine and validate block size
    int effectiveBlockSize = blockSize > 0 ? blockSize : 512;
    if (spec.execution.blockSize > 0)
        effectiveBlockSize = spec.execution.blockSize;

    // 3. Prepare processor with error isolation
    try
    {
        processor->setRateAndBufferSizeDetails(result.sampleRateHz, effectiveBlockSize);
        processor->prepareToPlay(result.sampleRateHz, effectiveBlockSize);
    }
    catch (const std::exception& e)
    {
        result.status = MeasurementStatus::failed;
        result.reason = std::string("prepare_failed: ") + e.what();
        return result;
    }
    catch (...)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "prepare_failed_unknown_exception";
        return result;
    }

    // 4. Generate deterministic audio stimulus
    std::vector<float> stimulus = MeasurementStimulusCoordinator::generateAudioStimulus(
        spec.stimulus, result.sampleRateHz);

    if (stimulus.empty())
    {
        processor->releaseResources();
        result.status = MeasurementStatus::failed;
        result.reason = "empty_stimulus";
        return result;
    }

    result.stimulusSha256 = MeasurementStimulusCoordinator::computeAudioHash(stimulus);
    result.stimulusAudio = stimulus; // Conserve input reference

    size_t totalSamples = stimulus.size();
    std::vector<float> destinationAudio(totalSamples, 0.0f);

    int numChannels = std::max(1, std::min(2, processor->getTotalNumInputChannels()));
    int numOutChannels = std::max(1, std::min(2, processor->getTotalNumOutputChannels()));
    int maxChannels = std::max(numChannels, numOutChannels);

    // 5. Execute synchronous processBlock loop by blocks (including variable / partial blocks)
    juce::AudioBuffer<float> blockBuf(maxChannels, effectiveBlockSize);
    juce::MidiBuffer midiBuf;

    size_t sampleOffset = 0;
    while (sampleOffset < totalSamples)
    {
        size_t samplesRemaining = totalSamples - sampleOffset;
        int currentBlockSize = static_cast<int>(std::min(static_cast<size_t>(effectiveBlockSize), samplesRemaining));

        if (currentBlockSize <= 0)
            break;

        blockBuf.clear();

        // Copy input sweep into all input channels
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = blockBuf.getWritePointer(ch);
            for (int s = 0; s < currentBlockSize; ++s)
            {
                channelData[s] = stimulus[sampleOffset + s];
            }
        }

        // Process block
        midiBuf.clear();
        try
        {
            processor->processBlock(blockBuf, midiBuf);
        }
        catch (const std::exception& e)
        {
            processor->releaseResources();
            result.status = MeasurementStatus::failed;
            result.reason = std::string("process_block_failed: ") + e.what();
            return result;
        }
        catch (...)
        {
            processor->releaseResources();
            result.status = MeasurementStatus::failed;
            result.reason = "process_block_failed_unknown_exception";
            return result;
        }

        // Retrieve output from first channel (or average if stereo)
        const float* outL = blockBuf.getReadPointer(0);
        const float* outR = (numOutChannels > 1) ? blockBuf.getReadPointer(1) : nullptr;

        for (int s = 0; s < currentBlockSize; ++s)
        {
            float val = outL[s];
            if (outR != nullptr)
                val = 0.5f * (val + outR[s]);
            destinationAudio[sampleOffset + s] = val;
        }

        sampleOffset += static_cast<size_t>(currentBlockSize);
    }

    result.latencySamples = processor->getLatencySamples();
    processor->releaseResources();

    // 6. Inspect audio integrity (reject NaN / Inf)
    float maxAmp = 0.0f;
    for (size_t i = 0; i < destinationAudio.size(); ++i)
    {
        float s = destinationAudio[i];
        if (std::isnan(s) || std::isinf(s))
        {
            result.status = MeasurementStatus::invalid;
            result.reason = "non_finite_audio_samples";
            result.capturedAudio.clear(); // Zero leakage of corrupt samples
            return result;
        }
        maxAmp = std::max(maxAmp, std::abs(s));
    }

    // 7. Detect silent or unobservable flat signal
    if (maxAmp < 1e-5f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "silent_or_flat_signal";
        result.capturedAudio = std::move(destinationAudio);
        result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
        result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
        return result;
    }

    // 8. Successfully completed capture
    result.status = MeasurementStatus::completed;
    result.reason = "capture_completed";
    result.capturedAudio = std::move(destinationAudio);
    result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
    result.numSamples = static_cast<int64_t>(result.capturedAudio.size());

    return result;
}

} // namespace abdaudiolab::measurement
