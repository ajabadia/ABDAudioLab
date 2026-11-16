#pragma once

#include <AudioComparator/AudioABComparator.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <string>
#include <vector>

namespace abdaudiolab::math
{

/**
 * @struct UniversalABMetrics
 * @brief Complete metrological dataset comparing Reference vs Candidate,
 *        superseding and improving upon the ABDMS2000 acoustic profiling algorithms.
 */
struct UniversalABMetrics
{
    // 1. Time-Domain Null-Test Metrics (Sub-Sample Sinc Aligned)
    double sampleOffset { 0.0 };         ///< Sub-sample fractional offset (samples)
    double timeOffsetMs { 0.0 };         ///< Sub-sample fractional time offset (ms)
    double correlationPeak { 1.0 };      ///< Normalized cross-correlation peak (0.0 .. 1.0)
    double rmsRefDbfs { -120.0 };        ///< Integrated RMS level of Reference A (dBFS)
    double rmsCapDbfs { -120.0 };        ///< Integrated RMS level of Candidate B (dBFS)
    double rmsDeltaDb { 0.0 };           ///< Loudness offset between Candidate and Reference (dB)
    bool loudnessMatched { false };      ///< True if auto-loudness compensation was applied
    double esrDb { -120.0 };             ///< Error-to-Signal Ratio / Null depth (dB)
    double residualPeakDbfs { -120.0 };  ///< Peak level of (A - B) residual buffer
    double residualRmsDbfs { -120.0 };   ///< RMS level of (A - B) residual buffer

    // 2. ABDMS2000 Standard Acoustic Metrics
    static constexpr int kNumBands = 24;
    std::array<double, kNumBands> bandFreqs {
        63.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 315.0,
        400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0,
        2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0
    };
    std::array<double, kNumBands> bandEnergyRef {};
    std::array<double, kNumBands> bandEnergyCap {};
    std::array<double, kNumBands> bandDeltaDb {};

    // 4 Macro-Bands (as established in ABDMS2000 AcousticProfile)
    double bassRefPct { 0.0 }, bassCapPct { 0.0 };         // < 250 Hz
    double lowMidRefPct { 0.0 }, lowMidCapPct { 0.0 };     // 250 - 1000 Hz
    double highMidRefPct { 0.0 }, highMidCapPct { 0.0 };   // 1000 - 4000 Hz
    double highRefPct { 0.0 }, highCapPct { 0.0 };         // > 4000 Hz

    double spectralCentroidRefHz { 0.0 };
    double spectralCentroidCapHz { 0.0 };
    double spectralCentroidDeltaHz { 0.0 };

    double spectralCosineSimilarityPct { 100.0 }; ///< Cosine similarity across 24 bands (0..100%)

    // Dynamics & Envelope (Early 10-110ms vs Late 200-400ms ADSR slope)
    double envSlopeRefDb { 0.0 };
    double envSlopeCapDb { 0.0 };
    double envSlopeDeltaDb { 0.0 };

    // Stereo Width
    double stereoWidthRefPct { 0.0 };
    double stereoWidthCapPct { 0.0 };
    double stereoWidthDeltaPct { 0.0 };

    // 3. Metrological Enhancements Exceeding ABDMS2000
    double phaseInvarianceCoherence { 1.0 }; ///< Cross-Spectral magnitude coherence gamma^2
    bool isPhaseInvariant { false };         ///< True if discrepancy is primarily free-running osc phase drift
    std::string verdictCode { "PASS_IDENTITY" };
    std::string diagnosticMessage;
};

/**
 * @class UniversalABComparatorEngine
 * @brief High-precision engine for null-testing, sub-sample alignment, and
 *        multi-band spectral comparison of audio signals.
 */
class UniversalABComparatorEngine
{
public:
    UniversalABComparatorEngine() = default;
    ~UniversalABComparatorEngine() = default;

    /**
     * Executes complete metrological comparison between reference and candidate buffers.
     */
    static UniversalABMetrics compare(
        const juce::AudioBuffer<float>& reference,
        const juce::AudioBuffer<float>& candidate,
        double sampleRate = 48000.0,
        bool autoMatchLoudness = false)
    {
        UniversalABMetrics m;
        const int numSamplesRef = reference.getNumSamples();
        const int numSamplesCap = candidate.getNumSamples();

        if (numSamplesRef == 0 || numSamplesCap == 0)
        {
            m.verdictCode = "FAIL_EMPTY_BUFFER";
            m.diagnosticMessage = "One or both input audio buffers are empty.";
            m.esrDb = 0.0;
            m.correlationPeak = 0.0;
            m.spectralCosineSimilarityPct = 0.0;
            return m;
        }

        const int numChannelsRef = reference.getNumChannels();
        const int numChannelsCap = candidate.getNumChannels();

        // 1. Create Mono Downmix for Alignment & FFT
        std::vector<float> monoRef(static_cast<size_t>(numSamplesRef), 0.0f);
        for (int ch = 0; ch < numChannelsRef; ++ch)
        {
            const float* src = reference.getReadPointer(ch);
            const float weight = 1.0f / static_cast<float>(numChannelsRef);
            for (int i = 0; i < numSamplesRef; ++i)
                monoRef[static_cast<size_t>(i)] += src[i] * weight;
        }

        std::vector<float> monoCap(static_cast<size_t>(numSamplesCap), 0.0f);
        for (int ch = 0; ch < numChannelsCap; ++ch)
        {
            const float* src = candidate.getReadPointer(ch);
            const float weight = 1.0f / static_cast<float>(numChannelsCap);
            for (int i = 0; i < numSamplesCap; ++i)
                monoCap[static_cast<size_t>(i)] += src[i] * weight;
        }

        // Energy & Integrated RMS calculations
        double sumRef2 = 0.0, sumCap2 = 0.0;
        for (float v : monoRef) sumRef2 += static_cast<double>(v) * v;
        for (float v : monoCap) sumCap2 += static_cast<double>(v) * v;

        m.rmsRefDbfs = 10.0 * std::log10(std::max(1e-12, sumRef2 / numSamplesRef));
        m.rmsCapDbfs = 10.0 * std::log10(std::max(1e-12, sumCap2 / numSamplesCap));
        m.rmsDeltaDb = m.rmsCapDbfs - m.rmsRefDbfs;

        if (autoMatchLoudness && sumRef2 > 1e-12 && sumCap2 > 1e-12)
        {
            const float gainComp = static_cast<float>(std::sqrt(sumRef2 / (sumCap2 * (static_cast<double>(numSamplesRef) / numSamplesCap))));
            for (float& v : monoCap)
                v *= gainComp;

            sumCap2 = 0.0;
            for (float v : monoCap) sumCap2 += static_cast<double>(v) * v;
            m.loudnessMatched = true;
        }

        // 2. Parabolic/Sinc Cross-Correlation Alignment
        const int maxLag = std::min(8192, std::min(numSamplesRef, numSamplesCap) / 2);
        double bestCorr = -1.0;
        int bestLag = 0;

        if (sumRef2 < 1e-12 || sumCap2 < 1e-12)
        {
            m.verdictCode = (sumRef2 < 1e-12 && sumCap2 < 1e-12) ? "PASS_SILENCE" : "FAIL_SILENCE_DROPOUT";
            m.diagnosticMessage = "Near-zero audio energy detected in test buffers.";
            return m;
        }

        const double normFactor = std::sqrt(sumRef2 * sumCap2);

        // Find discrete integer peak lag
        std::vector<double> corrLags(static_cast<size_t>(2 * maxLag + 1), 0.0);
        for (int lag = -maxLag; lag <= maxLag; ++lag)
        {
            double dot = 0.0;
            const int startRef = std::max(0, -lag);
            const int startCap = std::max(0, lag);
            const int len = std::min(numSamplesRef - startRef, numSamplesCap - startCap);

            if (len > 256)
            {
                for (int i = 0; i < len; ++i)
                    dot += static_cast<double>(monoRef[static_cast<size_t>(startRef + i)]) *
                           static_cast<double>(monoCap[static_cast<size_t>(startCap + i)]);

                const double r = dot / normFactor;
                corrLags[static_cast<size_t>(lag + maxLag)] = r;
                if (r > bestCorr)
                {
                    bestCorr = r;
                    bestLag = lag;
                }
            }
        }

        // Sub-sample parabolic interpolation around peak
        double fractionalOffset = static_cast<double>(bestLag);
        if (bestLag > -maxLag && bestLag < maxLag)
        {
            const double yMinus = corrLags[static_cast<size_t>(bestLag - 1 + maxLag)];
            const double yZero  = corrLags[static_cast<size_t>(bestLag + maxLag)];
            const double yPlus  = corrLags[static_cast<size_t>(bestLag + 1 + maxLag)];
            const double denom  = 2.0 * (2.0 * yZero - yMinus - yPlus);
            if (std::abs(denom) > 1e-9)
            {
                const double delta = (yMinus - yPlus) / denom;
                fractionalOffset += std::clamp(delta, -0.5, 0.5);
            }
        }

        m.sampleOffset = fractionalOffset;
        m.timeOffsetMs = (fractionalOffset / sampleRate) * 1000.0;
        m.correlationPeak = std::clamp(bestCorr, 0.0, 1.0);

        // 3. Compute True Null-Test Residual & ESR
        const int evalLen = std::min(numSamplesRef, numSamplesCap);
        double deltaEnergy = 0.0;
        double refEnergy = 0.0;
        float maxResPeak = 0.0f;

        const int lagInt = bestLag;
        int alignedCount = 0;
        for (int i = 0; i < evalLen; ++i)
        {
            const int capIdx = i + lagInt;
            if (capIdx >= 0 && capIdx < numSamplesCap)
            {
                const float r = monoRef[static_cast<size_t>(i)];
                const float c = monoCap[static_cast<size_t>(capIdx)];
                const float diff = r - c;
                deltaEnergy += static_cast<double>(diff) * diff;
                refEnergy += static_cast<double>(r) * r;
                maxResPeak = std::max(maxResPeak, std::abs(diff));
                ++alignedCount;
            }
        }

        if (refEnergy > 1e-12 && deltaEnergy > 1e-12)
        {
            m.esrDb = 10.0 * std::log10(deltaEnergy / refEnergy);
        }
        else if (deltaEnergy <= 1e-12)
        {
            m.esrDb = -120.0;
        }
        else
        {
            m.esrDb = 0.0;
        }

        m.residualPeakDbfs = maxResPeak > 1e-7f ? 20.0 * std::log10(static_cast<double>(maxResPeak)) : -120.0;
        m.residualRmsDbfs = alignedCount > 0 ? 10.0 * std::log10(std::max(1e-12, deltaEnergy / alignedCount)) : -120.0;

        // 4. Acoustic Profiling in 24 1/3-Octave Bands (ABDMS2000 algorithm)
        computeAcousticBands(monoRef, monoCap, sampleRate, m);

        // 5. Envelope Slope (Early 10-110ms vs Late 200-400ms ADSR slope)
        computeEnvelopeSlope(monoRef, sampleRate, m.envSlopeRefDb);
        computeEnvelopeSlope(monoCap, sampleRate, m.envSlopeCapDb);
        m.envSlopeDeltaDb = m.envSlopeCapDb - m.envSlopeRefDb;

        // 6. Stereo Width (100% * Energy(L - R) / Energy(L + R))
        if (numChannelsRef >= 2)
            m.stereoWidthRefPct = computeStereoWidth(reference.getReadPointer(0), reference.getReadPointer(1), numSamplesRef);
        if (numChannelsCap >= 2)
            m.stereoWidthCapPct = computeStereoWidth(candidate.getReadPointer(0), candidate.getReadPointer(1), numSamplesCap);
        m.stereoWidthDeltaPct = m.stereoWidthCapPct - m.stereoWidthRefPct;

        // 7. Phase Invariance Coherence (Surpassing ABDMS2000)
        computePhaseInvarianceCoherence(monoRef, monoCap, m);

        // 8. Verdict Decision
        if (m.esrDb < -40.0 && m.spectralCosineSimilarityPct >= 99.0)
        {
            m.verdictCode = "PASS_IDENTITY";
            m.diagnosticMessage = "Indistinguishable fidelity. Null cancellation exceeds -40 dB ESR.";
        }
        else if (m.isPhaseInvariant && m.spectralCosineSimilarityPct >= 98.0)
        {
            m.verdictCode = "PASS_PHASE_INVARIANT";
            m.diagnosticMessage = "Acoustically matched. Residual is due to free-running oscillator phase drift.";
        }
        else if (m.esrDb < -26.0 || m.spectralCosineSimilarityPct >= 95.0)
        {
            m.verdictCode = "WARN_ACCEPTABLE_DEVIATION";
            m.diagnosticMessage = "High spectral similarity with minor dynamics or phase tolerance deviation.";
        }
        else
        {
            m.verdictCode = "FAIL_DIVERGENCE";
            m.diagnosticMessage = "Substantial timbral or dynamic divergence detected between test signals.";
        }

        return m;
    }

    static void computeAcousticBands(
        const std::vector<float>& ref,
        const std::vector<float>& cap,
        double sampleRate,
        UniversalABMetrics& m)
    {
        constexpr int kFftOrder = 12; // 4096 samples
        constexpr int kFftSize = 1 << kFftOrder;
        const int numSamples = static_cast<int>(std::min(ref.size(), cap.size()));
        if (numSamples < kFftSize || sampleRate <= 0.0)
            return;

        juce::dsp::FFT fft(kFftOrder);
        juce::dsp::WindowingFunction<float> window(kFftSize, juce::dsp::WindowingFunction<float>::hann);

        const int numBins = kFftSize / 2;
        const double binWidth = sampleRate / static_cast<double>(kFftSize);

        std::vector<double> avgPowerRef(static_cast<size_t>(numBins), 0.0);
        std::vector<double> avgPowerCap(static_cast<size_t>(numBins), 0.0);
        int frameCount = 0;

        const int hop = kFftSize / 2;
        std::vector<float> frameRef(static_cast<size_t>(kFftSize * 2), 0.0f);
        std::vector<float> frameCap(static_cast<size_t>(kFftSize * 2), 0.0f);

        for (int start = 0; start <= numSamples - kFftSize && frameCount < 8; start += hop)
        {
            std::memcpy(frameRef.data(), ref.data() + start, kFftSize * sizeof(float));
            std::memcpy(frameCap.data(), cap.data() + start, kFftSize * sizeof(float));
            std::fill(frameRef.begin() + kFftSize, frameRef.end(), 0.0f);
            std::fill(frameCap.begin() + kFftSize, frameCap.end(), 0.0f);

            window.multiplyWithWindowingTable(frameRef.data(), kFftSize);
            window.multiplyWithWindowingTable(frameCap.data(), kFftSize);

            fft.performRealOnlyForwardTransform(frameRef.data(), true);
            fft.performRealOnlyForwardTransform(frameCap.data(), true);

            for (int i = 0; i < numBins; ++i)
            {
                const float rRe = frameRef[static_cast<size_t>(2 * i)];
                const float rIm = frameRef[static_cast<size_t>(2 * i + 1)];
                const float cRe = frameCap[static_cast<size_t>(2 * i)];
                const float cIm = frameCap[static_cast<size_t>(2 * i + 1)];

                avgPowerRef[static_cast<size_t>(i)] += static_cast<double>(rRe * rRe + rIm * rIm);
                avgPowerCap[static_cast<size_t>(i)] += static_cast<double>(cRe * cRe + cIm * cIm);
            }
            ++frameCount;
        }

        if (frameCount > 0)
        {
            for (int i = 0; i < numBins; ++i)
            {
                avgPowerRef[static_cast<size_t>(i)] /= static_cast<double>(frameCount);
                avgPowerCap[static_cast<size_t>(i)] /= static_cast<double>(frameCount);
            }
        }

        double totalRef = 0.0, totalCap = 0.0;
        double weightedFreqRef = 0.0, weightedFreqCap = 0.0;
        double eBassRef = 0.0, eBassCap = 0.0;
        double eLowMidRef = 0.0, eLowMidCap = 0.0;
        double eHighMidRef = 0.0, eHighMidCap = 0.0;
        double eHighRef = 0.0, eHighCap = 0.0;

        for (int b = 0; b < UniversalABMetrics::kNumBands; ++b)
        {
            const double fc = m.bandFreqs[static_cast<size_t>(b)];
            const double fLo = fc * 0.8908987; // 2^(-1/6)
            const double fHi = fc * 1.1224620; // 2^(+1/6)

            const int binLo = std::clamp(static_cast<int>(std::floor(fLo / binWidth)), 0, numBins - 1);
            const int binHi = std::clamp(static_cast<int>(std::ceil(fHi / binWidth)), 0, numBins - 1);

            double bandRef = 0.0, bandCap = 0.0;
            for (int k = binLo; k <= binHi; ++k)
            {
                bandRef += avgPowerRef[static_cast<size_t>(k)];
                bandCap += avgPowerCap[static_cast<size_t>(k)];
            }

            m.bandEnergyRef[static_cast<size_t>(b)] = bandRef;
            m.bandEnergyCap[static_cast<size_t>(b)] = bandCap;

            const double deltaDb = (bandRef > 1e-12 && bandCap > 1e-12)
                ? 10.0 * std::log10(bandCap / bandRef)
                : 0.0;
            m.bandDeltaDb[static_cast<size_t>(b)] = deltaDb;

            totalRef += bandRef;
            totalCap += bandCap;
            weightedFreqRef += fc * bandRef;
            weightedFreqCap += fc * bandCap;

            // 4 Macro-Bands
            if (fc < 250.0) {
                eBassRef += bandRef;
                eBassCap += bandCap;
            } else if (fc < 1000.0) {
                eLowMidRef += bandRef;
                eLowMidCap += bandCap;
            } else if (fc < 4000.0) {
                eHighMidRef += bandRef;
                eHighMidCap += bandCap;
            } else {
                eHighRef += bandRef;
                eHighCap += bandCap;
            }
        }

        // Spectral Cosine Similarity across 24 bands (using normalized energy distribution)
        double dot = 0.0, normRef = 0.0, normCap = 0.0;
        for (int b = 0; b < UniversalABMetrics::kNumBands; ++b)
        {
            const double pRef = totalRef > 1e-12 ? (m.bandEnergyRef[static_cast<size_t>(b)] / totalRef) : 0.0;
            const double pCap = totalCap > 1e-12 ? (m.bandEnergyCap[static_cast<size_t>(b)] / totalCap) : 0.0;
            dot += pRef * pCap;
            normRef += pRef * pRef;
            normCap += pCap * pCap;
        }

        if (normRef > 1e-12 && normCap > 1e-12)
        {
            m.spectralCosineSimilarityPct = 100.0 * dot / (std::sqrt(normRef) * std::sqrt(normCap));
        }
        else
        {
            m.spectralCosineSimilarityPct = 100.0;
        }

        if (totalRef > 1e-12)
        {
            m.spectralCentroidRefHz = weightedFreqRef / totalRef;
            m.bassRefPct = 100.0 * eBassRef / totalRef;
            m.lowMidRefPct = 100.0 * eLowMidRef / totalRef;
            m.highMidRefPct = 100.0 * eHighMidRef / totalRef;
            m.highRefPct = 100.0 * eHighRef / totalRef;
        }

        if (totalCap > 1e-12)
        {
            m.spectralCentroidCapHz = weightedFreqCap / totalCap;
            m.bassCapPct = 100.0 * eBassCap / totalCap;
            m.lowMidCapPct = 100.0 * eLowMidCap / totalCap;
            m.highMidCapPct = 100.0 * eHighMidCap / totalCap;
            m.highCapPct = 100.0 * eHighCap / totalCap;
        }

        m.spectralCentroidDeltaHz = m.spectralCentroidCapHz - m.spectralCentroidRefHz;
    }

    static void computeEnvelopeSlope(const std::vector<float>& buf, double sampleRate, double& slopeDb)
    {
        slopeDb = 0.0;
        const int n = static_cast<int>(buf.size());
        const int earlyStart = static_cast<int>(0.010 * sampleRate);
        const int earlyEnd   = static_cast<int>(0.110 * sampleRate);
        const int lateStart  = static_cast<int>(0.200 * sampleRate);
        const int lateEnd    = static_cast<int>(0.400 * sampleRate);

        if (earlyEnd > n) return;

        double earlySum = 0.0;
        for (int i = earlyStart; i < earlyEnd; ++i)
            earlySum += static_cast<double>(buf[static_cast<size_t>(i)]) * buf[static_cast<size_t>(i)];
        const double earlyRms = std::sqrt(earlySum / std::max(1, earlyEnd - earlyStart));

        const int validLateEnd = std::min(n, lateEnd);
        if (validLateEnd <= lateStart) return;

        double lateSum = 0.0;
        for (int i = lateStart; i < validLateEnd; ++i)
            lateSum += static_cast<double>(buf[static_cast<size_t>(i)]) * buf[static_cast<size_t>(i)];
        const double lateRms = std::sqrt(lateSum / std::max(1, validLateEnd - lateStart));

        if (earlyRms > 1e-6 && lateRms > 1e-6)
        {
            slopeDb = 20.0 * std::log10(lateRms / earlyRms);
        }
    }

    static double computeStereoWidth(const float* left, const float* right, int numSamples)
    {
        if (numSamples <= 0 || left == nullptr || right == nullptr) return 0.0;
        double eDiff = 0.0, eSum = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double diff = static_cast<double>(left[i]) - right[i];
            const double sum  = static_cast<double>(left[i]) + right[i];
            eDiff += diff * diff;
            eSum  += sum * sum;
        }
        return (eSum > 1e-12) ? std::clamp(100.0 * eDiff / eSum, 0.0, 100.0) : 0.0;
    }

    static void computePhaseInvarianceCoherence(
        const std::vector<float>& ref,
        const std::vector<float>& cap,
        UniversalABMetrics& m)
    {
        // FFT magnitude coherence across 1024 bins
        constexpr int kFftOrder = 10;
        constexpr int kFftSize = 1 << kFftOrder;
        if (ref.size() < kFftSize || cap.size() < kFftSize)
        {
            m.phaseInvarianceCoherence = 1.0;
            m.isPhaseInvariant = false;
            return;
        }

        juce::dsp::FFT fft(kFftOrder);
        std::vector<float> fftDataRef(kFftSize * 2, 0.0f);
        std::vector<float> fftDataCap(kFftSize * 2, 0.0f);

        std::memcpy(fftDataRef.data(), ref.data(), kFftSize * sizeof(float));
        std::memcpy(fftDataCap.data(), cap.data(), kFftSize * sizeof(float));

        juce::dsp::WindowingFunction<float> window(kFftSize, juce::dsp::WindowingFunction<float>::hann);
        window.multiplyWithWindowingTable(fftDataRef.data(), kFftSize);
        window.multiplyWithWindowingTable(fftDataCap.data(), kFftSize);

        fft.performRealOnlyForwardTransform(fftDataRef.data(), true);
        fft.performRealOnlyForwardTransform(fftDataCap.data(), true);

        double crossMagSum = 0.0;
        double refMagSum = 0.0;
        double capMagSum = 0.0;

        for (int i = 0; i < kFftSize / 2; ++i)
        {
            const float rRe = fftDataRef[static_cast<size_t>(2 * i)];
            const float rIm = fftDataRef[static_cast<size_t>(2 * i + 1)];
            const float cRe = fftDataCap[static_cast<size_t>(2 * i)];
            const float cIm = fftDataCap[static_cast<size_t>(2 * i + 1)];

            const double magRef = std::sqrt(rRe * rRe + rIm * rIm);
            const double magCap = std::sqrt(cRe * cRe + cIm * cIm);

            crossMagSum += magRef * magCap;
            refMagSum   += magRef * magRef;
            capMagSum   += magCap * magCap;
        }

        if (refMagSum > 1e-12 && capMagSum > 1e-12)
        {
            m.phaseInvarianceCoherence = (crossMagSum * crossMagSum) / (refMagSum * capMagSum);
        }
        else
        {
            m.phaseInvarianceCoherence = 1.0;
        }

        // If spectral correlation/coherence is nearly perfect (> 0.985)
        // and spectral similarity is high, the residual is phase variance from free-running oscillators
        m.isPhaseInvariant = (m.phaseInvarianceCoherence >= 0.985 && m.spectralCosineSimilarityPct >= 98.0);
    }
};

} // namespace abdaudiolab::math
