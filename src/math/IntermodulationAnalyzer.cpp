/**
 * @file IntermodulationAnalyzer.cpp
 * @brief Implementation of SMPTE and CCIF Intermodulation Distortion analysis.
 * @author ABDSynths
 * @date 2026
 */

#include "IntermodulationAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>

namespace abdaudiolab::math
{

std::vector<float> IntermodulationAnalyzer::generateSmpteStimulus(double sampleRate,
                                                                  double durationSec,
                                                                  float lowFreqHz,
                                                                  float highFreqHz,
                                                                  float amplitudeRatio)
{
    const size_t totalSamples = static_cast<size_t>(std::lround(sampleRate * durationSec));
    std::vector<float> stimulus(totalSamples, 0.0f);

    const float aLow = amplitudeRatio / (amplitudeRatio + 1.0f); // 4/5 = 0.8
    const float aHigh = 1.0f / (amplitudeRatio + 1.0f);          // 1/5 = 0.2

    const double phaseIncLow = 2.0 * std::numbers::pi * lowFreqHz / sampleRate;
    const double phaseIncHigh = 2.0 * std::numbers::pi * highFreqHz / sampleRate;

    double phaseLow = 0.0;
    double phaseHigh = 0.0;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        stimulus[i] = static_cast<float>(aLow * std::sin(phaseLow) + aHigh * std::sin(phaseHigh));
        phaseLow += phaseIncLow;
        phaseHigh += phaseIncHigh;
        if (phaseLow >= 2.0 * std::numbers::pi) phaseLow -= 2.0 * std::numbers::pi;
        if (phaseHigh >= 2.0 * std::numbers::pi) phaseHigh -= 2.0 * std::numbers::pi;
    }

    return stimulus;
}

std::vector<float> IntermodulationAnalyzer::generateCcifStimulus(double sampleRate,
                                                                 double durationSec,
                                                                 float f1Hz,
                                                                 float f2Hz)
{
    const size_t totalSamples = static_cast<size_t>(std::lround(sampleRate * durationSec));
    std::vector<float> stimulus(totalSamples, 0.0f);

    const double phaseInc1 = 2.0 * std::numbers::pi * f1Hz / sampleRate;
    const double phaseInc2 = 2.0 * std::numbers::pi * f2Hz / sampleRate;

    double phase1 = 0.0;
    double phase2 = 0.0;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        stimulus[i] = static_cast<float>(0.5 * (std::sin(phase1) + std::sin(phase2)));
        phase1 += phaseInc1;
        phase2 += phaseInc2;
        if (phase1 >= 2.0 * std::numbers::pi) phase1 -= 2.0 * std::numbers::pi;
        if (phase2 >= 2.0 * std::numbers::pi) phase2 -= 2.0 * std::numbers::pi;
    }

    return stimulus;
}

namespace
{
    float getPeakMagnitudeAroundFreq(const std::vector<float>& magDb, double sampleRate, size_t fftSize, float targetFreqHz, float windowBandHz = 30.0f)
    {
        const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        int centerBin = static_cast<int>(std::lround(targetFreqHz / binWidth));
        int deltaBins = std::max(1, static_cast<int>(std::ceil(windowBandHz / binWidth)));

        int s = std::max(0, centerBin - deltaBins);
        int e = std::min(static_cast<int>(magDb.size()), centerBin + deltaBins + 1);

        float maxLinear = 0.0f;
        for (int i = s; i < e; ++i)
        {
            float lin = std::pow(10.0f, magDb[static_cast<size_t>(i)] / 20.0f);
            if (lin > maxLinear)
                maxLinear = lin;
        }
        return maxLinear;
    }
}

ImdResult IntermodulationAnalyzer::analyzeSmpte(const std::vector<float>& recordedSignal,
                                               double sampleRate,
                                               float lowFreqHz,
                                               float highFreqHz)
{
    ImdResult result;
    if (recordedSignal.empty())
        return result;

    const int fftOrder = 13; // 8192 points for high frequency resolution
    const size_t fftSize = 1ULL << fftOrder;
    if (recordedSignal.size() < fftSize)
        return result;

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> timeData(fftSize * 2, 0.0f);

    // Take center segment to avoid edge transients
    size_t offset = (recordedSignal.size() > fftSize) ? (recordedSignal.size() - fftSize) / 2 : 0;
    for (size_t i = 0; i < fftSize; ++i)
    {
        float win = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(std::numbers::pi) * static_cast<float>(i) / static_cast<float>(fftSize)));
        timeData[i] = recordedSignal[offset + i] * win;
    }

    fft.performFrequencyOnlyForwardTransform(timeData.data());

    const size_t numBins = fftSize / 2;
    std::vector<float> magDb(numBins, -120.0f);
    for (size_t i = 0; i < numBins; ++i)
    {
        float mag = timeData[i] / static_cast<float>(fftSize);
        magDb[i] = (mag > 1e-6f) ? (20.0f * std::log10(mag)) : -120.0f;
    }

    // Carrier magnitude at highFreqHz (7000 Hz)
    float vCarrier = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, highFreqHz);
    if (vCarrier < 1e-5f)
        return result;

    // 2nd order sidebands: f_high ± f_low (6940, 7060 Hz)
    float v2Minus = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, highFreqHz - lowFreqHz);
    float v2Plus  = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, highFreqHz + lowFreqHz);

    // 3rd order sidebands: f_high ± 2*f_low (6880, 7120 Hz)
    float v3Minus = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, highFreqHz - 2.0f * lowFreqHz);
    float v3Plus  = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, highFreqHz + 2.0f * lowFreqHz);

    float sumD2Sqr = v2Minus * v2Minus + v2Plus * v2Plus;
    float sumD3Sqr = v3Minus * v3Minus + v3Plus * v3Plus;

    result.d2Percent = (std::sqrt(sumD2Sqr) / vCarrier) * 100.0f;
    result.d3Percent = (std::sqrt(sumD3Sqr) / vCarrier) * 100.0f;
    result.totalImdPercent = (std::sqrt(sumD2Sqr + sumD3Sqr) / vCarrier) * 100.0f;

    return result;
}

ImdResult IntermodulationAnalyzer::analyzeCcif(const std::vector<float>& recordedSignal,
                                              double sampleRate,
                                              float f1Hz,
                                              float f2Hz)
{
    ImdResult result;
    if (recordedSignal.empty())
        return result;

    const int fftOrder = 13; // 8192 points
    const size_t fftSize = 1ULL << fftOrder;
    if (recordedSignal.size() < fftSize)
        return result;

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> timeData(fftSize * 2, 0.0f);

    size_t offset = (recordedSignal.size() > fftSize) ? (recordedSignal.size() - fftSize) / 2 : 0;
    for (size_t i = 0; i < fftSize; ++i)
    {
        float win = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(std::numbers::pi) * static_cast<float>(i) / static_cast<float>(fftSize)));
        timeData[i] = recordedSignal[offset + i] * win;
    }

    fft.performFrequencyOnlyForwardTransform(timeData.data());

    const size_t numBins = fftSize / 2;
    std::vector<float> magDb(numBins, -120.0f);
    for (size_t i = 0; i < numBins; ++i)
    {
        float mag = timeData[i] / static_cast<float>(fftSize);
        magDb[i] = (mag > 1e-6f) ? (20.0f * std::log10(mag)) : -120.0f;
    }

    // Fundamental tones f1 and f2
    float v1 = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, f1Hz);
    float v2 = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, f2Hz);
    float vFund = (v1 + v2) * 0.5f;
    if (vFund < 1e-5f)
        return result;

    // 2nd order difference: |f2 - f1| (e.g. 1000 Hz)
    float diffFreq = std::abs(f2Hz - f1Hz);
    float vDiff = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, diffFreq);

    // 3rd order products: 2*f1 - f2 (18 kHz) and 2*f2 - f1 (21 kHz)
    float v3a = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, 2.0f * f1Hz - f2Hz);
    float v3b = getPeakMagnitudeAroundFreq(magDb, sampleRate, fftSize, 2.0f * f2Hz - f1Hz);

    result.d2Percent = (vDiff / vFund) * 100.0f;
    result.d3Percent = (std::sqrt(v3a * v3a + v3b * v3b) / vFund) * 100.0f;
    result.totalImdPercent = (std::sqrt(vDiff * vDiff + v3a * v3a + v3b * v3b) / vFund) * 100.0f;

    return result;
}

} // namespace abdaudiolab::math
