/**
 * @file ComplexEnvelopeAnalyzer.cpp
 * @brief Implementation of lockstep MultiDomainEnvelopeAnalyzer and SyntheticEnvelopeControlGenerator.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

namespace abdaudiolab::measurement
{

// ============================================================================
// Downmix & Unguided Detection Utilities
// ============================================================================

std::vector<float> ComplexEnvelopeAnalyzer::downmixToMono(
    const std::vector<float>& audioBuffer,
    int numChannels,
    ChannelDownmixPolicy policy)
{
    if (audioBuffer.empty())
        return {};

    if (numChannels <= 1)
        return audioBuffer;

    const size_t numFrames = audioBuffer.size() / static_cast<size_t>(numChannels);
    if (numFrames == 0)
        return {};

    if (policy == ChannelDownmixPolicy::MonoOnly)
    {
        // Rejects multi-channel when MonoOnly is explicitly enforced
        return {};
    }

    std::vector<float> mono(numFrames, 0.0f);

    if (policy == ChannelDownmixPolicy::LeftChannelOnly)
    {
        for (size_t i = 0; i < numFrames; ++i)
            mono[i] = audioBuffer[i * static_cast<size_t>(numChannels)];
    }
    else if (policy == ChannelDownmixPolicy::RightChannelOnly)
    {
        const size_t ch = std::min(static_cast<size_t>(1), static_cast<size_t>(numChannels - 1));
        for (size_t i = 0; i < numFrames; ++i)
            mono[i] = audioBuffer[i * static_cast<size_t>(numChannels) + ch];
    }
    else // AverageToMono
    {
        const float invCh = 1.0f / static_cast<float>(numChannels);
        for (size_t i = 0; i < numFrames; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                sum += audioBuffer[i * static_cast<size_t>(numChannels) + static_cast<size_t>(c)];
            mono[i] = sum * invCh;
        }
    }

    return mono;
}

std::optional<size_t> ComplexEnvelopeAnalyzer::detectEnergyOnset(
    const std::vector<float>& monoAudio,
    double sampleRate,
    double noiseFloorDbfs,
    double ratioOfPeak)
{
    if (monoAudio.empty() || sampleRate <= 0.0)
        return std::nullopt;

    float peak = 0.0f;
    for (float s : monoAudio)
    {
        float a = std::abs(s);
        if (a > peak) peak = a;
    }

    if (peak < 1e-5f)
        return std::nullopt; // Pure silence

    const float minNoiseLinear = static_cast<float>(std::pow(10.0, noiseFloorDbfs / 20.0));
    const float threshold = std::max(peak * static_cast<float>(ratioOfPeak), minNoiseLinear * 2.0f);

    const size_t winSize = 64;
    if (monoAudio.size() < winSize)
        return std::nullopt;

    for (size_t i = 0; i + winSize <= monoAudio.size(); i += 32)
    {
        float sumSq = 0.0f;
        for (size_t n = 0; n < winSize; ++n)
        {
            float v = monoAudio[i + n];
            sumSq += v * v;
        }
        float rms = std::sqrt(sumSq / static_cast<float>(winSize));
        if (rms >= threshold)
        {
            for (size_t n = 0; n < winSize; ++n)
            {
                if (std::abs(monoAudio[i + n]) >= threshold * 0.5f)
                    return i + n;
            }
            return i;
        }
    }

    return std::nullopt;
}

std::optional<size_t> ComplexEnvelopeAnalyzer::detectEnergyDecay(
    const std::vector<float>& monoAudio,
    size_t noteOnSample,
    double sampleRate,
    double noiseFloorDbfs)
{
    if (monoAudio.empty() || noteOnSample >= monoAudio.size() || sampleRate <= 0.0)
        return std::nullopt;

    size_t peakSample = noteOnSample;
    float peakVal = 0.0f;
    for (size_t i = noteOnSample; i < monoAudio.size(); ++i)
    {
        float a = std::abs(monoAudio[i]);
        if (a > peakVal)
        {
            peakVal = a;
            peakSample = i;
        }
    }

    if (peakVal < 1e-4f)
        return std::nullopt;

    const size_t winSize = 128;
    const float decayThreshold = peakVal * 0.15f; // Drop to 15% of peak (-16.5 dB)
    const float noiseFloorLinear = static_cast<float>(std::pow(10.0, noiseFloorDbfs / 20.0));

    for (size_t i = peakSample; i + winSize <= monoAudio.size(); i += 64)
    {
        float sumSq = 0.0f;
        for (size_t n = 0; n < winSize; ++n)
        {
            float v = monoAudio[i + n];
            sumSq += v * v;
        }
        float rms = std::sqrt(sumSq / static_cast<float>(winSize));
        if (rms <= std::max(decayThreshold, noiseFloorLinear * 3.0f))
        {
            return i;
        }
    }

    return std::nullopt;
}

// ============================================================================
// ComplexEnvelopeAnalyzer Core Implementation
// ============================================================================

MultiDomainEnvelopeCaptureRecord ComplexEnvelopeAnalyzer::analyze(
    const ComplexEnvelopeAnalysisInput& input,
    const ComplexEnvelopeAnalyzerConfig& config)
{
    MultiDomainEnvelopeCaptureRecord record;
    record.captureId = !input.captureId.empty() ? input.captureId : "capture_envelope_001";
    record.dut = input.dut;
    if (record.dut.name.empty()) record.dut.name = "GenericDUT";
    if (record.dut.model.empty()) record.dut.model = "AgnosticAudioModel";

    record.excitationFrequencyHz = input.nominalFrequencyHz;
    record.midiVelocity = input.midiVelocity;
    record.audioSha256 = input.audioSha256;
    record.stimulusSha256 = input.stimulusSha256;
    record.nativePatchStateSha256 = input.nativePatchStateSha256;

    // Downmix to mono
    std::vector<float> monoAudio = downmixToMono(input.audioBuffer, input.numChannels, input.downmixPolicy);

    // Effective sample rate handling (metadata tracking if input != config)
    const double effectiveSampleRate = (input.sampleRate > 0.0) ? input.sampleRate : config.sampleRateHz;
    const int hopSize = (config.hopSizeSamples > 0) ? config.hopSizeSamples : 256;
    const int winLen = (config.windowLengthSamples > 0) ? config.windowLengthSamples : 1024;
    int fftSize = (config.fftSize >= winLen) ? config.fftSize : 2048;

    // Ensure power of two for FFT
    int fftOrder = 11; // 2048 default
    while ((1 << fftOrder) < fftSize)
        ++fftOrder;
    fftSize = 1 << fftOrder;

    const double totalDurationMs = (effectiveSampleRate > 0.0)
        ? (static_cast<double>(monoAudio.size()) / effectiveSampleRate) * 1000.0
        : 0.0;
    record.totalDurationMs = totalDurationMs;

    // Determine onset & note-off markers
    std::optional<size_t> effectiveNoteOnSample;
    std::string effectiveNoteOnMethod;

    if (input.noteOnSample.has_value())
    {
        effectiveNoteOnSample = *input.noteOnSample;
        effectiveNoteOnMethod = !input.noteOnMethod.empty() ? input.noteOnMethod : "provided_midi_event";
    }
    else
    {
        effectiveNoteOnSample = detectEnergyOnset(monoAudio, effectiveSampleRate, config.noiseFloorDbfs, config.onsetEnergyThresholdRatio);
        if (effectiveNoteOnSample.has_value())
            effectiveNoteOnMethod = "energy_onset";
        else
            effectiveNoteOnMethod = "not_available";
    }

    std::optional<size_t> effectiveNoteOffSample;
    std::string effectiveNoteOffMethod;

    if (input.noteOffSample.has_value())
    {
        effectiveNoteOffSample = *input.noteOffSample;
        effectiveNoteOffMethod = !input.noteOffMethod.empty() ? input.noteOffMethod : "provided_midi_event";
    }
    else if (effectiveNoteOnSample.has_value())
    {
        effectiveNoteOffSample = detectEnergyDecay(monoAudio, *effectiveNoteOnSample, effectiveSampleRate, config.noiseFloorDbfs);
        if (effectiveNoteOffSample.has_value())
            effectiveNoteOffMethod = "energy_decay";
        else
            effectiveNoteOffMethod = "not_available";
    }
    else
    {
        effectiveNoteOffMethod = "not_available";
    }

    if (effectiveNoteOnSample.has_value() && effectiveNoteOffSample.has_value() && *effectiveNoteOffSample >= *effectiveNoteOnSample)
    {
        record.noteDurationMs = (static_cast<double>(*effectiveNoteOffSample - *effectiveNoteOnSample) / effectiveSampleRate) * 1000.0;
    }
    else
    {
        record.noteDurationMs = totalDurationMs;
    }

    // Build unified TemporalGrid
    const int frameCount = (hopSize > 0) ? static_cast<int>(monoAudio.size() / static_cast<size_t>(hopSize)) : 0;
    const double hopMs = (effectiveSampleRate > 0.0) ? (static_cast<double>(hopSize) / effectiveSampleRate) * 1000.0 : 0.0;
    const std::string gridId = "grid_" + std::to_string(static_cast<int>(std::round(effectiveSampleRate))) + "_hop" + std::to_string(hopSize);

    record.temporalGrid.gridId = gridId;
    record.temporalGrid.originMs = 0.0;
    record.temporalGrid.hopMs = hopMs;
    record.temporalGrid.frameCount = frameCount;
    record.temporalGrid.alignmentMethod = "stft_hop_synchronous";

    // Setup Trajectory structures
    EnvelopeSpectralMetadata spectralMeta;
    spectralMeta.sampleRateHz = effectiveSampleRate;
    spectralMeta.hopSizeSamples = hopSize;
    spectralMeta.fftSize = fftSize;
    spectralMeta.windowLengthSamples = winLen;
    spectralMeta.windowFunction = config.windowFunction;
    spectralMeta.noiseFloorDbfs = config.noiseFloorDbfs;

    auto initTraj = [&](EnvelopeTrajectory& traj, EnvelopeDomain domain, const std::string& label, const std::string& method) {
        traj.domain = domain;
        traj.trajectoryLabel = label;
        traj.temporalGridId = gridId;
        traj.alignmentStatus = "aligned";
        traj.extractionMethod = method;
        traj.nativeEnvelopeReconstruction = "not_claimed";
        traj.phaseDistortionProxy = "not_claimed";
        traj.normalizationReference = config.normalizationReference;
        traj.noteOnMethod = effectiveNoteOnMethod;
        traj.noteOffMethod = effectiveNoteOffMethod;
        traj.spectralMetadata = spectralMeta;
        traj.points.reserve(static_cast<size_t>(frameCount));
    };

    initTraj(record.pitchTrajectory, EnvelopeDomain::Pitch, config.pitchLabel, "stft_parabolic_peak_tracking");
    initTraj(record.timbreTrajectory, EnvelopeDomain::Timbre, config.timbreLabel, "stft_spectral_centroid_tracking");
    initTraj(record.amplitudeTrajectory, EnvelopeDomain::Amplitude, config.amplitudeLabel, "rms_windowed");

    // Global peak search for normalization
    float globalPeak = 0.0f;
    for (float s : monoAudio)
    {
        float a = std::abs(s);
        if (a > globalPeak) globalPeak = a;
    }
    if (globalPeak < 1e-6f) globalPeak = 1.0f;

    // Prepare FFT and Window buffer
    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftBuf(static_cast<size_t>(fftSize) * 2, 0.0f);
    std::vector<float> window(static_cast<size_t>(winLen), 0.0f);

    for (size_t n = 0; n < static_cast<size_t>(winLen); ++n)
    {
        // Hann window
        window[n] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f * static_cast<float>(n) / static_cast<float>(winLen - 1)));
    }

    const double binHz = effectiveSampleRate / static_cast<double>(fftSize);

    // Lockstep Frame Processing
    for (int frameIdx = 0; frameIdx < frameCount; ++frameIdx)
    {
        const double timeMs = frameIdx * hopMs;
        const size_t startSample = static_cast<size_t>(frameIdx * hopSize);

        // 1. DCA (Amplitude / RMS)
        double sumSq = 0.0;
        float peakInFrame = 0.0f;
        const size_t availableSamples = (startSample < monoAudio.size()) ? std::min<size_t>(static_cast<size_t>(winLen), monoAudio.size() - startSample) : 0;

        for (size_t n = 0; n < availableSamples; ++n)
        {
            float s = monoAudio[startSample + n];
            float a = std::abs(s);
            if (a > peakInFrame) peakInFrame = a;
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }

        const double frameRms = (availableSamples > 0) ? std::sqrt(sumSq / static_cast<double>(availableSamples)) : 0.0;
        const double frameRmsDbfs = (frameRms > 1e-7) ? (20.0 * std::log10(frameRms)) : config.noiseFloorDbfs;

        EnvelopeObservationPoint ptAmp;
        ptAmp.frameIndex = frameIdx;
        ptAmp.timeMs = timeMs;
        ptAmp.unit = "dBFS";
        ptAmp.normalizationReference = config.normalizationReference;
        ptAmp.resolution = 0.01;
        ptAmp.uncertaintyStatus = "not_estimated";

        const bool isSilent = (frameRmsDbfs <= config.silenceThresholdDbfs) || (availableSamples == 0);

        if (isSilent)
        {
            ptAmp.status = "silence";
            ptAmp.value = std::nullopt;
            ptAmp.rmsDbfs = std::nullopt;
            ptAmp.amplitudeNormalized = std::nullopt;
            ptAmp.confidence = 0.0;
            ptAmp.reason = "silence";
        }
        else
        {
            ptAmp.status = "valid";
            ptAmp.value = frameRmsDbfs;
            ptAmp.rmsDbfs = frameRmsDbfs;
            if (config.normalizationReference == "full_scale")
                ptAmp.amplitudeNormalized = static_cast<double>(peakInFrame);
            else // peak_observed
                ptAmp.amplitudeNormalized = static_cast<double>(peakInFrame / globalPeak);
            ptAmp.confidence = 1.0;
        }
        record.amplitudeTrajectory.points.push_back(ptAmp);

        // 2 & 3. DCW (Timbre) and DCO (Pitch)
        EnvelopeObservationPoint ptTimbre;
        ptTimbre.frameIndex = frameIdx;
        ptTimbre.timeMs = timeMs;
        ptTimbre.unit = "spectralCentroidHz";
        ptTimbre.resolution = 0.01;
        ptTimbre.uncertaintyStatus = "not_estimated";

        EnvelopeObservationPoint ptPitch;
        ptPitch.frameIndex = frameIdx;
        ptPitch.timeMs = timeMs;
        ptPitch.unit = config.pitchUnit;
        ptPitch.resolution = 0.01;
        ptPitch.uncertaintyStatus = "not_estimated";

        if (isSilent)
        {
            ptTimbre.status = "silence";
            ptTimbre.value = std::nullopt;
            ptTimbre.confidence = 0.0;
            ptTimbre.reason = "silence";

            ptPitch.status = "silence";
            ptPitch.value = std::nullopt;
            ptPitch.confidence = 0.0;
            ptPitch.voicedStatus = "silence";
            ptPitch.reason = "silence";
        }
        else
        {
            // Prepare windowed FFT buffer
            std::fill(fftBuf.begin(), fftBuf.end(), 0.0f);
            for (size_t n = 0; n < availableSamples; ++n)
            {
                fftBuf[n] = monoAudio[startSample + n] * window[n];
            }

            fft.performRealOnlyForwardTransform(fftBuf.data());

            // Compute magnitudes
            const int maxBin = fftSize / 2;
            std::vector<double> mags(static_cast<size_t>(maxBin), 0.0);
            double numCentroid = 0.0;
            double denCentroid = 0.0;
            double maxMag = -1.0;
            int peakBin = -1;

            for (int b = 1; b < maxBin; ++b)
            {
                float re = fftBuf[static_cast<size_t>(2 * b)];
                float im = fftBuf[static_cast<size_t>(2 * b + 1)];
                double m = std::sqrt(static_cast<double>(re * re + im * im));
                mags[static_cast<size_t>(b)] = m;

                const double freqHz = b * binHz;
                numCentroid += freqHz * m;
                denCentroid += m;

                if (m > maxMag)
                {
                    maxMag = m;
                    peakBin = b;
                }
            }

            // Timbre / Spectral Centroid extraction
            if (denCentroid > 1e-6)
            {
                ptTimbre.status = "valid";
                ptTimbre.value = numCentroid / denCentroid;
                ptTimbre.confidence = 1.0;
            }
            else
            {
                ptTimbre.status = "silence";
                ptTimbre.value = std::nullopt;
                ptTimbre.confidence = 0.0;
                ptTimbre.reason = "insufficient_spectral_energy";
            }

            // Pitch extraction & Ambiguity detection
            // Check if spectral distribution is flat/noise or multi-peak ambiguity
            double avgMag = denCentroid / static_cast<double>(maxBin);
            double peakToAvg = (avgMag > 1e-9) ? (maxMag / avgMag) : 0.0;

            // Check harmonicity / prominence of peak
            bool isAmbiguous = false;
            if (peakToAvg < 4.0 || peakBin <= 1 || peakBin >= maxBin - 2)
            {
                isAmbiguous = true;
            }
            else
            {
                // Check if second-highest peak is almost equal in amplitude (dual tone ambiguity)
                double secondMaxMag = -1.0;
                for (int b = 1; b < maxBin; ++b)
                {
                    if (std::abs(b - peakBin) > 2 && mags[static_cast<size_t>(b)] > secondMaxMag)
                    {
                        secondMaxMag = mags[static_cast<size_t>(b)];
                    }
                }
                if (secondMaxMag > 0.0 && (secondMaxMag / maxMag) > 0.85)
                {
                    isAmbiguous = true; // Ambiguous fundamental
                }
            }

            if (isAmbiguous)
            {
                ptPitch.status = "unreliable";
                ptPitch.value = std::nullopt;
                ptPitch.reason = "ambiguous_f0";
                ptPitch.confidence = 0.0;
                ptPitch.voicedStatus = "unvoiced";
            }
            else
            {
                // Sub-bin parabolic interpolation
                size_t p = static_cast<size_t>(peakBin);
                double alpha = std::log(std::max(mags[p - 1], 1e-12));
                double beta  = std::log(std::max(mags[p],     1e-12));
                double gamma = std::log(std::max(mags[p + 1], 1e-12));

                double denom = alpha - 2.0 * beta + gamma;
                double delta = (std::abs(denom) > 1e-12) ? 0.5 * (alpha - gamma) / denom : 0.0;
                delta = std::clamp(delta, -0.5, 0.5);

                double refinedBin = static_cast<double>(peakBin) + delta;
                double estimatedFreqHz = refinedBin * binHz;

                ptPitch.peakBin = peakBin;
                ptPitch.interpolatedBin = refinedBin;
                ptPitch.voicedStatus = "voiced";
                ptPitch.confidence = std::clamp((peakToAvg - 4.0) / 10.0, 0.2, 1.0);

                if (estimatedFreqHz >= 20.0 && estimatedFreqHz <= 18000.0)
                {
                    ptPitch.status = "valid";
                    if (config.pitchUnit == "cents")
                    {
                        double baseFreq = (input.nominalFrequencyHz > 0.0) ? input.nominalFrequencyHz : 261.6256;
                        double cents = 1200.0 * std::log2(estimatedFreqHz / baseFreq);
                        ptPitch.value = cents;
                    }
                    else // Hz
                    {
                        ptPitch.value = estimatedFreqHz;
                    }
                }
                else
                {
                    ptPitch.status = "unreliable";
                    ptPitch.value = std::nullopt;
                    ptPitch.reason = "ambiguous_f0";
                    ptPitch.voicedStatus = "unvoiced";
                }
            }
        }

        record.timbreTrajectory.points.push_back(ptTimbre);
        record.pitchTrajectory.points.push_back(ptPitch);
    }

    // Global Timing Metrics: Attack Time & Release Time
    if (effectiveNoteOnSample.has_value() && effectiveSampleRate > 0.0)
    {
        size_t onSample = *effectiveNoteOnSample;
        size_t peakSample = onSample;
        float peakVal = 0.0f;
        for (size_t i = onSample; i < monoAudio.size(); ++i)
        {
            float a = std::abs(monoAudio[i]);
            if (a > peakVal)
            {
                peakVal = a;
                peakSample = i;
            }
        }
        if (peakVal > 1e-4f && peakSample >= onSample)
        {
            record.amplitudeTrajectory.attackTimeMs = (static_cast<double>(peakSample - onSample) / effectiveSampleRate) * 1000.0;
            record.timbreTrajectory.attackTimeMs = record.amplitudeTrajectory.attackTimeMs;
            record.pitchTrajectory.attackTimeMs = record.amplitudeTrajectory.attackTimeMs;
        }
    }

    if (effectiveNoteOffSample.has_value() && effectiveSampleRate > 0.0)
    {
        size_t offSample = *effectiveNoteOffSample;
        float minNoiseLinear = static_cast<float>(std::pow(10.0, config.noiseFloorDbfs / 20.0));
        size_t silenceSample = monoAudio.size();

        for (size_t i = offSample; i < monoAudio.size(); ++i)
        {
            if (std::abs(monoAudio[i]) <= minNoiseLinear * 3.0f)
            {
                silenceSample = i;
                break;
            }
        }
        if (silenceSample >= offSample)
        {
            record.amplitudeTrajectory.releaseTimeMs = (static_cast<double>(silenceSample - offSample) / effectiveSampleRate) * 1000.0;
            record.timbreTrajectory.releaseTimeMs = record.amplitudeTrajectory.releaseTimeMs;
            record.pitchTrajectory.releaseTimeMs = record.amplitudeTrajectory.releaseTimeMs;
        }
    }

    return record;
}

// ============================================================================
// Synthetic Control Signal Generator
// ============================================================================

SyntheticEnvelopeControlGenerator::GenerationResult
SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal(const GenerationParams& params)
{
    GenerationResult res;
    res.sampleRate = (params.sampleRate > 0.0) ? params.sampleRate : 48000.0;
    res.numChannels = params.numChannels > 0 ? params.numChannels : 1;

    const double totalDurSec = params.totalDurationMs / 1000.0;
    const size_t totalSamples = static_cast<size_t>(totalDurSec * res.sampleRate);
    res.noteOnSample = static_cast<size_t>((params.noteOnMs / 1000.0) * res.sampleRate);
    res.noteOffSample = static_cast<size_t>((params.noteOffMs / 1000.0) * res.sampleRate);

    std::vector<float> mono(totalSamples, 0.0f);

    // Setup Synthetic Ground Truth with 8 stages
    res.groundTruth.expectedF0ToleranceHz = 4.0;
    res.groundTruth.expectedCentroidToleranceHz = 60.0;
    res.groundTruth.expectedRmsToleranceDb = 2.0;

    // Generate 8 stages for amplitude:
    // S1: Pre-onset silence (50ms)
    // S2: Attack ramp (60ms)
    // S3: Decay 1 (100ms)
    // S4: Decay 2 (150ms)
    // S5: Sustain (390ms)
    // S6: NoteOff release initial (100ms)
    // S7: Release final (150ms)
    // S8: Post-decay silence (190ms)
    for (int i = 1; i <= 8; ++i)
    {
        EnvelopeStageDescriptor st;
        st.stageIndex = i;
        st.parameterization = "rate_level";
        st.durationMs = 150.0;
        st.targetLevel = (i == 2) ? 1.0 : ((i >= 3 && i <= 5) ? 0.7 : 0.0);
        st.isSustainPoint = (i == 5);
        st.isEndKeyOnPoint = (i == 5);
        res.groundTruth.amplitudeStages.push_back(st);
        res.groundTruth.pitchStages.push_back(st);
        res.groundTruth.timbreStages.push_back(st);
    }

    // Mathematical Phase Distortion Synthesis:
    // y(t) = sin( phi(t) + I(t) * sin(phi(t)) ) * A(t)
    double phase = 0.0;
    const double twoPi = 2.0 * 3.14159265358979323846;

    for (size_t n = 0; n < totalSamples; ++n)
    {
        double tMs = (static_cast<double>(n) / res.sampleRate) * 1000.0;
        double amp = 0.0;
        double pdIndex = 0.0;
        double f0 = params.baseFrequencyHz;

        if (n >= res.noteOnSample && n < res.noteOffSample)
        {
            double relOnMs = tMs - params.noteOnMs;
            // Attack in 60 ms
            if (relOnMs < 60.0)
            {
                amp = relOnMs / 60.0;
                pdIndex = (relOnMs / 60.0) * 1.2; // Phase distortion index rises to 1.2
            }
            else
            {
                // Decay to sustain level 0.75
                double decayProgress = std::min(1.0, (relOnMs - 60.0) / 200.0);
                amp = 1.0 - 0.25 * decayProgress;
                pdIndex = 1.2 - 0.6 * decayProgress; // PD drops to 0.6 in sustain
            }
        }
        else if (n >= res.noteOffSample)
        {
            double relOffMs = tMs - params.noteOffMs;
            if (relOffMs < 200.0)
            {
                double relProgress = relOffMs / 200.0;
                amp = 0.75 * (1.0 - relProgress);
                pdIndex = 0.6 * (1.0 - relProgress);
            }
            else
            {
                amp = 0.0;
                pdIndex = 0.0;
            }
        }

        // Advance oscillator
        phase += twoPi * (f0 / res.sampleRate);
        if (phase >= twoPi) phase -= twoPi;

        // Phase distortion formula (CZ cosine PD emulation)
        double sampleVal = std::sin(phase + pdIndex * std::sin(phase)) * amp;
        mono[n] = static_cast<float>(sampleVal);
    }

    if (res.numChannels == 1)
    {
        res.audioBuffer = std::move(mono);
    }
    else
    {
        res.audioBuffer.resize(totalSamples * static_cast<size_t>(res.numChannels));
        for (size_t n = 0; n < totalSamples; ++n)
        {
            for (int c = 0; c < res.numChannels; ++c)
                res.audioBuffer[n * static_cast<size_t>(res.numChannels) + static_cast<size_t>(c)] = mono[n];
        }
    }

    return res;
}

SyntheticEnvelopeControlGenerator::GenerationResult
SyntheticEnvelopeControlGenerator::generatePitchSweepSignal(
    double sampleRate,
    double startFreqHz,
    double endFreqHz,
    double durationMs)
{
    GenerationResult res;
    res.sampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    res.numChannels = 1;

    const double durSec = durationMs / 1000.0;
    const size_t totalSamples = static_cast<size_t>(durSec * res.sampleRate);
    res.noteOnSample = 0;
    res.noteOffSample = totalSamples;

    res.audioBuffer.resize(totalSamples, 0.0f);
    double phase = 0.0;
    const double twoPi = 2.0 * 3.14159265358979323846;

    for (size_t n = 0; n < totalSamples; ++n)
    {
        double frac = static_cast<double>(n) / static_cast<double>(totalSamples);
        double f = startFreqHz + (endFreqHz - startFreqHz) * frac;
        phase += twoPi * (f / res.sampleRate);
        if (phase >= twoPi) phase -= twoPi;

        res.audioBuffer[n] = static_cast<float>(std::sin(phase) * 0.8);
    }

    return res;
}

SyntheticEnvelopeControlGenerator::GenerationResult
SyntheticEnvelopeControlGenerator::generatePhaseDistortionSweepSignal(
    double sampleRate,
    double baseFreqHz,
    double maxIndex,
    double durationMs)
{
    GenerationResult res;
    res.sampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    res.numChannels = 1;

    const double durSec = durationMs / 1000.0;
    const size_t totalSamples = static_cast<size_t>(durSec * res.sampleRate);
    res.noteOnSample = 0;
    res.noteOffSample = totalSamples;

    res.audioBuffer.resize(totalSamples, 0.0f);
    double phase = 0.0;
    const double twoPi = 2.0 * 3.14159265358979323846;

    for (size_t n = 0; n < totalSamples; ++n)
    {
        double frac = static_cast<double>(n) / static_cast<double>(totalSamples);
        // Triangle evolution: 0 -> maxIndex -> 0
        double pd = (frac < 0.5) ? (frac * 2.0 * maxIndex) : ((1.0 - frac) * 2.0 * maxIndex);

        phase += twoPi * (baseFreqHz / res.sampleRate);
        if (phase >= twoPi) phase -= twoPi;

        res.audioBuffer[n] = static_cast<float>(std::sin(phase + pd * std::sin(phase)) * 0.8);
    }

    return res;
}

SyntheticEnvelopeControlGenerator::GenerationResult
SyntheticEnvelopeControlGenerator::generateNoiseBurstSignal(
    double sampleRate,
    double durationMs)
{
    GenerationResult res;
    res.sampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    res.numChannels = 1;

    const double durSec = durationMs / 1000.0;
    const size_t totalSamples = static_cast<size_t>(durSec * res.sampleRate);
    res.noteOnSample = static_cast<size_t>(totalSamples * 0.1);
    res.noteOffSample = static_cast<size_t>(totalSamples * 0.8);

    res.audioBuffer.resize(totalSamples, 0.0f);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.8f, 0.8f);

    for (size_t n = res.noteOnSample; n < res.noteOffSample; ++n)
    {
        // Shaped envelope over noise
        float rawNoise = dist(rng);
        res.audioBuffer[n] = rawNoise;
    }

    return res;
}

} // namespace abdaudiolab::measurement
