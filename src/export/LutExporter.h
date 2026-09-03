/**
 * @file LutExporter.h
 * @brief Exporter for C++17 constexpr Look-Up Tables, JSON reports, and manifest metadata.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include <utility>
#include "../math/LabAnalyticEngine.h"
#include "../core/ProfilingSession.h"

namespace abdaudiolab::exporting
{

/**
 * @struct MeasuredPoint
 * @brief Holds statistical measurement output for a single N-dimensional parameter grid coordinate.
 */
struct MeasuredPoint
{
    std::string pointId;                 /**< Unique point identifier string (e.g. "P_001"). */
    std::string testId;                  /**< Associated test case ID. */
    std::string blockType;               /**< Circuit block type string. */
    std::string stimulusType;            /**< Audio stimulus type name. */
    float param1Normalized { 0.0f };     /**< Primary parameter normalized setting [0.0, 1.0]. */
    float param2Normalized { 0.0f };     /**< Secondary parameter normalized setting [0.0, 1.0]. */
    float thdPercent { 0.0f };           /**< Total Harmonic Distortion percentage (THD %). */
    float snrDb { 0.0f };                /**< Signal-to-Noise Ratio in dB. */
    std::vector<float> irSamples;        /**< Raw impulse response audio buffer. */
    math::StatisticalPair muSigmaValue;  /**< Primary statistical metric (mean, stddev). */
    math::StatisticalPair secondaryValue;/**< Secondary statistical metric (mean, stddev). */
    math::StatisticalPair thdValue;      /**< THD statistical metric (mean, stddev). */
};

/**
 * @struct ControlGridManifest
 * @brief Represents a single control parameter axis configuration.
 */
struct ControlGridManifest
{
    std::string controlName;             /**< Hardware control name. */
    int stepCount { 5 };                 /**< Step resolution. */
    std::vector<float> evaluatedValues;  /**< Sampled axis position values. */
};

/**
 * @struct SessionManifestData
 * @brief Detailed profiling session manifest dataset.
 */
struct SessionManifestData
{
    std::string hardwareId;              /**< Hardware ID. */
    std::string hardwareName;            /**< Hardware display name. */
    std::string brand;                   /**< Manufacturer brand. */
    std::string functionId;              /**< Hardware function ID. */
    std::string functionName;            /**< Hardware function display name. */
    std::string blockType;               /**< Circuit block type. */
    std::string deviceType;              /**< Target device type. */
    double sampleRate { 48000.0 };       /**< Sampling rate in Hz. */
    int bufferSize { 256 };              /**< Processing buffer size. */
    float autoTrimGainDb { 0.0f };       /**< Auto-trim gain offset in dB. */
    float noiseFloorRmsDb { -80.0f };    /**< Noise floor RMS level in dBFS. */
    float averageSnrDb { 30.0f };        /**< Average SNR across measurements. */
    std::vector<ControlGridManifest> gridConfig; /**< Control grid configuration vector. */
    std::string cppHeaderFilename;       /**< Generated C++ header filename. */
    std::string jsonReportFilename;      /**< Generated JSON report filename. */
};

/**
 * @class LutExporter
 * @brief Generates high-efficiency C++17 `constexpr` array headers and JSON telemetry reports.
 */
class LutExporter
{
public:
    LutExporter() = default;
    ~LutExporter() = default;

    /**
     * @brief Exports measured points into a production-ready C++17 `constexpr` header file.
     * @param destinationHeaderPath Target filesystem path for header output.
     * @param metadata Profiling session metadata.
     * @param tableName C++ variable identifier name for the generated LUT array.
     * @param points Vector of measured data points.
     * @return true on success, false on write error.
     */
    static bool exportToCppHeader(const std::string& destinationHeaderPath,
                                  const core::ProfilingMetadata& metadata,
                                  const std::string& tableName,
                                  const std::vector<MeasuredPoint>& points);

    /**
     * @brief Exports measured session points into a structured JSON report file.
     * @param destinationJsonPath Target filesystem path for JSON report.
     * @param metadata Profiling session metadata.
     * @param points Vector of measured data points.
     * @return true on success, false on write error.
     */
    static bool exportToJsonReport(const std::string& destinationJsonPath,
                                   const core::ProfilingMetadata& metadata,
                                   const std::vector<MeasuredPoint>& points);

    /**
     * @brief Exports full session manifest metadata and measurement points into JSON container file.
     * @param destinationManifestPath Target path for manifest JSON.
     * @param manifest Session manifest structure.
     * @param points Measured points vector.
     * @return true on success, false on error.
     */
    static bool exportSessionManifest(const std::string& destinationManifestPath,
                                      const SessionManifestData& manifest,
                                      const std::vector<MeasuredPoint>& points);
};

} // namespace abdaudiolab::exporting
