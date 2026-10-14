/**
 * @file LoopbackProfile.h
 * @brief Structured contract and serialization for .abdloop infrastructure profile files.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../math/LoopbackCalibrator.h"
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace abdaudiolab::calibration
{

/**
 * @struct LoopbackProfile
 * @brief Structured contract for .abdloop infrastructure profile files.
 *
 * Encapsulates audio interface loopback calibration, transfer function,
 * latency, and noise floor data, keyed by hardware signature.
 */
struct LoopbackProfile
{
    static constexpr int kCurrentSchemaVersion  = 1;
    static constexpr const char* kFileExtension = ".abdloop";

    int schemaVersion{kCurrentSchemaVersion};
    std::string profileId;  // e.g. "loopback-focusrite_usb_asio_48000_256"
    std::string createdAt;  // ISO 8601 UTC timestamp
    std::string deviceName; // Hardware device identifier
    std::string driverType{"Unknown"};
    double sampleRate{48000.0};
    int bufferSizeSamples{256};
    int inputChannelIndex{0};
    std::string inputChannelLabel{"Input 1"};
    int outputChannelIndex{0};
    std::string outputChannelLabel{"Output 1"};

    // Calibration measurement metrics
    bool isCalibrated{false};
    float peakInDbfs{-100.0f};
    float recommendedTrimGain{1.0f};
    float targetHeadroomDbfs{-3.0f};
    float roundTripLatencyMs{0.0f};
    int latencySamples{0};
    float thdPlusNoisePercent{0.001f};
    float snrDb{90.0f};
    float frequencyFlatnessDb{0.1f};
    bool phaseInversionDetected{false};
    bool clippingDetected{false};

    // Transfer function and inverse filter correction
    std::vector<float> freqsHz;
    std::vector<float> magnitudeDb;
    std::vector<float> inverseCorrectionDb;

    // Integrity hash
    std::string sha256Hash;

    /**
     * @brief Computes deterministic hardware matching key.
     */
    [[nodiscard]] static std::string generateHardwareKey(const std::string& devName, double sRate, int bufSize)
    {
        juce::String cleanName = juce::String(devName).toLowerCase().replaceCharacters(" :\\/*?\"<>|", "__________");
        return cleanName.toStdString() + "_" + std::to_string(static_cast<int>(std::round(sRate))) + "_" + std::to_string(bufSize);
    }

    [[nodiscard]] std::string getHardwareKey() const
    {
        return generateHardwareKey(deviceName, sampleRate, bufferSizeSamples);
    }

    [[nodiscard]] nlohmann::json toJson() const;
    static std::optional<LoopbackProfile> fromJson(const nlohmann::json& j);

    static std::optional<LoopbackProfile> fromCalibrationData(
        const math::LoopbackCalibrationData& data,
        const std::string& devName,
        const std::string& driver,
        double sRate,
        int bufSize,
        int inCh                    = 0,
        const std::string& inLabel  = "Input 1",
        int outCh                   = 0,
        const std::string& outLabel = "Output 1");

    [[nodiscard]] math::LoopbackCalibrationData toCalibrationData() const;
    [[nodiscard]] std::string computeSha256() const;
    [[nodiscard]] bool verifyHash() const;
};

} // namespace abdaudiolab::calibration
