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
    std::vector<core::ParameterStep> controlSteps; /**< Snapshot of all hardware control parameters for this point. */
    math::StatisticalPair muSigmaValue;  /**< Primary statistical metric (mean, stddev). */
    math::StatisticalPair secondaryValue;/**< Secondary statistical metric (mean, stddev). */
    math::StatisticalPair thdValue;      /**< THD statistical metric (mean, stddev). */
    int globalIndex { -1 };              /**< Global session point index. */
    int queueIndex { -1 };               /**< Queue test index. */
    int pointIndexInTest { -1 };         /**< Point index within test (1-based). */
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
 * @struct PackageArtifactFixity
 * @brief Cryptographic fixity entry for an exported package artifact.
 */
struct PackageArtifactFixity
{
    std::string name;      /**< Canonical relative filename (e.g., profile_lut.h). */
    std::string type;      /**< Artifact classification (e.g., cpp_header, telemetry_json). */
    uint64_t size { 0 };   /**< Exact size in bytes. */
    std::string sha256;    /**< SHA-256 digest in lowercase hexadecimal. */
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
    std::vector<PackageArtifactFixity> packageArtifacts; /**< Cryptographic fixity entries for all package artifacts. */
    std::string timestamp;               /**< Deterministic session timestamp. If empty, defaults to system clock. */

    // 1.7.12 Laboratory observations and environmental parameters
    std::string operatorNotes;           /**< Free-form operator notes and laboratory observations. */
    float ambientTemperatureC { 22.0f }; /**< Laboratory ambient temperature in Celsius. */
    int warmupTimeMinutes { 15 };        /**< Hardware warm-up time in minutes before profiling. */

    // 5.4 Wiener-Hammerstein Non-Linear (LNL) Model parameters
    bool hasWienerHammersteinModel { false };   /**< True if LNL identification was performed. */
    std::vector<float> whH1Taps;                /**< Linear input FIR filter taps. */
    float whNonLinearCoeffA { 0.0f };           /**< 3rd-order static non-linearity coefficient: f(u) = u + a * u^3. */
    std::vector<float> whH2Taps;                /**< Linear output FIR filter taps. */
    float whGoodnessOfFitR2 { 0.0f };           /**< Coefficient of determination R^2 [0.0, 1.0]. */
    float whResidualErrorRms { 0.0f };          /**< Root-mean-square error between model and target. */
    float whPreFilterCentroidHz { 0.0f };       /**< Spectral centroid of h1. */
    float whPostFilterCentroidHz { 0.0f };      /**< Spectral centroid of h2. */
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

    /**
     * @brief Exports measured points into an ES6 JavaScript module with Float32Array buffers for WebAudio / WASM.
     * @param destinationJsPath Target filesystem path for JavaScript output.
     * @param metadata Profiling session metadata.
     * @param tableName Export variable identifier name.
     * @param points Vector of measured data points.
     * @return true on success, false on write error.
     */
    static bool exportToJavaScriptModule(const std::string& destinationJsPath,
                                         const core::ProfilingMetadata& metadata,
                                         const std::string& tableName,
                                         const std::vector<MeasuredPoint>& points);
};

} // namespace abdaudiolab::exporting
