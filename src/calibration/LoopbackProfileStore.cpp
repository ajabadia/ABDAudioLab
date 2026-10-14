/**
 * @file LoopbackProfileStore.cpp
 * @brief Implementation of LoopbackProfileStore.
 * @author ABDSynths
 * @date 2026
 */

#include "LoopbackProfileStore.h"
#include "../core/LabDataDirectories.h"
#include <algorithm>
#include <fstream>

namespace abdaudiolab::calibration
{

LoopbackProfileStore::LoopbackProfileStore(juce::File storageDirectory)
    : storageDir(storageDirectory.exists() || !storageDirectory.getFullPathName().isEmpty()
                     ? storageDirectory
                     : getDefaultStorageDirectory())
{
    if (!storageDir.exists())
        storageDir.createDirectory();
}

void LoopbackProfileStore::setStorageDirectory(const juce::File& newDir)
{
    storageDir = newDir.exists() || !newDir.getFullPathName().isEmpty()
                     ? newDir
                     : getDefaultStorageDirectory();
    if (!storageDir.exists())
        storageDir.createDirectory();
}

juce::File LoopbackProfileStore::getDefaultStorageDirectory()
{
    auto labDirs = core::resolveLabDataDirectories();
    if (labDirs.loopbackProfiles.isDirectory() || labDirs.dataRoot.isDirectory())
    {
        if (!labDirs.loopbackProfiles.exists())
            labDirs.loopbackProfiles.createDirectory();
        return labDirs.loopbackProfiles;
    }

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("loopback_profiles");
}

LoopbackProfileStore::SaveResult LoopbackProfileStore::saveProfile(const LoopbackProfile& profile, bool overwrite)
{
    SaveResult res;
    res.profileId = profile.profileId.empty() ? ("loopback-" + profile.getHardwareKey()) : profile.profileId;

    if (!profile.isCalibrated || profile.clippingDetected)
    {
        res.errorMessage = "Profile cannot be saved: uncalibrated or clipping detected.";
        return res;
    }

    if (!storageDir.exists() && !storageDir.createDirectory())
    {
        res.errorMessage = "Failed to create directory: " + storageDir.getFullPathName().toStdString();
        return res;
    }

    const juce::String key      = juce::String(profile.getHardwareKey());
    const juce::File targetFile = storageDir.getChildFile(key + LoopbackProfile::kFileExtension);
    const juce::File tmpFile    = storageDir.getChildFile(key + LoopbackProfile::kFileExtension + ".tmp");

    if (targetFile.existsAsFile() && !overwrite)
    {
        res.errorMessage = "Profile already exists and overwrite is false.";
        return res;
    }

    try
    {
        auto jsonCopy = profile;
        if (jsonCopy.sha256Hash.empty())
            jsonCopy.sha256Hash = jsonCopy.computeSha256();

        const std::string serialized = jsonCopy.toJson().dump(2);

        if (tmpFile.existsAsFile())
            tmpFile.deleteFile();

        if (!tmpFile.replaceWithText(serialized, false, false, "utf-8"))
        {
            res.errorMessage = "Failed to write temporary file: " + tmpFile.getFullPathName().toStdString();
            return res;
        }

        if (targetFile.existsAsFile())
            targetFile.deleteFile();

        if (!tmpFile.moveFileTo(targetFile))
        {
            res.errorMessage = "Failed to atomically rename tmp to target: " + targetFile.getFullPathName().toStdString();
            tmpFile.deleteFile();
            return res;
        }

        res.success  = true;
        res.filePath = targetFile.getFullPathName().toStdString();
        return res;
    }
    catch (const std::exception& e)
    {
        res.errorMessage = std::string("Exception saving profile: ") + e.what();
        if (tmpFile.existsAsFile())
            tmpFile.deleteFile();
        return res;
    }
}

std::optional<LoopbackProfile> LoopbackProfileStore::loadProfile(const juce::File& file) const
{
    if (!file.existsAsFile())
        return std::nullopt;

    try
    {
        const juce::String text = file.loadFileAsString();
        if (text.isEmpty())
            return std::nullopt;

        const auto j = nlohmann::json::parse(text.toStdString());
        auto prof    = LoopbackProfile::fromJson(j);
        if (!prof.has_value() || !prof->isCalibrated)
            return std::nullopt;

        return prof;
    }
    catch (...)
    {
        // Tolerate corrupt / incomplete JSON files safely
        return std::nullopt;
    }
}

std::optional<LoopbackProfile> LoopbackProfileStore::findMatchingProfile(
    const std::string& deviceName,
    double sampleRate,
    int bufferSize) const
{
    if (deviceName.empty() || sampleRate <= 0.0)
        return std::nullopt;

    auto allProfiles = listProfiles();
    if (allProfiles.empty())
        return std::nullopt;

    const juce::String searchDev = juce::String(deviceName).trim().toLowerCase();

    // 1. Search for exact device, sampleRate (within ±1Hz) and bufferSize
    for (const auto& p : allProfiles)
    {
        const juce::String pDev = juce::String(p.deviceName).trim().toLowerCase();
        const bool devMatch     = (pDev == searchDev) || pDev.contains(searchDev) || searchDev.contains(pDev);
        const bool srMatch      = std::abs(p.sampleRate - sampleRate) <= 1.0;
        const bool bufMatch     = (bufferSize <= 0) || (p.bufferSizeSamples == bufferSize);

        if (devMatch && srMatch && bufMatch)
            return p;
    }

    // 2. Fallback: if exact bufferSize was not found, allow buffer size mismatch if sample rate matches
    if (bufferSize > 0)
    {
        for (const auto& p : allProfiles)
        {
            const juce::String pDev = juce::String(p.deviceName).trim().toLowerCase();
            const bool devMatch     = (pDev == searchDev) || pDev.contains(searchDev) || searchDev.contains(pDev);
            const bool srMatch      = std::abs(p.sampleRate - sampleRate) <= 1.0;

            if (devMatch && srMatch)
                return p;
        }
    }

    return std::nullopt;
}

std::vector<LoopbackProfile> LoopbackProfileStore::listProfiles() const
{
    std::vector<LoopbackProfile> list;
    if (!storageDir.exists())
        return list;

    const auto files = storageDir.findChildFiles(juce::File::findFiles, false, "*" + juce::String(LoopbackProfile::kFileExtension));
    for (const auto& f : files)
    {
        if (f.getFileName().endsWith(".tmp"))
            continue;

        auto p = loadProfile(f);
        if (p.has_value())
            list.push_back(std::move(*p));
    }

    // Sort descending by createdAt
    std::sort(list.begin(), list.end(), [](const LoopbackProfile& a, const LoopbackProfile& b) {
        return a.createdAt > b.createdAt;
    });

    return list;
}

std::vector<LoopbackProfile> LoopbackProfileStore::findProfilesForDevice(
    const std::string& deviceName,
    double sampleRate) const
{
    std::vector<LoopbackProfile> matches;
    if (deviceName.empty())
        return matches;

    auto allProfiles             = listProfiles();
    const juce::String searchDev = juce::String(deviceName).trim().toLowerCase();

    for (const auto& p : allProfiles)
    {
        const juce::String pDev = juce::String(p.deviceName).trim().toLowerCase();
        const bool devMatch     = (pDev == searchDev) || pDev.contains(searchDev) || searchDev.contains(pDev);
        const bool srMatch      = (sampleRate <= 0.0) || (std::abs(p.sampleRate - sampleRate) <= 1.0);

        if (devMatch && srMatch)
            matches.push_back(p);
    }
    return matches;
}

bool LoopbackProfileStore::removeProfile(const std::string& hardwareKeyOrId)
{
    if (!storageDir.exists())
        return false;

    juce::String clean = juce::String(hardwareKeyOrId);
    if (clean.startsWith("loopback-"))
        clean = clean.substring(9);

    const juce::File f = storageDir.getChildFile(clean + LoopbackProfile::kFileExtension);
    if (f.existsAsFile())
        return f.deleteFile();

    // Check if filename matches directly
    const auto files = storageDir.findChildFiles(juce::File::findFiles, false, "*" + juce::String(LoopbackProfile::kFileExtension));
    for (const auto& file : files)
    {
        if (file.getFileNameWithoutExtension().equalsIgnoreCase(clean))
            return file.deleteFile();
    }

    return false;
}

} // namespace abdaudiolab::calibration
