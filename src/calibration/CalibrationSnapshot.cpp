/**
 * @file CalibrationSnapshot.cpp
 * @brief Implementation of CalibrationSnapshot serialization, parsing and hashing.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationSnapshot.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace abdaudiolab::calibration
{

namespace
{
    bool floatAlmostEqual(float a, float b, float eps = 1e-4f) noexcept
    {
        return std::abs(a - b) <= eps;
    }

    bool doubleAlmostEqual(double a, double b, double eps = 1e-5) noexcept
    {
        return std::abs(a - b) <= eps;
    }
}

bool CalibrationCompatibility::operator==(const CalibrationCompatibility& other) const noexcept
{
    return deviceStableId == other.deviceStableId &&
           driverType == other.driverType &&
           doubleAlmostEqual(sampleRateHz, other.sampleRateHz) &&
           bufferSamples == other.bufferSamples &&
           inputChannelIndex == other.inputChannelIndex &&
           outputChannelIndex == other.outputChannelIndex &&
           routingDescription == other.routingDescription;
}

bool CalibrationCaptureMetadata::operator==(const CalibrationCaptureMetadata& other) const noexcept
{
    return sweepDurationMs == other.sweepDurationMs &&
           latencyMarginMs == other.latencyMarginMs &&
           decayTailMs == other.decayTailMs &&
           requiredSamples == other.requiredSamples &&
           capturedSamples == other.capturedSamples &&
           physicalAdcVerified == other.physicalAdcVerified &&
           mockHardwareIsolated == other.mockHardwareIsolated &&
           pluginPathBypassed == other.pluginPathBypassed;
}

bool CalibrationResultMetrics::operator==(const CalibrationResultMetrics& other) const noexcept
{
    return calibrationStatus == other.calibrationStatus &&
           rtlSamples == other.rtlSamples &&
           doubleAlmostEqual(rtlMs, other.rtlMs) &&
           floatAlmostEqual(peakDbfs, other.peakDbfs) &&
           floatAlmostEqual(flatnessDeltaDb, other.flatnessDeltaDb) &&
           floatAlmostEqual(snrDb, other.snrDb) &&
           floatAlmostEqual(interfaceTrimDb, other.interfaceTrimDb) &&
           clippingSamples == other.clippingSamples &&
           polarity == other.polarity;
}

bool CalibrationProcessingPolicy::operator==(const CalibrationProcessingPolicy& other) const noexcept
{
    return policyVersion == other.policyVersion &&
           latencyCompensationEnabled == other.latencyCompensationEnabled &&
           inverseCompensationEnabled == other.inverseCompensationEnabled &&
           floatAlmostEqual(inverseCompensationMaxBoostDb, other.inverseCompensationMaxBoostDb);
}

bool CalibrationNoiseBaseline::operator==(const CalibrationNoiseBaseline& other) const noexcept
{
    if (status != other.status ||
        durationSamples != other.durationSamples ||
        !doubleAlmostEqual(durationMs, other.durationMs) ||
        !floatAlmostEqual(rmsDbfs, other.rmsDbfs) ||
        !floatAlmostEqual(peakDbfs, other.peakDbfs) ||
        hasSpectralBands != other.hasSpectralBands ||
        outputMuted != other.outputMuted ||
        inputWasClipped != other.inputWasClipped ||
        directMonitorState != other.directMonitorState)
    {
        return false;
    }
    if (hasSpectralBands)
    {
        for (size_t i = 0; i < 32; ++i)
        {
            if (!floatAlmostEqual(spectralBandDbfs[i], other.spectralBandDbfs[i]))
                return false;
        }
    }
    return true;
}

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

std::string CalibrationSnapshot::computeHash() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["displayName"] = displayName;
    j["profileId"] = profileId;
    j["createdAt"] = createdAt;

    nlohmann::json compat;
    compat["deviceStableId"] = compatibility.deviceStableId;
    compat["driverType"] = compatibility.driverType;
    compat["sampleRateHz"] = compatibility.sampleRateHz;
    compat["bufferSamples"] = compatibility.bufferSamples;
    compat["inputChannelIndex"] = compatibility.inputChannelIndex;
    compat["outputChannelIndex"] = compatibility.outputChannelIndex;
    compat["routingDescription"] = compatibility.routingDescription;
    j["compatibility"] = compat;

    nlohmann::json cap;
    cap["capturedSamples"] = capture.capturedSamples;
    cap["decayTailMs"] = capture.decayTailMs;
    cap["latencyMarginMs"] = capture.latencyMarginMs;
    cap["mockHardwareIsolated"] = capture.mockHardwareIsolated;
    cap["physicalAdcVerified"] = capture.physicalAdcVerified;
    cap["pluginPathBypassed"] = capture.pluginPathBypassed;
    cap["requiredSamples"] = capture.requiredSamples;
    cap["sweepDurationMs"] = capture.sweepDurationMs;
    j["capture"] = cap;

    nlohmann::json base;
    base["directMonitorState"] = noiseBaseline.directMonitorState;
    base["durationMs"] = noiseBaseline.durationMs;
    base["durationSamples"] = noiseBaseline.durationSamples;
    base["hasSpectralBands"] = noiseBaseline.hasSpectralBands;
    base["inputWasClipped"] = noiseBaseline.inputWasClipped;
    base["outputMuted"] = noiseBaseline.outputMuted;
    base["peakDbfs"] = noiseBaseline.peakDbfs;
    base["rmsDbfs"] = noiseBaseline.rmsDbfs;
    nlohmann::json bands = nlohmann::json::array();
    for (float b : noiseBaseline.spectralBandDbfs)
        bands.push_back(b);
    base["spectralBandsDbfs"] = bands;
    base["status"] = noiseBaselineStatusToString(noiseBaseline.status);
    j["noiseBaseline"] = base;

    nlohmann::json res;
    res["calibrationStatus"] = result.calibrationStatus;
    res["rtlSamples"] = result.rtlSamples;
    res["rtlMs"] = result.rtlMs;
    res["peakDbfs"] = result.peakDbfs;
    res["flatnessDeltaDb"] = result.flatnessDeltaDb;
    res["snrDb"] = result.snrDb;
    res["interfaceTrimDb"] = result.interfaceTrimDb;
    res["clippingSamples"] = result.clippingSamples;
    res["polarity"] = result.polarity;
    j["result"] = res;

    nlohmann::json policy;
    policy["policyVersion"] = processingPolicy.policyVersion;
    policy["latencyCompensationEnabled"] = processingPolicy.latencyCompensationEnabled;
    policy["inverseCompensationEnabled"] = processingPolicy.inverseCompensationEnabled;
    policy["inverseCompensationMaxBoostDb"] = processingPolicy.inverseCompensationMaxBoostDb;
    j["processingPolicy"] = policy;

    nlohmann::json integ;
    integ["hashAlgorithm"] = integrity.hashAlgorithm;
    // NOTE: integrity.snapshotHash is strictly excluded from computeHash() input!
    j["integrity"] = integ;

    std::string canonicalStr = j.dump();
    return computeSha256Hex(canonicalStr);
}

bool CalibrationSnapshot::verifyIntegrity() const
{
    if (integrity.hashAlgorithm != "SHA-256")
        return false;
    if (integrity.snapshotHash.length() != 64)
        return false;
    for (char c : integrity.snapshotHash)
    {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return computeHash() == integrity.snapshotHash;
}

nlohmann::json CalibrationSnapshot::toJson() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["displayName"] = displayName;
    j["profileId"] = profileId;
    j["createdAt"] = createdAt;

    nlohmann::json compat;
    compat["deviceStableId"] = compatibility.deviceStableId;
    compat["driverType"] = compatibility.driverType;
    compat["sampleRateHz"] = compatibility.sampleRateHz;
    compat["bufferSamples"] = compatibility.bufferSamples;
    compat["inputChannelIndex"] = compatibility.inputChannelIndex;
    compat["outputChannelIndex"] = compatibility.outputChannelIndex;
    compat["routingDescription"] = compatibility.routingDescription;
    j["compatibility"] = compat;

    nlohmann::json cap;
    cap["sweepDurationMs"] = capture.sweepDurationMs;
    cap["latencyMarginMs"] = capture.latencyMarginMs;
    cap["decayTailMs"] = capture.decayTailMs;
    cap["requiredSamples"] = capture.requiredSamples;
    cap["capturedSamples"] = capture.capturedSamples;
    cap["physicalAdcVerified"] = capture.physicalAdcVerified;
    cap["mockHardwareIsolated"] = capture.mockHardwareIsolated;
    cap["pluginPathBypassed"] = capture.pluginPathBypassed;
    j["capture"] = cap;

    nlohmann::json base;
    base["status"] = noiseBaselineStatusToString(noiseBaseline.status);
    base["durationSamples"] = noiseBaseline.durationSamples;
    base["durationMs"] = noiseBaseline.durationMs;
    base["rmsDbfs"] = noiseBaseline.rmsDbfs;
    base["peakDbfs"] = noiseBaseline.peakDbfs;
    base["hasSpectralBands"] = noiseBaseline.hasSpectralBands;
    base["outputMuted"] = noiseBaseline.outputMuted;
    base["inputWasClipped"] = noiseBaseline.inputWasClipped;
    base["directMonitorState"] = noiseBaseline.directMonitorState;
    nlohmann::json bands = nlohmann::json::array();
    for (float b : noiseBaseline.spectralBandDbfs)
        bands.push_back(b);
    base["spectralBandsDbfs"] = bands;
    j["noiseBaseline"] = base;

    nlohmann::json res;
    res["calibrationStatus"] = result.calibrationStatus;
    res["rtlSamples"] = result.rtlSamples;
    res["rtlMs"] = result.rtlMs;
    res["peakDbfs"] = result.peakDbfs;
    res["flatnessDeltaDb"] = result.flatnessDeltaDb;
    res["snrDb"] = result.snrDb;
    res["interfaceTrimDb"] = result.interfaceTrimDb;
    res["clippingSamples"] = result.clippingSamples;
    res["polarity"] = result.polarity;
    j["result"] = res;

    nlohmann::json policy;
    policy["policyVersion"] = processingPolicy.policyVersion;
    policy["latencyCompensationEnabled"] = processingPolicy.latencyCompensationEnabled;
    policy["inverseCompensationEnabled"] = processingPolicy.inverseCompensationEnabled;
    policy["inverseCompensationMaxBoostDb"] = processingPolicy.inverseCompensationMaxBoostDb;
    j["processingPolicy"] = policy;

    nlohmann::json integ;
    integ["hashAlgorithm"] = integrity.hashAlgorithm;
    integ["snapshotHash"] = integrity.snapshotHash;
    j["integrity"] = integ;

    return j;
}

CalibrationSnapshot CalibrationSnapshot::fromJson(const nlohmann::json& j)
{
    CalibrationSnapshot s;
    if (j.contains("schemaVersion")) s.schemaVersion = j["schemaVersion"].get<int>();
    if (j.contains("displayName")) s.displayName = j["displayName"].get<std::string>();
    if (j.contains("profileId")) s.profileId = j["profileId"].get<std::string>();
    if (j.contains("createdAt")) s.createdAt = j["createdAt"].get<std::string>();

    if (j.contains("compatibility") && j["compatibility"].is_object())
    {
        const auto& c = j["compatibility"];
        if (c.contains("deviceStableId")) s.compatibility.deviceStableId = c["deviceStableId"].get<std::string>();
        if (c.contains("driverType")) s.compatibility.driverType = c["driverType"].get<std::string>();
        if (c.contains("sampleRateHz")) s.compatibility.sampleRateHz = c["sampleRateHz"].get<double>();
        if (c.contains("bufferSamples")) s.compatibility.bufferSamples = c["bufferSamples"].get<int>();
        if (c.contains("inputChannelIndex")) s.compatibility.inputChannelIndex = c["inputChannelIndex"].get<int>();
        if (c.contains("outputChannelIndex")) s.compatibility.outputChannelIndex = c["outputChannelIndex"].get<int>();
        if (c.contains("routingDescription")) s.compatibility.routingDescription = c["routingDescription"].get<std::string>();
    }

    if (j.contains("capture") && j["capture"].is_object())
    {
        const auto& cp = j["capture"];
        if (cp.contains("sweepDurationMs")) s.capture.sweepDurationMs = cp["sweepDurationMs"].get<int>();
        if (cp.contains("latencyMarginMs")) s.capture.latencyMarginMs = cp["latencyMarginMs"].get<int>();
        if (cp.contains("decayTailMs")) s.capture.decayTailMs = cp["decayTailMs"].get<int>();
        if (cp.contains("requiredSamples")) s.capture.requiredSamples = cp["requiredSamples"].get<int>();
        if (cp.contains("capturedSamples")) s.capture.capturedSamples = cp["capturedSamples"].get<int>();
        if (cp.contains("physicalAdcVerified")) s.capture.physicalAdcVerified = cp["physicalAdcVerified"].get<bool>();
        if (cp.contains("mockHardwareIsolated")) s.capture.mockHardwareIsolated = cp["mockHardwareIsolated"].get<bool>();
        if (cp.contains("pluginPathBypassed")) s.capture.pluginPathBypassed = cp["pluginPathBypassed"].get<bool>();
    }

    if (j.contains("noiseBaseline") && j["noiseBaseline"].is_object())
    {
        const auto& nb = j["noiseBaseline"];
        if (nb.contains("status")) s.noiseBaseline.status = noiseBaselineStatusFromString(nb["status"].get<std::string>());
        if (nb.contains("durationSamples")) s.noiseBaseline.durationSamples = nb["durationSamples"].get<int>();
        if (nb.contains("durationMs")) s.noiseBaseline.durationMs = nb["durationMs"].get<double>();
        if (nb.contains("rmsDbfs")) s.noiseBaseline.rmsDbfs = nb["rmsDbfs"].get<float>();
        if (nb.contains("peakDbfs")) s.noiseBaseline.peakDbfs = nb["peakDbfs"].get<float>();
        if (nb.contains("hasSpectralBands")) s.noiseBaseline.hasSpectralBands = nb["hasSpectralBands"].get<bool>();
        if (nb.contains("outputMuted")) s.noiseBaseline.outputMuted = nb["outputMuted"].get<bool>();
        if (nb.contains("inputWasClipped")) s.noiseBaseline.inputWasClipped = nb["inputWasClipped"].get<bool>();
        if (nb.contains("directMonitorState")) s.noiseBaseline.directMonitorState = nb["directMonitorState"].get<std::string>();
        if (nb.contains("spectralBandsDbfs") && nb["spectralBandsDbfs"].is_array())
        {
            const auto& arr = nb["spectralBandsDbfs"];
            size_t n = std::min(arr.size(), s.noiseBaseline.spectralBandDbfs.size());
            for (size_t i = 0; i < n; ++i)
                s.noiseBaseline.spectralBandDbfs[i] = arr[i].get<float>();
        }
    }
    else
    {
        s.noiseBaseline.status = NoiseBaselineStatus::NotMeasured;
    }

    if (j.contains("result") && j["result"].is_object())
    {
        const auto& r = j["result"];
        if (r.contains("calibrationStatus")) s.result.calibrationStatus = r["calibrationStatus"].get<std::string>();
        if (r.contains("rtlSamples")) s.result.rtlSamples = r["rtlSamples"].get<int>();
        if (r.contains("rtlMs")) s.result.rtlMs = r["rtlMs"].get<double>();
        if (r.contains("peakDbfs")) s.result.peakDbfs = r["peakDbfs"].get<float>();
        if (r.contains("flatnessDeltaDb")) s.result.flatnessDeltaDb = r["flatnessDeltaDb"].get<float>();
        if (r.contains("snrDb")) s.result.snrDb = r["snrDb"].get<float>();
        if (r.contains("interfaceTrimDb")) s.result.interfaceTrimDb = r["interfaceTrimDb"].get<float>();
        if (r.contains("clippingSamples")) s.result.clippingSamples = r["clippingSamples"].get<int>();
        if (r.contains("polarity")) s.result.polarity = r["polarity"].get<std::string>();
    }

    if (j.contains("processingPolicy") && j["processingPolicy"].is_object())
    {
        const auto& p = j["processingPolicy"];
        if (p.contains("policyVersion")) s.processingPolicy.policyVersion = p["policyVersion"].get<int>();
        if (p.contains("latencyCompensationEnabled")) s.processingPolicy.latencyCompensationEnabled = p["latencyCompensationEnabled"].get<bool>();
        if (p.contains("inverseCompensationEnabled")) s.processingPolicy.inverseCompensationEnabled = p["inverseCompensationEnabled"].get<bool>();
        if (p.contains("inverseCompensationMaxBoostDb")) s.processingPolicy.inverseCompensationMaxBoostDb = p["inverseCompensationMaxBoostDb"].get<float>();
    }

    if (j.contains("integrity") && j["integrity"].is_object())
    {
        const auto& ig = j["integrity"];
        if (ig.contains("hashAlgorithm")) s.integrity.hashAlgorithm = ig["hashAlgorithm"].get<std::string>();
        if (ig.contains("snapshotHash")) s.integrity.snapshotHash = ig["snapshotHash"].get<std::string>();
    }

    return s;
}

std::optional<CalibrationSnapshot> CalibrationSnapshot::fromJsonSafe(const nlohmann::json& j, std::string* outError)
{
    try
    {
        auto s = fromJson(j);
        return s;
    }
    catch (const std::exception& e)
    {
        if (outError != nullptr)
            *outError = e.what();
        return std::nullopt;
    }
    catch (...)
    {
        if (outError != nullptr)
            *outError = "Unknown exception parsing CalibrationSnapshot JSON";
        return std::nullopt;
    }
}

std::string CalibrationSnapshot::generateDefaultProfileId(const std::string& deviceId, const std::string& timestampIso)
{
    juce::String cleanDevice = juce::String(deviceId).toLowerCase();
    juce::String sanitized;
    for (int i = 0; i < cleanDevice.length(); ++i)
    {
        auto c = cleanDevice[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            sanitized += c;
        else if (c == ' ' || c == '_' || c == '-' || c == '.')
            sanitized += '-';
    }
    while (sanitized.contains("--"))
        sanitized = sanitized.replace("--", "-");
    sanitized = sanitized.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    if (sanitized.isEmpty())
        sanitized = "generic-audio-device";

    juce::String cleanTs = juce::String(timestampIso);
    juce::String digitsOnly;
    for (int i = 0; i < cleanTs.length(); ++i)
    {
        auto c = cleanTs[i];
        if (c >= '0' && c <= '9')
            digitsOnly += c;
    }

    if (digitsOnly.isEmpty())
        digitsOnly = juce::String(juce::Time::currentTimeMillis());

    return (sanitized + "-loopback-" + digitsOnly).toStdString();
}

std::string CalibrationSnapshot::getCurrentUtcIsoTimestamp()
{
    auto now = juce::Time::getCurrentTime();
    return now.toISO8601(true).toStdString();
}

CalibrationSnapshot CalibrationSnapshot::create(
    const std::string& displayName,
    const std::string& profileId,
    const CalibrationCompatibility& compat,
    const CalibrationCaptureMetadata& capture,
    const math::LoopbackCalibrationData& resultData,
    int clippingSamples,
    const CalibrationProcessingPolicy& policy,
    const CalibrationNoiseBaseline& baseline)
{
    auto draft = CalibrationDraft::fromMeasurement(
        displayName, profileId, compat, capture, resultData, clippingSamples, policy, baseline);
    return draft.sealSnapshot();
}

CalibrationSnapshot CalibrationDraft::sealSnapshot() const
{
    CalibrationSnapshot s;
    s.schemaVersion = 1;
    s.displayName = displayName;
    s.profileId = profileId;
    s.createdAt = createdAt.empty() ? CalibrationSnapshot::getCurrentUtcIsoTimestamp() : createdAt;

    s.compatibility = compatibility;
    s.capture = capture;
    s.noiseBaseline = noiseBaseline;
    s.result = result;
    s.processingPolicy = processingPolicy;

    s.integrity.hashAlgorithm = "SHA-256";
    s.integrity.snapshotHash = s.computeHash();
    return s;
}

CalibrationDraft CalibrationDraft::fromMeasurement(
    const std::string& suggestedDisplayName,
    const std::string& profileId,
    const CalibrationCompatibility& compat,
    const CalibrationCaptureMetadata& capture,
    const math::LoopbackCalibrationData& resultData,
    int clippingSamples,
    const CalibrationProcessingPolicy& policy,
    const CalibrationNoiseBaseline& baseline)
{
    CalibrationDraft d;
    d.displayName = suggestedDisplayName;
    d.profileId = profileId;
    d.createdAt = CalibrationSnapshot::getCurrentUtcIsoTimestamp();

    d.compatibility = compat;
    d.capture = capture;
    d.noiseBaseline = baseline;

    d.result.calibrationStatus = (resultData.isCalibrated && !resultData.clippingDetected && clippingSamples == 0)
                                     ? "Valid"
                                     : (clippingSamples > 0 || resultData.clippingDetected ? "Clipped" : "Invalid");
    d.result.rtlSamples = resultData.latencySamples;
    d.result.rtlMs = resultData.roundTripLatencyMs;
    d.result.peakDbfs = resultData.peakInDbfs;
    d.result.flatnessDeltaDb = resultData.frequencyFlatnessDb;
    d.result.snrDb = resultData.snrDb;

    if (resultData.recommendedTrimGain > 1e-4f)
        d.result.interfaceTrimDb = 20.0f * std::log10(resultData.recommendedTrimGain);
    else
        d.result.interfaceTrimDb = 0.0f;

    d.result.clippingSamples = clippingSamples;
    d.result.polarity = "Normal";

    d.processingPolicy = policy;
    return d;
}

CalibrationDraft CalibrationDraft::fromSnapshot(const CalibrationSnapshot& snapshot)
{
    CalibrationDraft d;
    d.displayName = snapshot.displayName;
    d.profileId = snapshot.profileId;
    d.createdAt = snapshot.createdAt;
    d.compatibility = snapshot.compatibility;
    d.capture = snapshot.capture;
    d.noiseBaseline = snapshot.noiseBaseline;
    d.result = snapshot.result;
    d.processingPolicy = snapshot.processingPolicy;
    return d;
}

} // namespace abdaudiolab::calibration
