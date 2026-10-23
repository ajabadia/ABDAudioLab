/**
 * @file CalibrationSnapshot.h
 * @brief Immutable, serializable and portable calibration snapshot contract.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "../math/LoopbackCalibrator.h"

namespace abdaudiolab::calibration
{

/**
 * @struct CalibrationCompatibility
 * @brief Hardware and stream routing parameters defining calibration validity scope.
 */
struct CalibrationCompatibility
{
    std::string deviceStableId;
    std::string driverType { "Unknown" };
    double sampleRateHz { 0.0 };
    int bufferSamples { 0 };
    int inputChannelIndex { 0 };
    int outputChannelIndex { 0 };
    std::string routingDescription;

    [[nodiscard]] bool operator==(const CalibrationCompatibility& other) const noexcept;
    [[nodiscard]] bool operator!=(const CalibrationCompatibility& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @struct CalibrationCaptureMetadata
 * @brief Physical capture parameters and sample counts during loopback calibration.
 */
struct CalibrationCaptureMetadata
{
    int sweepDurationMs { 1000 };
    int latencyMarginMs { 200 };
    int decayTailMs { 100 };
    int requiredSamples { 0 };
    int capturedSamples { 0 };
    bool physicalAdcVerified { true };
    bool mockHardwareIsolated { true };
    bool pluginPathBypassed { true };

    [[nodiscard]] bool operator==(const CalibrationCaptureMetadata& other) const noexcept;
    [[nodiscard]] bool operator!=(const CalibrationCaptureMetadata& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @struct CalibrationResultMetrics
 * @brief Technical metrics measured from the loopback capture.
 */
struct CalibrationResultMetrics
{
    std::string calibrationStatus { "Uncalibrated" }; // "Valid", "Clipped", "Invalid", "Uncalibrated"
    int rtlSamples { 0 };
    double rtlMs { 0.0 };
    float peakDbfs { -100.0f };
    float flatnessDeltaDb { 0.0f };
    float snrDb { 0.0f };
    float interfaceTrimDb { 0.0f };
    int clippingSamples { 0 };
    std::string polarity { "Normal" }; // "Normal", "Inverted"

    [[nodiscard]] bool isValid() const noexcept
    {
        return calibrationStatus == "Valid" && rtlSamples >= 0 && clippingSamples == 0;
    }

    [[nodiscard]] bool operator==(const CalibrationResultMetrics& other) const noexcept;
    [[nodiscard]] bool operator!=(const CalibrationResultMetrics& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @struct CalibrationProcessingPolicy
 * @brief Policies governing active signal processing and compensation.
 */
struct CalibrationProcessingPolicy
{
    int policyVersion { 1 };
    bool latencyCompensationEnabled { true };
    bool inverseCompensationEnabled { false };
    float inverseCompensationMaxBoostDb { 0.0f };

    [[nodiscard]] bool operator==(const CalibrationProcessingPolicy& other) const noexcept;
    [[nodiscard]] bool operator!=(const CalibrationProcessingPolicy& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @enum class NoiseBaselineStatus
 * @brief Explicit diagnostic state of physical noise floor measurement.
 */
enum class NoiseBaselineStatus
{
    Valid,
    BelowMeasurementFloor,
    Contaminated,
    Clipped,
    DeviceStopped,
    NotMeasured
};

inline const char* noiseBaselineStatusToString(NoiseBaselineStatus s) noexcept
{
    switch (s)
    {
        case NoiseBaselineStatus::Valid: return "Valid";
        case NoiseBaselineStatus::BelowMeasurementFloor: return "BelowMeasurementFloor";
        case NoiseBaselineStatus::Contaminated: return "Contaminated";
        case NoiseBaselineStatus::Clipped: return "Clipped";
        case NoiseBaselineStatus::DeviceStopped: return "DeviceStopped";
        case NoiseBaselineStatus::NotMeasured: return "NotMeasured";
    }
    return "NotMeasured";
}

inline NoiseBaselineStatus noiseBaselineStatusFromString(const std::string& str) noexcept
{
    if (str == "Valid") return NoiseBaselineStatus::Valid;
    if (str == "BelowMeasurementFloor") return NoiseBaselineStatus::BelowMeasurementFloor;
    if (str == "Contaminated") return NoiseBaselineStatus::Contaminated;
    if (str == "Clipped") return NoiseBaselineStatus::Clipped;
    if (str == "DeviceStopped") return NoiseBaselineStatus::DeviceStopped;
    return NoiseBaselineStatus::NotMeasured;
}

/**
 * @struct CalibrationNoiseBaseline
 * @brief Hardware noise floor baseline measured prior to the loopback sweep with output muted.
 */
struct CalibrationNoiseBaseline
{
    NoiseBaselineStatus status { NoiseBaselineStatus::NotMeasured };
    int durationSamples { 0 };
    double durationMs { 0.0 };
    float rmsDbfs { -120.0f };
    float peakDbfs { -120.0f };
    std::array<float, 32> spectralBandDbfs {};
    bool hasSpectralBands { false };
    bool outputMuted { true };
    bool inputWasClipped { false };
    std::string directMonitorState { "UserConfirmedOff" };

    [[nodiscard]] bool operator==(const CalibrationNoiseBaseline& other) const noexcept;
    [[nodiscard]] bool operator!=(const CalibrationNoiseBaseline& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @struct CalibrationIntegrity
 * @brief Cryptographic hash for provenance, integrity and corruption detection.
 */
struct CalibrationIntegrity
{
    std::string hashAlgorithm { "SHA-256" };
    std::string snapshotHash;

    [[nodiscard]] bool operator==(const CalibrationIntegrity& other) const noexcept
    {
        return hashAlgorithm == other.hashAlgorithm && snapshotHash == other.snapshotHash;
    }
    [[nodiscard]] bool operator!=(const CalibrationIntegrity& other) const noexcept
    {
        return !(*this == other);
    }
};

struct CalibrationDraft;

/**
 * @struct CalibrationSnapshot
 * @brief Immutable, portable, and verifiable record of a loopback calibration.
 */
struct CalibrationSnapshot
{
    int schemaVersion { 1 };
    std::string displayName;
    std::string profileId;
    std::string createdAt; // ISO 8601 UTC timestamp

    CalibrationCompatibility compatibility;
    CalibrationCaptureMetadata capture;
    CalibrationNoiseBaseline noiseBaseline;
    CalibrationResultMetrics result;
    CalibrationProcessingPolicy processingPolicy;
    CalibrationIntegrity integrity;

    /**
     * @brief Computes deterministic SHA-256 hash over canonical payload (excluding integrity.snapshotHash).
     */
    [[nodiscard]] std::string computeHash() const;

    /**
     * @brief Verifies whether the integrity hash matches the current payload using SHA-256.
     */
    [[nodiscard]] bool verifyIntegrity() const;

    /**
     * @brief Serializes the complete snapshot to JSON.
     */
    [[nodiscard]] nlohmann::json toJson() const;

    /**
     * @brief Deserializes a snapshot from JSON. Throws on error.
     */
    static CalibrationSnapshot fromJson(const nlohmann::json& j);

    /**
     * @brief Safely parses a snapshot from JSON without throwing exceptions.
     */
    static std::optional<CalibrationSnapshot> fromJsonSafe(const nlohmann::json& j, std::string* outError = nullptr);

    /**
     * @brief Helper to generate a default profile ID.
     */
    static std::string generateDefaultProfileId(const std::string& deviceId, const std::string& timestampIso);

    /**
     * @brief Generates an ISO 8601 UTC timestamp string for the current moment.
     */
    static std::string getCurrentUtcIsoTimestamp();

    /**
     * @brief Factory creating a sealed snapshot from LoopbackCalibrationData, noise baseline and context.
     * Computes the integrity SHA-256 hash automatically.
     */
    static CalibrationSnapshot create(
        const std::string& displayName,
        const std::string& profileId,
        const CalibrationCompatibility& compat,
        const CalibrationCaptureMetadata& capture,
        const math::LoopbackCalibrationData& resultData,
        int clippingSamples = 0,
        const CalibrationProcessingPolicy& policy = {},
        const CalibrationNoiseBaseline& baseline = {});
};

/**
 * @struct CalibrationDraft
 * @brief Mutable staging state in UI prior to saving or persisting.
 * Editing the draft does not mutate existing sealed CalibrationSnapshot instances.
 */
struct CalibrationDraft
{
    std::string displayName;
    std::string profileId;
    std::string createdAt;
    CalibrationCompatibility compatibility;
    CalibrationCaptureMetadata capture;
    CalibrationNoiseBaseline noiseBaseline;
    CalibrationResultMetrics result;
    CalibrationProcessingPolicy processingPolicy;

    /**
     * @brief Freezes the draft into an immutable, sealed CalibrationSnapshot with SHA-256 integrity.
     */
    [[nodiscard]] CalibrationSnapshot sealSnapshot() const;

    static CalibrationDraft fromMeasurement(
        const std::string& suggestedDisplayName,
        const std::string& profileId,
        const CalibrationCompatibility& compat,
        const CalibrationCaptureMetadata& capture,
        const math::LoopbackCalibrationData& resultData,
        int clippingSamples = 0,
        const CalibrationProcessingPolicy& policy = {},
        const CalibrationNoiseBaseline& baseline = {});

    static CalibrationDraft fromSnapshot(const CalibrationSnapshot& snapshot);
};

} // namespace abdaudiolab::calibration
