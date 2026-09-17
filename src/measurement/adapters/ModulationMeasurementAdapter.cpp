/**
 * @file ModulationMeasurementAdapter.cpp
 * @brief Implementation of ModulationMeasurementAdapter.
 * @author ABDSynths
 * @date 2026
 */

#include "ModulationMeasurementAdapter.h"
#include "../../synth/SynthPitchEstimator.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace abdaudiolab::measurement
{

MeasurementResult ModulationMeasurementAdapter::measure(synth::ISynthTarget* target,
                                                        const MeasurementSpec& spec,
                                                        const synth::SynthPresetState* state)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "modulation";
    result.measurementDomain = "synthesizedSpectralResponse";
    result.modulationDestination = !spec.modulationDestination.empty() ? spec.modulationDestination : "pitch";

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "SynthesizerTarget";
    result.dut.format = "ISynthTarget";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.stimulus = spec.stimulus;
    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;
    result.presetStateHash = spec.presetStateHash;
    result.measurementWindowStartMs = spec.measurementWindowStartMs;
    result.measurementWindowEndMs = spec.measurementWindowEndMs;

    if (target == nullptr)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "target_null";
        result.observability.status = "not_observable";
        result.observability.reason = "ISynthTarget is null";
        return result;
    }

    // Capture closed-loop excitation
    MeasurementSpec singleSpec = spec;
    if (singleSpec.stimulus.durationSec <= 0.0)
        singleSpec.stimulus.durationSec = 2.0; // Needs sufficient cycles to observe LFO

    CaptureResult cap = MeasurementCaptureCoordinator::captureSynchronous(target, singleSpec, state);
    if (cap.status == MeasurementStatus::failed)
    {
        result.status = MeasurementStatus::failed;
        result.reason = cap.reason.empty() ? "capture_failed" : cap.reason;
        return result;
    }

    return analyzeBuffer(spec, cap.capturedAudio, cap.sampleRateHz, "", cap.capturedAudioSha256);
}

MeasurementResult ModulationMeasurementAdapter::analyzeBuffer(const MeasurementSpec& spec,
                                                             const std::vector<float>& audio,
                                                             double sampleRate,
                                                             const std::string& audioArtifactPath,
                                                             const std::string& audioSha256)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "modulation";
    result.measurementDomain = "synthesizedSpectralResponse";
    result.modulationDestination = !spec.modulationDestination.empty() ? spec.modulationDestination : "pitch";

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "SynthesizerTarget";
    result.dut.format = "ISynthTarget";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.execution.sampleRateHz = sampleRate;
    result.stimulus = spec.stimulus;
    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;
    result.presetStateHash = spec.presetStateHash;
    result.measurementWindowStartMs = spec.measurementWindowStartMs;
    result.measurementWindowEndMs = spec.measurementWindowEndMs;
    result.artifacts.audioPath = audioArtifactPath;
    result.artifacts.audioSha256 = audioSha256;

    if (audio.empty() || sampleRate <= 0.0)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "empty_audio_buffer";
        result.observability.status = "not_observable";
        result.observability.reason = "Audio buffer contains no samples or invalid sample rate";
        return result;
    }

    // Check NaN or Inf
    for (float s : audio)
    {
        if (std::isnan(s) || std::isinf(s))
        {
            result.status = MeasurementStatus::invalid;
            result.reason = "non_finite_audio_samples";
            result.observability.status = "invalid";
            result.observability.reason = "Audio buffer contains NaN or Infinite samples";
            return result;
        }
    }

    float maxAmp = 0.0f;
    for (float s : audio)
        maxAmp = std::max(maxAmp, std::abs(s));

    if (maxAmp < 1e-4f)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "signal_below_noise_floor_or_too_short";
        result.observability.status = "unreliable";
        result.observability.reason = "Audio signal is below noise floor; no modulation observable";
        return result;
    }

    ModulationResultData mod;
    mod.targetDestination = result.modulationDestination;

    double nominalCarrierHz = spec.stimulus.startFreqHz > 20.0f 
        ? static_cast<double>(spec.stimulus.startFreqHz) 
        : synth::SynthPitchEstimator::midiNoteToFrequencyHz(spec.stimulus.midiNoteNumber > 0 ? spec.stimulus.midiNoteNumber : 60);

    std::vector<double> timesMs;
    std::vector<double> demodValues;

    std::string defaultMethod = "spectral_peak";

    if (result.modulationDestination == "amplitude")
    {
        demodulateAmplitude(audio, sampleRate, 256, timesMs, demodValues);
        mod.depth.unit = "dB";
        mod.depth.name = "tremolo_depth";
        defaultMethod = "amplitude_demodulation";
    }
    else // "pitch" or default
    {
        demodulatePitch(audio, sampleRate, nominalCarrierHz, 256, timesMs, demodValues);
        mod.depth.unit = "cents";
        mod.depth.name = "vibrato_depth";
        defaultMethod = "pitch_tracking";
    }

    if (demodValues.size() < 8)
    {
        result.status = MeasurementStatus::unreliable;
        result.reason = "insufficient_demodulated_points";
        return result;
    }

    // Populate demodulated time curve
    mod.timeCurve.xName = "time";
    mod.timeCurve.xUnit = "ms";
    mod.timeCurve.yName = result.modulationDestination == "amplitude" ? "levelDelta" : "pitchDelta";
    mod.timeCurve.yUnit = mod.depth.unit;
    mod.timeCurve.x = timesMs;
    mod.timeCurve.y = demodValues;

    // Estimate modulation depth
    double maxVal = *std::max_element(demodValues.begin(), demodValues.end());
    double minVal = *std::min_element(demodValues.begin(), demodValues.end());
    double peakToPeak = std::max(0.0, maxVal - minVal);
    mod.depth.value = peakToPeak / 2.0; // Peak deviation around carrier
    mod.depth.status = (peakToPeak > 0.1) ? "observed" : "unreliable";

    // Estimate LFO rate declaring explicit method
    double effectiveRateHz = sampleRate / 256.0;
    std::string chosenMethod;
    double lfoRate = estimateLfoRate(timesMs, demodValues, effectiveRateHz, defaultMethod, chosenMethod);

    mod.rateHz.name = "lfo_rate";
    mod.rateHz.value = lfoRate;
    mod.rateHz.unit = "Hz";
    mod.rateHz.status = (lfoRate > 0.05) ? "observed" : "unreliable";
    mod.rateMethod = chosenMethod;

    // Estimate waveform shape and confidence
    if (result.modulationDestination == "amplitude")
    {
        std::vector<double> linValues(demodValues.size());
        for (size_t i = 0; i < demodValues.size(); ++i)
            linValues[i] = std::pow(10.0, demodValues[i] / 20.0);
        mod.waveform = classifyWaveform(linValues);
    }
    else
    {
        mod.waveform = classifyWaveform(demodValues);
    }

    // Extract carrier-linked spectral sidebands via high-resolution FFT
    mod.sidebands = extractSidebands(audio, sampleRate, nominalCarrierHz, lfoRate, 2);

    // Spectral analysis metadata
    mod.spectralMetadata.fftSize = 4096;
    mod.spectralMetadata.hopSize = 1024;
    mod.spectralMetadata.window = "hann";
    mod.spectralMetadata.frequencyResolutionHz = sampleRate / 4096.0;
    mod.spectralMetadata.averagingCount = 1;

    // Build modulation spectrum curve (low-frequency or carrier zoom)
    constexpr int kFftSize = 4096;
    std::vector<float> fftBuf(static_cast<size_t>(kFftSize) * 2, 0.0f);
    size_t copyLen = std::min<size_t>(audio.size(), static_cast<size_t>(kFftSize));
    for (size_t i = 0; i < copyLen; ++i)
    {
        double w = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(copyLen - 1)));
        fftBuf[i] = audio[i] * static_cast<float>(w);
    }
    juce::dsp::FFT fft(12); // 4096
    fft.performRealOnlyForwardTransform(fftBuf.data());

    mod.spectrumCurve.xName = "frequency";
    mod.spectrumCurve.xUnit = "Hz";
    mod.spectrumCurve.yName = "magnitude";
    mod.spectrumCurve.yUnit = "dBFS";

    double binHz = sampleRate / static_cast<double>(kFftSize);
    int maxBin = std::min(kFftSize / 2, static_cast<int>(2000.0 / binHz)); // Up to 2000 Hz for inspection
    for (int k = 1; k < maxBin; k += 2)
    {
        float re = fftBuf[static_cast<size_t>(2 * k)];
        float im = fftBuf[static_cast<size_t>(2 * k + 1)];
        double mag = std::sqrt(static_cast<double>(re * re + im * im)) / static_cast<double>(kFftSize);
        double db = 20.0 * std::log10(std::max(mag, 1e-6));
        mod.spectrumCurve.x.push_back(static_cast<double>(k) * binHz);
        mod.spectrumCurve.y.push_back(db);
    }

    result.modulationResult = mod;
    result.curve = mod.timeCurve;
    result.status = (mod.rateHz.status == "observed" && mod.depth.status == "observed")
                    ? MeasurementStatus::completed
                    : MeasurementStatus::unreliable;
    result.reason = result.status == MeasurementStatus::completed ? "modulation_observed" : "modulation_weak_or_unreliable";
    result.observability.status = measurementStatusToString(result.status);

    // Summary metrics
    MeasurementMetric rateMet = mod.rateHz;
    result.metrics.push_back(rateMet);

    MeasurementMetric depthMet = mod.depth;
    result.metrics.push_back(depthMet);

    return result;
}

void ModulationMeasurementAdapter::demodulateAmplitude(const std::vector<float>& audio,
                                                       double sampleRate,
                                                       size_t hopSamples,
                                                       std::vector<double>& outTimesMs,
                                                       std::vector<double>& outEnvelopeDb)
{
    outTimesMs.clear();
    outEnvelopeDb.clear();

    if (audio.empty() || sampleRate <= 0.0 || hopSamples == 0)
        return;

    size_t numHops = audio.size() / hopSamples;
    outTimesMs.reserve(numHops);
    outEnvelopeDb.reserve(numHops);

    for (size_t h = 0; h < numHops; ++h)
    {
        size_t start = h * hopSamples;
        size_t end = std::min(audio.size(), start + hopSamples);
        double sumSq = 0.0;
        for (size_t i = start; i < end; ++i)
        {
            double s = static_cast<double>(audio[i]);
            sumSq += s * s;
        }
        double rms = std::sqrt(sumSq / std::max<size_t>(1, end - start));
        double db = 20.0 * std::log10(std::max(rms, 1e-6));

        double tMs = (static_cast<double>(start + hopSamples / 2) / sampleRate) * 1000.0;
        outTimesMs.push_back(tMs);
        outEnvelopeDb.push_back(db);
    }
}

void ModulationMeasurementAdapter::demodulatePitch(const std::vector<float>& audio,
                                                   double sampleRate,
                                                   double nominalCarrierHz,
                                                   size_t hopSamples,
                                                   std::vector<double>& outTimesMs,
                                                   std::vector<double>& outPitchDeltaCents)
{
    outTimesMs.clear();
    outPitchDeltaCents.clear();

    if (audio.empty() || sampleRate <= 0.0 || hopSamples == 0 || nominalCarrierHz <= 0.0)
        return;

    size_t windowLen = 1024;
    if (audio.size() < windowLen)
        return;

    size_t numHops = (audio.size() - windowLen) / hopSamples;
    outTimesMs.reserve(numHops);
    outPitchDeltaCents.reserve(numHops);

    for (size_t h = 0; h < numHops; ++h)
    {
        size_t start = h * hopSamples;
        synth::PitchEstimate est = synth::SynthPitchEstimator::estimatePitch(
            audio, sampleRate, nominalCarrierHz, start, windowLen);

        double deltaCents = 0.0;
        if (est.status == synth::MetricStatus::Observed && est.frequencyHz > 10.0)
        {
            deltaCents = est.centsError;
        }

        double tMs = (static_cast<double>(start + windowLen / 2) / sampleRate) * 1000.0;
        outTimesMs.push_back(tMs);
        outPitchDeltaCents.push_back(deltaCents);
    }
}

double ModulationMeasurementAdapter::estimateLfoRate(const std::vector<double>& timesMs,
                                                     const std::vector<double>& values,
                                                     [[maybe_unused]] double sampleRate,
                                                     const std::string& preferredMethod,
                                                     std::string& outMethod)
{
    outMethod = preferredMethod;
    if (values.size() < 8)
    {
        outMethod = "none";
        return 0.0;
    }

    // Subtract DC mean
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    std::vector<double> zeroMean(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        zeroMean[i] = values[i] - mean;

    // Autocorrelation search for periodic fundamental cycle
    int maxLag = static_cast<int>(values.size() / 2);
    int minLag = 2; // At least 2 points per cycle

    std::vector<double> corr(static_cast<size_t>(maxLag), 0.0);
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        double sum = 0.0;
        size_t count = values.size() - static_cast<size_t>(lag);
        for (size_t i = 0; i < count; ++i)
            sum += zeroMean[i] * zeroMean[i + static_cast<size_t>(lag)];

        corr[static_cast<size_t>(lag)] = sum / static_cast<double>(count);
    }

    // Find first local peak AFTER zero crossing (ensures fundamental, avoids subharmonic 2T / 3T)
    bool crossedZero = false;
    int firstPeakLag = -1;

    for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
    {
        size_t idx = static_cast<size_t>(lag);
        if (corr[idx] < 0.0)
            crossedZero = true;

        if (crossedZero && corr[idx] > 0.0)
        {
            if (corr[idx] >= corr[idx - 1] && corr[idx] >= corr[idx + 1])
            {
                firstPeakLag = lag;
                break;
            }
        }
    }

    // If no clean zero-crossing found, search for first local peak
    if (firstPeakLag < 0)
    {
        for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
        {
            size_t idx = static_cast<size_t>(lag);
            if (corr[idx] > corr[idx - 1] && corr[idx] > corr[idx + 1] && corr[idx] > 0.0)
            {
                firstPeakLag = lag;
                break;
            }
        }
    }

    if (firstPeakLag > 0 && timesMs.size() > static_cast<size_t>(firstPeakLag))
    {
        // Parabolic interpolation for sub-sample accuracy
        size_t p = static_cast<size_t>(firstPeakLag);
        double y0 = corr[p - 1];
        double y1 = corr[p];
        double y2 = corr[p + 1];
        double denom = 2.0 * (2.0 * y1 - y0 - y2);
        double delta = (std::abs(denom) > 1e-12) ? (y2 - y0) / denom : 0.0;
        delta = std::clamp(delta, -0.5, 0.5);

        double refinedLag = static_cast<double>(firstPeakLag) + delta;
        double avgHopSec = (timesMs.back() - timesMs.front()) / (static_cast<double>(timesMs.size() - 1) * 1000.0);
        double periodSec = refinedLag * avgHopSec;

        if (periodSec > 1e-4)
        {
            double rate = 1.0 / periodSec;
            if (rate >= 0.05 && rate <= 40.0)
                return rate;
        }
    }

    return 1.0;
}

std::vector<ModulationSideband> ModulationMeasurementAdapter::extractSidebands(const std::vector<float>& audio,
                                                                               double sampleRate,
                                                                               double carrierFreqHz,
                                                                               double lfoRateHz,
                                                                               int maxOrder)
{
    std::vector<ModulationSideband> sidebands;
    if (audio.size() < 2048 || sampleRate <= 0.0 || carrierFreqHz <= 0.0 || lfoRateHz <= 0.05)
        return sidebands;

    // Use high-resolution FFT (up to 65536 points, ~0.73 Hz/bin at 48kHz) to resolve closely-spaced LFO sidebands
    int fftOrder = 14;
    if (audio.size() >= 65536)
        fftOrder = 16;
    else if (audio.size() >= 32768)
        fftOrder = 15;

    int kFftSize = 1 << fftOrder;
    std::vector<float> fftBuf(static_cast<size_t>(kFftSize) * 2, 0.0f);
    size_t copyLen = std::min<size_t>(audio.size(), static_cast<size_t>(kFftSize));
    for (size_t i = 0; i < copyLen; ++i)
    {
        // Blackman-Harris window for high dynamic range and steep sidelobe rejection
        double a0 = 0.35875;
        double a1 = 0.48829;
        double a2 = 0.14128;
        double a3 = 0.01168;
        double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(copyLen - 1);
        double w = a0 - a1 * std::cos(theta) + a2 * std::cos(2.0 * theta) - a3 * std::cos(3.0 * theta);
        fftBuf[i] = audio[i] * static_cast<float>(w);
    }
    juce::dsp::FFT fft(fftOrder);
    fft.performRealOnlyForwardTransform(fftBuf.data());

    double binHz = sampleRate / static_cast<double>(kFftSize);

    // Find carrier peak bin
    int nominalCarrierBin = static_cast<int>(std::round(carrierFreqHz / binHz));
    nominalCarrierBin = std::clamp(nominalCarrierBin, 2, kFftSize / 2 - 2);

    int carrierBin = nominalCarrierBin;
    double maxCarrierMag = -1e9;
    for (int b = std::max(1, nominalCarrierBin - 2); b <= std::min(kFftSize / 2 - 1, nominalCarrierBin + 2); ++b)
    {
        float re = fftBuf[static_cast<size_t>(2 * b)];
        float im = fftBuf[static_cast<size_t>(2 * b + 1)];
        double mag = std::sqrt(static_cast<double>(re * re + im * im));
        if (mag > maxCarrierMag)
        {
            maxCarrierMag = mag;
            carrierBin = b;
        }
    }
    double carrierDb = 20.0 * std::log10(std::max(maxCarrierMag, 1e-6));

    for (int order = 1; order <= maxOrder; ++order)
    {
        for (int sign : { 1, -1 })
        {
            int ord = sign * order;
            double targetFreqHz = carrierFreqHz + static_cast<double>(ord) * lfoRateHz;
            if (targetFreqHz <= 10.0 || targetFreqHz >= sampleRate / 2.0)
                continue;

            int nominalSbBin = static_cast<int>(std::round(targetFreqHz / binHz));
            nominalSbBin = std::clamp(nominalSbBin, 1, kFftSize / 2 - 1);

            // Find local peak around nominal sideband frequency, strictly excluding carrier and opposite side
            int bestSbBin = nominalSbBin;
            double maxSbMag = -1e9;
            for (int b = std::max(1, nominalSbBin - 2); b <= std::min(kFftSize / 2 - 1, nominalSbBin + 2); ++b)
            {
                if (b == carrierBin || (ord > 0 && b <= carrierBin) || (ord < 0 && b >= carrierBin))
                    continue;

                float re = fftBuf[static_cast<size_t>(2 * b)];
                float im = fftBuf[static_cast<size_t>(2 * b + 1)];
                double mag = std::sqrt(static_cast<double>(re * re + im * im));
                if (mag > maxSbMag)
                {
                    maxSbMag = mag;
                    bestSbBin = b;
                }
            }

            double sbDb = 20.0 * std::log10(std::max(maxSbMag, 1e-6));

            ModulationSideband sb;
            sb.carrierFrequencyHz = carrierFreqHz;
            sb.sidebandFrequencyHz = static_cast<double>(bestSbBin) * binHz;
            sb.order = ord;
            sb.levelRelativeToCarrierDb = sbDb - carrierDb;

            sidebands.push_back(sb);
        }
    }

    return sidebands;
}

WaveformEstimate ModulationMeasurementAdapter::classifyWaveform(const std::vector<double>& values)
{
    WaveformEstimate est;
    est.waveform = "none";
    est.status = "not_observable";
    est.confidence = 0.0;

    if (values.size() < 16)
        return est;

    // Check peak-to-peak amplitude to ensure signal is modulated
    double maxV = *std::max_element(values.begin(), values.end());
    double minV = *std::min_element(values.begin(), values.end());
    if (maxV - minV < 0.2) // Less than 0.2 dB or 0.2 cents
    {
        est.status = "not_observable";
        return est;
    }

    // Normalized zero-mean curve
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    std::vector<double> norm(values.size());
    double maxAbs = 0.0;
    for (size_t i = 0; i < values.size(); ++i)
    {
        norm[i] = values[i] - mean;
        maxAbs = std::max(maxAbs, std::abs(norm[i]));
    }
    if (maxAbs < 1e-6)
        return est;

    for (double& v : norm)
        v /= maxAbs;

    // Estimate single cycle length via autocorrelation (first local peak after zero-crossing)
    int maxLag = static_cast<int>(norm.size() / 2);
    int minLag = 4;
    std::vector<double> corr(static_cast<size_t>(maxLag), 0.0);
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        double sum = 0.0;
        size_t count = norm.size() - static_cast<size_t>(lag);
        for (size_t i = 0; i < count; ++i)
            sum += norm[i] * norm[i + static_cast<size_t>(lag)];
        corr[static_cast<size_t>(lag)] = sum / static_cast<double>(count);
    }

    bool crossedZero = false;
    int bestLag = -1;
    for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
    {
        size_t idx = static_cast<size_t>(lag);
        if (corr[idx] < 0.0)
            crossedZero = true;

        if (crossedZero && corr[idx] > 0.0)
        {
            if (corr[idx] >= corr[idx - 1] && corr[idx] >= corr[idx + 1])
            {
                bestLag = lag;
                break;
            }
        }
    }

    if (bestLag < 0)
    {
        for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
        {
            size_t idx = static_cast<size_t>(lag);
            if (corr[idx] > corr[idx - 1] && corr[idx] > corr[idx + 1] && corr[idx] > 0.0)
            {
                bestLag = lag;
                break;
            }
        }
    }

    size_t cycleLen = (bestLag > 4) ? static_cast<size_t>(bestLag) : norm.size();
    cycleLen = std::min(cycleLen, norm.size());
    if (cycleLen < 8)
        return est;

    // Estimate fundamental phase offset phi to align reference templates
    double sumCos = 0.0;
    double sumSin = 0.0;
    double normSSq = 0.0;

    for (size_t i = 0; i < cycleLen; ++i)
    {
        double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(cycleLen);
        double s = norm[i];
        sumCos += s * std::cos(theta);
        sumSin += s * std::sin(theta);
        normSSq += s * s;
    }

    if (normSSq < 1e-12)
        return est;

    double normS = std::sqrt(normSSq);
    double phi = std::atan2(sumCos, sumSin);

    // Compute normalized Pearson correlation for each canonical template
    double sumSine = 0.0, sumSineSq = 0.0;
    double sumTri = 0.0, sumTriSq = 0.0;
    double sumSq = 0.0, sumSqSq = 0.0;

    for (size_t i = 0; i < cycleLen; ++i)
    {
        double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(cycleLen) + phi;
        double s = norm[i];

        // 1. Sine template
        double rSine = std::sin(theta);
        sumSine += s * rSine;
        sumSineSq += rSine * rSine;

        // 2. Triangle template
        double thNorm = std::fmod(theta, 2.0 * 3.14159265358979323846);
        if (thNorm < 0.0) thNorm += 2.0 * 3.14159265358979323846;
        double rTri = 2.0 * std::abs((thNorm / 3.14159265358979323846) - 1.0) - 1.0;
        sumTri += s * rTri;
        sumTriSq += rTri * rTri;

        // 3. Square template
        double rSquare = (std::sin(theta) >= 0.0) ? 1.0 : -1.0;
        sumSq += s * rSquare;
        sumSqSq += rSquare * rSquare;
    }

    double corrSine = (sumSineSq > 1e-9) ? (sumSine / (normS * std::sqrt(sumSineSq))) : 0.0;
    double corrTri = (sumTriSq > 1e-9) ? (sumTri / (normS * std::sqrt(sumTriSq))) : 0.0;
    double corrSquare = (sumSqSq > 1e-9) ? (sumSq / (normS * std::sqrt(sumSqSq))) : 0.0;

    double absSine = std::abs(corrSine);
    double absTri = std::abs(corrTri);
    double absSquare = std::abs(corrSquare);

    double bestShapeCorr = std::max({ absSine, absTri, absSquare });

    if (absSine >= absTri && absSine >= absSquare)
    {
        est.waveform = "sine";
        est.confidence = std::clamp(absSine, 0.5, 0.99);
        est.status = "inferred";
    }
    else if (absTri >= absSquare)
    {
        est.waveform = "triangle";
        est.confidence = std::clamp(absTri, 0.5, 0.99);
        est.status = "inferred";
    }
    else
    {
        est.waveform = "square";
        est.confidence = std::clamp(absSquare, 0.5, 0.99);
        est.status = "inferred";
    }

    if (bestShapeCorr < 0.50)
    {
        est.waveform = "complex";
        est.status = "observed";
        est.confidence = 0.60;
    }

    return est;
}

} // namespace abdaudiolab::measurement
