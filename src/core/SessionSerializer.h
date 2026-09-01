#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include "../audio/LabStimulusGenerator.h"
#include "../gui/TestConfigModal.h"
#include "../export/LutExporter.h"

namespace abdaudiolab::core
{

struct SessionManifest
{
    std::string appVersion { "1.0.0" };
    int buildNumber { 130 };
    std::string formatVersion { "1.0" };
    std::string timestamp;
    
    std::string hardwareId;
    std::string hardwareDisplayName;
    std::string activeFunctionId;
    std::string activeFunctionName;
    
    double sampleRate { 96000.0 };
    float lineCalibrationGainDb { -3.0f };
    float noiseFloorThresholdDb { -85.0f };

    std::vector<gui::TestConfiguration> tests;
    int totalMeasuredPoints { 0 };
    std::vector<std::string> recordedAudioFiles;
    std::vector<std::string> matrixFiles;
};

/**
 * @brief High-performance Session Serializer & Container Manager using .abdlabtest ZIP format.
 */
class SessionSerializer
{
public:
    SessionSerializer();
    ~SessionSerializer();

    [[nodiscard]] const juce::File& getWorkingTempDirectory() const noexcept { return workingTempDir; }
    [[nodiscard]] const juce::File& getActiveSessionFile() const noexcept { return activeSessionFile; }
    void setActiveSessionFile(const juce::File& file) { activeSessionFile = file; }

    /**
     * @brief Creates a self-contained .abdlabtest package from the working folder and manifest.
     */
    bool saveSessionToPackage(const juce::File& targetPackageFile,
                              const SessionManifest& manifest,
                              const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Opens and validates a .abdlabtest package, extracting contents to working folder.
     */
    bool loadSessionFromPackage(const juce::File& sourcePackageFile,
                                SessionManifest& outManifest,
                                std::vector<exporting::MeasuredPoint>& outPoints,
                                juce::String& outErrorMessage);

    /**
     * @brief Saves incremental auto-save file without blocking audio thread.
     */
    bool triggerIncrementalAutoSave(const SessionManifest& manifest,
                                    const std::vector<exporting::MeasuredPoint>& points);

    /**
     * @brief Checks if a recoverable auto-save package exists.
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
