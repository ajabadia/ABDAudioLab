#pragma once

#include <juce_core/juce_core.h>
#include "CalibrationRecord.h"
#include "CalibrationSnapshot.h"
#include <optional>
#include <vector>
#include <string>

namespace abdaudiolab::calibration
{

class CalibrationProfileStore
{
public:
    struct SaveResult
    {
        bool success { false };
        std::string profileId;
        std::string errorMessage;
    };

    /**
     * @brief Constructs store. If storageDirectory is empty/invalid, uses default %APPDATA% location.
     */
    explicit CalibrationProfileStore(juce::File storageDirectory = {});

    /**
     * @brief Saves a calibration record using atomic two-phase write ({profileId}.json.tmp -> {profileId}.json).
     * @param record The record to save.
     * @param overwrite If false and target already exists, save will fail without modifying existing file.
     */
    SaveResult save(const CalibrationRecord& record, bool overwrite = false);

    /**
     * @brief Saves a CalibrationSnapshot using atomic two-phase write ({profileId}.json.tmp -> {profileId}.json).
     */
    SaveResult saveSnapshot(const CalibrationSnapshot& snapshot, bool overwrite = false);

    /**
     * @brief Loads a calibration record by profileId.
     * Returns std::nullopt if not found, corrupted, or unsupported schema version.
     */
    [[nodiscard]] std::optional<CalibrationRecord> load(const std::string& profileId) const;

    /**
     * @brief Loads a CalibrationSnapshot by profileId. Supports legacy record migration.
     */
    [[nodiscard]] std::optional<CalibrationSnapshot> loadSnapshot(const std::string& profileId) const;

    /**
     * @brief Lists all valid calibration records in store, sorted by createdAt descending (most recent first).
     * Corrupted files and temporary files (.tmp) are ignored safely.
     */
    [[nodiscard]] std::vector<CalibrationRecord> list() const;

    /**
     * @brief Lists all valid calibration snapshots in store, sorted by createdAt descending.
     */
    [[nodiscard]] std::vector<CalibrationSnapshot> listSnapshots() const;

    /**
     * @brief Deletes a profile by profileId. Returns true if file was successfully removed.
     */
    bool remove(const std::string& profileId);

    /**
     * @brief Returns current active storage directory.
     */
    [[nodiscard]] juce::File getStorageDirectory() const noexcept { return storageDir; }

    /**
     * @brief Returns the default %APPDATA%/ABDAudioLab/calibration_profiles directory.
     */
    [[nodiscard]] static juce::File getDefaultStorageDirectory();

    /**
     * @brief Validates that profileId contains only safe characters [A-Za-z0-9_-] and is non-empty.
     */
    [[nodiscard]] static bool isValidProfileId(const std::string& profileId);

    /**
     * @brief Checks if a record meets strict criteria for persistence (valid ID, calibrated, no clipping).
     */
    [[nodiscard]] static bool isRecordValidForSaving(const CalibrationRecord& record, std::string& outError);

    /**
     * @brief Generates current UTC ISO 8601 string (e.g. "2026-10-04T10:30:00Z").
     */
    [[nodiscard]] static std::string getCurrentUtcIsoTimestamp();

    /**
     * @brief Generates a stable unique profileId string based on device and timestamp.
     */
    [[nodiscard]] static std::string generateDefaultProfileId(const std::string& deviceName, const std::string& isoTimestamp);

private:
    juce::File storageDir;
};

} // namespace abdaudiolab::calibration
