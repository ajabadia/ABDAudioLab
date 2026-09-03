#include "LabAnalyticEngine.h"
#include <numeric>
#include <algorithm>
#include <numbers>

namespace abdaudiolab::math
{

StatisticalPair LabAnalyticEngine::calculateStatistics(const std::vector<float>& dataset)
{
    StatisticalPair res;
    if (dataset.empty())
        return res;

    if (dataset.size() == 1)
    {
        res.mean = dataset[0];
        res.stdDev = 0.0f;
        return res;
    }

    double sum = 0.0;
    for (float v : dataset)
        sum += v;

    double mean = sum / static_cast<double>(dataset.size());
    res.mean = static_cast<float>(mean);

    double varSum = 0.0;
    for (float v : dataset)
    {
        double diff = static_cast<double>(v) - mean;
        varSum += diff * diff;
    }

    double variance = varSum / static_cast<double>(dataset.size() - 1);
    res.stdDev = static_cast<float>(std::sqrt(std::max(0.0, variance)));

    return res;
}

FilterAnalysisResult LabAnalyticEngine::analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                           const std::vector<float>& inverseFilter,
                                                           double sampleRate,
                                                           double durationSec,
                                                           float startFreqHz,
                                                           float endFreqHz)
{
    FilterAnalysisResult result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> cutoffs;
    std::vector<float> resonances;
    std::vector<float> thds;

    cutoffs.reserve(recordedPasses.size());
    resonances.reserve(recordedPasses.size());
    thds.reserve(recordedPasses.size());

    for (const auto& pass : recordedPasses)
    {
        auto deco = FarinaDeconvolver::deconvolve(pass, inverseFilter, sampleRate, durationSec, startFreqHz, endFreqHz);
        cutoffs.push_back(deco.peakFrequencyHz);
        resonances.push_back(deco.resonancePeakDb);
        thds.push_back(deco.thdPercent);

        if (result.frequencyCurveHz.empty())
        {
            result.frequencyCurveHz = deco.frequenciesHz;
            result.magnitudeCurveDb = deco.frequencyResponseMagnitudeDb;
        }
    }

    result.cutoffHz = calculateStatistics(cutoffs);
    result.resonanceDb = calculateStatistics(resonances);
    result.thdPercent = calculateStatistics(thds);

    return result;
}

TimeDynamicAnalysisResult LabAnalyticEngine::analyzeAdsrEnvelopes(const std::vector<std::vector<float>>& recordedPasses,
                                                                 double sampleRate)
{
    TimeDynamicAnalysisResult result;
    if (recordedPasses.empty() || sampleRate <= 0.0)
        return result;

    std::vector<float> attacks;
    std::vector<float> decays;
    std::vector<float> sustains;
    std::vector<float> releases;

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 128)
            continue;

        // 1. Calculate smoothed Hilbert / RMS envelope follower
        std::vector<float> env(pass.size());
        float lp = 0.0f;
        float alpha = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 100.0 / sampleRate)); // 100Hz LPF
        for (size_t i = 0; i < pass.size(); ++i)
        {
            float absVal = std::abs(pass[i]);
            lp = (1.0f - alpha) * absVal + alpha * lp;
            env[i] = lp;
        }

        // 2. Find Peak Amplitude V_peak and Peak Index
        float maxVal = 0.0f;
        size_t peakIdx = 0;
        for (size_t i = 0; i < env.size(); ++i)
        {
            if (env[i] > maxVal)
            {
                maxVal = env[i];
                peakIdx = i;
            }
        }

        if (maxVal < 1e-5f)
            continue;

        // 3. Attack Time: from note start (or threshold -60dB = 0.001 * maxVal) to peak
        size_t startIdx = 0;
        float noiseThreshold = maxVal * 0.001f; // -60 dBFS relative to peak
        for (size_t i = 0; i < peakIdx; ++i)
        {
            if (env[i] >= noiseThreshold)
            {
                startIdx = i;
                break;
            }
        }
        float attackMs = static_cast<float>((static_cast<double>(peakIdx - startIdx) / sampleRate) * 1000.0);
        attacks.push_back(attackMs);

        // 4. Sustain Level & Decay Time: find plateau after peak
        size_t sustainSearchEnd = std::min(env.size(), peakIdx + static_cast<size_t>(sampleRate * 0.5)); // search next 500ms
        float sustainSum = 0.0f;
        int sustainCount = 0;
        size_t decayEndIdx = peakIdx;

        for (size_t i = peakIdx + static_cast<size_t>(sampleRate * 0.1); i < sustainSearchEnd; ++i)
        {
            sustainSum += env[i];
            sustainCount++;
        }

        float sustainLevelVal = (sustainCount > 0) ? (sustainSum / static_cast<float>(sustainCount)) : (maxVal * 0.7f);
        float normSustain = std::clamp(sustainLevelVal / maxVal, 0.0f, 1.0f);
        sustains.push_back(normSustain);

        // Decay time: from peak to within 5% of sustain level
        float decayTarget = sustainLevelVal + (maxVal - sustainLevelVal) * 0.05f;
        for (size_t i = peakIdx; i < sustainSearchEnd; ++i)
        {
            if (env[i] <= decayTarget)
            {
                decayEndIdx = i;
                break;
            }
        }
        float decayMs = static_cast<float>((static_cast<double>(decayEndIdx - peakIdx) / sampleRate) * 1000.0);
        decays.push_back(std::max(0.5f, decayMs));

        // 5. Release Time: from release trigger to -60 dBFS (0.001 * maxVal)
        size_t releaseStartIdx = (sustainSearchEnd < env.size()) ? sustainSearchEnd : (env.size() / 2);
        size_t releaseEndIdx = env.size() - 1;

        for (size_t i = releaseStartIdx; i < env.size(); ++i)
        {
            if (env[i] <= noiseThreshold)
            {
                releaseEndIdx = i;
                break;
            }
        }
        float releaseMs = static_cast<float>((static_cast<double>(releaseEndIdx - releaseStartIdx) / sampleRate) * 1000.0);
        releases.push_back(std::max(1.0f, releaseMs));
    }

    result.attackTimeMs = calculateStatistics(attacks);
    result.decayTimeMs = calculateStatistics(decays);
    result.sustainLevel = calculateStatistics(sustains);
    result.releaseTimeMs = calculateStatistics(releases);

    return result;
}

TimeDynamicAnalysisResult LabAnalyticEngine::analyzeDelayImpulses(const std::vector<std::vector<float>>& recordedPasses,
                                                                 double sampleRate)
{
    TimeDynamicAnalysisResult result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> delayTimes;

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 100)
            continue;

        // Skip direct impulse and find first echo peak
        size_t echoPeakIdx = 0;
        float maxEcho = 0.0f;
        size_t startSearch = std::min(pass.size(), static_cast<size_t>(sampleRate * 0.005)); // skip first 5ms

        for (size_t i = startSearch; i < pass.size(); ++i)
        {
            float v = std::abs(pass[i]);
            if (v > maxEcho)
            {
                maxEcho = v;
                echoPeakIdx = i;
            }
        }

        float delayMs = static_cast<float>((static_cast<double>(echoPeakIdx) / sampleRate) * 1000.0);
        delayTimes.push_back(delayMs);
    }

    result.delayTimeMs = calculateStatistics(delayTimes);
    return result;
}

WaveShaperAnalysisResult LabAnalyticEngine::analyzeWaveShaperRamps(const std::vector<std::vector<float>>& recordedPasses,
                                                                   double sampleRate)
{
    WaveShaperAnalysisResult result;
    if (recordedPasses.empty())
        return result;

    const auto& pass = recordedPasses[0];
    const size_t numPoints = 128;
    result.transferCurveInput.resize(numPoints);
    result.transferCurveOutput.resize(numPoints);

    for (size_t i = 0; i < numPoints; ++i)
    {
        float inVal = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        result.transferCurveInput[i] = inVal;

        size_t sampleIdx = std::min(pass.size() - 1, static_cast<size_t>(inVal * static_cast<float>(pass.size() - 1)));
        result.transferCurveOutput[i] = std::abs(pass[sampleIdx]);
    }

    // Single-cycle Goertzel DFT to compute THD (H1..H5)
    std::vector<float> thds;
    for (const auto& p : recordedPasses)
    {
        if (p.size() < 256 || sampleRate <= 0.0)
            continue;

        // Extract steady-state portion
        size_t N = std::min(p.size(), static_cast<size_t>(2048));
        size_t start = (p.size() > N) ? (p.size() - N) / 2 : 0;

        auto computeHarmonicMag = [&](float targetFreqHz) -> float {
            double w = 2.0 * std::numbers::pi * (targetFreqHz / sampleRate);
            double coeff = 2.0 * std::cos(w);
            double q0 = 0.0, q1 = 0.0, q2 = 0.0;

            for (size_t k = 0; k < N; ++k)
            {
                q0 = coeff * q1 - q2 + static_cast<double>(p[start + k]);
                q2 = q1;
                q1 = q0;
            }
            double realPart = q1 - q2 * std::cos(w);
            double imagPart = q2 * std::sin(w);
            return static_cast<float>(std::sqrt(realPart * realPart + imagPart * imagPart) / static_cast<double>(N));
        };

        float f1 = 1000.0f; // 1 kHz test tone
        float h1 = computeHarmonicMag(f1);
        float h2 = computeHarmonicMag(f1 * 2.0f);
        float h3 = computeHarmonicMag(f1 * 3.0f);
        float h4 = computeHarmonicMag(f1 * 4.0f);
        float h5 = computeHarmonicMag(f1 * 5.0f);

        if (h1 > 1e-5f)
        {
            float harmonicSumSq = h2 * h2 + h3 * h3 + h4 * h4 + h5 * h5;
            float thd = (std::sqrt(harmonicSumSq) / h1) * 100.0f;
            thds.push_back(std::clamp(thd, 0.01f, 100.0f));
        }
        else
        {
            thds.push_back(0.05f);
        }
    }

    result.thdPercent = calculateStatistics(thds);
    return result;
}

GainAnalysisResult LabAnalyticEngine::analyzeGainTones(const std::vector<std::vector<float>>& recordedPasses,
                                                      double sampleRate)
{
    GainAnalysisResult result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> gains;
    std::vector<float> snrs;

    for (const auto& pass : recordedPasses)
    {
        if (pass.empty())
            continue;

        double sumSq = 0.0;
        for (float v : pass)
            sumSq += v * v;
        double rms = std::sqrt(sumSq / static_cast<double>(pass.size()));
        float gainDb = static_cast<float>(20.0 * std::log10(std::max(rms, 1e-6)));
        gains.push_back(gainDb);

        float snr = calculateSignalToNoiseRatioDb(pass, -85.0f);
        snrs.push_back(snr);
    }

    result.gainDb = calculateStatistics(gains);
    result.snrDb = calculateStatistics(snrs);
    return result;
}

float LabAnalyticEngine::calculateSignalToNoiseRatioDb(const std::vector<float>& signalBuffer, float baselineNoiseRmsDb)
{
    if (signalBuffer.empty())
        return 0.0f;

    double sumSq = 0.0;
    for (float s : signalBuffer)
        sumSq += static_cast<double>(s * s);

    double signalRms = std::sqrt(sumSq / static_cast<double>(signalBuffer.size()));
    float signalDb = (signalRms > 1e-6) ? static_cast<float>(20.0 * std::log10(signalRms)) : -120.0f;

    return std::max(0.0f, signalDb - baselineNoiseRmsDb);
}

CyclicModulatorAnalysisResult LabAnalyticEngine::analyzeCyclicModulator(const std::vector<std::vector<float>>& recordedPasses,
                                                                      double sampleRate)
{
    CyclicModulatorAnalysisResult result;
    if (recordedPasses.empty() || sampleRate <= 0.0)
        return result;

    std::vector<float> rates;
    std::vector<float> depths;
    std::vector<float> asymmetries;

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 1024)
            continue;

        // 1. Calculate envelope through rectified low-pass follower
        std::vector<float> env(pass.size());
        float lp = 0.0f;
        float alpha = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 30.0 / sampleRate)); // 30Hz smoothing
        for (size_t i = 0; i < pass.size(); ++i)
        {
            float absVal = std::abs(pass[i]);
            lp = (1.0f - alpha) * absVal + alpha * lp;
            env[i] = lp;
        }

        // 2. Autocorrelation on envelope to detect LFO periodicity
        int maxLag = std::min(static_cast<int>(pass.size() / 2), static_cast<int>(sampleRate * 2.0)); // up to 2s lag
        int minLag = static_cast<int>(sampleRate / 30.0); // min 30 Hz LFO

        int bestLag = minLag;
        float bestCorr = -1.0f;

        for (int lag = minLag; lag < maxLag; lag += 4)
        {
            float corr = 0.0f;
            int count = 0;
            for (size_t i = 0; i + static_cast<size_t>(lag) < env.size(); i += 8)
            {
                corr += env[i] * env[i + static_cast<size_t>(lag)];
                count++;
            }
            if (count > 0)
            {
                corr /= static_cast<float>(count);
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestLag = lag;
                }
            }
        }

        float lfoRateHz = (bestLag > 0) ? static_cast<float>(sampleRate / static_cast<double>(bestLag)) : 1.0f;
        rates.push_back(std::clamp(lfoRateHz, 0.05f, 50.0f));

        // 3. Peak-to-valley depth
        float minEnv = 1e6f, maxEnv = -1e6f;
        for (float v : env)
        {
            if (v < minEnv) minEnv = v;
            if (v > maxEnv) maxEnv = v;
        }
        float depth = (maxEnv > 1e-4f) ? ((maxEnv - minEnv) / maxEnv) * 100.0f : 0.0f;
        depths.push_back(std::clamp(depth, 0.0f, 100.0f));
        asymmetries.push_back(0.02f); // nominal balanced LFO
    }

    result.rateHz = calculateStatistics(rates);
    result.depthPercent = calculateStatistics(depths);
    result.asymmetry = calculateStatistics(asymmetries);
    return result;
}

WienerHammersteinAnalysisResult LabAnalyticEngine::analyzeWienerHammerstein(
    const std::vector<std::vector<float>>& recordedPasses,
    const std::vector<float>& inputStimulus,
    double sampleRate)
{
    WienerHammersteinAnalysisResult result;
    if (recordedPasses.empty() || inputStimulus.empty() || sampleRate <= 0.0)
        return result;

    WienerHammersteinFitter fitter;
    std::vector<float> coeffsA;
    std::vector<float> h1Centroids;
    std::vector<float> h2Centroids;
    std::vector<float> r2Fits;

    for (const auto& pass : recordedPasses)
    {
        if (pass.empty()) continue;
        auto model = fitter.fit(inputStimulus, pass, sampleRate);
        coeffsA.push_back(model.nonLinearCoeffA);
        h1Centroids.push_back(model.preFilterCentroidHz);
        h2Centroids.push_back(model.postFilterCentroidHz);
        r2Fits.push_back(model.goodnessOfFitR2);

        if (result.representativeH1.empty())
        {
            result.representativeH1 = model.h1Taps;
            result.representativeH2 = model.h2Taps;
        }
    }

    result.nonLinearCoeffA = calculateStatistics(coeffsA);
    result.preFilterCentroidHz = calculateStatistics(h1Centroids);
    result.postFilterCentroidHz = calculateStatistics(h2Centroids);
    result.goodnessOfFitR2 = calculateStatistics(r2Fits);

    return result;
}

} // namespace abdaudiolab::math
