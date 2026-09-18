/**
 * @file ComplexEnvelopeOrchestrator.cpp
 * @brief Implementation of capture orchestration, multi-mode alignment, and observable comparison.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeOrchestrator.h"
#include <algorithm>

namespace abdaudiolab::measurement
{

ComplexEnvelopeOrchestrationResult ComplexEnvelopeOrchestrator::orchestrate(
    const ComplexEnvelopeStimulus& stimulus,
    ComplexEnvelopeCaptureSession& session,
    const std::optional<AudioChainCalibration>& calibration,
    const INativeStateProvider* nativeProvider,
    std::span<const float> loopbackReferenceAudio,
    const OrchestratorConfig& config) noexcept
{
    ComplexEnvelopeOrchestrationResult result;
    result.orchestrationId = "orch_" + stimulus.stimulusId;
    result.sourceRawAudioSha256 = session.getRawCapture().getSha256();

    // 1. Apply calibration if present
    if (calibration.has_value())
    {
        session.applyCalibration(*calibration);
        result.appliedCalibration = calibration;
    }
    result.compensatedAudioSha256 = session.getCompensatedSha256();

    auto audioSpan = session.getCompensatedSamples();
    double sampleRate = session.getRawCapture().getSampleRate();

    // 2. Resolve timing reference
    TimingResolution timingRes;
    switch (config.preferredTimingType)
    {
        case TimingReferenceType::ProvidedEvent:
            timingRes = TimingReferenceResolver::resolveFromProvidedEvent(
                stimulus.expectedNoteOnSample,
                sampleRate);
            break;

        case TimingReferenceType::AudioOnset:
            timingRes = TimingReferenceResolver::resolveFromAudioOnset(
                audioSpan,
                sampleRate,
                config.timingConfig);
            break;

        case TimingReferenceType::LoopbackCorrelation:
            if (!loopbackReferenceAudio.empty())
            {
                // We correlate stimulus vs loopbackReferenceAudio
                std::vector<float> stimDummy(480, 0.5f); // representative segment if stimulus raw waveform not provided
                timingRes = TimingReferenceResolver::resolveFromLoopbackCorrelation(
                    stimDummy,
                    loopbackReferenceAudio,
                    sampleRate,
                    48000,
                    config.timingConfig);
            }
            else
            {
                timingRes.status = "not_available";
                timingRes.resolutionDetails = "No loopback reference audio provided for correlation mode";
            }
            break;

        case TimingReferenceType::ManualMarker:
        case TimingReferenceType::Unresolved:
        default:
            timingRes.status = "not_available";
            timingRes.resolutionDetails = "Unresolved or unhandled timing mode";
            break;
    }

    result.timingResolution = timingRes;

    // 3. Prepare input for core ComplexEnvelopeAnalyzer
    ComplexEnvelopeAnalysisInput input;
    input.audioBuffer.assign(audioSpan.begin(), audioSpan.end());
    input.sampleRate = sampleRate;
    input.nominalFrequencyHz = stimulus.nominalFrequencyHz;
    input.midiVelocity = stimulus.midiVelocity;
    input.stimulusSha256 = stimulus.stimulusAudioSha256;
    input.audioSha256 = result.compensatedAudioSha256;

    if (timingRes.status == "resolved" && timingRes.offsetSamples.has_value() && *timingRes.offsetSamples >= 0)
    {
        int effectiveOffset = *timingRes.offsetSamples;
        if (calibration.has_value() && calibration->compensationDomain == CompensationDomain::TimeAxisOnly)
        {
            effectiveOffset = std::max(0, effectiveOffset - calibration->roundTripLatencySamples);
        }
        input.noteOnSample = static_cast<size_t>(effectiveOffset);
        input.noteOnMethod = timingReferenceTypeToString(timingRes.type);
    }
    else
    {
        // Let analyzer use autonomous onset detection
        input.noteOnMethod = "energy_onset";
    }

    // 4. Run core analysis
    result.captureRecord = ComplexEnvelopeAnalyzer::analyze(input, config.analyzerConfig);

    // 5. Evaluate comparisons against native state provider if available
    if (nativeProvider != nullptr)
    {
        auto bindings = nativeProvider->getBindings();
        result.comparisons.reserve(bindings.size());

        for (const auto& binding : bindings)
        {
            const EnvelopeTrajectory* obsTraj = nullptr;
            switch (binding.observableDomain)
            {
                case EnvelopeDomain::Pitch:
                    obsTraj = &result.captureRecord.pitchTrajectory;
                    break;
                case EnvelopeDomain::Timbre:
                    obsTraj = &result.captureRecord.timbreTrajectory;
                    break;
                case EnvelopeDomain::Amplitude:
                    obsTraj = &result.captureRecord.amplitudeTrajectory;
                    break;
                default:
                    break;
            }

            if (obsTraj != nullptr)
            {
                auto refTraj = nativeProvider->getObservableReference(binding.nativePath);
                auto compReport = ObservableComparisonEngine::compare(
                    *obsTraj,
                    refTraj,
                    binding.nativePath,
                    config.comparisonConfig);

                result.comparisons.push_back(compReport);
            }
        }
    }
    else
    {
        // INativeStateProvider absent -> document not_compared
        ObservableComparisonReport rep;
        rep.observableDomain = "Timbre";
        rep.nativeParameterPath = "unbound.envelope";
        rep.comparisonStatus = "not_compared";
        rep.limitations = "No INativeStateProvider supplied to orchestrator.";
        result.comparisons.push_back(rep);
    }

    // Overall status determination
    if (timingRes.status == "ambiguous" || timingRes.status == "insufficient_signal")
    {
        result.status = "insufficient_alignment";
    }
    else if (audioSpan.empty())
    {
        result.status = "failed";
    }
    else
    {
        result.status = "success";
    }

    return result;
}

} // namespace abdaudiolab::measurement
