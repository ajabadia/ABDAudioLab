/**
 * @file LoopbackProfile.cpp
 * @brief Implementation of LoopbackProfile serialization, hashing and conversion.
 * @author ABDSynths
 * @date 2026
 */

#include "LoopbackProfile.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace abdaudiolab::calibration
{

namespace
{
    // Standard FIPS 180-4 SHA-256 implementation
    inline uint32_t rotr32(uint32_t x, uint32_t n) noexcept { return (x >> n) | (x << (32 - n)); }
    inline uint32_t choose32(uint32_t e, uint32_t f, uint32_t g) noexcept { return (e & f) ^ (~e & g); }
    inline uint32_t majority32(uint32_t a, uint32_t b, uint32_t c) noexcept { return (a & b) ^ (a & c) ^ (b & c); }
    inline uint32_t sig0(uint32_t x) noexcept { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
    inline uint32_t sig1(uint32_t x) noexcept { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
    inline uint32_t gam0(uint32_t x) noexcept { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
    inline uint32_t gam1(uint32_t x) noexcept { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

    static const uint32_t K256[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    std::string computeSha256Hex(const std::string& input)
    {
        uint32_t H[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        uint64_t bitLen = static_cast<uint64_t>(input.size()) * 8ULL;
        std::vector<uint8_t> msg(input.begin(), input.end());
        msg.push_back(0x80);
        while ((msg.size() % 64) != 56)
        {
            msg.push_back(0x00);
        }
        for (int i = 7; i >= 0; --i)
        {
            msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
        }

        for (size_t chunk = 0; chunk < msg.size(); chunk += 64)
        {
            uint32_t W[64];
            for (int t = 0; t < 16; ++t)
            {
                size_t idx = chunk + static_cast<size_t>(t * 4);
                W[t] = (static_cast<uint32_t>(msg[idx]) << 24) |
                       (static_cast<uint32_t>(msg[idx + 1]) << 16) |
                       (static_cast<uint32_t>(msg[idx + 2]) << 8) |
                       (static_cast<uint32_t>(msg[idx + 3]));
            }
            for (int t = 16; t < 64; ++t)
            {
                W[t] = gam1(W[t - 2]) + W[t - 7] + gam0(W[t - 15]) + W[t - 16];
            }

            uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
            uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

            for (int t = 0; t < 64; ++t)
            {
                uint32_t T1 = h + sig1(e) + choose32(e, f, g) + K256[t] + W[t];
                uint32_t T2 = sig0(a) + majority32(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            H[0] += a;
            H[1] += b;
            H[2] += c;
            H[3] += d;
            H[4] += e;
            H[5] += f;
            H[6] += g;
            H[7] += h;
        }

        char hexBuf[65];
        std::snprintf(hexBuf, sizeof(hexBuf),
                      "%08x%08x%08x%08x%08x%08x%08x%08x",
                      H[0], H[1], H[2], H[3], H[4], H[5], H[6], H[7]);
        return std::string(hexBuf);
    }
}

nlohmann::json LoopbackProfile::toJson() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["profileId"] = profileId;
    j["createdAt"] = createdAt;

    nlohmann::json dev;
    dev["deviceName"] = deviceName;
    dev["driverType"] = driverType;
    dev["sampleRate"] = sampleRate;
    dev["bufferSizeSamples"] = bufferSizeSamples;
    j["device"] = dev;

    nlohmann::json routing;
    routing["inputChannelIndex"] = inputChannelIndex;
    routing["inputChannelLabel"] = inputChannelLabel;
    routing["outputChannelIndex"] = outputChannelIndex;
    routing["outputChannelLabel"] = outputChannelLabel;
    j["routing"] = routing;

    nlohmann::json metrics;
    metrics["isCalibrated"] = isCalibrated;
    metrics["peakInDbfs"] = peakInDbfs;
    metrics["recommendedTrimGain"] = recommendedTrimGain;
    metrics["targetHeadroomDbfs"] = targetHeadroomDbfs;
    metrics["roundTripLatencyMs"] = roundTripLatencyMs;
    metrics["latencySamples"] = latencySamples;
    metrics["thdPlusNoisePercent"] = thdPlusNoisePercent;
    metrics["snrDb"] = snrDb;
    metrics["frequencyFlatnessDb"] = frequencyFlatnessDb;
    metrics["phaseInversionDetected"] = phaseInversionDetected;
    metrics["clippingDetected"] = clippingDetected;
    j["metrics"] = metrics;

    j["frequenciesHz"] = freqsHz;
    j["magnitudeDb"] = magnitudeDb;
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
        p.profileId = j.value("profileId", "");
        p.createdAt = j.value("createdAt", "");

        if (j.contains("device") && j["device"].is_object())
        {
            const auto& dev = j["device"];
            p.deviceName = dev.value("deviceName", "");
            p.driverType = dev.value("driverType", "Unknown");
            p.sampleRate = dev.value("sampleRate", 48000.0);
            p.bufferSizeSamples = dev.value("bufferSizeSamples", 256);
        }

        if (j.contains("routing") && j["routing"].is_object())
        {
            const auto& r = j["routing"];
            p.inputChannelIndex = r.value("inputChannelIndex", 0);
            p.inputChannelLabel = r.value("inputChannelLabel", "Input 1");
            p.outputChannelIndex = r.value("outputChannelIndex", 0);
            p.outputChannelLabel = r.value("outputChannelLabel", "Output 1");
        }

        if (j.contains("metrics") && j["metrics"].is_object())
        {
            const auto& m = j["metrics"];
            p.isCalibrated = m.value("isCalibrated", false);
            p.peakInDbfs = m.value("peakInDbfs", -100.0f);
            p.recommendedTrimGain = m.value("recommendedTrimGain", 1.0f);
            p.targetHeadroomDbfs = m.value("targetHeadroomDbfs", -3.0f);
            p.roundTripLatencyMs = m.value("roundTripLatencyMs", 0.0f);
            p.latencySamples = m.value("latencySamples", 0);
            p.thdPlusNoisePercent = m.value("thdPlusNoisePercent", 0.001f);
            p.snrDb = m.value("snrDb", 90.0f);
            p.frequencyFlatnessDb = m.value("frequencyFlatnessDb", 0.1f);
            p.phaseInversionDetected = m.value("phaseInversionDetected", false);
            p.clippingDetected = m.value("clippingDetected", false);
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
    p.schemaVersion = kCurrentSchemaVersion;
    p.deviceName = devName;
    p.driverType = driver;
    p.sampleRate = (sRate > 0.0) ? sRate : data.sampleRate;
    p.bufferSizeSamples = bufSize;
    p.inputChannelIndex = inCh;
    p.inputChannelLabel = inLabel;
    p.outputChannelIndex = outCh;
    p.outputChannelLabel = outLabel;

    p.profileId = "loopback-" + p.getHardwareKey();
    p.createdAt = juce::Time::getCurrentTime().toISO8601(true).toStdString();

    p.isCalibrated = data.isCalibrated;
    p.peakInDbfs = data.peakInDbfs;
    p.recommendedTrimGain = data.recommendedTrimGain;
    p.targetHeadroomDbfs = data.targetHeadroomDbfs;
    p.roundTripLatencyMs = data.roundTripLatencyMs;
    p.latencySamples = data.latencySamples;
    p.thdPlusNoisePercent = data.thdPlusNoisePercent;
    p.snrDb = data.snrDb;
    p.frequencyFlatnessDb = data.frequencyFlatnessDb;
    p.phaseInversionDetected = data.phaseInversionDetected;
    p.clippingDetected = data.clippingDetected;

    p.freqsHz = data.freqsHz;
    p.magnitudeDb = data.magnitudeDb;
    p.inverseCorrectionDb = data.inverseCorrectionDb;

    p.sha256Hash = p.computeSha256();
    return p;
}

math::LoopbackCalibrationData LoopbackProfile::toCalibrationData() const
{
    math::LoopbackCalibrationData d;
    d.isCalibrated = isCalibrated;
    d.sampleRate = sampleRate;
    d.peakInDbfs = peakInDbfs;
    d.recommendedTrimGain = recommendedTrimGain;
    d.targetHeadroomDbfs = targetHeadroomDbfs;
    d.roundTripLatencyMs = roundTripLatencyMs;
    d.latencySamples = latencySamples;
    d.thdPlusNoisePercent = thdPlusNoisePercent;
    d.snrDb = snrDb;
    d.frequencyFlatnessDb = frequencyFlatnessDb;
    d.phaseInversionDetected = phaseInversionDetected;
    d.clippingDetected = clippingDetected;
    d.freqsHz = freqsHz;
    d.magnitudeDb = magnitudeDb;
    d.inverseCorrectionDb = inverseCorrectionDb;
    d.deviceName = juce::String(deviceName);
    d.timestamp = juce::String(createdAt);
    return d;
}

std::string LoopbackProfile::computeSha256() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["profileId"] = profileId;
    j["createdAt"] = createdAt;

    nlohmann::json dev;
    dev["deviceName"] = deviceName;
    dev["driverType"] = driverType;
    dev["sampleRate"] = sampleRate;
    dev["bufferSizeSamples"] = bufferSizeSamples;
    j["device"] = dev;

    nlohmann::json metrics;
    metrics["isCalibrated"] = isCalibrated;
    metrics["peakInDbfs"] = peakInDbfs;
    metrics["recommendedTrimGain"] = recommendedTrimGain;
    metrics["roundTripLatencyMs"] = roundTripLatencyMs;
    metrics["latencySamples"] = latencySamples;
    metrics["snrDb"] = snrDb;
    metrics["frequencyFlatnessDb"] = frequencyFlatnessDb;
    j["metrics"] = metrics;

    j["frequenciesHz"] = freqsHz;
    j["magnitudeDb"] = magnitudeDb;
    j["inverseCorrectionDb"] = inverseCorrectionDb;

    std::string canonical = j.dump();
    return computeSha256Hex(canonical);
}

bool LoopbackProfile::verifyHash() const
{
    if (sha256Hash.empty())
        return false;
    return computeSha256() == sha256Hash;
}

} // namespace abdaudiolab::calibration
