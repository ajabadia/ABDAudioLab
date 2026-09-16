/**
 * @file EnvelopeMeasurementAdapter.cpp
 * @brief Implementation of EnvelopeMeasurementAdapter.
 * @author ABDSynths
 * @date 2026
 */

#include "EnvelopeMeasurementAdapter.h"
#include "../../synth/SynthEnvelopeAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::measurement
{

MeasurementCurve EnvelopeMeasurementAdapter::extractTemporalCurve(const std::vector<float>& audioBuffer,
                                                                 double sampleRate,
                                                                 size_t hopSamples)
{
    MeasurementCurve curve;
    curve.xName = "time";
    curve.xUnit = "ms";
    curve.yName = "amplitude";
    curve.yUnit = "dBFS";

    if (audioBuffer.empty() || sampleRate <= 0.0 || hopSamples == 0)
        return curve;

    size_t numPoints = (audioBuffer.size() + hopSamples - 1) / hopSamples;
    curve.x.reserve(numPoints);
    curve.y.reserve(numPoints);

    for (size_t i = 0; i < audioBuffer.size(); i += hopSamples)
    {
        size_t blockEnd = std::min(i + hopSamples, audioBuffer.size());
        float maxVal = 0.0f;

        for (size_t k = i; k < blockEnd; ++k)
        {
            float absSample = std::abs(audioBuffer[k]);
            if (absSample > maxVal)
                maxVal = absSample;
        }

        double timeMs = (static_cast<double>(i) / sampleRate) * 1000.0;
        double dbfs = 20.0 * std::log10(std::max(1e-5f, maxVal));

        curve.x.push_back(timeMs);
        curve.y.push_back(dbfs);
    }

    return curve;
}

MeasurementResult EnvelopeMeasurementAdapter::measure(const MeasurementSpec& spec,
                                                     const std::vector<float>& audioBuffer,
                                                     double sampleRate,
                                                     size_t noteOnSample,
                                                     size_t noteOffSample,
                                                     size_t detectedOnsetSample,
                                                     const std::string& audioArtifactPath,
                                                     const std::string& audioSha256)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "envelope";

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "Target";
    result.dut.format = "VST3";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.execution.sampleRateHz = sampleRate;
    result.execution.numSamples = static_cast<int64_t>(audioBuffer.size());
    result.stimulus = spec.stimulus;

    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;

    result.artifacts.audioPath = audioArtifactPath;
    result.artifacts.audioSha256 = audioSha256;
    result.integrityVerified = !audioSha256.empty();

    // 1. Guard against empty buffer
    if (audioBuffer.empty() || sampleRate <= 0.0)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "empty_audio_buffer";
        result.observability.status = "not_observable";
        result.observability.reason = "Audio buffer contains no samples or sample rate is invalid";
        return result;
    }

    // 2. Guard against NaN or Infinite samples
    for (size_t i = 0; i < audioBuffer.size(); ++i)
    {
        float s = audioBuffer[i];
        if (std::isnan(s) || std::isinf(s))
        {
            result.status = MeasurementStatus::invalid;
            result.reason = "non_finite_audio_samples";
            result.observability.status = "invalid";
            result.observability.reason = "Audio buffer contains NaN or Infinite samples";
            return result;
        }
    }

    // 3. Guard against silent / flat signal
    float maxAmp = 0.0f;
    for (float s : audioBuffer)
    {
        maxAmp = std::max(maxAmp, std::abs(s));
    }

    if (maxAmp < 1e-5f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "silent_or_flat_signal";
        result.observability.status = "unreliable";
        result.observability.reason = "Signal peak is below noise floor (-100 dBFS)";
        result.metrics.push_back({ "peakAmplitude", -100.0, "dBFS", "unreliable" });
        return result;
    }

    // 4. Guard against invalid note boundaries
    if (noteOffSample <= noteOnSample || noteOnSample >= audioBuffer.size())
    {
        result.status = MeasurementStatus::invalid;
        result.reason = "invalid_note_boundaries";
        result.observability.status = "invalid";
        result.observability.reason = "NoteOff sample is less than or equal to NoteOn sample, or NoteOn is out of bounds";
        return result;
    }

    // 5. Onset detection & guard against absent onset
    size_t onsetSample = detectedOnsetSample;
    if (onsetSample == 0)
    {
        float onsetThreshold = maxAmp * 0.02f; // -34 dB relative to peak
        bool found = false;
        for (size_t i = noteOnSample; i < std::min(noteOffSample, audioBuffer.size()); ++i)
        {
            if (std::abs(audioBuffer[i]) >= onsetThreshold)
            {
                onsetSample = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            result.status = MeasurementStatus::unreliable;
            result.reason = "onset_not_detected";
            result.observability.status = "unreliable";
            result.observability.reason = "Acoustic onset was not detected within note gate";
            result.metrics.push_back({ "peakAmplitude", 20.0 * std::log10(maxAmp), "dBFS", "observed" });
            return result;
        }
    }

    // 6. Delegate entirely to SynthEnvelopeAnalyzer without re-calculating DSP
    auto envMetrics = synth::SynthEnvelopeAnalyzer::analyzeEnvelope(
        audioBuffer, sampleRate, noteOnSample, noteOffSample, onsetSample);

    // 7. Translate EnvelopeMetrics to strongly-typed MeasurementMetric entries
    auto mapStatus = [](synth::MetricStatus s) -> juce::String {
        switch (s)
        {
            case synth::MetricStatus::Observed:
            case synth::MetricStatus::EstimatedWithUncertainty:
                return "observed";
            case synth::MetricStatus::NotObservableInGate:
            case synth::MetricStatus::Unreliable:
                return "unreliable";
            case synth::MetricStatus::Invalid:
            default:
                return "invalid";
        }
    };

    MeasurementMetric mAttack;
    mAttack.name = "attackTime";
    mAttack.value = envMetrics.attackTimeMs.value;
    mAttack.unit = "ms";
    mAttack.status = mapStatus(envMetrics.attackTimeMs.status);
    result.metrics.push_back(mAttack);

    MeasurementMetric mDecay;
    mDecay.name = "decayTime";
    mDecay.value = envMetrics.decayTimeMs.value;
    mDecay.unit = "ms";
    mDecay.status = mapStatus(envMetrics.decayTimeMs.status);
    result.metrics.push_back(mDecay);

    MeasurementMetric mSustain;
    mSustain.name = "sustainLevel";
    mSustain.value = envMetrics.sustainLevelDb.value;
    mSustain.unit = "dBFS";
    mSustain.status = mapStatus(envMetrics.sustainLevelDb.status);
    result.metrics.push_back(mSustain);

    MeasurementMetric mRelease;
    mRelease.name = "releaseTime";
    mRelease.value = envMetrics.releaseTimeMs.value;
    mRelease.unit = "ms";
    mRelease.status = mapStatus(envMetrics.releaseTimeMs.status);
    result.metrics.push_back(mRelease);

    MeasurementMetric mPeak;
    mPeak.name = "peakAmplitude";
    mPeak.value = envMetrics.peakAmplitudeDbfs;
    mPeak.unit = "dBFS";
    mPeak.status = "observed";
    result.metrics.push_back(mPeak);

    // 8. Determine overall measurement status & observability
    bool allObserved = (envMetrics.attackTimeMs.status == synth::MetricStatus::Observed ||
                        envMetrics.attackTimeMs.status == synth::MetricStatus::EstimatedWithUncertainty) &&
                       (envMetrics.decayTimeMs.status == synth::MetricStatus::Observed ||
                        envMetrics.decayTimeMs.status == synth::MetricStatus::EstimatedWithUncertainty) &&
                       (envMetrics.sustainLevelDb.status == synth::MetricStatus::Observed ||
                        envMetrics.sustainLevelDb.status == synth::MetricStatus::EstimatedWithUncertainty) &&
                       (envMetrics.releaseTimeMs.status == synth::MetricStatus::Observed ||
                        envMetrics.releaseTimeMs.status == synth::MetricStatus::EstimatedWithUncertainty);

    if (allObserved)
    {
        result.status = MeasurementStatus::completed;
        result.reason = "Envelope successfully observed";
        result.observability.status = "observed";
        result.observability.reason = std::nullopt;
    }
    else
    {
        // Measurement executed, but certain sections (e.g. decay or sustain) were unobservable in gate
        result.status = MeasurementStatus::completed;
        result.observability.status = "unreliable";

        if (envMetrics.decayTimeMs.status == synth::MetricStatus::NotObservableInGate)
        {
            result.reason = "decay_not_observable_gate_too_short";
            result.observability.reason = "Note gate too short to observe decay plateau";
        }
        else if (envMetrics.sustainLevelDb.status == synth::MetricStatus::NotObservableInGate)
        {
            result.reason = "sustain_not_observable_gate_too_short";
            result.observability.reason = "Note gate too short to observe stable sustain";
        }
        else
        {
            result.reason = "envelope_partial_observability";
            result.observability.reason = "One or more envelope phases lack sufficient margin for high certainty";
        }
    }

    // 9. Populate temporal curve for visualization and persistence
    result.curve = extractTemporalCurve(audioBuffer, sampleRate, 128);

    return result;
}

} // namespace abdaudiolab::measurement
