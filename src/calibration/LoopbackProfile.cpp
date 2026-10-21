/**
 * @file LoopbackProfile.cpp
 * @brief Implementation of LoopbackProfile serialization, hashing and conversion.
 * @author ABDSynths
 * @date 2026
 */

#include "LoopbackProfile.h"
#include "../synth/Sha256.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <juce_core/juce_core.h>

namespace abdaudiolab::calibration
{

nlohmann::json LoopbackProfile::toJson() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["profileId"]     = profileId;
    j["createdAt"]     = createdAt;

    nlohmann::json dev;
    dev["deviceName"]        = deviceName;
    dev["driverType"]        = driverType;
    dev["sampleRate"]        = sampleRate;
    dev["bufferSizeSamples"] = bufferSizeSamples;
    j["device"]              = dev;

    nlohmann::json routing;
    routing["inputChannelIndex"]  = inputChannelIndex;
    routing["inputChannelLabel"]  = inputChannelLabel;
    routing["outputChannelIndex"] = outputChannelIndex;
    routing["outputChannelLabel"] = outputChannelLabel;
    j["routing"]                  = routing;

    nlohmann::json metrics;
    metrics["isCalibrated"]           = isCalibrated;
    metrics["peakInDbfs"]             = peakInDbfs;
    metrics["recommendedTrimGain"]    = recommendedTrimGain;
    metrics["targetHeadroomDbfs"]     = targetHeadroomDbfs;
    metrics["roundTripLatencyMs"]     = roundTripLatencyMs;
    metrics["latencySamples"]         = latencySamples;
    metrics["thdPlusNoisePercent"]    = thdPlusNoisePercent;
    metrics["snrDb"]                  = snrDb;
    metrics["frequencyFlatnessDb"]    = frequencyFlatnessDb;
    metrics["phaseInversionDetected"] = phaseInversionDetected;
    metrics["clippingDetected"]       = clippingDetected;
    j["metrics"]                      = metrics;

    j["frequenciesHz"]       = freqsHz;
    j["magnitudeDb"]         = magnitudeDb;
    j["inverseCorrectionDb"] = inverseCorrectionDb;

    j["sha256Hash"] = sha256Hash;
    return j;
}

std::optional<LoopbackProfile> LoopbackProfile::fromJson(const nlohmann::json& j)
{
    try
    {
        if (!j.is_object())
            return std::nullopt;

        LoopbackProfile p;
        p.schemaVersion = j.value("schemaVersion", 1);
        p.profileId     = j.value("profileId", "");
        p.createdAt     = j.value("createdAt", "");

        if (j.contains("device") && j["device"].is_object())
        {
            const auto& dev     = j["device"];
            p.deviceName        = dev.value("deviceName", "");
            p.driverType        = dev.value("driverType", "Unknown");
            p.sampleRate        = dev.value("sampleRate", 48000.0);
            p.bufferSizeSamples = dev.value("bufferSizeSamples", 256);
        }

        if (j.contains("routing") && j["routing"].is_object())
        {
            const auto& r        = j["routing"];
            p.inputChannelIndex  = r.value("inputChannelIndex", 0);
            p.inputChannelLabel  = r.value("inputChannelLabel", "Input 1");
            p.outputChannelIndex = r.value("outputChannelIndex", 0);
            p.outputChannelLabel = r.value("outputChannelLabel", "Output 1");
        }

        if (j.contains("metrics") && j["metrics"].is_object())
        {
            const auto& m            = j["metrics"];
            p.isCalibrated           = m.value("isCalibrated", false);
            p.peakInDbfs             = m.value("peakInDbfs", -100.0f);
            p.recommendedTrimGain    = m.value("recommendedTrimGain", 1.0f);
            p.targetHeadroomDbfs     = m.value("targetHeadroomDbfs", -3.0f);
            p.roundTripLatencyMs     = m.value("roundTripLatencyMs", 0.0f);
            p.latencySamples         = m.value("latencySamples", 0);
            p.thdPlusNoisePercent    = m.value("thdPlusNoisePercent", 0.001f);
            p.snrDb                  = m.value("snrDb", 90.0f);
            p.frequencyFlatnessDb    = m.value("frequencyFlatnessDb", 0.1f);
            p.phaseInversionDetected = m.value("phaseInversionDetected", false);
            p.clippingDetected       = m.value("clippingDetected", false);
        }

        if (j.contains("frequenciesHz") && j["frequenciesHz"].is_array())
            p.freqsHz = j["frequenciesHz"].get<std::vector<float>>();

        if (j.contains("magnitudeDb") && j["magnitudeDb"].is_array())
            p.magnitudeDb = j["magnitudeDb"].get<std::vector<float>>();

        if (j.contains("inverseCorrectionDb") && j["inverseCorrectionDb"].is_array())
            p.inverseCorrectionDb = j["inverseCorrectionDb"].get<std::vector<float>>();

        p.sha256Hash = j.value("sha256Hash", "");
        return p;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<LoopbackProfile> LoopbackProfile::fromCalibrationData(
    const math::LoopbackCalibrationData& data,
    const std::string& devName,
    const std::string& driver,
    double sRate,
    int bufSize,
    int inCh,
    const std::string& inLabel,
    int outCh,
    const std::string& outLabel)
{
    if (!data.isCalibrated || data.clippingDetected)
        return std::nullopt;

    LoopbackProfile p;
    p.schemaVersion      = kCurrentSchemaVersion;
    p.deviceName         = devName;
    p.driverType         = driver;
    p.sampleRate         = (sRate > 0.0) ? sRate : data.sampleRate;
    p.bufferSizeSamples  = bufSize;
    p.inputChannelIndex  = inCh;
    p.inputChannelLabel  = inLabel;
    p.outputChannelIndex = outCh;
    p.outputChannelLabel = outLabel;

    p.profileId = "loopback-" + p.getHardwareKey();
    p.createdAt = juce::Time::getCurrentTime().toISO8601(true).toStdString();

    p.isCalibrated           = data.isCalibrated;
    p.peakInDbfs             = data.peakInDbfs;
    p.recommendedTrimGain    = data.recommendedTrimGain;
    p.targetHeadroomDbfs     = data.targetHeadroomDbfs;
    p.roundTripLatencyMs     = data.roundTripLatencyMs;
    p.latencySamples         = data.latencySamples;
    p.thdPlusNoisePercent    = data.thdPlusNoisePercent;
    p.snrDb                  = data.snrDb;
    p.frequencyFlatnessDb    = data.frequencyFlatnessDb;
    p.phaseInversionDetected = data.phaseInversionDetected;
    p.clippingDetected       = data.clippingDetected;

    p.freqsHz             = data.freqsHz;
    p.magnitudeDb         = data.magnitudeDb;
    p.inverseCorrectionDb = data.inverseCorrectionDb;

    p.sha256Hash = p.computeSha256();
    return p;
}

math::LoopbackCalibrationData LoopbackProfile::toCalibrationData() const
{
    math::LoopbackCalibrationData d;
    d.isCalibrated           = isCalibrated;
    d.sampleRate             = sampleRate;
    d.peakInDbfs             = peakInDbfs;
    d.recommendedTrimGain    = recommendedTrimGain;
    d.targetHeadroomDbfs     = targetHeadroomDbfs;
    d.roundTripLatencyMs     = roundTripLatencyMs;
    d.latencySamples         = latencySamples;
    d.thdPlusNoisePercent    = thdPlusNoisePercent;
    d.snrDb                  = snrDb;
    d.frequencyFlatnessDb    = frequencyFlatnessDb;
    d.phaseInversionDetected = phaseInversionDetected;
    d.clippingDetected       = clippingDetected;
    d.freqsHz                = freqsHz;
    d.magnitudeDb            = magnitudeDb;
    d.inverseCorrectionDb    = inverseCorrectionDb;
    d.deviceName             = juce::String(deviceName);
    d.timestamp              = juce::String(createdAt);
    return d;
}

std::string LoopbackProfile::computeSha256() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["profileId"]     = profileId;
    j["createdAt"]     = createdAt;

    nlohmann::json dev;
    dev["deviceName"]        = deviceName;
    dev["driverType"]        = driverType;
    dev["sampleRate"]        = sampleRate;
    dev["bufferSizeSamples"] = bufferSizeSamples;
    j["device"]              = dev;

    nlohmann::json metrics;
    metrics["isCalibrated"]        = isCalibrated;
    metrics["peakInDbfs"]          = peakInDbfs;
    metrics["recommendedTrimGain"] = recommendedTrimGain;
    metrics["roundTripLatencyMs"]  = roundTripLatencyMs;
    metrics["latencySamples"]      = latencySamples;
    metrics["snrDb"]               = snrDb;
    metrics["frequencyFlatnessDb"] = frequencyFlatnessDb;
    j["metrics"]                   = metrics;

    j["frequenciesHz"]       = freqsHz;
    j["magnitudeDb"]         = magnitudeDb;
    j["inverseCorrectionDb"] = inverseCorrectionDb;

    std::string canonical = j.dump();
    return synth::Sha256::computeHex(canonical);
}

bool LoopbackProfile::verifyHash() const
{
    if (sha256Hash.empty())
        return false;
    return computeSha256() == sha256Hash;
}

} // namespace abdaudiolab::calibration
