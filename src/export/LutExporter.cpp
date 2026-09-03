#include "LutExporter.h"
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>

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
    std::string safeName;
    for (char c : tableName)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            safeName += c;
        else
            safeName += '_';
    }
    if (safeName.empty() || std::isdigit(static_cast<unsigned char>(safeName[0])))
        safeName = "lut_" + safeName;

    out << "inline constexpr size_t " << safeName << "_SIZE = " << points.size() << ";\n\n";
    out << "inline const alignas(16) AbdBatchedPoint " << safeName << "[" << safeName << "_SIZE] = {\n";

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
    return out.good();
}

bool LutExporter::exportToJsonReport(const std::string& destinationJsonPath,
                                    const core::ProfilingMetadata& metadata,
                                    const std::vector<MeasuredPoint>& points)
{
    nlohmann::json root;
    root["generator"] = "ABDAudioLab Analytic Engine";
    root["hardwareName"] = metadata.hardwareName;
    root["targetModule"] = metadata.targetModule;
    root["operatorMode"] = metadata.operatorMode;
    root["sampleRate"] = metadata.sampleRate;
    root["totalPoints"] = points.size();

    nlohmann::json ptsJson = nlohmann::json::array();
    for (const auto& p : points)
    {
        nlohmann::json pj;
        pj["testId"] = p.testId;
        pj["p1"] = p.param1Normalized;
        pj["p2"] = p.param2Normalized;
        pj["mu"] = p.muSigmaValue.mean;
        pj["sigma"] = p.muSigmaValue.stdDev;
        pj["secondary_mu"] = p.secondaryValue.mean;
        pj["secondary_sigma"] = p.secondaryValue.stdDev;
        pj["thd_percent"] = p.thdValue.mean;
        ptsJson.push_back(pj);
    }
    root["measuredPoints"] = ptsJson;

    std::ofstream out(destinationJsonPath);
    if (!out.is_open())
        return false;

    out << std::setw(2) << root << std::endl;
    return out.good();
}

bool LutExporter::exportSessionManifest(const std::string& destinationManifestPath,
                                       const SessionManifestData& manifest,
                                       const std::vector<MeasuredPoint>& points)
{
    nlohmann::json root;
    root["sessionManifestVersion"] = "2.0";
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_c)))
    {
        root["timestamp"] = std::string(buf);
    }

    // Hardware Details
    nlohmann::json hwJson;
    hwJson["id"] = manifest.hardwareId;
    hwJson["name"] = manifest.hardwareName;
    hwJson["brand"] = manifest.brand;
    hwJson["functionId"] = manifest.functionId;
    hwJson["functionName"] = manifest.functionName;
    hwJson["blockType"] = manifest.blockType;
    hwJson["deviceType"] = manifest.deviceType;
    root["hardware"] = hwJson;

    // Audio & Calibration
    nlohmann::json audioJson;
    audioJson["sampleRate"] = manifest.sampleRate;
    audioJson["bufferSize"] = manifest.bufferSize;
    audioJson["inputAutoTrimGainDb"] = manifest.autoTrimGainDb;
    audioJson["noiseFloorRmsDb"] = manifest.noiseFloorRmsDb;
    audioJson["averageSnrDb"] = manifest.averageSnrDb;
    root["audioCalibration"] = audioJson;

    // Custom Grid Configuration Used
    nlohmann::json gridJson = nlohmann::json::array();
    for (const auto& g : manifest.gridConfig)
    {
        nlohmann::json gj;
        gj["controlName"] = g.controlName;
        gj["stepsCount"] = g.stepCount;
        gj["evaluatedValues"] = g.evaluatedValues;
        gridJson.push_back(gj);
    }
    root["gridConfigurationUsed"] = gridJson;

    root["totalPointsMeasured"] = points.size();

    // Output Artifacts
    nlohmann::json artJson;
    artJson["cppHeaderTable"] = manifest.cppHeaderFilename;
    artJson["jsonReport"] = manifest.jsonReportFilename;
    root["outputArtifacts"] = artJson;

    std::ofstream out(destinationManifestPath);
    if (!out.is_open())
        return false;

    out << std::setw(2) << root << std::endl;
    return out.good();
}

} // namespace abdaudiolab::exporting
