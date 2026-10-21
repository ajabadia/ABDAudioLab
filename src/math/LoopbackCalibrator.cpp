#include "LoopbackCalibrator.h"
#include "FarinaDeconvolver.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <complex>

namespace abdaudiolab::math
{

NaturalCubicSpline1D::NaturalCubicSpline1D(const std::vector<float>& xKnots, const std::vector<float>& yKnots)
{
    build(xKnots, yKnots);
}

bool NaturalCubicSpline1D::build(const std::vector<float>& xKnots, const std::vector<float>& yKnots)
{
    x_.clear();
    a_.clear();
    b_.clear();
    c_.clear();
    d_.clear();

    if (xKnots.empty() || xKnots.size() != yKnots.size())
        return false;

    // Filter strictly increasing finite knots
    for (size_t i = 0; i < xKnots.size(); ++i)
    {
        const double xi = static_cast<double>(xKnots[i]);
        const double yi = static_cast<double>(yKnots[i]);
        if (!std::isfinite(xi) || !std::isfinite(yi))
            continue;
        if (x_.empty() || xi > x_.back() + 1e-9)
        {
            x_.push_back(xi);
            a_.push_back(yi);
        }
    }

    const size_t n = x_.size();
    if (n == 0)
        return false;

    b_.assign(n, 0.0);
    c_.assign(n, 0.0);
    d_.assign(n, 0.0);

    if (n == 1)
        return true;

    if (n == 2)
    {
        const double h = x_[1] - x_[0];
        b_[0] = (h > 0.0) ? ((a_[1] - a_[0]) / h) : 0.0;
        b_[1] = b_[0];
        return true;
    }

    const size_t m = n - 1;
    std::vector<double> h(m, 0.0);
    for (size_t i = 0; i < m; ++i)
        h[i] = x_[i + 1] - x_[i];

    std::vector<double> alpha(m, 0.0);
    for (size_t i = 1; i < m; ++i)
    {
        alpha[i] = (3.0 / h[i]) * (a_[i + 1] - a_[i]) - (3.0 / h[i - 1]) * (a_[i] - a_[i - 1]);
    }

    // Thomas algorithm for natural boundary conditions c_[0] = c_[n-1] = 0
    std::vector<double> l(n, 1.0);
    std::vector<double> mu(n, 0.0);
    std::vector<double> z(n, 0.0);

    for (size_t i = 1; i < m; ++i)
    {
        l[i] = 2.0 * (x_[i + 1] - x_[i - 1]) - h[i - 1] * mu[i - 1];
        if (std::abs(l[i]) < 1e-12)
            l[i] = 1e-12;
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    for (size_t j = m; j-- > 0;)
    {
        c_[j] = z[j] - mu[j] * c_[j + 1];
        b_[j] = ((a_[j + 1] - a_[j]) / h[j]) - (h[j] * (c_[j + 1] + 2.0 * c_[j]) / 3.0);
        d_[j] = (c_[j + 1] - c_[j]) / (3.0 * h[j]);
    }

    return true;
}

float NaturalCubicSpline1D::evaluate(float xVal) const noexcept
{
    if (x_.empty())
        return 0.0f;
    if (x_.size() == 1)
        return static_cast<float>(a_.front());

    const double x = static_cast<double>(xVal);
    if (x <= x_.front())
        return static_cast<float>(a_.front());
    if (x >= x_.back())
        return static_cast<float>(a_.back());

    auto it = std::upper_bound(x_.begin(), x_.end(), x);
    size_t idx = static_cast<size_t>(std::distance(x_.begin(), it));
    if (idx > 0)
        --idx;
    if (idx >= x_.size() - 1)
        idx = x_.size() - 2;

    const double dx = x - x_[idx];
    const double val = a_[idx] + dx * (b_[idx] + dx * (c_[idx] + dx * d_[idx]));
    return static_cast<float>(val);
}

double LoopbackCalibrator::besselI0(double x) noexcept
{
    const double ax = std::abs(x);
    if (ax < 3.75)
    {
        // Abramowitz & Stegun 9.8.1: t = (x / 3.75)^2 = x^2 / 14.0625
        const double t = (x * x) / 14.0625;
        return 1.0 + t * (3.5156229 + t * (3.0899424 + t * (1.2067492
                   + t * (0.2659732 + t * (0.0360768 + t * 0.0045813)))));
    }

    // Abramowitz & Stegun 9.8.2: t = 3.75 / |x|
    const double t = 3.75 / ax;
    const double poly = 0.39894228 + t * (0.01328592 + t * (0.00225319 + t * (-0.00157565
                        + t * (0.00916281 + t * (-0.02057706 + t * (0.02635537
                        + t * (-0.01647633 + t * 0.00392377)))))));
    return (std::exp(ax) / std::sqrt(ax)) * poly;
}

std::vector<float> LoopbackCalibrator::generateKaiserWindow(size_t length, double beta)
{
    if (length == 0)
        return {};
    if (length == 1)
        return { 1.0f };

    std::vector<float> window(length, 1.0f);
    const double denom = besselI0(beta);
    if (denom <= 0.0)
        return window;

    const double halfM = 0.5 * static_cast<double>(length - 1);
    for (size_t n = 0; n < length; ++n)
    {
        const double ratio = (static_cast<double>(n) - halfM) / halfM;
        const double rad = std::max(0.0, 1.0 - ratio * ratio);
        window[n] = static_cast<float>(besselI0(beta * std::sqrt(rad)) / denom);
    }
    return window;
}

double LoopbackCalibrator::freqToErbBandwidthHz(double freqHz) noexcept
{
    const double f = std::max(0.0, freqHz);
    return 24.7 * (0.00437 * f + 1.0);
}

double LoopbackCalibrator::freqToErbRate(double freqHz) noexcept
{
    const double f = std::max(0.0, freqHz);
    return 21.4 * std::log10(0.00437 * f + 1.0);
}

double LoopbackCalibrator::erbRateToFreq(double erbRate) noexcept
{
    const double e = std::max(0.0, erbRate);
    return (std::pow(10.0, e / 21.4) - 1.0) / 0.00437;
}

std::vector<float> LoopbackCalibrator::smoothSpectrumErb(
    const std::vector<float>& frequenciesHz,
    const std::vector<float>& magnitudesDb,
    float erbBandwidthFactor)
{
    if (frequenciesHz.empty() || frequenciesHz.size() != magnitudesDb.size())
        return magnitudesDb;

    const size_t N = frequenciesHz.size();
    std::vector<double> linearPower(N, 0.0);
    for (size_t i = 0; i < N; ++i)
    {
        const double db = std::clamp(static_cast<double>(magnitudesDb[i]), -160.0, 60.0);
        linearPower[i] = std::pow(10.0, db / 10.0);
    }

    const double scale = std::max(0.05, static_cast<double>(erbBandwidthFactor));
    std::vector<float> smoothedDb(N, 0.0f);

    for (size_t k = 0; k < N; ++k)
    {
        const double fk = static_cast<double>(frequenciesHz[k]);
        if (fk <= 0.0)
        {
            smoothedDb[k] = magnitudesDb[k];
            continue;
        }

        const double halfBw = 0.5 * scale * freqToErbBandwidthHz(fk);
        const double fLow = std::max(0.0, fk - halfBw);
        const double fHigh = fk + halfBw;

        auto itStart = std::lower_bound(frequenciesHz.begin(), frequenciesHz.end(), static_cast<float>(fLow));
        auto itEnd = std::upper_bound(frequenciesHz.begin(), frequenciesHz.end(), static_cast<float>(fHigh));

        size_t iStart = static_cast<size_t>(std::distance(frequenciesHz.begin(), itStart));
        size_t iEnd = static_cast<size_t>(std::distance(frequenciesHz.begin(), itEnd));

        double weightedSum = 0.0;
        double weightTotal = 0.0;

        for (size_t i = iStart; i < iEnd; ++i)
        {
            const double dist = std::abs(static_cast<double>(frequenciesHz[i]) - fk);
            const double w = (halfBw > 1e-6) ? std::max(0.0, 1.0 - (dist / halfBw)) : 1.0;
            weightedSum += w * linearPower[i];
            weightTotal += w;
        }

        if (weightTotal > 1e-12)
        {
            const double avgPow = std::max(weightedSum / weightTotal, 1e-16);
            smoothedDb[k] = static_cast<float>(10.0 * std::log10(avgPow));
        }
        else
        {
            smoothedDb[k] = magnitudesDb[k];
        }
    }

    return smoothedDb;
}

MinimumPhaseFirResult LoopbackCalibrator::synthesizeMinimumPhaseFir(
    const LoopbackCalibrationData& calData,
    double sampleRate,
    size_t firLength,
    double kaiserBeta)
{
    if (sampleRate <= 0.0)
        sampleRate = (calData.sampleRate > 0.0) ? calData.sampleRate : 48000.0;

    MinimumPhaseFirResult res;
    if (firLength == 0 || sampleRate <= 0.0)
        return res;

    // Choose FFT size >= 4 * firLength for accurate Hilbert / Real Cepstrum wrap-free liftering
    int cepOrder = 12; // 4096 minimum
    while ((1ULL << cepOrder) < firLength * 4)
        ++cepOrder;

    const size_t cepSize = 1ULL << cepOrder;
    const size_t halfCep = cepSize / 2;
    juce::dsp::FFT cepFft(cepOrder);

    NaturalCubicSpline1D spline;
    const bool hasSpline = (!calData.freqsHz.empty() &&
                            calData.freqsHz.size() == calData.inverseCorrectionDb.size() &&
                            spline.build(calData.freqsHz, calData.inverseCorrectionDb));

    float minAllowedDb = -12.0f;
    float maxAllowedDb = 6.0f;
    if (!calData.inverseCorrectionDb.empty())
    {
        auto [minIt, maxIt] = std::minmax_element(calData.inverseCorrectionDb.begin(),
                                                  calData.inverseCorrectionDb.end());
        minAllowedDb = std::clamp(*minIt, -12.0f, 6.0f);
        maxAllowedDb = std::clamp(*maxIt, -12.0f, 6.0f);
        if (minAllowedDb > maxAllowedDb)
            std::swap(minAllowedDb, maxAllowedDb);
    }

    std::vector<std::complex<float>> specIn(cepSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> cepstrum(cepSize, { 0.0f, 0.0f });

    const float binWidth = static_cast<float>(sampleRate / static_cast<double>(cepSize));
    const double ln10Over20 = std::numbers::ln10 / 20.0;

    double bandPowerSum = 0.0;
    size_t bandCount = 0;
    float maxBoostDb = -120.0f;

    for (size_t k = 0; k <= halfCep; ++k)
    {
        const float f = static_cast<float>(k) * binWidth;
        float corrDb = 0.0f;
        if (hasSpline)
            corrDb = std::clamp(spline.evaluate(f), minAllowedDb, maxAllowedDb);
        else if (!calData.inverseCorrectionDb.empty())
            corrDb = std::clamp(calData.inverseCorrectionDb.front(), minAllowedDb, maxAllowedDb);

        if (f >= 20.0f && f <= 20000.0f)
        {
            bandPowerSum += std::pow(10.0, static_cast<double>(corrDb) / 10.0);
            ++bandCount;
            if (corrDb > maxBoostDb)
                maxBoostDb = corrDb;
        }

        const float logMag = static_cast<float>(static_cast<double>(corrDb) * ln10Over20);
        specIn[k] = { logMag, 0.0f };
        if (k > 0 && k < halfCep)
            specIn[cepSize - k] = { logMag, 0.0f };
    }

    if (bandCount > 0)
    {
        res.averagePowerDb = static_cast<float>(10.0 * std::log10(std::max(bandPowerSum / static_cast<double>(bandCount), 1e-12)));
        res.maxBoostDb = maxBoostDb;
        res.safeHeadroomLinear = (maxBoostDb > 0.0f)
            ? std::pow(10.0f, -maxBoostDb / 20.0f)
            : 1.0f;
    }

    // 1. Real Cepstrum: c_hat[n] = IFFT(ln|H_inv[k]|)
    cepFft.perform(specIn.data(), cepstrum.data(), true);

    // 2. Causal Minimum-Phase Lifter w_min[n]: {1 at n=0, N/2; 2 for 1..N/2-1; 0 for N/2+1..N-1}
    for (size_t n = 1; n < halfCep; ++n)
        cepstrum[n] *= 2.0f;
    for (size_t n = halfCep + 1; n < cepSize; ++n)
        cepstrum[n] = { 0.0f, 0.0f };

    // 3. Complex Minimum-Phase Spectrum: H_min[k] = exp(FFT(w_min * c_hat))
    std::vector<std::complex<float>> minPhaseSpec(cepSize, { 0.0f, 0.0f });
    cepFft.perform(cepstrum.data(), minPhaseSpec.data(), false);

    for (size_t k = 0; k < cepSize; ++k)
        minPhaseSpec[k] = std::exp(minPhaseSpec[k]);

    // 4. Causal Impulse Response: h_min[n] = Re{IFFT(H_min[k])}
    std::vector<std::complex<float>> minPhaseIr(cepSize, { 0.0f, 0.0f });
    cepFft.perform(minPhaseSpec.data(), minPhaseIr.data(), true);

    // 5. Truncate to firLength with one-sided (right-half) Kaiser window so h[0] retains unity weight
    auto fullKaiser = generateKaiserWindow(firLength * 2, kaiserBeta);
    res.filterIr.resize(firLength, 0.0f);
    for (size_t n = 0; n < firLength; ++n)
    {
        const float w = fullKaiser[firLength + n];
        res.filterIr[n] = minPhaseIr[n].real() * w;
    }

    res.impulseResponse = res.filterIr;
    res.isValid = !res.filterIr.empty();

    return res;
}

float LoopbackCalibrator::computeSafeSweepMaxHz(double sampleRate) noexcept
{
    if (sampleRate <= 0.0)
        return 20000.0f;
    return static_cast<float>(std::min(20000.0, 0.45 * sampleRate));
}

std::pair<float, float> LoopbackCalibrator::computeFlatnessBandHz(double sampleRate) noexcept
{
    constexpr float lowHz = 40.0f;
    if (sampleRate <= 0.0)
        return { lowHz, 18000.0f };
    float highHz = static_cast<float>(std::min(18000.0, 0.40 * sampleRate));
    if (highHz < lowHz)
        highHz = lowHz;
    return { lowHz, highHz };
}

FlatnessEvaluation LoopbackCalibrator::evaluateFlatnessInBand(
    const std::vector<float>& frequenciesHz,
    const std::vector<float>& magnitudesDb,
    float lowHz,
    float highHz) noexcept
{
    FlatnessEvaluation eval;
    eval.hasValidBins = false;
    eval.deltaDb = std::numeric_limits<float>::infinity();
    eval.validBinCount = 0;

    if (frequenciesHz.empty() || frequenciesHz.size() != magnitudesDb.size() || lowHz >= highHz)
        return eval;

    float minMag = std::numeric_limits<float>::infinity();
    float maxMag = -std::numeric_limits<float>::infinity();

    for (size_t i = 0; i < frequenciesHz.size(); ++i)
    {
        float f = frequenciesHz[i];
        if (f >= lowHz && f <= highHz)
        {
            float mag = magnitudesDb[i];
            if (!std::isnan(mag) && !std::isinf(mag))
            {
                if (mag < minMag)
                {
                    minMag = mag;
                    eval.minFreqHz = f;
                }
                if (mag > maxMag)
                {
                    maxMag = mag;
                    eval.maxFreqHz = f;
                }
                eval.validBinCount++;
            }
        }
    }

    if (eval.validBinCount > 0 && maxMag >= minMag)
    {
        eval.hasValidBins = true;
        eval.deltaDb = maxMag - minMag;
        eval.minMagDb = minMag;
        eval.maxMagDb = maxMag;
    }

    return eval;
}

LoopbackCalibrationData LoopbackCalibrator::analyzeLoopback(const std::vector<float>& recordedResponse,
                                                          double sampleRate,
                                                          double sweepDurationSec,
                                                          float startFreqHz,
                                                          float endFreqHz,
                                                          float targetDbfs,
                                                          std::optional<float> measuredNoiseFloorRmsDb)
{
    LoopbackCalibrationData result;
    result.sampleRate = sampleRate;
    result.targetHeadroomDbfs = targetDbfs;
    result.timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S");

    if (recordedResponse.empty() || sampleRate <= 0.0)
        return result;

    if (endFreqHz <= 0.0f)
        endFreqHz = computeSafeSweepMaxHz(sampleRate);

    // 1. Calculate Peak Level & RMS
    float maxVal = 0.0f;
    double sumSq = 0.0;
    int clipCount = 0;
    double dcSum = 0.0;
    size_t dcSamplesCount = std::min(recordedResponse.size(), static_cast<size_t>(1024));

    for (size_t i = 0; i < recordedResponse.size(); ++i)
    {
        float s = recordedResponse[i];
        float a = std::abs(s);
        if (a > maxVal) maxVal = a;
        sumSq += static_cast<double>(s) * s;

        if (a >= 0.999f)
            ++clipCount;

        if (i < dcSamplesCount)
            dcSum += static_cast<double>(s);
    }

    result.clippingDetected = (clipCount > 0);
    result.clippedSamplesCount = clipCount;
    result.dcOffsetVolts = (dcSamplesCount > 0) ? static_cast<float>(dcSum / static_cast<double>(dcSamplesCount)) : 0.0f;

    if (maxVal < 1e-5f)
    {
        // Silence or disconnected loopback
        result.peakInDbfs = -100.0f;
        result.isCalibrated = false;
        return result;
    }

    result.peakInDbfs = 20.0f * std::log10(std::max(maxVal, 1e-6f));
    float targetLinear = std::pow(10.0f, targetDbfs / 20.0f); // e.g. -3 dBfs -> 0.7079
    result.recommendedTrimGain = targetLinear / maxVal;
    result.recommendedTrimGain = std::clamp(result.recommendedTrimGain, 0.01f, 100.0f);

    // 2. Farina Deconvolution for Frequency Response & IR
    auto invFilter = FarinaDeconvolver::generateInverseFilter(sampleRate, sweepDurationSec, startFreqHz, endFreqHz);
    auto decon = FarinaDeconvolver::deconvolve(recordedResponse, invFilter, sampleRate, sweepDurationSec, startFreqHz, endFreqHz);

    result.freqsHz = decon.frequenciesHz;
    result.magnitudeDb = smoothSpectrumErb(decon.frequenciesHz, decon.frequencyResponseMagnitudeDb, 0.25f);
    result.thdPlusNoisePercent = decon.thdPercent;

    // 3. Extract Latency & Phase Polarity (Peak of Linear IR)
    if (!decon.linearIR.empty())
    {
        auto maxIt = std::max_element(decon.linearIR.begin(), decon.linearIR.end(),
                                     [](float a, float b) { return std::abs(a) < std::abs(b); });
        result.latencySamples = static_cast<int>(std::distance(decon.linearIR.begin(), maxIt));
        result.roundTripLatencyMs = static_cast<float>((result.latencySamples / sampleRate) * 1000.0);

        // Polarity check: inspect signed value at the impulse response peak
        float peakAmp = *maxIt;
        result.phaseInversionCorrelation = peakAmp;
        result.phaseInversionDetected = (peakAmp < -1e-4f);
    }

    // 4. Compute Flatness and Inverse Compensation Curve
    result.inverseCorrectionDb.resize(result.magnitudeDb.size());

    // Normalize curve around 1 kHz reference
    float ref1kHzDb = 0.0f;
    for (size_t i = 0; i < result.freqsHz.size(); ++i)
    {
        if (result.freqsHz[i] >= 900.0f && result.freqsHz[i] <= 1100.0f)
        {
            ref1kHzDb = result.magnitudeDb[i];
            break;
        }
    }

    const double nyquist = sampleRate * 0.5;
    constexpr float epsPassband = 1e-4f; // Epsilon in 20Hz - 20kHz
    constexpr float maxInverseGainLinear = 2.0f; // Strict +6 dB maximum correction ceiling

    for (size_t i = 0; i < result.magnitudeDb.size(); ++i)
    {
        float normalizedDb = result.magnitudeDb[i] - ref1kHzDb;
        result.magnitudeDb[i] = normalizedDb;

        float freq = (i < result.freqsHz.size()) ? result.freqsHz[i] : 1000.0f;

        // Dynamic epsilon(f) using smooth Tukey-style transition
        float eps = epsPassband;
        if (freq < 20.0f)
        {
            // Subsonic transition: 0Hz -> 20Hz
            float ratio = std::clamp(freq / 20.0f, 0.0f, 1.0f);
            float taper = 0.5f * (1.0f + std::cos(static_cast<float>(std::numbers::pi) * ratio));
            eps = epsPassband + (1.0f - epsPassband) * taper;
        }
        else if (freq > 20000.0f && nyquist > 20000.0)
        {
            // Ultrasonic transition: 20kHz -> Nyquist
            float ratio = std::clamp(static_cast<float>((freq - 20000.0) / (nyquist - 20000.0)), 0.0f, 1.0f);
            float taper = 0.5f * (1.0f - std::cos(static_cast<float>(std::numbers::pi) * ratio));
            eps = epsPassband + (1.0f - epsPassband) * taper;
        }

        // Kirkeby regularized inversion in linear domain: H_inv = H / (H^2 + eps)
        float hLinear = std::pow(10.0f, normalizedDb / 20.0f);
        float hInv = hLinear / (hLinear * hLinear + eps);

        // Security clamp: max +6 dB (factor 2.0f)
        hInv = std::min(hInv, maxInverseGainLinear);

        result.inverseCorrectionDb[i] = 20.0f * std::log10(std::max(hInv, 1e-5f));
    }

    // Flatness evaluation within adaptive sample rate passband
    auto [flatLowHz, flatHighHz] = computeFlatnessBandHz(sampleRate);
    auto flatnessEval = evaluateFlatnessInBand(result.freqsHz, result.magnitudeDb, flatLowHz, flatHighHz);
    result.frequencyFlatnessDb = flatnessEval.deltaDb;
    result.flatnessMinMagDb = flatnessEval.minMagDb;
    result.flatnessMaxMagDb = flatnessEval.maxMagDb;
    result.flatnessMinFreqHz = flatnessEval.minFreqHz;
    result.flatnessMaxFreqHz = flatnessEval.maxFreqHz;

    // 5. Signal-to-Noise Ratio (SNR)
    double rms = std::sqrt(sumSq / static_cast<double>(recordedResponse.size()));
    float rmsDb = 20.0f * std::log10(std::max(static_cast<float>(rms), 1e-6f));
    if (measuredNoiseFloorRmsDb.has_value())
    {
        // Real physical calibrated SNR: L_signal,RMS,dBFS - L_noise,RMS,dBFS
        result.snrDb = std::clamp(rmsDb - (*measuredNoiseFloorRmsDb), 0.0f, 140.0f);
        result.snrMethod = SnrMeasurementMethod::PhysicalNoiseBaseline;
    }
    else
    {
        // Legacy fallback when baseline was not measured
        result.snrDb = std::clamp(rmsDb - (-96.0f), 20.0f, 130.0f);
        result.snrMethod = SnrMeasurementMethod::LegacyAssumedNoiseFloor;
    }

    result.isCalibrated = (flatnessEval.hasValidBins &&
                           !result.clippingDetected &&
                           result.peakInDbfs > -40.0f &&
                           result.frequencyFlatnessDb < 6.0f);
    return result;
}

bool LoopbackCalibrator::saveCalibrationToJson(const LoopbackCalibrationData& data, const juce::File& file)
{
    try
    {
        nlohmann::json j;
        j["schemaVersion"] = "1.0";
        j["timestamp"] = data.timestamp.toStdString();
        j["deviceName"] = data.deviceName.toStdString();
        j["sampleRate"] = data.sampleRate;
        j["peakInDbfs"] = data.peakInDbfs;
        j["recommendedTrimGain"] = data.recommendedTrimGain;
        j["targetHeadroomDbfs"] = data.targetHeadroomDbfs;
        j["roundTripLatencyMs"] = data.roundTripLatencyMs;
        j["latencySamples"] = data.latencySamples;
        j["thdPlusNoisePercent"] = data.thdPlusNoisePercent;
        j["snrDb"] = data.snrDb;
        j["frequencyFlatnessDb"] = data.frequencyFlatnessDb;
        j["phaseInversionDetected"] = data.phaseInversionDetected;
        j["phaseInversionCorrelation"] = data.phaseInversionCorrelation;
        j["clippingDetected"] = data.clippingDetected;
        j["clippedSamplesCount"] = data.clippedSamplesCount;
        j["dcOffsetVolts"] = data.dcOffsetVolts;
        j["isCalibrated"] = data.isCalibrated;

        j["frequenciesHz"] = data.freqsHz;
        j["magnitudeDb"] = data.magnitudeDb;
        j["inverseCorrectionDb"] = data.inverseCorrectionDb;

        std::ofstream out(file.getFullPathName().toStdString());
        if (!out.is_open()) return false;
        out << j.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

LoopbackCalibrationData LoopbackCalibrator::loadCalibrationFromJson(const juce::File& file)
{
    LoopbackCalibrationData data;
    if (!file.existsAsFile()) return data;

    try
    {
        std::ifstream in(file.getFullPathName().toStdString());
        if (!in.is_open()) return data;
        nlohmann::json j;
        in >> j;

        data.timestamp = juce::String(j.value("timestamp", std::string("")));
        data.deviceName = juce::String(j.value("deviceName", std::string("")));
        data.sampleRate = j.value("sampleRate", 96000.0);
        data.peakInDbfs = j.value("peakInDbfs", -100.0f);
        data.recommendedTrimGain = j.value("recommendedTrimGain", 1.0f);
        data.targetHeadroomDbfs = j.value("targetHeadroomDbfs", -3.0f);
        data.roundTripLatencyMs = j.value("roundTripLatencyMs", 0.0f);
        data.latencySamples = j.value("latencySamples", 0);
        data.thdPlusNoisePercent = j.value("thdPlusNoisePercent", 0.0f);
        data.snrDb = j.value("snrDb", 90.0f);
        data.frequencyFlatnessDb = j.value("frequencyFlatnessDb", 0.1f);
        data.phaseInversionDetected = j.value("phaseInversionDetected", false);
        data.phaseInversionCorrelation = j.value("phaseInversionCorrelation", 1.0f);
        data.clippingDetected = j.value("clippingDetected", false);
        data.clippedSamplesCount = j.value("clippedSamplesCount", 0);
        data.dcOffsetVolts = j.value("dcOffsetVolts", 0.0f);
        data.isCalibrated = j.value("isCalibrated", false);

        if (j.contains("frequenciesHz") && j["frequenciesHz"].is_array())
            data.freqsHz = j["frequenciesHz"].get<std::vector<float>>();

        if (j.contains("magnitudeDb") && j["magnitudeDb"].is_array())
            data.magnitudeDb = j["magnitudeDb"].get<std::vector<float>>();

        if (j.contains("inverseCorrectionDb") && j["inverseCorrectionDb"].is_array())
            data.inverseCorrectionDb = j["inverseCorrectionDb"].get<std::vector<float>>();
    }
    catch (...)
    {
        data.isCalibrated = false;
    }
    return data;
}

void LoopbackCalibrator::applyInverseCompensation(std::vector<float>& audio,
                                                 const LoopbackCalibrationData& calData,
                                                 double sampleRate,
                                                 bool avoidClippingEnabled)
{
    if (!calData.isCalibrated || calData.inverseCorrectionDb.empty() || audio.empty() || sampleRate <= 0.0)
        return;

    // 1. Synthesize causal Minimum-Phase FIR kernel (M = 1024 taps) with Kaiser window (beta = 8.0)
    constexpr size_t firLen = 1024;
    constexpr int olaOrder = 11; // N_OLA = 2048 >= L (1024) + M (1024) - 1
    constexpr size_t olaSize = 1ULL << olaOrder;
    constexpr size_t blockStep = olaSize - firLen; // L = 1024 samples per hop

    const auto firRes = synthesizeMinimumPhaseFir(calData, sampleRate, firLen, 8.0);
    if (firRes.filterIr.empty())
        return;

    juce::dsp::FFT olaFft(olaOrder);
    std::vector<std::complex<float>> filterTime(olaSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> filterSpec(olaSize, { 0.0f, 0.0f });

    const float headroomScale = avoidClippingEnabled ? firRes.safeHeadroomLinear : 1.0f;
    for (size_t n = 0; n < firRes.filterIr.size(); ++n)
        filterTime[n] = { firRes.filterIr[n] * headroomScale, 0.0f };

    olaFft.perform(filterTime.data(), filterSpec.data(), false);

    // 2. Fast Linear Convolution via Overlap-Add (OLA)
    std::vector<std::complex<float>> blockTime(olaSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> blockSpec(olaSize, { 0.0f, 0.0f });
    std::vector<float> overlapTail(firLen - 1, 0.0f);

    size_t pos = 0;
    const size_t totalSamples = audio.size();
    while (pos < totalSamples)
    {
        const size_t L = std::min(blockStep, totalSamples - pos);
        for (size_t i = 0; i < olaSize; ++i)
        {
            blockTime[i] = (i < L) ? std::complex<float>{ audio[pos + i], 0.0f }
                                   : std::complex<float>{ 0.0f, 0.0f };
        }

        olaFft.perform(blockTime.data(), blockSpec.data(), false);

        for (size_t k = 0; k < olaSize; ++k)
            blockSpec[k] *= filterSpec[k];

        olaFft.perform(blockSpec.data(), blockTime.data(), true);

        // Write valid L output samples adding previous overlap tail
        for (size_t i = 0; i < L; ++i)
        {
            const float tail = (i < overlapTail.size()) ? overlapTail[i] : 0.0f;
            audio[pos + i] = blockTime[i].real() + tail;
        }

        // Update overlapTail for next block
        const size_t tailLen = overlapTail.size();
        for (size_t i = 0; i < tailLen; ++i)
        {
            const float prevRemainder = (i + L < tailLen) ? overlapTail[i + L] : 0.0f;
            const float newTail = (i + L < olaSize) ? blockTime[i + L].real() : 0.0f;
            overlapTail[i] = prevRemainder + newTail;
        }

        pos += L;
    }
}

DeadTimeCompensatedPidController::DeadTimeCompensatedPidController(
    const DeadTimePidConfig& cfg,
    double chunkDurationSec,
    double roundTripDeadTimeSec)
{
    configure(cfg, chunkDurationSec, roundTripDeadTimeSec);
}

void DeadTimeCompensatedPidController::configure(
    const DeadTimePidConfig& cfg,
    double chunkDurationSec,
    double roundTripDeadTimeSec) noexcept
{
    cfg_ = cfg;
    const double dtc = std::max(1e-4, chunkDurationSec);
    const double tau = std::max(0.0, roundTripDeadTimeSec);
    eta_ = static_cast<float>(dtc / (dtc + tau));
    reset(0.0f);
}

void DeadTimeCompensatedPidController::reset(float initialGainDb) noexcept
{
    currentGainDb_ = std::clamp(initialGainDb, cfg_.minGainDb, cfg_.maxGainDb);
    integralError_ = 0.0f;
    prevError_ = 0.0f;
    stableChunksCount_ = 0;
    hasPrevError_ = false;
}

float DeadTimeCompensatedPidController::getCurrentGainLinear() const noexcept
{
    return std::pow(10.0f, currentGainDb_ / 20.0f);
}

DeadTimePidStepResult DeadTimeCompensatedPidController::update(
    float measuredPeakDbfs,
    bool hardwareClipped) noexcept
{
    DeadTimePidStepResult res;
    res.deadTimeAttenuationEta = eta_;

    // [LIMIT-PID RESET]: Instant anti-windup protection on hardware/digital saturation
    if (hardwareClipped || !std::isfinite(measuredPeakDbfs) || measuredPeakDbfs >= cfg_.clipThresholdDbfs)
    {
        integralError_ = 0.0f;
        prevError_ = 0.0f;
        hasPrevError_ = false;
        stableChunksCount_ = 0;
        currentGainDb_ = std::clamp(currentGainDb_ - 6.0f, cfg_.minGainDb, cfg_.maxGainDb);

        res.gainDb = currentGainDb_;
        res.currentGainDb = currentGainDb_;
        res.gainLinear = getCurrentGainLinear();
        res.errorDb = cfg_.targetDbfs - measuredPeakDbfs;
        res.stableChunksCount = 0;
        res.isStable = false;
        res.limitPidResetTriggered = true;
        return res;
    }

    const float errorDb = cfg_.targetDbfs - measuredPeakDbfs;
    const float derivError = hasPrevError_ ? (errorDb - prevError_) : 0.0f;

    // Candidate integral update with anti-windup clamping
    const float maxIntegralSpan = (cfg_.ki > 1e-6f)
        ? ((cfg_.maxGainDb - cfg_.minGainDb) / cfg_.ki)
        : 0.0f;
    integralError_ = std::clamp(integralError_ + errorDb, -maxIntegralSpan, maxIntegralSpan);

    const float rawDeltaDb = cfg_.kp * errorDb + cfg_.ki * integralError_ + cfg_.kd * derivError;
    const float attenuatedDeltaDb = eta_ * rawDeltaDb;

    currentGainDb_ = std::clamp(currentGainDb_ + attenuatedDeltaDb, cfg_.minGainDb, cfg_.maxGainDb);
    prevError_ = errorDb;
    hasPrevError_ = true;

    if (std::abs(errorDb) <= cfg_.toleranceDb)
        ++stableChunksCount_;
    else
        stableChunksCount_ = 0;

    res.gainDb = currentGainDb_;
    res.currentGainDb = currentGainDb_;
    res.gainLinear = getCurrentGainLinear();
    res.errorDb = errorDb;
    res.stableChunksCount = stableChunksCount_;
    res.isStable = (stableChunksCount_ >= cfg_.stabilityMinChunks);
    res.limitPidResetTriggered = false;
    return res;
}

bool ZeroLatencyPartitionedConvolver::prepare(const std::vector<float>& firKernel, size_t partitionSize)
{
    prepared_ = false;
    if (firKernel.empty() || partitionSize == 0)
        return false;

    int partOrder = 4; // minimum 16 samples
    while ((1ULL << partOrder) < partitionSize)
        ++partOrder;

    partSize_ = 1ULL << partOrder;
    fftSize_ = partSize_ * 2;
    fft_ = std::make_unique<juce::dsp::FFT>(partOrder + 1); // 2 * B

    // 1. Direct time-domain head h[0 .. B-1]
    headCoeffs_.assign(partSize_, 0.0f);
    const size_t headCopy = std::min(firKernel.size(), partSize_);
    for (size_t i = 0; i < headCopy; ++i)
        headCoeffs_[i] = firKernel[i];

    headHistory_.assign(partSize_, 0.0f);
    inputBlock_.assign(partSize_, 0.0f);
    tailCurrentBlock_.assign(partSize_, 0.0f);
    tailOverlap_.assign(partSize_, 0.0f);

    fftTimeScratch_.assign(fftSize_, { 0.0f, 0.0f });
    accumSpecScratch_.assign(fftSize_, { 0.0f, 0.0f });

    // 2. Partitioned FDL tail for h[B .. M-1]
    tailPartitionsSpec_.clear();
    fdl_.clear();

    if (firKernel.size() > partSize_)
    {
        const size_t tailSamples = firKernel.size() - partSize_;
        const size_t numTailParts = (tailSamples + partSize_ - 1) / partSize_;
        tailPartitionsSpec_.resize(numTailParts, std::vector<std::complex<float>>(fftSize_, { 0.0f, 0.0f }));
        fdl_.resize(numTailParts, std::vector<std::complex<float>>(fftSize_, { 0.0f, 0.0f }));

        for (size_t p = 0; p < numTailParts; ++p)
        {
            std::fill(fftTimeScratch_.begin(), fftTimeScratch_.end(), std::complex<float>{ 0.0f, 0.0f });
            const size_t startTap = (p + 1) * partSize_;
            const size_t copyLen = std::min(partSize_, firKernel.size() - startTap);
            for (size_t k = 0; k < copyLen; ++k)
                fftTimeScratch_[k] = { firKernel[startTap + k], 0.0f };

            fft_->perform(fftTimeScratch_.data(), tailPartitionsSpec_[p].data(), false);
        }
    }

    reset();
    prepared_ = true;
    return true;
}

void ZeroLatencyPartitionedConvolver::reset() noexcept
{
    headWritePos_ = 0;
    blockPos_ = 0;
    fdlWriteIdx_ = 0;
    std::fill(headHistory_.begin(), headHistory_.end(), 0.0f);
    std::fill(inputBlock_.begin(), inputBlock_.end(), 0.0f);
    std::fill(tailCurrentBlock_.begin(), tailCurrentBlock_.end(), 0.0f);
    std::fill(tailOverlap_.begin(), tailOverlap_.end(), 0.0f);
    for (auto& slot : fdl_)
        std::fill(slot.begin(), slot.end(), std::complex<float>{ 0.0f, 0.0f });
}

void ZeroLatencyPartitionedConvolver::stepFrequencyDomainTail() noexcept
{
    const size_t numTailParts = tailPartitionsSpec_.size();
    if (numTailParts == 0 || !fft_)
        return;

    // 1. Transform just-completed inputBlock_[0..B-1] into fdl_[fdlWriteIdx_]
    for (size_t i = 0; i < partSize_; ++i)
        fftTimeScratch_[i] = { inputBlock_[i], 0.0f };
    for (size_t i = partSize_; i < fftSize_; ++i)
        fftTimeScratch_[i] = { 0.0f, 0.0f };

    fft_->perform(fftTimeScratch_.data(), fdl_[fdlWriteIdx_].data(), false);

    // 2. Frequency-Domain Delay Line multiply-accumulate across tail partitions p = 0 .. numTailParts-1
    std::fill(accumSpecScratch_.begin(), accumSpecScratch_.end(), std::complex<float>{ 0.0f, 0.0f });
    for (size_t p = 0; p < numTailParts; ++p)
    {
        const size_t fdlIdx = (fdlWriteIdx_ + numTailParts - p) % numTailParts;
        const auto& X = fdl_[fdlIdx];
        const auto& H = tailPartitionsSpec_[p];
        for (size_t k = 0; k < fftSize_; ++k)
            accumSpecScratch_[k] += X[k] * H[k];
    }

    // Advance circular FDL write pointer for next block
    fdlWriteIdx_ = (fdlWriteIdx_ + 1) % numTailParts;

    // 3. Inverse FFT and Overlap-Add for the upcoming block of B samples
    fft_->perform(accumSpecScratch_.data(), fftTimeScratch_.data(), true);

    for (size_t i = 0; i < partSize_; ++i)
    {
        tailCurrentBlock_[i] = fftTimeScratch_[i].real() + tailOverlap_[i];
        tailOverlap_[i] = fftTimeScratch_[partSize_ + i].real();
    }
}

void ZeroLatencyPartitionedConvolver::processBlock(float* inPlaceAudio, size_t numSamples) noexcept
{
    if (!prepared_ || inPlaceAudio == nullptr || numSamples == 0)
        return;

    const size_t mask = partSize_ - 1; // partSize_ is a power of 2
    for (size_t n = 0; n < numSamples; ++n)
    {
        const float x = inPlaceAudio[n];
        headHistory_[headWritePos_] = x;
        inputBlock_[blockPos_] = x;

        // Direct time-domain head convolution: sum_{m=0}^{B-1} h[m] * x[n - m]
        float acc = tailCurrentBlock_[blockPos_];
        for (size_t m = 0; m < partSize_; ++m)
        {
            const size_t histIdx = (headWritePos_ - m) & mask;
            acc += headCoeffs_[m] * headHistory_[histIdx];
        }

        inPlaceAudio[n] = acc;
        headWritePos_ = (headWritePos_ + 1) & mask;
        ++blockPos_;

        if (blockPos_ == partSize_)
        {
            blockPos_ = 0;
            stepFrequencyDomainTail();
        }
    }
}

void ZeroLatencyPartitionedConvolver::processBlock(const float* input, float* output, size_t numSamples) noexcept
{
    if (!prepared_ || input == nullptr || output == nullptr || numSamples == 0)
        return;

    if (input == output)
    {
        processBlock(output, numSamples);
        return;
    }

    const size_t mask = partSize_ - 1; // partSize_ is a power of 2
    for (size_t n = 0; n < numSamples; ++n)
    {
        const float x = input[n];
        headHistory_[headWritePos_] = x;
        inputBlock_[blockPos_] = x;

        // Direct time-domain head convolution: sum_{m=0}^{B-1} h[m] * x[n - m]
        float acc = tailCurrentBlock_[blockPos_];
        for (size_t m = 0; m < partSize_; ++m)
        {
            const size_t histIdx = (headWritePos_ - m) & mask;
            acc += headCoeffs_[m] * headHistory_[histIdx];
        }

        output[n] = acc;
        headWritePos_ = (headWritePos_ + 1) & mask;
        ++blockPos_;

        if (blockPos_ == partSize_)
        {
            blockPos_ = 0;
            stepFrequencyDomainTail();
        }
    }
}

} // namespace abdaudiolab::math



