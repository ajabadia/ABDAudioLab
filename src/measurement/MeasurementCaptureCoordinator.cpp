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

namespace abdaudiolab::measurement
{

CaptureResult MeasurementCaptureCoordinator::captureSynchronous(synth::ISynthTarget* target,
                                                               const MeasurementSpec& spec,
                                                               const synth::SynthPresetState* state)
{
    CaptureResult result;
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
        result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
        result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));
        return result;
    }

    // 9. Successfully completed capture
    result.status = MeasurementStatus::completed;
    result.reason = "capture_completed";
    result.capturedAudio = std::move(destinationAudio);
    result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
    result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));

    return result;
}

} // namespace abdaudiolab::measurement
