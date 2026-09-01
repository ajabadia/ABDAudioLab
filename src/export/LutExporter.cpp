#include "LutExporter.h"
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace abdaudiolab::exporting
{

bool LutExporter::exportToCppHeader(const std::string& destinationHeaderPath,
                                    const core::ProfilingMetadata& metadata,
                                    const std::string& tableName,
                                    const std::vector<MeasuredPoint>& points)
{
    std::ofstream out(destinationHeaderPath);
    if (!out.is_open())
        return false;

    out << "// ==============================================================================\n";
    out << "// ABDAudioLab — Auto-Generated Hardware Profile Look-Up Table\n";
    out << "// Target Hardware : " << metadata.hardwareName << "\n";
    out << "// Target Module   : " << metadata.targetModule << "\n";
    out << "// Operator Mode   : " << metadata.operatorMode << "\n";
    out << "// Sample Rate     : " << metadata.sampleRate << " Hz\n";
    out << "// Total Points    : " << points.size() << "\n";
    out << "// ==============================================================================\n\n";
    out << "#pragma once\n\n";
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n\n";
    out << "namespace abdaudiolab::lut\n";
    out << "{\n\n";
    out << "struct alignas(16) AbdBatchedPoint\n";
    out << "{\n";
    out << "    float p1;         // Parameter 1 (Normalized [0, 1])\n";
    out << "    float p2;         // Parameter 2 (Normalized [0, 1])\n";
    out << "    float mu;         // Primary Value Mean (Stable control response)\n";
    out << "    float sigma;      // Primary Value StdDev (Thermal drift / ACB chaos)\n";
    out << "    float sec_mu;     // Secondary Value Mean (e.g. Q factor / sustain)\n";
    out << "    float sec_sigma;  // Secondary Value StdDev\n";
    out << "    float thd_percent;// Total Harmonic Distortion %\n";
    out << "    float reserved;   // 16-byte alignment padding\n";
    out << "};\n\n";
    out << "static constexpr size_t " << tableName << "_SIZE = " << points.size() << ";\n\n";
    out << "static const alignas(16) AbdBatchedPoint " << tableName << "[" << tableName << "_SIZE] = {\n";

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];
        out << "    { "
            << std::fixed << std::setprecision(5) << p.param1Normalized << "f, "
            << p.param2Normalized << "f, "
            << p.muSigmaValue.mean << "f, "
            << p.muSigmaValue.stdDev << "f, "
            << p.secondaryValue.mean << "f, "
            << p.secondaryValue.stdDev << "f, "
            << p.thdValue.mean << "f, 0.0f }";

        if (i + 1 < points.size())
            out << ",";
        out << " // " << p.testId << "\n";
    }

    out << "};\n\n";
    out << "} // namespace abdaudiolab::lut\n";

    return true;
}

bool LutExporter::exportToJsonReport(const std::string& destinationJsonPath,
                                     const core::ProfilingMetadata& metadata,
                                     const std::vector<MeasuredPoint>& points)
{
    nlohmann::json j;
    j["metadata"]["hardware_name"] = metadata.hardwareName;
    j["metadata"]["target_module"] = metadata.targetModule;
    j["metadata"]["operator_mode"] = metadata.operatorMode;
    j["metadata"]["sample_rate"] = metadata.sampleRate;
    j["metadata"]["bit_depth"] = metadata.bitDepth;
    j["metadata"]["total_points"] = points.size();

    j["points"] = nlohmann::json::array();
    for (const auto& p : points)
    {
        nlohmann::json pj;
        pj["test_id"] = p.testId;
        pj["param_1"] = p.param1Normalized;
        pj["param_2"] = p.param2Normalized;
        pj["primary_mean"] = p.muSigmaValue.mean;
        pj["primary_std_dev"] = p.muSigmaValue.stdDev;
        pj["secondary_mean"] = p.secondaryValue.mean;
        pj["secondary_std_dev"] = p.secondaryValue.stdDev;
        pj["thd_percent"] = p.thdValue.mean;
        j["points"].push_back(pj);
    }

    std::ofstream out(destinationJsonPath);
    if (!out.is_open())
        return false;

    out << j.dump(2);
    return true;
}

} // namespace abdaudiolab::exporting
