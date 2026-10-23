#include "FarinaDeconvolver.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math
{

FarinaDeconvolver::FarinaDeconvolver()
{
}

void FarinaDeconvolver::applyAsymmetricHalfHannWindow(std::vector<float>& buffer,
                                                      double sampleRate,
                                                      double fadeInMs,
                                                      double fadeOutMs) noexcept
{
    if (buffer.empty() || sampleRate <= 0.0)
        return;

    const size_t N = buffer.size();
    size_t fadeInSamples = (fadeInMs > 0.0)
        ? static_cast<size_t>(std::lround((fadeInMs * 0.001) * sampleRate))
        : 0;
    size_t fadeOutSamples = (fadeOutMs > 0.0)
        ? static_cast<size_t>(std::lround((fadeOutMs * 0.001) * sampleRate))
        : 0;

    if (fadeInSamples + fadeOutSamples > N)
    {
        const double scale = static_cast<double>(N) / static_cast<double>(fadeInSamples + fadeOutSamples);
        fadeInSamples = static_cast<size_t>(std::floor(static_cast<double>(fadeInSamples) * scale));
        fadeOutSamples = N - fadeInSamples;
    }

    if (fadeInSamples > 0)
    {
        const double invIn = std::numbers::pi / static_cast<double>(fadeInSamples);
        for (size_t n = 0; n < fadeInSamples; ++n)
        {
            const float w = static_cast<float>(0.5 * (1.0 - std::cos(invIn * static_cast<double>(n))));
            buffer[n] *= w;
        }
    }

    if (fadeOutSamples > 0)
    {
        const size_t startOut = N - fadeOutSamples;
        const double invOut = std::numbers::pi / static_cast<double>(fadeOutSamples);
        for (size_t k = 0; k < fadeOutSamples; ++k)
        {
            const float w = static_cast<float>(0.5 * (1.0 + std::cos(invOut * static_cast<double>(k + 1))));
            buffer[startOut + k] *= w;
        }
    }
}

std::vector<float> FarinaDeconvolver::generateLogFarinaSweep(double sampleRate,
                                                             double durationSec,
                                                             float startFreqHz,
                                                             float endFreqHz,
                                                             double outputFadeInMs,
                                                             double outputFadeOutMs)
{
    const int totalSamples = static_cast<int>(std::lround(durationSec * sampleRate));
    if (totalSamples <= 0 || sampleRate <= 0.0 || startFreqHz <= 0.0f || endFreqHz <= startFreqHz)
        return {};

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

    // Scale fade durations proportionally if durationSec < 1.0 s so passband [40 Hz, 18 kHz] is preserved
    const double durationScale = std::min(1.0, std::max(0.0, durationSec));
    const double effectiveFadeInMs = (outputFadeInMs > 0.0) ? (outputFadeInMs * durationScale) : 0.0;
    const double effectiveFadeOutMs = (outputFadeOutMs > 0.0) ? (outputFadeOutMs * durationScale) : 0.0;
    applyAsymmetricHalfHannWindow(sweep, sampleRate, effectiveFadeInMs, effectiveFadeOutMs);

    return sweep;
}

std::vector<float> FarinaDeconvolver::generateGuardedLogFarinaSweep(double sampleRate,
                                                                    double durationSec,
                                                                    float startFreqHz,
                                                                    float endFreqHz,
                                                                    const FarinaSweepWindowConfig& config)
{
    if (sampleRate <= 0.0 || durationSec <= 0.0)
        return {};

    auto coreSweep = generateLogFarinaSweep(sampleRate, durationSec, startFreqHz, endFreqHz,
                                            config.outputFadeInMs, config.outputFadeOutMs);
    const size_t preSamples = (config.safeSilenceMs > 0.0)
        ? static_cast<size_t>(std::lround((config.safeSilenceMs * 0.001) * sampleRate))
        : 0;
    const size_t postSamples = (config.interSignalSilenceMs > 0.0)
        ? static_cast<size_t>(std::lround((config.interSignalSilenceMs * 0.001) * sampleRate))
        : 0;

    std::vector<float> guarded(preSamples + coreSweep.size() + postSamples, 0.0f);
    std::copy(coreSweep.begin(), coreSweep.end(), guarded.begin() + static_cast<std::ptrdiff_t>(preSamples));
    return guarded;
}

std::vector<float> FarinaDeconvolver::generateInverseFilter(double sampleRate, double durationSec, float startFreqHz, float endFreqHz)
{
    const int totalSamples = static_cast<int>(std::lround(durationSec * sampleRate));
    if (totalSamples <= 0 || sampleRate <= 0.0 || startFreqHz <= 0.0f || endFreqHz <= startFreqHz)
        return {};

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
                                                 float endFreqHz,
                                                 double inputFadeInMs,
                                                 double inputFadeOutMs)
{
    DeconvolutionResult result;
    if (recordedResponse.empty() || inverseFilter.empty())
        return result;

    std::vector<float> windowedResponse = recordedResponse;
    const double durationScale = std::min(1.0, std::max(0.0, sweepDurationSec));
    applyAsymmetricHalfHannWindow(windowedResponse,
                                  sampleRate,
                                  (inputFadeInMs > 0.0) ? (inputFadeInMs * durationScale) : 0.0,
                                  (inputFadeOutMs > 0.0) ? (inputFadeOutMs * durationScale) : 0.0);

    const size_t n1 = windowedResponse.size();
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
        in1[i] = { windowedResponse[i], 0.0f };

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

PilotSweepLayout FarinaDeconvolver::generatePilotGuardedSweep(
    double sampleRate,
    double durationSec,
    float startFreqHz,
    float endFreqHz,
    const PilotPulseSweepConfig& config)
{
    PilotSweepLayout layout;
    if (sampleRate <= 0.0 || durationSec <= 0.0 || startFreqHz <= 0.0f || endFreqHz <= startFreqHz)
        return layout;

    const size_t preSilence = static_cast<size_t>(std::max(0L, std::lround((config.preSilenceMs * 0.001) * sampleRate)));
    size_t pilotLen = static_cast<size_t>(std::max(17L, std::lround((config.pilotDurationMs * 0.001) * sampleRate)));
    if ((pilotLen % 2) == 0)
        ++pilotLen; // Odd length so sinc center is an exact integer sample

    const size_t pilotToSweep = static_cast<size_t>(std::max(0L, std::lround((config.pilotToSweepGuardMs * 0.001) * sampleRate)));
    const size_t sweepToPost = static_cast<size_t>(std::max(0L, std::lround((config.sweepToPostPilotGuardMs * 0.001) * sampleRate)));
    const size_t postSilence = static_cast<size_t>(std::max(0L, std::lround((config.postSilenceMs * 0.001) * sampleRate)));

    // Generate Hann-windowed bandlimited sinc-acoustical pilot (passband 0.04*fs .. 0.38*fs)
    layout.referencePilot.resize(pilotLen, 0.0f);
    const double center = 0.5 * static_cast<double>(pilotLen - 1);
    const double fcHigh = 0.38;
    const double fcLow = 0.04;
    double maxAbs = 0.0;

    for (size_t n = 0; n < pilotLen; ++n)
    {
        const double x = static_cast<double>(n) - center;
        const double hann = 0.5 * (1.0 - std::cos((2.0 * std::numbers::pi * static_cast<double>(n)) / static_cast<double>(pilotLen - 1)));
        auto normSinc = [](double u) -> double {
            if (std::abs(u) < 1e-9)
                return 1.0;
            const double pix = std::numbers::pi * u;
            return std::sin(pix) / pix;
        };
        const double bandpassSinc = (2.0 * fcHigh * normSinc(2.0 * fcHigh * x))
                                  - (2.0 * fcLow * normSinc(2.0 * fcLow * x));
        const double val = hann * bandpassSinc;
        layout.referencePilot[n] = static_cast<float>(val);
        maxAbs = std::max(maxAbs, std::abs(val));
    }

    if (maxAbs > 1e-12)
    {
        const float scale = config.pilotAmplitude / static_cast<float>(maxAbs);
        for (float& v : layout.referencePilot)
            v *= scale;
    }

    auto coreSweep = generateLogFarinaSweep(sampleRate, durationSec, startFreqHz, endFreqHz);
    layout.prePilotStartSample = preSilence;
    layout.sweepStartSample = preSilence + pilotLen + pilotToSweep;
    layout.sweepLengthSamples = coreSweep.size();
    layout.postPilotStartSample = layout.sweepStartSample + coreSweep.size() + sweepToPost;
    layout.nominalPilotDistanceSamples = static_cast<double>(layout.postPilotStartSample - layout.prePilotStartSample);

    const size_t totalLen = layout.postPilotStartSample + pilotLen + postSilence;
    layout.compositeSignal.assign(totalLen, 0.0f);

    std::copy(layout.referencePilot.begin(),
              layout.referencePilot.end(),
              layout.compositeSignal.begin() + static_cast<std::ptrdiff_t>(layout.prePilotStartSample));

    std::copy(coreSweep.begin(),
              coreSweep.end(),
              layout.compositeSignal.begin() + static_cast<std::ptrdiff_t>(layout.sweepStartSample));

    std::copy(layout.referencePilot.begin(),
              layout.referencePilot.end(),
              layout.compositeSignal.begin() + static_cast<std::ptrdiff_t>(layout.postPilotStartSample));

    return layout;
}

ClockDriftPilotResult FarinaDeconvolver::analyzePilotClockDrift(
    const std::vector<float>& recordedSignal,
    const PilotSweepLayout& layout,
    double sampleRate,
    const PilotPulseSweepConfig& config)
{
    ClockDriftPilotResult res;
    if (recordedSignal.empty() || layout.referencePilot.empty() || sampleRate <= 0.0)
        return res;

    const size_t pilotLen = layout.referencePilot.size();
    if (recordedSignal.size() < layout.postPilotStartSample + pilotLen)
        return res;

    auto findPilotSubSample = [&](size_t searchStart, size_t searchEnd) -> std::pair<bool, double> {
        if (searchEnd <= searchStart + pilotLen || searchEnd > recordedSignal.size())
            return { false, 0.0 };

        const size_t maxLag = searchEnd - pilotLen;
        std::vector<double> corr(maxLag - searchStart + 1, 0.0);
        size_t bestIdx = 0;
        double bestVal = -1.0;

        for (size_t lag = searchStart; lag <= maxLag; ++lag)
        {
            double sum = 0.0;
            for (size_t k = 0; k < pilotLen; ++k)
                sum += static_cast<double>(recordedSignal[lag + k]) * static_cast<double>(layout.referencePilot[k]);

            const size_t cIdx = lag - searchStart;
            corr[cIdx] = sum;
            if (sum > bestVal)
            {
                bestVal = sum;
                bestIdx = cIdx;
            }
        }

        if (bestVal <= 1e-9)
            return { false, 0.0 };

        double delta = 0.0;
        if (bestIdx > 0 && bestIdx + 1 < corr.size())
        {
            const double ym1 = corr[bestIdx - 1];
            const double y0  = corr[bestIdx];
            const double yp1 = corr[bestIdx + 1];
            const double denom = 2.0 * (ym1 - 2.0 * y0 + yp1);
            if (std::abs(denom) > 1e-12)
                delta = std::clamp((ym1 - yp1) / denom, -0.5, 0.5);
        }

        return { true, static_cast<double>(searchStart + bestIdx) + delta };
    };

    const size_t preSearchEnd = std::min(recordedSignal.size(), layout.sweepStartSample);
    const size_t postSearchStart = layout.sweepStartSample + layout.sweepLengthSamples;
    const size_t postSearchEnd = recordedSignal.size();

    auto [okPre, prePos] = findPilotSubSample(0, preSearchEnd);
    auto [okPost, postPos] = findPilotSubSample(postSearchStart, postSearchEnd);
    if (!okPre || !okPost || postPos <= prePos)
        return res;

    res.measuredPrePilotSample = prePos;
    res.measuredPostPilotSample = postPos;
    res.nominalDistanceSamples = layout.nominalPilotDistanceSamples;
    res.measuredDistanceSamples = postPos - prePos;
    res.sampleDrift = res.measuredDistanceSamples - res.nominalDistanceSamples;
    res.driftPpm = (res.nominalDistanceSamples > 0.0)
        ? ((res.sampleDrift / res.nominalDistanceSamples) * 1.0e6)
        : 0.0;
    res.dropoutDetected = std::abs(res.sampleDrift) >= config.dropoutThresholdSamples;
    res.clockDriftExcessive = std::abs(res.driftPpm) > config.maxAllowedDriftPpm;
    res.valid = true;
    return res;
}

std::vector<float> FarinaDeconvolver::computeKirkebyRegularizationProfile(
    size_t fftSize,
    double sampleRate,
    float startFreqHz,
    float endFreqHz,
    const KirkebyRegularizationConfig& config)
{
    const size_t numBins = fftSize / 2 + 1;
    if (fftSize == 0 || sampleRate <= 0.0)
        return {};

    const float epsIn = std::max(1e-12f, config.passbandEpsilon);
    const float epsOut = std::max(epsIn, config.outOfBandEpsilon);
    const double logEpsIn = std::log10(static_cast<double>(epsIn));
    const double logEpsOut = std::log10(static_cast<double>(epsOut));
    const double transOct = std::max(0.05, static_cast<double>(config.transitionOctaves));

    const double f1 = std::max(1.0, static_cast<double>(startFreqHz));
    const double f2 = std::max(f1 + 1.0, static_cast<double>(endFreqHz));
    const double fLowStop = f1 * std::pow(2.0, -transOct);
    const double fHighStop = f2 * std::pow(2.0, transOct);
    const double binWidth = sampleRate / static_cast<double>(fftSize);

    std::vector<float> profile(numBins, epsOut);
    for (size_t k = 0; k < numBins; ++k)
    {
        const double fk = static_cast<double>(k) * binWidth;
        if (fk <= fLowStop || fk >= fHighStop)
        {
            profile[k] = epsOut;
        }
        else if (fk >= f1 && fk <= f2)
        {
            profile[k] = epsIn;
        }
        else if (fk < f1)
        {
            const double u = std::clamp(std::log2(f1 / fk) / transOct, 0.0, 1.0);
            const double w = 0.5 * (1.0 - std::cos(std::numbers::pi * u));
            const double logEps = logEpsIn + w * (logEpsOut - logEpsIn);
            profile[k] = static_cast<float>(std::pow(10.0, logEps));
        }
        else
        {
            const double u = std::clamp(std::log2(fk / f2) / transOct, 0.0, 1.0);
            const double w = 0.5 * (1.0 - std::cos(std::numbers::pi * u));
            const double logEps = logEpsIn + w * (logEpsOut - logEpsIn);
            profile[k] = static_cast<float>(std::pow(10.0, logEps));
        }
    }

    return profile;
}

DeconvolutionResult FarinaDeconvolver::deconvolveKirkeby(
    const std::vector<float>& recordedResponse,
    const std::vector<float>& referenceStimulus,
    double sampleRate,
    float startFreqHz,
    float endFreqHz,
    const KirkebyRegularizationConfig& config)
{
    DeconvolutionResult result;
    if (recordedResponse.empty() || referenceStimulus.empty() || sampleRate <= 0.0)
        return result;

    const size_t maxLen = std::max(recordedResponse.size(), referenceStimulus.size());
    int fftOrder = 1;
    while ((1ULL << fftOrder) < maxLen * 2)
        ++fftOrder;

    const size_t fftSize = 1ULL << fftOrder;
    const size_t halfFft = fftSize / 2;
    juce::dsp::FFT fft(fftOrder);

    std::vector<std::complex<float>> yTime(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> xTime(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> Y(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> X(fftSize, { 0.0f, 0.0f });

    for (size_t i = 0; i < recordedResponse.size(); ++i)
        yTime[i] = { recordedResponse[i], 0.0f };
    for (size_t i = 0; i < referenceStimulus.size(); ++i)
        xTime[i] = { referenceStimulus[i], 0.0f };

    fft.perform(yTime.data(), Y.data(), false);
    fft.perform(xTime.data(), X.data(), false);

    float maxPower = 1e-12f;
    for (size_t k = 0; k <= halfFft; ++k)
        maxPower = std::max(maxPower, std::norm(X[k]));

    auto epsProfile = computeKirkebyRegularizationProfile(fftSize, sampleRate, startFreqHz, endFreqHz, config);
    std::vector<std::complex<float>> H(fftSize, { 0.0f, 0.0f });

    for (size_t k = 0; k <= halfFft; ++k)
    {
        const float p = std::norm(X[k]);
        const float reg = epsProfile[k] * maxPower;
        const float denom = std::max(p + reg, 1e-20f);
        H[k] = (Y[k] * std::conj(X[k])) / denom;
        if (k > 0 && k < halfFft)
            H[fftSize - k] = std::conj(H[k]);
    }

    std::vector<std::complex<float>> hTime(fftSize, { 0.0f, 0.0f });
    fft.perform(H.data(), hTime.data(), true);

    result.fullDeconvolvedIR.resize(fftSize, 0.0f);
    for (size_t i = 0; i < fftSize; ++i)
        result.fullDeconvolvedIR[i] = hTime[i].real();

    const size_t irWindowSize = std::min<size_t>(4096, fftSize);
    result.linearIR.assign(result.fullDeconvolvedIR.begin(),
                           result.fullDeconvolvedIR.begin() + static_cast<std::ptrdiff_t>(irWindowSize));

    computeFrequencyResponse(result.linearIR, sampleRate, result.frequenciesHz,
                             result.frequencyResponseMagnitudeDb,
                             result.peakFrequencyHz, result.resonancePeakDb);
    computePhaseAndGroupDelay(result.linearIR, sampleRate, result.frequenciesHz,
                              result.phaseResponseRad, result.groupDelaySamples);
    return result;
}

} // namespace abdaudiolab::math

