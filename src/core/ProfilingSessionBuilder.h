/**
 * @file ProfilingSessionBuilder.h
 * @brief Pure decoupled builder for ProfilingSession and TestCase cartesian orchestration.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ProfilingSession.h"
#include "HardwareContractRegistry.h"
#include "../gui/suite/SuiteDataModels.h"
#include <string>
#include <vector>

namespace abdaudiolab::core
{

/**
 * @brief Strongly-typed status for session construction.
 */
enum class BuildStatus
{
    Success,
    EmptyQueue,
    InvalidSampleRate,
    InvalidPatch,
    InvalidHardwareContract,
    InvalidInput
};

[[nodiscard]] inline std::string buildStatusToString(BuildStatus s) noexcept
{
    switch (s)
    {
        case BuildStatus::Success:                 return "Success";
        case BuildStatus::EmptyQueue:              return "EmptyQueue";
        case BuildStatus::InvalidSampleRate:       return "InvalidSampleRate";
        case BuildStatus::InvalidPatch:            return "InvalidPatch";
        case BuildStatus::InvalidHardwareContract: return "InvalidHardwareContract";
        case BuildStatus::InvalidInput:            return "InvalidInput";
    }
    return "Unknown";
}

/**
 * @brief Explicit point coordinate for targeted re-measurement / patching.
 */
struct PatchPoint
{
    int testIndex { -1 };  ///< Index of the test in the queue (qIdx)
    int pointIndex { -1 }; ///< Index of the measured point within that test (0-based)

    bool operator==(const PatchPoint& other) const noexcept
    {
        return testIndex == other.testIndex && pointIndex == other.pointIndex;
    }
};

/**
 * @brief Environmental, temporal, and audio settings provided explicitly.
 * Pure builders must never query clocks (juce::Time) or audio devices.
 */
struct SessionMetadataConfig
{
    std::string hardwareName;
    std::string targetModule;
    std::string operatorMode; // e.g. "MOCK_DSP", "AUTOMATED_SYSEX", "MANUAL_EURORACK"
    double sampleRate { 96000.0 };
    int bitDepth { 24 };
    std::string timestampIso8601;
    std::string operatorNotes;
    float ambientTemperatureC { 22.0f };
    int warmupTimeMinutes { 15 };
};

/**
 * @brief Completely materialized, immutable snapshot of hardware specifications.
 * Retains zero raw or const pointers to mutable registries.
 */
struct HardwareContractSnapshot
{
    std::string selectedHardwareId;
    std::string selectedFunctionId;
    bool isAutonomousSynth { false };
    std::vector<HardwareContract> contracts;
};

/**
 * @brief Deterministic, non-throwing return value for session construction.
 */
struct BuildResult
{
    BuildStatus status { BuildStatus::InvalidInput };
    ProfilingSession session;
    std::string errorCode;
    std::string message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == BuildStatus::Success;
    }
};

/**
 * @brief Pure domain transformer that converts test queues and hardware contracts
 * into executable ProfilingSession objects without side effects.
 */
class ProfilingSessionBuilder
{
public:
    [[nodiscard]] static BuildResult buildFromQueue(
        const std::vector<gui::QueueItem>& queue,
        const HardwareContractSnapshot& hardware,
        const SessionMetadataConfig& config);

    [[nodiscard]] static BuildResult buildPatch(
        const std::vector<PatchPoint>& pointsToPatch,
        const std::vector<gui::QueueItem>& queue,
        const HardwareContractSnapshot& hardware,
        const SessionMetadataConfig& config);

    [[nodiscard]] static std::string mapBadgeToBlockType(const juce::String& badgeText);
};

} // namespace abdaudiolab::core
