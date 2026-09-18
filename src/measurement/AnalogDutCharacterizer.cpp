/**
 * @file AnalogDutCharacterizer.cpp
 * @brief Implementation of unified metrological engine for Analog DUT Characterization.
 * @author ABDSynths
 * @date 2026
 */

#include "AnalogDutCharacterizer.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace abdaudiolab::measurement
{

FrequencyResponseResult AnalogDutCharacterizer::adaptFarinaResult(
    const math::DeconvolutionResult& decoResult,
    double sampleRate,
    double sweepDurationSec,
    float startFreqHz,
    float endFreqHz)
{
    FrequencyResponseResult res;
    res.sampleRateHz = sampleRate;
    res.sweepDurationSec = sweepDurationSec;
    res.startFrequencyHz = static_cast<double>(startFreqHz);
    res.endFrequencyHz = static_cast<double>(endFreqHz);

    res.peakFrequencyHz = static_cast<double>(decoResult.peakFrequencyHz);
    res.peakMagnitudeDb = static_cast<double>(decoResult.resonancePeakDb);
    res.deconvolutionThdPercent = static_cast<double>(decoResult.thdPercent);

    res.frequenciesHz = decoResult.frequenciesHz;
    res.magnitudeDb = decoResult.frequencyResponseMagnitudeDb;
    res.phaseRad = decoResult.phaseResponseRad;
    res.groupDelaySamples = decoResult.groupDelaySamples;

    res.h2FractionPercent = static_cast<double>(decoResult.h2Percent);
    res.h3FractionPercent = static_cast<double>(decoResult.h3Percent);
    res.h4FractionPercent = static_cast<double>(decoResult.h4Percent);
    res.h5FractionPercent = static_cast<double>(decoResult.h5Percent);

    res.status = res.frequenciesHz.empty() ? "insufficient_signal" : "resolved";
    return res;
}

FrequencyResponseResult AnalogDutCharacterizer::measureFrequencyResponse(
    std::span<const float> responseAudio,
    std::span<const float> inverseFilter,
    double sampleRate,
    double sweepDurationSec,
    float startFreqHz,
    float endFreqHz)
{
    const std::vector<float> respVec(responseAudio.begin(), responseAudio.end());
    const std::vector<float> invVec(inverseFilter.begin(), inverseFilter.end());

    const auto deco = math::FarinaDeconvolver::deconvolve(
        respVec,
        invVec,
        sampleRate,
        sweepDurationSec,
        startFreqHz,
        endFreqHz);

    return adaptFarinaResult(deco, sampleRate, sweepDurationSec, startFreqHz, endFreqHz);
}

HarmonicDistortionResult AnalogDutCharacterizer::measureHarmonicDistortion(
    std::span<const float> signal,
    double sampleRate,
    double fundamentalHz,
    ThdConvention convention,
    int firstHarmonic,
    int lastHarmonic,
    MeasurementWindow window,
    int fftSize)
{
    HarmonicDistortionResult res;
    res.fundamentalFrequencyHz = fundamentalHz;
    res.convention = convention;
    res.windowType = measurementWindowToString(window);
    res.coherentGain = getWindowCoherentGain(window);
    res.integrationBandwidthBins = 2;

    const double rmsDbfs = computeRmsDbfs(signal);
    if (signal.empty() || rmsDbfs <= -90.0)
    {
        res.status = "insufficient_signal";
        return res;
    }

    SpectralAnalysisConfig config;
    config.sampleRateHz = sampleRate;
    config.fftSize = fftSize;
    config.window = window;
    config.coherentSampling = false;
    config.harmonicIntegrationBins = 2;

    std::vector<float> mags(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
    if (!performFftMagnitudes(signal, config, mags))
    {
        res.status = "insufficient_signal";
        return res;
    }

    const double binWidth = sampleRate / static_cast<double>(fftSize);
    const int fundNominalBin = static_cast<int>(std::round(fundamentalHz / binWidth));

    // 1. Locate fundamental peak within +/- 4 bins
    int bestFundBin = fundNominalBin;
    float maxFundMag = 0.0f;
    const int sFund = std::max(1, fundNominalBin - 4);
    const int eFund = std::min(fftSize / 2 - 1, fundNominalBin + 4);

    for (int b = sFund; b <= eFund; ++b)
    {
        if (mags[static_cast<size_t>(b)] > maxFundMag)
        {
            maxFundMag = mags[static_cast<size_t>(b)];
            bestFundBin = b;
        }
    }

    if (maxFundMag < 1e-6f || bestFundBin <= 0 || bestFundBin >= fftSize / 2)
    {
        res.status = "ambiguous_fundamental";
        return res;
    }

    const auto subFund = refineSubbinPeak(
        static_cast<double>(mags[static_cast<size_t>(bestFundBin - 1)]),
        static_cast<double>(mags[static_cast<size_t>(bestFundBin)]),
        static_cast<double>(mags[static_cast<size_t>(bestFundBin + 1)]),
        static_cast<double>(bestFundBin),
        binWidth);

    res.measuredFundamentalHz = subFund.valid ? subFund.frequencyHz : (bestFundBin * binWidth);
    res.fundamentalMagnitudeDbfs = subFund.magnitudeDbfs;

    // Measured fundamental RMS (integrate peak lobe +/- 2 bins)
    const double fundRms = integrateBandEnergy(mags, bestFundBin, 2);
    res.fundamentalRms = std::max(1e-9, fundRms);

    // 2. Measure individual harmonics
    double sumHarmonicsSq = 0.0;
    const double nyquistHz = sampleRate / 2.0;

    for (int h = firstHarmonic; h <= lastHarmonic; ++h)
    {
        const double harmFreq = res.measuredFundamentalHz * static_cast<double>(h);
        if (harmFreq >= nyquistHz) break;

        const int harmNominalBin = static_cast<int>(std::round(harmFreq / binWidth));
        if (harmNominalBin >= fftSize / 2 - 1) break;

        // Search local maximum within +/- 2 bins
        int bestHarmBin = harmNominalBin;
        float maxHarmMag = 0.0f;
        const int sH = std::max(1, harmNominalBin - 2);
        const int eH = std::min(fftSize / 2 - 1, harmNominalBin + 2);

        for (int b = sH; b <= eH; ++b)
        {
            if (mags[static_cast<size_t>(b)] > maxHarmMag)
            {
                maxHarmMag = mags[static_cast<size_t>(b)];
                bestHarmBin = b;
            }
        }

        const auto subHarm = refineSubbinPeak(
            static_cast<double>(mags[static_cast<size_t>(bestHarmBin - 1)]),
            static_cast<double>(mags[static_cast<size_t>(bestHarmBin)]),
            static_cast<double>(mags[static_cast<size_t>(bestHarmBin + 1)]),
            static_cast<double>(bestHarmBin),
            binWidth);

        const double harmRms = integrateBandEnergy(mags, bestHarmBin, 2);
        sumHarmonicsSq += harmRms * harmRms;

        HarmonicComponent comp;
        comp.harmonicOrder = h;
        comp.expectedFrequencyHz = harmFreq;
        comp.measuredFrequencyHz = subHarm.valid ? subHarm.frequencyHz : (bestHarmBin * binWidth);
        comp.measuredBin = subHarm.valid ? subHarm.bin : static_cast<double>(bestHarmBin);
        comp.linearMagnitude = subHarm.valid ? subHarm.magnitude : static_cast<double>(maxHarmMag);
        comp.rmsAmplitude = harmRms;
        comp.levelDbc = 20.0 * std::log10(std::max(1e-9, harmRms / res.fundamentalRms));
        res.harmonics.push_back(comp);
    }

    res.totalHarmonicsRms = std::sqrt(sumHarmonicsSq);

    // 3. Compute THD according to both IEEE and IEC conventions
    res.thdFundamentalReferencedPercent = (res.totalHarmonicsRms / res.fundamentalRms) * 100.0;

    const double totalRms = std::sqrt(res.fundamentalRms * res.fundamentalRms + sumHarmonicsSq);
    res.thdTotalRmsReferencedPercent = (res.totalHarmonicsRms / totalRms) * 100.0;

    if (convention == ThdConvention::FundamentalReferenced)
    {
        res.thdPercent = res.thdFundamentalReferencedPercent;
        res.thdRatio = res.totalHarmonicsRms / res.fundamentalRms;
    }
    else
    {
        res.thdPercent = res.thdTotalRmsReferencedPercent;
        res.thdRatio = res.totalHarmonicsRms / totalRms;
    }

    res.thdDb = (res.thdRatio > 1e-9) ? 20.0 * std::log10(res.thdRatio) : -120.0;
    res.status = "resolved";
    return res;
}

IntermodulationResult AnalogDutCharacterizer::measureIntermodulation(
    std::span<const float> signal,
    double sampleRate,
    ImdConvention convention,
    double f1Hz,
    double f2Hz,
    int fftSize)
{
    IntermodulationResult res;
    res.convention = convention;
    res.f1Hz = f1Hz;
    res.f2Hz = f2Hz;

    const double rmsDbfs = computeRmsDbfs(signal);
    if (signal.empty() || rmsDbfs <= -90.0)
    {
        res.status = "insufficient_signal";
        return res;
    }

    SpectralAnalysisConfig config;
    config.sampleRateHz = sampleRate;
    config.fftSize = fftSize;
    config.window = MeasurementWindow::Hann;
    config.coherentSampling = false;
    config.harmonicIntegrationBins = 2;

    std::vector<float> mags(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
    if (!performFftMagnitudes(signal, config, mags))
    {
        res.status = "insufficient_signal";
        return res;
    }

    const double binWidth = sampleRate / static_cast<double>(fftSize);

    // Helper to find peak around target frequency
    auto getPeakAround = [&](double targetFreq, int searchRadiusBins = 4) {
        const int nomBin = static_cast<int>(std::round(targetFreq / binWidth));
        int bestBin = nomBin;
        float maxM = 0.0f;
        const int sB = std::max(1, nomBin - searchRadiusBins);
        const int eB = std::min(fftSize / 2 - 1, nomBin + searchRadiusBins);

        for (int b = sB; b <= eB; ++b)
        {
            if (mags[static_cast<size_t>(b)] > maxM)
            {
                maxM = mags[static_cast<size_t>(b)];
                bestBin = b;
            }
        }
        const double rms = integrateBandEnergy(mags, bestBin, 2);
        return std::make_pair(bestBin, rms);
    };

    const auto [bin1, rms1] = getPeakAround(f1Hz);
    const auto [bin2, rms2] = getPeakAround(f2Hz);
    res.f1Rms = rms1;
    res.f2Rms = rms2;

    if (rms1 < 1e-6 || rms2 < 1e-6)
    {
        res.status = "tones_unresolved";
        return res;
    }

    double refRms = rms2; // Default for SMPTE (carrier f2)
    std::vector<std::pair<double, std::string>> targets;

    if (convention == ImdConvention::Smpte || convention == ImdConvention::Din)
    {
        res.referenceTone = "f2_high_carrier";
        refRms = res.f2Rms;

        // 2nd order sidebands: f2 ± f1
        targets.push_back({ f2Hz - f1Hz, "f2 - f1" });
        targets.push_back({ f2Hz + f1Hz, "f2 + f1" });

        // 3rd order sidebands: f2 ± 2*f1
        targets.push_back({ f2Hz - 2.0 * f1Hz, "f2 - 2*f1" });
        targets.push_back({ f2Hz + 2.0 * f1Hz, "f2 + 2*f1" });
    }
    else // CCIF / ITU-R
    {
        res.referenceTone = "equal_split_power";
        refRms = std::sqrt(rms1 * rms1 + rms2 * rms2);

        // 2nd order difference: f2 - f1
        targets.push_back({ f2Hz - f1Hz, "f2 - f1" });

        // 3rd order cubic products: 2*f1 - f2, 2*f2 - f1
        targets.push_back({ 2.0 * f1Hz - f2Hz, "2*f1 - f2" });
        targets.push_back({ 2.0 * f2Hz - f1Hz, "2*f2 - f1" });
    }

    double sumD2Sq = 0.0;
    double sumD3Sq = 0.0;

    for (const auto& [freq, label] : targets)
    {
        if (freq <= 0.0 || freq >= sampleRate / 2.0) continue;

        const auto [pBin, pRms] = getPeakAround(freq, 2);
        const int order = (label.find("2*") != std::string::npos || label.find("2f") != std::string::npos) ? 3 : 2;

        IntermodulationProduct prod;
        prod.frequencyHz = pBin * binWidth;
        prod.order = order;
        prod.linearMagnitude = mags[static_cast<size_t>(pBin)];
        prod.rmsAmplitude = pRms;
        prod.levelDbc = 20.0 * std::log10(std::max(1e-9, pRms / refRms));
        prod.productLabel = label;
        res.products.push_back(prod);

        if (order == 2) sumD2Sq += pRms * pRms;
        else sumD3Sq += pRms * pRms;
    }

    const double totalProductRms = std::sqrt(sumD2Sq + sumD3Sq);
    res.d2Percent = (std::sqrt(sumD2Sq) / refRms) * 100.0;
    res.d3Percent = (std::sqrt(sumD3Sq) / refRms) * 100.0;
    res.totalImdPercent = (totalProductRms / refRms) * 100.0;
    res.totalImdDb = (res.totalImdPercent > 1e-9) ? 20.0 * std::log10(res.totalImdPercent / 100.0) : -120.0;
    res.status = "resolved";

    return res;
}

ClippingThresholdResult AnalogDutCharacterizer::analyzeLevelSweep(
    const std::vector<LevelSweepPoint>& sweepPoints)
{
    ClippingThresholdResult res;
    res.sweepPoints = sweepPoints;
    res.interpolationMethod = "linear_monotone";

    if (sweepPoints.size() < 2)
    {
        res.status = "insufficient_points";
        return res;
    }

    // Sort by input level
    auto sortedPts = sweepPoints;
    std::sort(sortedPts.begin(), sortedPts.end(), [](const auto& a, const auto& b) {
        return a.inputLevelDbfs < b.inputLevelDbfs;
    });

    // Check for hard clipping observation
    for (const auto& pt : sortedPts)
    {
        if (pt.hardClipDetectedInCapture)
        {
            res.hardClippingObserved = true;
            break;
        }
    }

    // Estimate small-signal gain G0 (average gain of the lower third points, or points <= -20 dBFS)
    double sumG0 = 0.0;
    int countG0 = 0;
    for (const auto& pt : sortedPts)
    {
        if (pt.inputLevelDbfs <= -20.0 || countG0 < 2)
        {
            sumG0 += pt.gainDb;
            ++countG0;
        }
    }
    res.smallSignalGainDb = (countG0 > 0) ? (sumG0 / countG0) : sortedPts.front().gainDb;

    // Helper for monotone linear interpolation of threshold crossing: y = yTarget
    auto findCrossing = [&](auto getValue) -> std::optional<double> {
        for (size_t i = 1; i < sortedPts.size(); ++i)
        {
            const double y0 = getValue(sortedPts[i - 1]);
            const double y1 = getValue(sortedPts[i]);
            const double x0 = sortedPts[i - 1].inputLevelDbfs;
            const double x1 = sortedPts[i].inputLevelDbfs;

            if ((y0 <= 0.0 && y1 >= 0.0) || (y0 >= 0.0 && y1 <= 0.0))
            {
                if (std::abs(y1 - y0) < 1e-9) return x0;
                const double fraction = -y0 / (y1 - y0);
                return x0 + fraction * (x1 - x0);
            }
        }
        return std::nullopt;
    };

    // 1% THD crossing (thdPercent = 1.0)
    res.thd1PercentInputDbfs = findCrossing([](const LevelSweepPoint& pt) {
        return pt.thdPercent - 1.0;
    });

    // 3% THD crossing (thdPercent = 3.0)
    res.thd3PercentInputDbfs = findCrossing([](const LevelSweepPoint& pt) {
        return pt.thdPercent - 3.0;
    });

    // P1dB crossing (gainDb = G0 - 1.0 => gainDrop = gainDb - (G0 - 1.0))
    res.p1dbInputDbfs = findCrossing([&](const LevelSweepPoint& pt) {
        return pt.gainDb - (res.smallSignalGainDb - 1.0);
    });

    res.status = "resolved";
    return res;
}

bool AnalogDutCharacterizer::detectHardClipping(
    std::span<const float> signal,
    float threshold) noexcept
{
    if (signal.empty()) return false;

    int consecutiveClips = 0;
    for (float s : signal)
    {
        if (std::abs(s) >= threshold)
        {
            ++consecutiveClips;
            if (consecutiveClips >= 3) return true; // At least 3 consecutive samples at rail
        }
        else
        {
            consecutiveClips = 0;
        }
    }
    return false;
}

} // namespace abdaudiolab::measurement
