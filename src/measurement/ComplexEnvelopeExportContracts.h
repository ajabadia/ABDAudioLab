/**
 * @file ComplexEnvelopeExportContracts.h
 * @brief Canonical contracts for FAIR/LNL container export of complex envelope measurements.
 * @author ABDSynths
 * @date 2026
 *
 * Implements strict metrological honesty, RFC 8785 canonical JSON serialization,
 * non-circular manifest hashing, and deterministic timestamp policies.
 */

#pragma once

#include "ComplexEnvelopeContracts.h"
#include "ComplexEnvelopeOrchestratorContracts.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Timestamp generation policy ensuring deterministic canonical hashing for testability.
 */
enum class TimestampPolicy
{
    FixedForTest,              /**< Uses deterministic fixed timestamp "2026-01-01T00:00:00Z" */
    SuppliedByCaller,          /**< Uses explicit caller-supplied timestamp */
    OmitFromCanonicalPayload   /**< Omits timestamp from canonical SHA-256 payload */
};

[[nodiscard]] inline std::string timestampPolicyToString(TimestampPolicy policy) noexcept
{
    switch (policy)
    {
        case TimestampPolicy::FixedForTest:            return "FixedForTest";
        case TimestampPolicy::SuppliedByCaller:        return "SuppliedByCaller";
        case TimestampPolicy::OmitFromCanonicalPayload: return "OmitFromCanonicalPayload";
    }
    return "FixedForTest";
}

/**
 * @brief Specification guiding the FAIR/LNL container export process.
 */
struct ComplexEnvelopeExportSpec
{
    std::string experimentId { "exp_complex_env_001" };
    std::string author { "ABDAudioLab Metrology Engine" };
    std::string creationClock { "2026-01-01T00:00:00Z" };
    std::string notes;
    TimestampPolicy timestampPolicy { TimestampPolicy::FixedForTest };
    std::string fixedTimestamp { "2026-01-01T00:00:00Z" };

    bool includeRawAudio { true };
    bool includeCompensatedAudio { true };
    bool generateSvgOverlay { true };
    bool generateInteractiveHtml { true };
    double audioSampleRateHz { 48000.0 };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["audioSampleRateHz"] = audioSampleRateHz;
        j["author"] = author;
        if (timestampPolicy != TimestampPolicy::OmitFromCanonicalPayload)
        {
            j["creationClock"] = (timestampPolicy == TimestampPolicy::FixedForTest) ? fixedTimestamp : creationClock;
        }
        else
        {
            j["creationClock"] = nullptr;
        }
        j["experimentId"] = experimentId;
        j["generateInteractiveHtml"] = generateInteractiveHtml;
        j["generateSvgOverlay"] = generateSvgOverlay;
        j["includeCompensatedAudio"] = includeCompensatedAudio;
        j["includeRawAudio"] = includeRawAudio;
        j["notes"] = notes;
        j["timestampPolicy"] = timestampPolicyToString(timestampPolicy);
        return j;
    }
};

/**
 * @brief Metadata descriptor for an individual artifact stored in the FAIR container.
 */
struct ComplexEnvelopeExportArtifact
{
    std::string relativePath;      /**< Relative path inside container (e.g. "data/envelope_record.json") */
    std::string role;              /**< FAIR role (e.g. "envelope_record", "vector_svg_overlay") */
    std::string mediaType;         /**< MIME type (e.g. "application/json", "image/svg+xml", "audio/wav") */
    uint64_t byteLength { 0 };     /**< Exact file size in bytes */
    std::string sha256;            /**< Hexadecimal SHA-256 hash of file contents */
    std::string schemaVersion { "1.0.0" };
    std::string canonicalization { "none" }; /**< "rfc8785", "pcm_wav", "none" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["byteLength"] = byteLength;
        j["canonicalization"] = canonicalization;
        j["mediaType"] = mediaType;
        j["relativePath"] = relativePath;
        j["role"] = role;
        j["schemaVersion"] = schemaVersion;
        j["sha256"] = sha256;
        return j;
    }
};

/**
 * @brief Explicit multi-dimensional licensing policies distinguishing code, data, audio and third-party notices.
 */
struct ContainerLicensePolicy
{
    std::string dataLicense { "CC-BY-4.0" };
    std::string softwareLicense { "Proprietary/Internal" };
    std::string audioLicense { "not_specified" };
    std::string thirdPartyNotice { "Casio and CZ-101 are trademarks of Casio Computer Co., Ltd. No official affiliation claimed." };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["audioLicense"] = audioLicense;
        j["dataLicense"] = dataLicense;
        j["softwareLicense"] = softwareLicense;
        j["thirdPartyNotice"] = thirdPartyNotice;
        return j;
    }
};

/**
 * @brief Normalized non-volatile execution environment metadata guaranteeing audit stability.
 */
struct ReproducibleEnvironmentDescriptor
{
    std::string osFamily { "windows" };
    std::string architecture { "x86_64" };
    std::string compilerId { "MSVC" };
    std::string buildConfiguration { "Release" };
    std::string frameworkVersion { "JUCE_8" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["architecture"] = architecture;
        j["buildConfiguration"] = buildConfiguration;
        j["compilerId"] = compilerId;
        j["frameworkVersion"] = frameworkVersion;
        j["osFamily"] = osFamily;
        return j;
    }
};

/**
 * @brief Top-level canonical manifest describing the complete FAIR/LNL container.
 * Excludes itself from its artifacts list to guarantee strictly non-circular auditing.
 */
struct ComplexEnvelopeContainerManifest
{
    std::string schemaVersion { "1.0.0" };
    std::string containerId { "lnl_envelope_001" };
    std::string experimentId { "exp_complex_env_001" };
    std::string exportTimestampUtc { "2026-01-01T00:00:00Z" };
    TimestampPolicy timestampPolicy { TimestampPolicy::FixedForTest };

    ContainerLicensePolicy license;
    ReproducibleEnvironmentDescriptor environment;
    std::string methodology { "FAIR-LNL-Complex-Envelope" };
    std::string limitations { "Acoustic spectral centroid and rolloff are perceptual proxies in normalized space [0, 1], not internal hardware state reconstruction. phaseDistortionProxy: not_claimed" };

    nlohmann::ordered_json dutIdentity;
    nlohmann::ordered_json timingResolution;
    nlohmann::ordered_json calibration;
    nlohmann::ordered_json measurementConditions;
    std::vector<ComplexEnvelopeExportArtifact> artifacts;

    /**
     * @brief Computes the canonical JSON representation of the manifest.
     * Note: Manifest excludes self-referential circular hashes.
     */
    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;

        // Artifacts sorted alphabetically by relativePath for canonical determinism
        auto sortedArtifacts = artifacts;
        std::sort(sortedArtifacts.begin(), sortedArtifacts.end(),
                  [](const ComplexEnvelopeExportArtifact& a, const ComplexEnvelopeExportArtifact& b) {
                      return a.relativePath < b.relativePath;
                  });

        nlohmann::ordered_json artArray = nlohmann::ordered_json::array();
        for (const auto& art : sortedArtifacts)
        {
            artArray.push_back(art.toCanonicalJson());
        }

        j["artifacts"] = artArray;
        j["calibration"] = calibration;
        j["containerId"] = containerId;
        j["dutIdentity"] = dutIdentity;
        j["environment"] = environment.toCanonicalJson();
        j["experimentId"] = experimentId;
        if (timestampPolicy != TimestampPolicy::OmitFromCanonicalPayload)
        {
            j["exportTimestampUtc"] = exportTimestampUtc;
        }
        else
        {
            j["exportTimestampUtc"] = nullptr;
        }
        j["license"] = license.toCanonicalJson();
        j["limitations"] = limitations;
        j["measurementConditions"] = measurementConditions;
        j["methodology"] = methodology;
        j["schemaVersion"] = schemaVersion;
        j["timingResolution"] = timingResolution;
        return j;
    }
};

} // namespace abdaudiolab::measurement
