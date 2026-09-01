#pragma once

#include <string>
#include <vector>
#include <utility>
#include "../math/LabAnalyticEngine.h"
#include "../core/ProfilingSession.h"

namespace abdaudiolab::exporting
{

struct MeasuredPoint
{
    std::string testId;
    float param1Normalized { 0.0f };
    float param2Normalized { 0.0f };
    math::StatisticalPair muSigmaValue; // e.g. Cutoff Hz, ADSR Attack ms, Gain dB
    math::StatisticalPair secondaryValue; // e.g. Resonance peak dB, Sustain level
    math::StatisticalPair thdValue;
};

struct ControlGridManifest
{
    std::string controlName;
    int stepCount { 5 };
    std::vector<float> evaluatedValues;
};

struct SessionManifestData
{
    std::string hardwareId;
    std::string hardwareName;
    std::string brand;
    std::string functionId;
    std::string functionName;
    std::string blockType;
    std::string deviceType;
    double sampleRate { 48000.0 };
    int bufferSize { 256 };
    float autoTrimGainDb { 0.0f };
    float noiseFloorRmsDb { -80.0f };
    float averageSnrDb { 30.0f };
    std::vector<ControlGridManifest> gridConfig;
    std::string cppHeaderFilename;
    std::string jsonReportFilename;
};

class LutExporter
{
public:
    LutExporter() = default;
    ~LutExporter() = default;

    static bool exportToCppHeader(const std::string& destinationHeaderPath,
                                  const core::ProfilingMetadata& metadata,
                                  const std::string& tableName,
                                  const std::vector<MeasuredPoint>& points);

    static bool exportToJsonReport(const std::string& destinationJsonPath,
                                   const core::ProfilingMetadata& metadata,
                                   const std::vector<MeasuredPoint>& points);

    static bool exportSessionManifest(const std::string& destinationManifestPath,
                                      const SessionManifestData& manifest,
                                      const std::vector<MeasuredPoint>& points);
};

} // namespace abdaudiolab::exporting
