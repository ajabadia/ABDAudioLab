#include "LoopbackCalibrator.h"
#include "FarinaDeconvolver.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
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
    for (float s : recordedResponse)
    {
        float a = std::abs(s);
        if (a > maxVal) maxVal = a;
        sumSq += static_cast<double>(s) * s;
    }

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

    // 3. Extract Latency (Peak of Linear IR)
    if (!decon.linearIR.empty())
    {
        auto maxIt = std::max_element(decon.linearIR.begin(), decon.linearIR.end(),
                                     [](float a, float b) { return std::abs(a) < std::abs(b); });
        result.latencySamples = static_cast<int>(std::distance(decon.linearIR.begin(), maxIt));
        result.roundTripLatencyMs = static_cast<float>((result.latencySamples / sampleRate) * 1000.0);
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

    for (size_t i = 0; i < result.magnitudeDb.size(); ++i)
    {
        float normalizedDb = result.magnitudeDb[i] - ref1kHzDb;
        result.magnitudeDb[i] = normalizedDb;
        result.inverseCorrectionDb[i] = -normalizedDb; // Exact inverse correction

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

} // namespace abdaudiolab::math
