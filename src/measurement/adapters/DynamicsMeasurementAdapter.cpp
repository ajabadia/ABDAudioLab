/**
 * @file DynamicsMeasurementAdapter.cpp
 * @brief Implementation of DynamicsMeasurementAdapter.
 * @author ABDSynths
 * @date 2026
 */

#include "DynamicsMeasurementAdapter.h"
#include "../../synth/Sha256.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace abdaudiolab::measurement
{

std::vector<int> DynamicsMeasurementAdapter::getDefaultVelocityGrid()
{
    return { 0, 1, 8, 16, 24, 32, 48, 64, 80, 96, 112, 120, 127 };
}

MeasurementResult DynamicsMeasurementAdapter::measure(synth::ISynthTarget* target,
                                                     const MeasurementSpec& spec,
                                                     const synth::SynthPresetState* state)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "dynamics";
    result.measurementDomain = "synthesizedSpectralResponse";
    result.filterTopology = "not_applicable";
    result.modulationDestination = "amplitude";

    result.dut.name = !spec.parameterName.empty() ? spec.parameterName : "SynthesizerTarget";
    result.dut.format = "ISynthTarget";
    result.dut.version = "1.0.0";
    result.dut.type = deviceUnderTestToString(spec.dutType);

    result.execution = spec.execution;
    result.stimulus = spec.stimulus;
    result.analyzer.name = kAnalyzerName;
    result.analyzer.version = kAnalyzerVersion;

    if (target == nullptr)
    {
        result.status = MeasurementStatus::failed;
        result.reason = "target_null";
        result.observability.status = "not_observable";
        result.observability.reason = "ISynthTarget is null";
        return result;
    }

    std::vector<int> grid = !spec.velocityGrid.empty() ? spec.velocityGrid : getDefaultVelocityGrid();
    std::vector<VelocityTake> takes;
    takes.reserve(grid.size());

    double sampleRate = spec.execution.sampleRateHz > 0.0 ? spec.execution.sampleRateHz : 48000.0;

    for (int v : grid)
    {
        VelocityTake take;
        take.velocity = v;
        take.sampleRateHz = sampleRate;

        if (v == 0)
        {
            // Explicit special case: zero velocity note-on
            take.audio = std::vector<float>(static_cast<size_t>(sampleRate * 0.1), 0.0f);
            take.noteOnSample = 0;
            take.noteOffSample = take.audio.size();
            take.presetStateHash = spec.presetStateHash;
            take.audioArtifactHash = "";
            takes.push_back(std::move(take));
            continue;
        }

        // Sequential cycle: restore preset -> NoteOn(v) -> capture -> NoteOff
        MeasurementSpec singleSpec = spec;
        singleSpec.stimulus.type = StimulusType::midiNote;
        singleSpec.stimulus.midiVelocity = static_cast<float>(v) / 127.0f;
        if (singleSpec.stimulus.durationSec <= 0.0)
            singleSpec.stimulus.durationSec = 1.0;

        CaptureResult cap = MeasurementCaptureCoordinator::captureSynchronous(target, singleSpec, state);
        if (cap.status == MeasurementStatus::failed)
        {
            result.status = MeasurementStatus::failed;
            result.reason = "capture_failed_at_velocity_" + std::to_string(v);
            return result;
        }

        take.audio = std::move(cap.capturedAudio);
        take.noteOnSample = singleSpec.stimulus.noteOnSample;
        take.noteOffSample = singleSpec.stimulus.noteOffSample > 0 
                             ? singleSpec.stimulus.noteOffSample 
                             : static_cast<size_t>(singleSpec.stimulus.durationSec * sampleRate);
        take.presetStateHash = cap.presetStateSha256;
        take.audioArtifactHash = cap.capturedAudioSha256;
        takes.push_back(std::move(take));
    }

    return analyzeTakes(spec, takes);
}

MeasurementResult DynamicsMeasurementAdapter::analyzeTakes(const MeasurementSpec& spec,
                                                          const std::vector<VelocityTake>& takes)
{
    MeasurementResult result;
    result.schemaVersion = "response-measurement-1.0";
    result.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    result.measurementId = spec.measurementId;
    result.measurementType = "dynamics";
    result.measurementDomain = "synthesizedSpectralResponse";
    result.filterTopology = "not_applicable";
    result.modulationDestination = "amplitude";

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

    if (takes.empty())
    {
        result.status = MeasurementStatus::failed;
        result.reason = "empty_takes";
        result.observability.status = "not_observable";
        result.observability.reason = "No velocity takes supplied for dynamics analysis";
        return result;
    }

    DynamicResponseResult dyn;
    dyn.points.reserve(takes.size());

    std::vector<int> velocities;
    std::vector<double> rmsLevels;
    std::vector<double> validVelocitiesForFit;
    std::vector<double> validRmsForFit;
    std::vector<double> validBrightnessForFit;

    double sampleRate = takes.front().sampleRateHz > 0.0 ? takes.front().sampleRateHz : 48000.0;
    result.execution.sampleRateHz = sampleRate;

    for (const auto& take : takes)
    {
        DynamicPoint pt = analyzeSinglePoint(take.velocity,
                                            take.audio,
                                            take.sampleRateHz,
                                            take.noteOnSample,
                                            take.noteOffSample,
                                            spec.measurementWindowStartMs,
                                            spec.measurementWindowEndMs,
                                            take.presetStateHash,
                                            take.audioArtifactHash);
        dyn.points.push_back(pt);

        velocities.push_back(pt.velocity);
        rmsLevels.push_back(pt.rmsDbfs);

        if (pt.status == "observed" && pt.velocity > 0)
        {
            validVelocitiesForFit.push_back(static_cast<double>(pt.velocity));
            validRmsForFit.push_back(pt.rmsDbfs);
            validBrightnessForFit.push_back(pt.spectralCentroidHz);
        }
    }

    // Build aggregate 2D curves
    dyn.amplitudeCurve.xName = "velocity";
    dyn.amplitudeCurve.xUnit = "midi_val";
    dyn.amplitudeCurve.yName = "rmsDbfs";
    dyn.amplitudeCurve.yUnit = "dBFS";

    dyn.brightnessCurve.xName = "velocity";
    dyn.brightnessCurve.xUnit = "midi_val";
    dyn.brightnessCurve.yName = "spectralCentroidHz";
    dyn.brightnessCurve.yUnit = "Hz";

    for (const auto& pt : dyn.points)
    {
        dyn.amplitudeCurve.x.push_back(static_cast<double>(pt.velocity));
        dyn.amplitudeCurve.y.push_back(pt.rmsDbfs);

        dyn.brightnessCurve.x.push_back(static_cast<double>(pt.velocity));
        dyn.brightnessCurve.y.push_back(pt.spectralCentroidHz);
    }

    // Dynamic Range calculation across observed non-zero velocities
    if (!validRmsForFit.empty())
    {
        double maxRms = *std::max_element(validRmsForFit.begin(), validRmsForFit.end());
        double minRms = *std::min_element(validRmsForFit.begin(), validRmsForFit.end());
        dyn.dynamicRangeDb = std::max(0.0, maxRms - minRms);
    }
    else
    {
        dyn.dynamicRangeDb = 0.0;
    }

    // Curve Fitting without assuming linearity
    if (validVelocitiesForFit.size() >= 3)
    {
        dyn.amplitudeFit = fitCurveModel(validVelocitiesForFit, validRmsForFit, "velocity", "rmsDbfs");
        dyn.brightnessFit = fitCurveModel(validVelocitiesForFit, validBrightnessForFit, "velocity", "spectralCentroidHz");
    }

    // Empirical Discontinuity Detection (Ajuste 2: observation, not layer claim)
    dyn.discontinuity = detectDiscontinuity(velocities, rmsLevels, 6.0);

    // Spectral analysis metadata
    SpectralAnalysisMetadata specMeta;
    specMeta.fftSize = 2048;
    specMeta.hopSize = 512;
    specMeta.window = "hann";
    specMeta.frequencyResolutionHz = sampleRate / 2048.0;
    specMeta.averagingCount = 1;
    dyn.spectralMetadata = specMeta;

    result.dynamicResult = dyn;
    result.curve = dyn.amplitudeCurve;
    result.status = MeasurementStatus::completed;
    result.reason = "dynamic_velocity_series_analyzed";
    result.observability.status = "observed";

    // Populate summary metrics
    MeasurementMetric drMet;
    drMet.name = "dynamic_range";
    drMet.value = dyn.dynamicRangeDb;
    drMet.unit = "dB";
    drMet.status = "observed";
    result.metrics.push_back(drMet);

    if (dyn.amplitudeFit.has_value())
    {
        MeasurementMetric r2Met;
        r2Met.name = juce::String("amplitude_fit_r2_") + juce::String(dyn.amplitudeFit->model);
        r2Met.value = dyn.amplitudeFit->rSquared;
        r2Met.unit = "ratio";
        r2Met.status = "observed";
        result.metrics.push_back(r2Met);
    }

    MeasurementMetric discMet;
    discMet.name = "discontinuity_detected";
    discMet.value = dyn.discontinuity.detected ? 1.0 : 0.0;
    discMet.unit = "bool";
    discMet.status = "observed";
    if (dyn.discontinuity.detected)
        discMet.reason = dyn.discontinuity.reason;
    result.metrics.push_back(discMet);

    return result;
}

DynamicPoint DynamicsMeasurementAdapter::analyzeSinglePoint(int velocity,
                                                           const std::vector<float>& audio,
                                                           double sampleRate,
                                                           size_t noteOnSample,
                                                           size_t noteOffSample,
                                                           double windowStartMs,
                                                           double windowEndMs,
                                                           const std::string& presetStateHash,
                                                           const std::string& audioArtifactHash)
{
    DynamicPoint pt;
    pt.velocity = velocity;
    pt.presetStateHash = presetStateHash;
    pt.audioArtifactHash = audioArtifactHash;

    if (velocity == 0)
    {
        pt.peakDbfs = -96.0;
        pt.rmsDbfs = -96.0;
        pt.spectralCentroidHz = 0.0;
        pt.spectralRolloffHz = 0.0;
        pt.attackTimeMs = 0.0;
        pt.status = "skipped";
        pt.reason = "midi_note_on_velocity_zero";
        pt.measurementWindowStartMs = windowStartMs;
        pt.measurementWindowEndMs = windowEndMs;
        return pt;
    }

    if (audio.empty() || sampleRate <= 0.0)
    {
        pt.status = "unreliable";
        pt.reason = "empty_audio_buffer";
        return pt;
    }

    // Check for NaN or Inf
    for (float s : audio)
    {
        if (std::isnan(s) || std::isinf(s))
        {
            pt.status = "invalid";
            pt.reason = "non_finite_audio_samples";
            return pt;
        }
    }

    // Measure overall peak amplitude
    float maxAbs = 0.0f;
    size_t peakIdx = noteOnSample;
    for (size_t i = 0; i < audio.size(); ++i)
    {
        float a = std::abs(audio[i]);
        if (a > maxAbs)
        {
            maxAbs = a;
            peakIdx = i;
        }
    }

    if (maxAbs < 1e-4f)
    {
        pt.peakDbfs = maxAbs > 1e-9f ? 20.0 * std::log10(maxAbs) : -96.0;
        pt.rmsDbfs = -96.0;
        pt.spectralCentroidHz = 0.0; // Do not invent zeros for spectral properties
        pt.spectralRolloffHz = 0.0;
        pt.attackTimeMs = 0.0;
        pt.status = "unreliable";
        pt.reason = "signal_below_noise_floor_or_too_short";
        return pt;
    }

    pt.peakDbfs = 20.0 * std::log10(std::max(maxAbs, 1e-6f));

    // Attack Time: Time from Note-On to 90% peak or peakIdx
    float thresh10 = 0.10f * maxAbs;
    float thresh90 = 0.90f * maxAbs;
    size_t t10 = noteOnSample;
    size_t t90 = peakIdx;
    bool found10 = false;

    for (size_t i = noteOnSample; i <= peakIdx && i < audio.size(); ++i)
    {
        float a = std::abs(audio[i]);
        if (!found10 && a >= thresh10)
        {
            t10 = i;
            found10 = true;
        }
        if (found10 && a >= thresh90)
        {
            t90 = i;
            break;
        }
    }

    if (t90 > t10)
        pt.attackTimeMs = (static_cast<double>(t90 - t10) / sampleRate) * 1000.0;
    else
        pt.attackTimeMs = (static_cast<double>(peakIdx > noteOnSample ? peakIdx - noteOnSample : 0) / sampleRate) * 1000.0;

    // Determine steady-state measurement window
    size_t startSample = 0;
    size_t endSample = audio.size();

    if (windowStartMs > 0.0 && windowEndMs > windowStartMs)
    {
        startSample = static_cast<size_t>((windowStartMs / 1000.0) * sampleRate);
        endSample = static_cast<size_t>((windowEndMs / 1000.0) * sampleRate);
        startSample = std::min(startSample, audio.size());
        endSample = std::min(endSample, audio.size());
    }
    else
    {
        // Auto-window: from after attack to before note-off
        size_t noteEnd = noteOffSample > noteOnSample ? noteOffSample : audio.size();
        size_t postAttack = peakIdx + static_cast<size_t>(sampleRate * 0.02);
        if (postAttack < noteEnd)
        {
            startSample = postAttack;
            endSample = noteEnd;
        }
        else
        {
            startSample = noteOnSample;
            endSample = std::min(noteEnd, audio.size());
        }
    }

    if (startSample >= endSample)
    {
        startSample = 0;
        endSample = audio.size();
    }

    pt.measurementWindowStartMs = (static_cast<double>(startSample) / sampleRate) * 1000.0;
    pt.measurementWindowEndMs = (static_cast<double>(endSample) / sampleRate) * 1000.0;

    // Steady-state RMS level
    double sumSq = 0.0;
    size_t count = endSample - startSample;
    for (size_t i = startSample; i < endSample; ++i)
    {
        double s = static_cast<double>(audio[i]);
        sumSq += s * s;
    }
    double rms = std::sqrt(sumSq / std::max<size_t>(1, count));
    pt.rmsDbfs = 20.0 * std::log10(std::max(rms, 1e-6));

    // Spectral Centroid & Rolloff via FFT (2048 points with Hann window)
    constexpr int kFftOrder = 11; // 2048 points
    constexpr int kFftSize = 1 << kFftOrder;
    std::vector<float> fftBuffer(static_cast<size_t>(kFftSize) * 2, 0.0f);

    size_t copyCount = std::min(static_cast<size_t>(kFftSize), count);
    for (size_t i = 0; i < copyCount; ++i)
    {
        double wHann = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(copyCount - 1)));
        fftBuffer[i] = audio[startSample + i] * static_cast<float>(wHann);
    }

    juce::dsp::FFT fft(kFftOrder);
    fft.performRealOnlyForwardTransform(fftBuffer.data());

    // Compute magnitudes and frequency centroid
    double binHz = sampleRate / static_cast<double>(kFftSize);
    int numBins = kFftSize / 2;
    double weightedFreqSum = 0.0;
    double magSum = 0.0;
    double totalEnergy = 0.0;
    std::vector<double> energyPerBin(static_cast<size_t>(numBins), 0.0);

    for (int k = 1; k < numBins; ++k)
    {
        float real = fftBuffer[static_cast<size_t>(2 * k)];
        float imag = fftBuffer[static_cast<size_t>(2 * k + 1)];
        double mag = std::sqrt(static_cast<double>(real * real + imag * imag));
        double freq = static_cast<double>(k) * binHz;

        weightedFreqSum += freq * mag;
        magSum += mag;

        double energy = mag * mag;
        energyPerBin[static_cast<size_t>(k)] = energy;
        totalEnergy += energy;
    }

    if (magSum > 1e-7)
        pt.spectralCentroidHz = weightedFreqSum / magSum;
    else
        pt.spectralCentroidHz = 0.0;

    // 85% Spectral Rolloff
    if (totalEnergy > 1e-12)
    {
        double cumulative = 0.0;
        double targetEnergy = 0.85 * totalEnergy;
        double rolloffHz = static_cast<double>(numBins) * binHz;
        for (int k = 1; k < numBins; ++k)
        {
            cumulative += energyPerBin[static_cast<size_t>(k)];
            if (cumulative >= targetEnergy)
            {
                rolloffHz = static_cast<double>(k) * binHz;
                break;
            }
        }
        pt.spectralRolloffHz = rolloffHz;
    }
    else
    {
        pt.spectralRolloffHz = 0.0;
    }

    pt.status = "observed";
    pt.reason = "";

    return pt;
}

CurveFitMetadata DynamicsMeasurementAdapter::fitCurveModel(const std::vector<double>& x,
                                                           const std::vector<double>& y,
                                                           const std::string& xVar,
                                                           const std::string& yVar)
{
    CurveFitMetadata bestFit;
    bestFit.model = "none";
    bestFit.rSquared = 0.0;
    bestFit.xVariable = xVar;
    bestFit.yVariable = yVar;

    size_t n = x.size();
    if (n < 3 || y.size() != n)
        return bestFit;

    double yMean = std::accumulate(y.begin(), y.end(), 0.0) / static_cast<double>(n);
    double ssTot = 0.0;
    for (double val : y)
        ssTot += (val - yMean) * (val - yMean);

    if (ssTot < 1e-9)
    {
        bestFit.model = "linear";
        bestFit.rSquared = 1.0;
        return bestFit;
    }

    // 1. Model: Linear (y = a*x + b)
    double xMean = std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(n);
    double ssXx = 0.0;
    double ssXy = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        ssXx += (x[i] - xMean) * (x[i] - xMean);
        ssXy += (x[i] - xMean) * (y[i] - yMean);
    }

    double aLin = (ssXx > 1e-9) ? (ssXy / ssXx) : 0.0;
    double bLin = yMean - aLin * xMean;
    double ssResLin = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double yPred = aLin * x[i] + bLin;
        ssResLin += (y[i] - yPred) * (y[i] - yPred);
    }
    double r2Lin = std::clamp(1.0 - (ssResLin / ssTot), 0.0, 1.0);

    // 2. Model: Logarithmic (y = a*ln(x) + b) for x > 0
    bool canLog = true;
    for (double val : x)
    {
        if (val <= 0.0) { canLog = false; break; }
    }

    double r2Log = 0.0;
    if (canLog)
    {
        std::vector<double> lnX(n, 0.0);
        for (size_t i = 0; i < n; ++i)
            lnX[i] = std::log(x[i]);

        double lnXMean = std::accumulate(lnX.begin(), lnX.end(), 0.0) / static_cast<double>(n);
        double ssLnX = 0.0;
        double ssLnXY = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            ssLnX += (lnX[i] - lnXMean) * (lnX[i] - lnXMean);
            ssLnXY += (lnX[i] - lnXMean) * (y[i] - yMean);
        }

        double aLog = (ssLnX > 1e-9) ? (ssLnXY / ssLnX) : 0.0;
        double bLog = yMean - aLog * lnXMean;
        double ssResLog = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            double yPred = aLog * lnX[i] + bLog;
            ssResLog += (y[i] - yPred) * (y[i] - yPred);
        }
        r2Log = std::clamp(1.0 - (ssResLog / ssTot), 0.0, 1.0);
    }

    // 3. Select best model without assuming linearity equals good
    if (r2Log > r2Lin && r2Log >= 0.60)
    {
        bestFit.model = "logarithmic";
        bestFit.rSquared = r2Log;
    }
    else if (r2Lin >= 0.60)
    {
        bestFit.model = "linear";
        bestFit.rSquared = r2Lin;
    }
    else
    {
        // Low R^2 across simple continuous models indicates non-linear / piecewise / layer structure
        bestFit.model = "piecewise";
        bestFit.rSquared = std::max(r2Lin, r2Log);
    }

    return bestFit;
}

DiscontinuityObservation DynamicsMeasurementAdapter::detectDiscontinuity(const std::vector<int>& velocities,
                                                                         const std::vector<double>& values,
                                                                         double jumpThresholdDb)
{
    DiscontinuityObservation obs;
    obs.detected = false;
    obs.lowerVelocity = 0;
    obs.upperVelocity = 0;
    obs.jumpDb = 0.0;
    obs.confidence = 0.0;
    obs.reason = "";

    // Calculate global slope across non-zero valid points
    std::vector<int> validV;
    std::vector<double> validY;
    for (size_t i = 0; i < velocities.size(); ++i)
    {
        if (velocities[i] > 0 && values[i] > -85.0)
        {
            validV.push_back(velocities[i]);
            validY.push_back(values[i]);
        }
    }

    double globalSlope = 0.0;
    if (validV.size() >= 2 && validV.back() > validV.front())
    {
        globalSlope = (validY.back() - validY.front()) / static_cast<double>(validV.back() - validV.front());
    }

    double maxJump = 0.0;
    int bestLower = 0;
    int bestUpper = 0;

    for (size_t i = 0; i < velocities.size() - 1; ++i)
    {
        int v0 = velocities[i];
        int v1 = velocities[i + 1];

        // Skip transitions from velocity 0 (which simply activate the note)
        if (v0 == 0 || values[i] <= -85.0)
            continue;

        double delta = values[i + 1] - values[i];
        int dv = std::max(1, v1 - v0);
        double localSlope = delta / static_cast<double>(dv);

        // Discontinuity observed if jump exceeds threshold AND represents an anomalous gradient
        // (or non-monotonic reversal) compared to the global slope
        bool isAnomalous = (std::abs(delta) >= jumpThresholdDb);
        if (isAnomalous && globalSlope > 1e-4)
        {
            // If local slope is at least 1.5x the global slope or negative
            isAnomalous = (localSlope >= 1.4 * globalSlope) || (localSlope < 0.0);
        }

        if (isAnomalous && std::abs(delta) > maxJump)
        {
            maxJump = std::abs(delta);
            bestLower = v0;
            bestUpper = v1;
        }
    }

    if (maxJump >= jumpThresholdDb)
    {
        obs.detected = true;
        obs.lowerVelocity = bestLower;
        obs.upperVelocity = bestUpper;
        obs.jumpDb = maxJump;
        obs.confidence = std::clamp(maxJump / 12.0, 0.5, 0.99);
        // Metrological requirement: strictly "discontinuity observed", never "layer switching confirmed"
        obs.reason = "discontinuity observed between velocity " + juce::String(bestLower) +
                     " and " + juce::String(bestUpper) + " (jump: " + juce::String(maxJump, 1) + " dB)";
    }

    return obs;
}

} // namespace abdaudiolab::measurement
