/**
 * @file FilterMeasurementAdapter.cpp
 * @brief Implementation of FilterMeasurementAdapter.
 * @author ABDSynths
 * @date 2026
 */

#include "FilterMeasurementAdapter.h"
#include "../../math/FarinaDeconvolver.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::measurement
{

MeasurementResult FilterMeasurementAdapter::measure(const MeasurementSpec& spec,
                                                   const std::vector<float>& capturedAudio,
                                                   double sampleRate,
                                                   const std::string& audioArtifactPath,
                                                   const std::string& audioSha256)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "filter";
    result.measurementDomain = spec.measurementDomain.empty() ? "directTransferFunction" : spec.measurementDomain;
    result.filterTopology = spec.filterTopology.empty() ? "unknown" : spec.filterTopology;

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "FilterTarget";
    result.dut.format = "AudioProcessor";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.execution.sampleRateHz = sampleRate;
    result.execution.numSamples = static_cast<int64_t>(capturedAudio.size());
    result.stimulus = spec.stimulus;

    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;

    result.artifacts.audioPath = audioArtifactPath;
    result.artifacts.audioSha256 = audioSha256;
    result.integrityVerified = !audioSha256.empty();

    // 1. Guard against empty buffer
    if (capturedAudio.empty() || sampleRate <= 0.0)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "empty_audio_buffer";
        result.observability.status = "not_observable";
        result.observability.reason = "Audio buffer contains no samples or sample rate is invalid";
        return result;
    }

    // 2. Guard against NaN or Infinite samples
    for (size_t i = 0; i < capturedAudio.size(); ++i)
    {
        float s = capturedAudio[i];
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
    for (float s : capturedAudio)
    {
        maxAmp = std::max(maxAmp, std::abs(s));
    }

    if (maxAmp < 1e-5f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "silent_or_flat_signal";
        result.observability.status = "unreliable";
        result.observability.reason = "Signal peak is below noise floor (-100 dBFS)";
        result.metrics.push_back({ "peakAmplitude", -100.0, "dBFS", "unreliable", "silent_signal" });
        return result;
    }

    // 4. Determine sweep deconvolution parameters
    double durationSec = spec.stimulus.durationSec > 0.0 ? spec.stimulus.durationSec : (static_cast<double>(capturedAudio.size()) / sampleRate);
    float startFreq = spec.stimulus.startFreqHz > 0.0f ? spec.stimulus.startFreqHz : 20.0f;
    float endFreq = spec.stimulus.endFreqHz > 0.0f ? spec.stimulus.endFreqHz : 20000.0f;

    // 5. Generate inverse filter and execute Farina deconvolution
    auto invFilter = math::FarinaDeconvolver::generateInverseFilter(sampleRate, durationSec, startFreq, endFreq);
    auto deco = math::FarinaDeconvolver::deconvolve(capturedAudio, invFilter, sampleRate, durationSec, startFreq, endFreq);

    // 6. Delegate to analytical measurement engine
    return measureFromDeconvolution(spec, deco, sampleRate, audioArtifactPath, audioSha256);
}

MeasurementResult FilterMeasurementAdapter::measureFromDeconvolution(
    const MeasurementSpec& spec,
    const math::DeconvolutionResult& deco,
    double sampleRate,
    const std::string& audioArtifactPath,
    const std::string& audioSha256)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "filter";
    result.measurementDomain = spec.measurementDomain.empty() ? "directTransferFunction" : spec.measurementDomain;
    result.filterTopology = spec.filterTopology.empty() ? "unknown" : spec.filterTopology;

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "FilterTarget";
    result.dut.format = "AudioProcessor";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.execution.sampleRateHz = sampleRate;
    result.execution.numSamples = static_cast<int64_t>(deco.fullDeconvolvedIR.size());
    result.stimulus = spec.stimulus;

    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;

    result.artifacts.audioPath = audioArtifactPath;
    result.artifacts.audioSha256 = audioSha256;
    result.integrityVerified = !audioSha256.empty();

    auto domain = measurementDomainFromString(result.measurementDomain);
    auto topology = filterTopologyFromString(result.filterTopology);

    // 1. Run detailed metrological analysis
    auto analysis = math::analytics::FilterAnalytics::analyzeDeconvolvedFilter(
        deco, sampleRate, topology, domain);

    // 2. Map metrics according to measurement domain
    if (domain == MeasurementDomain::directTransferFunction)
    {
        // Metric 1: Cutoff Frequency (compatibility alias)
        result.metrics.push_back(analysis.cutoffFrequencyHz);

        // Metric 2: Lower Cutoff (for bandpass/highpass)
        if (analysis.cutoff.lowerCutoffHz.status != "not_applicable")
            result.metrics.push_back(analysis.cutoff.lowerCutoffHz);

        // Metric 3: Upper Cutoff (for bandpass/lowpass)
        if (analysis.cutoff.upperCutoffHz.status != "not_applicable")
            result.metrics.push_back(analysis.cutoff.upperCutoffHz);

        // Metric 4: Bandwidth (for bandpass)
        if (analysis.cutoff.bandwidthHz.status != "not_applicable")
            result.metrics.push_back(analysis.cutoff.bandwidthHz);

        // Metric 5: Resonance Frequency
        result.metrics.push_back(analysis.resonancePeakHz);

        // Metric 6: Resonance Gain
        result.metrics.push_back(analysis.resonanceGainDb);

        // Metric 7: Roll-off slope (asymptotic dB/oct)
        result.metrics.push_back(analysis.rollOffSlopeDbPerOct);

        // Metric 8: Q factor
        result.metrics.push_back(analysis.qFactor);

        // Metric 9: Passband Gain
        MeasurementMetric passMetric;
        passMetric.name = "passbandGain";
        passMetric.value = analysis.passbandGainDbfs;
        passMetric.unit = "dBFS";
        passMetric.status = analysis.passbandObservable ? "observed" : "unreliable";
        result.metrics.push_back(passMetric);

        // Metric 10: Latency
        result.metrics.push_back(analysis.passbandLatencyMs);

        // Metric 11..13: Distortion
        result.metrics.push_back(analysis.thdPercent);
        result.metrics.push_back(analysis.h2Percent);
        result.metrics.push_back(analysis.h3Percent);
    }
    else
    {
        // Synthesized Spectral Response: honest naming under declared MIDI excitation
        MeasurementMetric mPeak = analysis.resonancePeakHz;
        mPeak.name = "observedSpectralPeak";
        result.metrics.push_back(mPeak);

        MeasurementMetric mSlope = analysis.rollOffSlopeDbPerOct;
        mSlope.name = "observedSpectralRolloff";
        result.metrics.push_back(mSlope);

        MeasurementMetric mCutoff = analysis.cutoffFrequencyHz;
        mCutoff.name = "observedCutoffProxy";
        result.metrics.push_back(mCutoff);

        MeasurementMetric mLevel;
        mLevel.name = "observedSpectralLevel";
        mLevel.value = analysis.passbandGainDbfs;
        mLevel.unit = "dBFS";
        mLevel.status = "observed";
        result.metrics.push_back(mLevel);

        result.metrics.push_back(analysis.thdPercent);
        result.metrics.push_back(analysis.h2Percent);
        result.metrics.push_back(analysis.h3Percent);
    }

    // 3. Attach slope fit metadata
    if (analysis.slopeFit.sampleCount > 0)
    {
        result.slopeFit = analysis.slopeFit;
    }

    // 4. Determine overall status and observability
    if (domain == MeasurementDomain::directTransferFunction)
    {
        if (analysis.cutoffFrequencyHz.status == "observed" || analysis.resonancePeakHz.status == "observed")
        {
            result.status = MeasurementStatus::completed;
            result.reason = "Filter response successfully observed via deconvolution";
            result.observability.status = "observed";
            result.observability.reason = std::nullopt;
        }
        else if (topology == FilterTopology::allPass || topology == FilterTopology::comb)
        {
            result.status = MeasurementStatus::completed;
            result.reason = "All-pass or comb response observed without cutoff transition";
            result.observability.status = "observed";
            result.observability.reason = std::nullopt;
        }
        else
        {
            result.status = MeasurementStatus::completed;
            result.reason = "Filter response lacks observable 3 dB cutoff transition";
            result.observability.status = "unreliable";
            result.observability.reason = "No 3 dB crossing or recognizable transition knee observed";
        }
    }
    else
    {
        result.status = MeasurementStatus::completed;
        result.reason = "Spectral envelope observed under declared MIDI excitation";
        result.observability.status = "observed";
        result.observability.reason = std::nullopt;
    }

    // 5. Populate frequency magnitude curve
    result.curve.xName = "frequency";
    result.curve.xUnit = "Hz";
    result.curve.yName = "magnitude";
    result.curve.yUnit = "dBFS";
    result.curve.x = analysis.frequencyBinsHz;
    result.curve.y = analysis.magnitudeDb;

    return result;
}

} // namespace abdaudiolab::measurement
