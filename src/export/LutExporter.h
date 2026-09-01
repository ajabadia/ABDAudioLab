#pragma once

#include <string>
#include <vector>
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
};

} // namespace abdaudiolab::exporting
