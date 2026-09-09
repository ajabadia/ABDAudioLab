#include "LoopbackCalibrator.h"
#include "FarinaDeconvolver.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math
{

LoopbackCalibrationData LoopbackCalibrator::analyzeLoopback(const std::vector<float>& recordedResponse,
                                                          double sampleRate,
                                                          double sweepDurationSec,
                                                          float startFreqHz,
                                                          float endFreqHz,
                                                          float targetDbfs)
{
    LoopbackCalibrationData result;
    result.sampleRate = sampleRate;
    result.targetHeadroomDbfs = targetDbfs;
    result.timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S");

    if (recordedResponse.empty() || sampleRate <= 0.0)
        return result;

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
    result.magnitudeDb = decon.frequencyResponseMagnitudeDb;
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
    float minMag = 100.0f;
    float maxMag = -100.0f;
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

        // Flatness window within audible band 20 Hz - 20 kHz
        if (i < result.freqsHz.size() && result.freqsHz[i] >= 20.0f && result.freqsHz[i] <= 20000.0f)
        {
            minMag = std::min(minMag, normalizedDb);
            maxMag = std::max(maxMag, normalizedDb);
        }
    }

    result.frequencyFlatnessDb = (maxMag >= minMag) ? (maxMag - minMag) : 0.0f;

    // 5. Signal-to-Noise Ratio (SNR)
    double rms = std::sqrt(sumSq / static_cast<double>(recordedResponse.size()));
    float rmsDb = 20.0f * std::log10(std::max(static_cast<float>(rms), 1e-6f));
    result.snrDb = std::clamp(rmsDb - (-96.0f), 20.0f, 130.0f);

    result.isCalibrated = (result.peakInDbfs > -40.0f && result.frequencyFlatnessDb < 6.0f);
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
                                                 double sampleRate)
{
    if (!calData.isCalibrated || calData.inverseCorrectionDb.empty() || audio.empty())
        return;

    // Use partitioned overlap-add or FFT-based filtering in blocks of 2048/4096 points
    const int fftOrder = 11; // 2048 points
    const size_t fftSize = 1ULL << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    const size_t numBins = fftSize / 2;
    float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // Build frequency-domain multiplier per bin interpolated from calData.inverseCorrectionDb
    std::vector<float> gainFactors(numBins, 1.0f);
    for (size_t b = 0; b < numBins; ++b)
    {
        float f = static_cast<float>(b) * binWidth;
        // Nearest or linear lookup in calData
        float corrDb = 0.0f;
        if (!calData.freqsHz.empty())
        {
            auto it = std::lower_bound(calData.freqsHz.begin(), calData.freqsHz.end(), f);
            if (it == calData.freqsHz.end())
                corrDb = calData.inverseCorrectionDb.back();
            else if (it == calData.freqsHz.begin())
                corrDb = calData.inverseCorrectionDb.front();
            else
            {
                size_t idx = static_cast<size_t>(std::distance(calData.freqsHz.begin(), it));
                float f0 = calData.freqsHz[idx - 1];
                float f1 = calData.freqsHz[idx];
                float t = (f1 > f0) ? ((f - f0) / (f1 - f0)) : 0.0f;
                corrDb = calData.inverseCorrectionDb[idx - 1] * (1.0f - t) + calData.inverseCorrectionDb[idx] * t;
            }
        }
        gainFactors[b] = std::pow(10.0f, std::clamp(corrDb, -6.0f, 6.0f) / 20.0f);
    }

    // Process audio buffer in blocks
    std::vector<std::complex<float>> inData(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> outData(fftSize, { 0.0f, 0.0f });

    size_t processed = 0;
    while (processed < audio.size())
    {
        size_t blockLen = std::min(fftSize / 2, audio.size() - processed);
        for (size_t i = 0; i < fftSize; ++i)
        {
            if (i < blockLen)
                inData[i] = { audio[processed + i], 0.0f };
            else
                inData[i] = { 0.0f, 0.0f };
        }

        fft.perform(inData.data(), outData.data(), false);

        for (size_t b = 0; b < numBins; ++b)
        {
            outData[b] *= gainFactors[b];
            if (b > 0 && b < numBins)
                outData[fftSize - b] *= gainFactors[b];
        }

        fft.perform(outData.data(), inData.data(), true);

        for (size_t i = 0; i < blockLen; ++i)
        {
            audio[processed + i] = inData[i].real();
        }

        processed += blockLen;
    }
}

} // namespace abdaudiolab::math
