/**
 * @file LoopbackProfileStore.h
 * @brief Thread-safe local repository for .abdloop infrastructure profile files.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include "LoopbackProfile.h"
#include <optional>
#include <vector>
#include <string>

namespace abdaudiolab::calibration
{

/**
 * @class LoopbackProfileStore
 * @brief Manages persistence, atomic storage, and lookup of .abdloop hardware profiles.
 */
class LoopbackProfileStore
{
public:
    struct SaveResult
    {
        bool success { false };
        std::string profileId;
        std::string filePath;
        std::string errorMessage;
    };

    /**
     * @brief Constructs store. If storageDirectory is empty, uses %APPDATA%/ABDAudioLab/loopback_profiles/.
     */
    explicit LoopbackProfileStore(juce::File storageDirectory = {});

    /**
     * @brief Saves a profile using two-phase atomic write ({key}.abdloop.tmp -> {key}.abdloop).
     */
    SaveResult saveProfile(const LoopbackProfile& profile, bool overwrite = true);

    /**
     * @brief Loads a profile directly from a file.
     * Returns std::nullopt if missing, corrupted JSON, or invalid schema.
     */
    [[nodiscard]] std::optional<LoopbackProfile> loadProfile(const juce::File& file) const;

    /**
     * @brief Finds the best matching profile for a given device, sample rate and buffer size.
     * Matches deviceName (case-insensitive), sampleRate (±1.0 Hz), and buffer size.
     * When multiple matches exist, returns the most recently created.
     */
    [[nodiscard]] std::optional<LoopbackProfile> findMatchingProfile(
        const std::string& deviceName,
        double sampleRate,
        int bufferSize = 0) const;

    /**
     * @brief Lists all valid .abdloop profiles in store, ordered by createdAt descending.
     */
    [[nodiscard]] std::vector<LoopbackProfile> listProfiles() const;

    /**
     * @brief Finds all profiles matching a device name, optionally filtered by sample rate (±1.0 Hz).
     * Ordered by createdAt descending (most recent first).
     */
    [[nodiscard]] std::vector<LoopbackProfile> findProfilesForDevice(
        const std::string& deviceName,
        double sampleRate = 0.0) const;

    /**
     * @brief Removes a profile by hardware key or profile ID.
     */
    bool removeProfile(const std::string& hardwareKeyOrId);

    /**
     * @brief Returns the active storage directory.
     */
    [[nodiscard]] juce::File getStorageDirectory() const noexcept { return storageDir; }

    /**
     * @brief Sets and creates a new active storage directory.
     */
    void setStorageDirectory(const juce::File& newDir);

    /**
     * @brief Returns default unified loopback_profiles/ directory.
     */
    [[nodiscard]] static juce::File getDefaultStorageDirectory();

private:
    juce::File storageDir;
};

} // namespace abdaudiolab::calibration
