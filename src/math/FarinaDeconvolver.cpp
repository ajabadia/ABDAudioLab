#include "FarinaDeconvolver.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math
{

FarinaDeconvolver::FarinaDeconvolver()
{
}

std::vector<float> FarinaDeconvolver::generateLogFarinaSweep(double sampleRate, double durationSec, float startFreqHz, float endFreqHz)
{
    const int totalSamples = static_cast<int>(std::lround(durationSec * sampleRate));
    std::vector<float> sweep(static_cast<size_t>(totalSamples), 0.0f);

    const double twoPi = 2.0 * std::numbers::pi;
    const double w1 = twoPi * startFreqHz;
    const double w2 = twoPi * endFreqHz;
    const double T = durationSec;
    const double logRatio = std::log(w2 / w1);
    const double K = (w1 * T) / logRatio;
    const double L = T / logRatio;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double phase = K * (std::exp(t / L) - 1.0);
        sweep[static_cast<size_t>(i)] = static_cast<float>(std::sin(phase));
    }

    return sweep;
}

std::vector<float> FarinaDeconvolver::generateInverseFilter(double sampleRate, double durationSec, float startFreqHz, float endFreqHz)
{
    const int totalSamples = static_cast<int>(std::lround(durationSec * sampleRate));
    std::vector<float> invFilter(static_cast<size_t>(totalSamples), 0.0f);

    const double twoPi = 2.0 * std::numbers::pi;
    const double w1 = twoPi * startFreqHz;
    const double w2 = twoPi * endFreqHz;
    const double T = durationSec;
    const double logRatio = std::log(w2 / w1);
    const double K = (w1 * T) / logRatio;
    const double L = T / logRatio;

    // Time-reversed sweep with exponential amplitude decay (-6 dB/octave)
    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double revT = T - t;

        // Instantaneous amplitude envelope: drops at -6 dB/oct
        // envelope = exp(-t * log(w2/w1) / T)
        double envelope = std::exp(-t * (logRatio / T));

        double phase = K * (std::exp(revT / L) - 1.0);
        invFilter[static_cast<size_t>(i)] = static_cast<float>(envelope * std::sin(phase));
    }

    return invFilter;
}

std::vector<float> FarinaDeconvolver::extractImpulseResponse(const std::vector<float>& recordedResponse,
                                                              const std::vector<float>& /*stimulusSweep*/,
                                                              double sampleRate,
                                                              double sweepDurationSec,
                                                              float startFreqHz,
                                                              float endFreqHz)
{
    auto invFilter = generateInverseFilter(sampleRate, sweepDurationSec, startFreqHz, endFreqHz);
    auto res = deconvolve(recordedResponse, invFilter, sampleRate, sweepDurationSec, startFreqHz, endFreqHz);
    return res.fullDeconvolvedIR;
}

DeconvolutionResult FarinaDeconvolver::deconvolve(const std::vector<float>& recordedResponse,
                                                 const std::vector<float>& inverseFilter,
                                                 double sampleRate,
                                                 double sweepDurationSec,
                                                 float startFreqHz,
                                                 float endFreqHz)
{
    DeconvolutionResult result;
    if (recordedResponse.empty() || inverseFilter.empty())
        return result;

    const size_t n1 = recordedResponse.size();
    const size_t n2 = inverseFilter.size();
    const size_t convLen = n1 + n2 - 1;

    // Find next power of 2 for FFT
    int fftOrder = 1;
    while ((1ULL << fftOrder) < convLen)
        fftOrder++;

    const size_t fftSize = 1ULL << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<std::complex<float>> in1(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> in2(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> out1(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> out2(fftSize, { 0.0f, 0.0f });

    for (size_t i = 0; i < n1; ++i)
        in1[i] = { recordedResponse[i], 0.0f };

    for (size_t i = 0; i < n2; ++i)
        in2[i] = { inverseFilter[i], 0.0f };

    fft.perform(in1.data(), out1.data(), false);
    fft.perform(in2.data(), out2.data(), false);

    // Frequency-domain multiplication
    std::vector<std::complex<float>> mult(fftSize, { 0.0f, 0.0f });
    for (size_t i = 0; i < fftSize; ++i)
    {
        mult[i] = out1[i] * out2[i];
    }

    // Inverse FFT
    std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
    fft.perform(mult.data(), timeDomain.data(), true);

    result.fullDeconvolvedIR.resize(convLen);
    float normFactor = 1.0f / static_cast<float>(fftSize);
    for (size_t i = 0; i < convLen; ++i)
    {
        result.fullDeconvolvedIR[i] = timeDomain[i].real() * normFactor;
    }

    // Locate main peak (linear impulse response arrival)
    auto maxIt = std::max_element(result.fullDeconvolvedIR.begin(), result.fullDeconvolvedIR.end(),
        [](float a, float b) { return std::abs(a) < std::abs(b); });

    size_t peakIndex = static_cast<size_t>(std::distance(result.fullDeconvolvedIR.begin(), maxIt));

    // Window around linear impulse response (e.g. 4096 samples around main peak)
    size_t irWindowSize = 4096;
    size_t irStart = (peakIndex > 256) ? (peakIndex - 256) : 0;
    size_t irEnd = std::min(irStart + irWindowSize, result.fullDeconvolvedIR.size());

    result.linearIR.assign(result.fullDeconvolvedIR.begin() + irStart, result.fullDeconvolvedIR.begin() + irEnd);

    // Compute Frequency Response of linear IR
    computeFrequencyResponse(result.linearIR, sampleRate, result.frequenciesHz, result.frequencyResponseMagnitudeDb,
                             result.peakFrequencyHz, result.resonancePeakDb);

    // Compute Phase and Group Delay
    computePhaseAndGroupDelay(result.linearIR, sampleRate, result.frequenciesHz,
                              result.phaseResponseRad, result.groupDelaySamples);

    // Calculate THD by measuring energy of harmonic distortion peaks before main linear peak
    // Farina offset for harmonics N=2..5: delta_t = T * ln(N) / ln(w2/w1)
    double logRatio = std::log(endFreqHz / startFreqHz);
    double dt2 = sweepDurationSec * std::log(2.0) / logRatio;
    double dt3 = sweepDurationSec * std::log(3.0) / logRatio;
    double dt4 = sweepDurationSec * std::log(4.0) / logRatio;
    double dt5 = sweepDurationSec * std::log(5.0) / logRatio;

    int sampleOffsetH2 = static_cast<int>(std::lround(dt2 * sampleRate));
    int sampleOffsetH3 = static_cast<int>(std::lround(dt3 * sampleRate));
    int sampleOffsetH4 = static_cast<int>(std::lround(dt4 * sampleRate));
    int sampleOffsetH5 = static_cast<int>(std::lround(dt5 * sampleRate));

    float linearEnergy = 0.0f;
    for (float v : result.linearIR)
        linearEnergy += v * v;

    float harmonicEnergy = 0.0f;
    float energyH2 = 0.0f;
    float energyH3 = 0.0f;
    float energyH4 = 0.0f;
    float energyH5 = 0.0f;

    auto extractHarmonicWindow = [&](int targetIdx, std::vector<float>& outIR) -> float {
        outIR.clear();
        if (targetIdx <= 0) return 0.0f;
        int win = 512;
        int s = std::max(0, targetIdx - win / 4);
        int e = std::min(static_cast<int>(result.fullDeconvolvedIR.size()), s + win);
        if (s >= e) return 0.0f;
        outIR.assign(result.fullDeconvolvedIR.begin() + s, result.fullDeconvolvedIR.begin() + e);
        float eSum = 0.0f;
        for (float val : outIR)
            eSum += val * val;
        return eSum;
    };

    if (peakIndex >= static_cast<size_t>(sampleOffsetH2))
    {
        energyH2 = extractHarmonicWindow(static_cast<int>(peakIndex) - sampleOffsetH2, result.h2IR);
        harmonicEnergy += energyH2;
    }
    if (peakIndex >= static_cast<size_t>(sampleOffsetH3))
    {
        energyH3 = extractHarmonicWindow(static_cast<int>(peakIndex) - sampleOffsetH3, result.h3IR);
        harmonicEnergy += energyH3;
    }
    if (peakIndex >= static_cast<size_t>(sampleOffsetH4))
    {
        energyH4 = extractHarmonicWindow(static_cast<int>(peakIndex) - sampleOffsetH4, result.h4IR);
        harmonicEnergy += energyH4;
    }
    if (peakIndex >= static_cast<size_t>(sampleOffsetH5))
    {
        energyH5 = extractHarmonicWindow(static_cast<int>(peakIndex) - sampleOffsetH5, result.h5IR);
        harmonicEnergy += energyH5;
    }

    if (linearEnergy > 1e-12f)
    {
        result.thdPercent = std::sqrt(harmonicEnergy / linearEnergy) * 100.0f;
        result.h2Percent  = std::sqrt(energyH2 / linearEnergy) * 100.0f;
        result.h3Percent  = std::sqrt(energyH3 / linearEnergy) * 100.0f;
        result.h4Percent  = std::sqrt(energyH4 / linearEnergy) * 100.0f;
        result.h5Percent  = std::sqrt(energyH5 / linearEnergy) * 100.0f;
    }
    else
    {
        result.thdPercent = 0.0f;
        result.h2Percent  = 0.0f;
        result.h3Percent  = 0.0f;
        result.h4Percent  = 0.0f;
        result.h5Percent  = 0.0f;
    }

    // Compute H2..H5 frequency responses if IRs extracted
    std::vector<float> hFreqs;
    float dummyPeakFreq = 0.0f, dummyPeakDb = -120.0f;
    if (!result.h2IR.empty())
    {
        computeFrequencyResponse(result.h2IR, sampleRate, hFreqs, result.h2MagnitudeDb,
                                 dummyPeakFreq, dummyPeakDb);
    }
    if (!result.h3IR.empty())
    {
        computeFrequencyResponse(result.h3IR, sampleRate, hFreqs, result.h3MagnitudeDb,
                                 dummyPeakFreq, dummyPeakDb);
    }
    if (!result.h4IR.empty())
    {
        computeFrequencyResponse(result.h4IR, sampleRate, hFreqs, result.h4MagnitudeDb,
                                 dummyPeakFreq, dummyPeakDb);
    }
    if (!result.h5IR.empty())
    {
        computeFrequencyResponse(result.h5IR, sampleRate, hFreqs, result.h5MagnitudeDb,
                                 dummyPeakFreq, dummyPeakDb);
    }

    // Compute THD(f) vs frequency curve across H2..H5
    const size_t numBins = result.frequenciesHz.size();
    result.thdVsFreqPercent.resize(numBins, 0.0f);
    for (size_t k = 0; k < numBins; ++k)
    {
        float linMag = std::pow(10.0f, result.frequencyResponseMagnitudeDb[k] / 20.0f);
        if (linMag > 1e-5f)
        {
            float h2Mag = (k < result.h2MagnitudeDb.size()) ? std::pow(10.0f, result.h2MagnitudeDb[k] / 20.0f) : 0.0f;
            float h3Mag = (k < result.h3MagnitudeDb.size()) ? std::pow(10.0f, result.h3MagnitudeDb[k] / 20.0f) : 0.0f;
            float h4Mag = (k < result.h4MagnitudeDb.size()) ? std::pow(10.0f, result.h4MagnitudeDb[k] / 20.0f) : 0.0f;
            float h5Mag = (k < result.h5MagnitudeDb.size()) ? std::pow(10.0f, result.h5MagnitudeDb[k] / 20.0f) : 0.0f;
            float thd = std::sqrt(h2Mag * h2Mag + h3Mag * h3Mag + h4Mag * h4Mag + h5Mag * h5Mag) / linMag * 100.0f;
            result.thdVsFreqPercent[k] = std::min(100.0f, thd);
        }
    }

    return result;
}

void FarinaDeconvolver::computeFrequencyResponse(const std::vector<float>& impulseResponse,
                                                 double sampleRate,
                                                 std::vector<float>& outFreqs,
                                                 std::vector<float>& outMagsDb,
                                                 float& outPeakFreq,
                                                 float& outPeakDb)
{
    const int fftOrder = 12; // 4096 points
    const size_t fftSize = 1ULL << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<float> timeData(fftSize * 2, 0.0f);
    size_t copyLen = std::min(impulseResponse.size(), fftSize);
    
    // Apply gentle right-tail taper (Tukey window) so the impulse arrival at the beginning
    // is preserved at unity gain without being distorted by a symmetric window zeroing at t=0.
    size_t taperStart = static_cast<size_t>(copyLen * 0.85);
    for (size_t i = 0; i < copyLen; ++i)
    {
        float win = 1.0f;
        if (i >= taperStart && copyLen > taperStart)
        {
            float phase = static_cast<float>(i - taperStart) / static_cast<float>(copyLen - taperStart);
            win = 0.5f * (1.0f + std::cos(static_cast<float>(std::numbers::pi) * phase));
        }
        timeData[i] = impulseResponse[i] * win;
    }

    fft.performFrequencyOnlyForwardTransform(timeData.data());

    const size_t numBins = fftSize / 2;
    outFreqs.resize(numBins);
    outMagsDb.resize(numBins);

    outPeakFreq = 0.0f;
    outPeakDb = -120.0f;

    float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    for (size_t i = 0; i < numBins; ++i)
    {
        float freq = static_cast<float>(i) * binWidth;
        float magLinear = std::max(timeData[i], 1e-6f);
        float magDb = 20.0f * std::log10(magLinear);

        outFreqs[i] = freq;
        outMagsDb[i] = magDb;

        // Track peak in audio band 20Hz - 20kHz
        if (freq >= 20.0f && freq <= 20000.0f && magDb > outPeakDb)
        {
            outPeakDb = magDb;
            outPeakFreq = freq;
        }
    }
}

void FarinaDeconvolver::computePhaseAndGroupDelay(const std::vector<float>& impulseResponse,
                                                 double sampleRate,
                                                 std::vector<float>& outFreqs,
                                                 std::vector<float>& outPhaseRad,
                                                 std::vector<float>& outGroupDelaySamples)
{
    const int fftOrder = 12; // 4096 points
    const size_t fftSize = 1ULL << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<std::complex<float>> inData(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> outData(fftSize, { 0.0f, 0.0f });

    size_t copyLen = std::min(impulseResponse.size(), fftSize);
    for (size_t i = 0; i < copyLen; ++i)
    {
        inData[i] = { impulseResponse[i], 0.0f };
    }

    fft.perform(inData.data(), outData.data(), false);

    const size_t numBins = fftSize / 2;
    outFreqs.resize(numBins);
    outPhaseRad.resize(numBins);
    outGroupDelaySamples.resize(numBins, 0.0f);

    float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // Compute raw phase
    for (size_t i = 0; i < numBins; ++i)
    {
        outFreqs[i] = static_cast<float>(i) * binWidth;
        outPhaseRad[i] = std::atan2(outData[i].imag(), outData[i].real());
    }

    // Phase unwrapping
    const float twoPi = 2.0f * static_cast<float>(std::numbers::pi);
    for (size_t i = 1; i < numBins; ++i)
    {
        float diff = outPhaseRad[i] - outPhaseRad[i - 1];
        while (diff > static_cast<float>(std::numbers::pi))
        {
            outPhaseRad[i] -= twoPi;
            diff -= twoPi;
        }
        while (diff < -static_cast<float>(std::numbers::pi))
        {
            outPhaseRad[i] += twoPi;
            diff += twoPi;
        }
    }

    // Group delay: tau_g[k] = - d(phi) / d(omega)
    // d(omega) = 2 * pi * binWidth / sampleRate = 2 * pi / fftSize rad/sample
    const float deltaOmega = twoPi / static_cast<float>(fftSize);
    for (size_t i = 1; i + 1 < numBins; ++i)
    {
        float dPhi = (outPhaseRad[i + 1] - outPhaseRad[i - 1]) * 0.5f;
        outGroupDelaySamples[i] = -dPhi / deltaOmega;
    }
    if (numBins > 1)
    {
        outGroupDelaySamples[0] = outGroupDelaySamples[1];
        outGroupDelaySamples[numBins - 1] = outGroupDelaySamples[numBins - 2];
    }
}

} // namespace abdaudiolab::math
