/**
 * @file SessionSerializer.h
 * @brief High-performance Session Serializer & Container Manager using .abdlabtest ZIP format.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include "../BuildVersion.h"
#include "../audio/LabStimulusGenerator.h"
#include "../gui/TestConfigModal.h"
#include "../export/LutExporter.h"

namespace abdaudiolab::core
{

/**
 * @struct SessionManifest
 * @brief Contains session metadata, hardware profile configuration, and measurement test definitions.
 */
struct SessionManifest
{
    std::string sessionTitle { "ABDAudioLab_Session" }; /**< User session title. */
    std::string appVersion { version::kAppVersion };    /**< ABDAudioLab application version string. */
    int buildNumber { version::kBuildNumber };           /**< Build number. */
    std::string formatVersion { "1.0" };                /**< Manifest JSON format version. */
    std::string timestamp;                             /**< ISO8601 timestamp of creation. */
    
    std::string hardwareId;                             /**< Unique hardware device contract ID. */
    std::string hardwareName;                           /**< Hardware device display name. */
    std::string hardwareDisplayName;                    /**< Full hardware display string. */
    std::string activeFunctionId;                       /**< Selected hardware function ID. */
    std::string activeFunctionName;                     /**< Selected hardware function display name. */
    std::string targetModule;                           /**< Target hardware submodule name. */
    
    double sampleRate { 96000.0 };                       /**< Audio sampling rate in Hz. */
    float lineCalibrationGainDb { -3.0f };               /**< Analog loopback calibration gain offset in dB. */
    float noiseFloorThresholdDb { -85.0f };             /**< Baseline noise floor threshold in dBFS. */

    std::vector<gui::TestConfiguration> tests;           /**< Configured test suite definitions. */
    int totalMeasuredPoints { 0 };                       /**< Total measured point count in session. */
    int totalPointsMeasured { 0 };                       /**< Total points measured metric. */
    std::vector<std::string> recordedAudioFiles;         /**< Recorded raw audio artifact file paths. */
    std::vector<std::string> matrixFiles;                /**< Exported matrix dataset file paths. */
};

/**
 * @class SessionSerializer
 * @brief Manages compressed session container creation, incremental auto-saving, and deserialization.
 */
class SessionSerializer
{
public:
    SessionSerializer();
    ~SessionSerializer();

    /**
     * @brief Gets the temporary working folder for current session artifacts.
     */
    [[nodiscard]] const juce::File& getWorkingTempDirectory() const noexcept { return workingTempDir; }

    /**
     * @brief Gets the active package file path if saved.
     */
    [[nodiscard]] const juce::File& getActiveSessionFile() const noexcept { return activeSessionFile; }

    /**
     * @brief Sets the active package file path.
     */
    void setActiveSessionFile(const juce::File& file) { activeSessionFile = file; }

    /**
     * @brief Creates a self-contained .abdlabtest package from the working folder and manifest.
     * @param targetPackageFile Output package file path.
     * @param manifest Session manifest structure.
     * @param points Vector of measured data points.
     * @return true on successful packaging, false on error.
     */
    bool saveSessionToPackage(const juce::File& targetPackageFile,
                              const SessionManifest& manifest,
                              const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Opens and validates a .abdlabtest package, extracting contents to working folder.
     * @param sourcePackageFile Source package file path.
     * @param outManifest Extracted manifest structure.
     * @param outPoints Extracted measured data points.
     * @param outErrorMessage Diagnostic error message output if failure occurs.
     * @return true on successful extraction, false on error.
     */
    bool loadSessionFromPackage(const juce::File& sourcePackageFile,
                                SessionManifest& outManifest,
                                std::vector<exporting::MeasuredPoint>& outPoints,
                                juce::String& outErrorMessage);

    /**
     * @brief Saves incremental auto-save file asynchronously without blocking audio thread.
     */
    bool triggerIncrementalAutoSave(const SessionManifest& manifest,
                                    const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Checks if a recoverable auto-save package exists on disk.
     */
    [[nodiscard]] juce::File getRecoverableAutoSaveFile() const;

    /**
     * @brief Discards temporary working session files.
     */
    void cleanupTempSession();

private:
    juce::File workingTempDir;
    juce::File activeSessionFile;
    juce::File autoSaveFile;

    static nlohmann::json serializeManifestToJson(const SessionManifest& manifest);
    static bool deserializeManifestFromJson(const nlohmann::json& j, SessionManifest& outManifest);
};

} // namespace abdaudiolab::core
