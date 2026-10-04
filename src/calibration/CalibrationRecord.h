#pragma once

#include <string>
#include <vector>
#include "../math/LoopbackCalibrator.h"

namespace abdaudiolab::calibration
{

struct DeviceSnapshot
{
    std::string deviceName;
    std::string driverType { "Unknown" };
    double sampleRate { 48000.0 };
    int bufferSizeSamples { 256 };
};

struct RoutingSnapshot
{
    int inputChannelIndex { 0 };
    std::string inputChannelLabel { "Input 1" };
    int outputChannelIndex { 0 };
    std::string outputChannelLabel { "Output 1" };
};

struct Provenance
{
    std::string applicationVersion;
    int calibrationAlgorithmVersion { 1 };
};

struct CalibrationRecord
{
    int schemaVersion { 1 };
    std::string profileId;      // e.g. "calibration-2026-10-04-103000"
    std::string createdAt;      // ISO 8601 UTC (e.g. "2026-10-04T10:30:00Z")
    DeviceSnapshot deviceSnapshot;
    RoutingSnapshot routingSnapshot;
    math::LoopbackCalibrationData calibrationResult;
    Provenance provenance;
};

} // namespace abdaudiolab::calibration
