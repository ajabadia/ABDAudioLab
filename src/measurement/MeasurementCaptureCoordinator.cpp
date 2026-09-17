/**
 * @file MeasurementCaptureCoordinator.cpp
 * @brief Implementation of MeasurementCaptureCoordinator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementCaptureCoordinator.h"
#include "../synth/Sha256.h"
#include "../synth/ExternalPluginFixture.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <ctime>

#include "../synth/ISynthTarget.h"

namespace abdaudiolab::measurement
{

namespace
{
std::string getIso8601UtcNow()
{
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &itt);
#else
    gmtime_r(&itt, &tmUtc);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    return std::string(buf);
}
} // namespace

std::string CaptureArtifactMetadata::toJsonString(int indent) const
{
    nlohmann::ordered_json j;
    j["sampleRateHz"] = sampleRateHz;
    j["bufferSize"] = bufferSize;
    j["numChannels"] = numChannels;
    j["effectiveLatencySamples"] = effectiveLatencySamples;
    j["hardwareClockDriftPpm"] = hardwareClockDriftPpm;
    j["underruns"] = underruns;
    j["overruns"] = overruns;
    j["sha256Audio"] = sha256Audio;
    j["sha256Midi"] = sha256Midi;
    j["sha256SysEx"] = sha256SysEx;
    j["sha256State"] = sha256State;
    j["wallClockStartIso"] = wallClockStartIso;
    j["wallClockEndIso"] = wallClockEndIso;
    return (indent >= 0) ? j.dump(indent) : j.dump();
}

CaptureResult MeasurementCaptureCoordinator::captureSynchronousWithSysEx(
    synth::ISynthTarget* target,
    const MeasurementSpec& spec,
    const synth::SysExArtifact& sysEx)
{
    synth::SynthPresetState state;
    state.presetName = "SysEx_" + (sysEx.sha256.size() >= 8 ? sysEx.sha256.substr(0, 8) : "artifact");
    state.rawSysEx = sysEx.bytes;
    state.stateHash = sysEx.sha256;
    state.stateStatus = (sysEx.semanticStatus == "valid") 
                            ? synth::StateAppliedStatus::Passed 
                            : synth::StateAppliedStatus::Failed;

    return captureSynchronous(target, spec, &state);
}

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

    std::string startIso = getIso8601UtcNow();

    // 2. Orden estricto (Fase 20.11 T3.2) Paso 1: Prepare target with execution specifications
    synth::ProcessingSpec procSpec;
    procSpec.sampleRate = spec.execution.sampleRateHz;
    procSpec.blockSize = spec.execution.blockSize > 0 ? spec.execution.blockSize : 512;
    procSpec.numChannels = spec.execution.numChannels > 0 ? spec.execution.numChannels : 2;
    target->prepare(procSpec);

    // 3. Orden estricto Paso 2: Reset state (fases, envolventes, buffers internos)
    target->resetState();

    // 4. Orden estricto Paso 3 & 4: Load and verify preset state / SysEx
    std::string sysexHash;
    if (state != nullptr)
    {
        if (!target->loadState(*state))
        {
            result.status = MeasurementStatus::failed;
            result.reason = "state_load_failed";
            return result;
        }

        if (target->verifyState() != synth::StateAppliedStatus::Passed)
        {
            result.status = MeasurementStatus::failed;
            result.reason = "state_verification_failed";
            return result;
        }

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
            if (!state->rawSysEx.empty())
            {
                hasher.update("RAW_SYSEX");
                hasher.update(state->rawSysEx.data(), state->rawSysEx.size());
            }
            result.presetStateSha256 = hasher.finalHex();
        }

        if (!state->rawSysEx.empty())
        {
            synth::Sha256 sysHasher;
            sysHasher.update(state->rawSysEx.data(), state->rawSysEx.size());
            sysexHash = sysHasher.finalHex();
        }
    }
    else
    {
        result.presetStateSha256 = "PRESET_DEFAULT_UNSPECIFIED";
    }

    // 5. Generate deterministic stimulus
    synth::MidiExcitationSequence seq = MeasurementStimulusCoordinator::generateMidiStimulus(
        spec.stimulus, spec.execution.sampleRateHz);
    result.stimulusSha256 = !seq.canonicalSha256.empty() ? seq.canonicalSha256 : seq.sequenceHash;

    size_t expectedSamples = static_cast<size_t>(std::lround(seq.totalDurationSec * spec.execution.sampleRateHz));
    if (expectedSamples == 0)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "invalid_sequence_duration";
        return result;
    }

    // 6. Execute synchronous render into preallocated destination buffer
    std::vector<float> destinationAudio(expectedSamples, 0.0f);
    target->render(seq, destinationAudio);

    std::string endIso = getIso8601UtcNow();

    // 7. Verify capture completion
    if (destinationAudio.empty())
    {
        result.status = MeasurementStatus::failed;
        result.reason = "capture_incomplete";
        return result;
    }

    // 8. Inspect audio integrity (reject NaN / Inf)
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

    // 9. Inspect telemetría de render si target es ExternalPluginFixture
    int32_t underruns = 0;
    int32_t overruns = 0;
    if (auto* extFixture = dynamic_cast<synth::ExternalPluginFixture*>(target))
    {
        const auto& tele = extFixture->getLastRenderTelemetry();
        underruns = static_cast<int32_t>(tele.underruns);
        overruns = static_cast<int32_t>(tele.overruns);
    }

    // 10. Detect silent or unobservable flat signal
    if (maxAmp < 1e-5f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "silent_or_flat_signal";
        result.capturedAudio = std::move(destinationAudio);
        result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
        result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
        result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));
        
        // Poblado de telemetría FAIR
        result.telemetryMetadata.sampleRateHz = spec.execution.sampleRateHz;
        result.telemetryMetadata.bufferSize = procSpec.blockSize;
        result.telemetryMetadata.numChannels = procSpec.numChannels;
        result.telemetryMetadata.effectiveLatencySamples = target->timingInfo().declaredLatencySamples;
        result.telemetryMetadata.hardwareClockDriftPpm = 0.0;
        result.telemetryMetadata.underruns = underruns;
        result.telemetryMetadata.overruns = overruns;
        result.telemetryMetadata.sha256Audio = result.capturedAudioSha256;
        result.telemetryMetadata.sha256Midi = result.stimulusSha256;
        result.telemetryMetadata.sha256SysEx = sysexHash;
        result.telemetryMetadata.sha256State = result.presetStateSha256;
        result.telemetryMetadata.wallClockStartIso = startIso;
        result.telemetryMetadata.wallClockEndIso = endIso;

        return result;
    }

    // 11. Successfully completed capture
    result.status = MeasurementStatus::completed;
    result.reason = "capture_completed";
    result.capturedAudio = std::move(destinationAudio);
    result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
    result.numSamples = static_cast<int64_t>(result.capturedAudio.size());
    result.latencySamples = static_cast<int>(std::lround(target->timingInfo().declaredLatencySamples));

    // Poblado de telemetría FAIR (Fase 20.11 T3.3)
    result.telemetryMetadata.sampleRateHz = spec.execution.sampleRateHz;
    result.telemetryMetadata.bufferSize = procSpec.blockSize;
    result.telemetryMetadata.numChannels = procSpec.numChannels;
    result.telemetryMetadata.effectiveLatencySamples = target->timingInfo().declaredLatencySamples;
    result.telemetryMetadata.hardwareClockDriftPpm = 0.0;
    result.telemetryMetadata.underruns = underruns;
    result.telemetryMetadata.overruns = overruns;
    result.telemetryMetadata.sha256Audio = result.capturedAudioSha256;
    result.telemetryMetadata.sha256Midi = result.stimulusSha256;
    result.telemetryMetadata.sha256SysEx = sysexHash;
    result.telemetryMetadata.sha256State = result.presetStateSha256;
    result.telemetryMetadata.wallClockStartIso = startIso;
    result.telemetryMetadata.wallClockEndIso = endIso;

    return result;
}

CaptureResult MeasurementCaptureCoordinator::captureAudioThrough(juce::AudioProcessor* processor,
                                                                 const MeasurementSpec& spec,
                                                                 int blockSize)
{
    CaptureResult result;
    result.measurementDomain = "directTransferFunction";
    result.sampleRateHz = spec.execution.sampleRateHz > 0.0 ? spec.execution.sampleRateHz : 48000.0;

    std::string startIso = getIso8601UtcNow();

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

    std::string endIso = getIso8601UtcNow();

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

        result.telemetryMetadata.sampleRateHz = result.sampleRateHz;
        result.telemetryMetadata.bufferSize = effectiveBlockSize;
        result.telemetryMetadata.numChannels = numChannels;
        result.telemetryMetadata.effectiveLatencySamples = static_cast<double>(result.latencySamples);
        result.telemetryMetadata.hardwareClockDriftPpm = 0.0;
        result.telemetryMetadata.underruns = 0;
        result.telemetryMetadata.overruns = 0;
        result.telemetryMetadata.sha256Audio = result.capturedAudioSha256;
        result.telemetryMetadata.sha256Midi = "";
        result.telemetryMetadata.sha256SysEx = "";
        result.telemetryMetadata.sha256State = "";
        result.telemetryMetadata.wallClockStartIso = startIso;
        result.telemetryMetadata.wallClockEndIso = endIso;

        return result;
    }

    // 8. Successfully completed capture
    result.status = MeasurementStatus::completed;
    result.reason = "capture_completed";
    result.capturedAudio = std::move(destinationAudio);
    result.capturedAudioSha256 = MeasurementStimulusCoordinator::computeAudioHash(result.capturedAudio);
    result.numSamples = static_cast<int64_t>(result.capturedAudio.size());

    result.telemetryMetadata.sampleRateHz = result.sampleRateHz;
    result.telemetryMetadata.bufferSize = effectiveBlockSize;
    result.telemetryMetadata.numChannels = numChannels;
    result.telemetryMetadata.effectiveLatencySamples = static_cast<double>(result.latencySamples);
    result.telemetryMetadata.hardwareClockDriftPpm = 0.0;
    result.telemetryMetadata.underruns = 0;
    result.telemetryMetadata.overruns = 0;
    result.telemetryMetadata.sha256Audio = result.capturedAudioSha256;
    result.telemetryMetadata.sha256Midi = "";
    result.telemetryMetadata.sha256SysEx = "";
    result.telemetryMetadata.sha256State = "";
    result.telemetryMetadata.wallClockStartIso = startIso;
    result.telemetryMetadata.wallClockEndIso = endIso;

    return result;
}

} // namespace abdaudiolab::measurement
