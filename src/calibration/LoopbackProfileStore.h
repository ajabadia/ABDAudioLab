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
     * @brief Removes a profile by hardware key or profile ID.
     */
    bool removeProfile(const std::string& hardwareKeyOrId);

    /**
     * @brief Returns the active storage directory.
     */
    [[nodiscard]] juce::File getStorageDirectory() const noexcept { return storageDir; }

    /**
     * @brief Returns default %APPDATA%/ABDAudioLab/loopback_profiles/ directory.
     */
    [[nodiscard]] static juce::File getDefaultStorageDirectory();

private:
    juce::File storageDir;
};

} // namespace abdaudiolab::calibration
