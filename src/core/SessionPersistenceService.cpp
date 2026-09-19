/**
 * @file SessionPersistenceService.cpp
 * @brief Implementation of pure session persistence, validation, and atomic container I/O.
 * @author ABDSynths
 * @date 2026
 */

#include "SessionPersistenceService.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::core
{

SessionIoResult SessionPersistenceService::save(const SessionSaveRequest& request)
{
    SessionIoResult res;

    // 1. Destination validation
    if (request.destination == juce::File())
    {
        res.status = SessionIoStatus::CannotWrite;
        res.errorCode = "DESTINATION_EMPTY";
        res.message = "Destination file cannot be empty.";
        return res;
    }

    auto parentDir = request.destination.getParentDirectory();
    if (!parentDir.exists())
    {
        if (!parentDir.createDirectory())
        {
            res.status = SessionIoStatus::CannotWrite;
            res.errorCode = "PARENT_DIR_CREATE_FAILED";
            res.message = "Could not create parent directory: " + parentDir.getFullPathName();
            return res;
        }
    }

    // 2. Atomic write using temporary file in the same parent directory
    juce::File tempZip = parentDir.getChildFile("~tmp_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()) + ".abdlabtest");
    if (tempZip.existsAsFile())
        tempZip.deleteFile();

    // Use a private serializer instance to handle package contents
    SessionSerializer serializer;
    bool ok = serializer.saveSessionToPackage(tempZip, request.manifest, request.points);

    if (!ok || !tempZip.existsAsFile() || tempZip.getSize() == 0)
    {
        if (tempZip.existsAsFile())
            tempZip.deleteFile();

        res.status = SessionIoStatus::CannotWrite;
        res.errorCode = "PACKAGE_WRITE_FAILED";
        res.message = "Failed to write session container archive to temporary file.";
        return res;
    }

    // 3. Replace destination atomically
    if (request.destination.existsAsFile())
    {
        if (!request.destination.deleteFile())
        {
            tempZip.deleteFile();
            res.status = SessionIoStatus::CannotWrite;
            res.errorCode = "DESTINATION_REPLACE_FAILED";
            res.message = "Could not replace existing destination file: " + request.destination.getFullPathName();
            return res;
        }
    }

    if (!tempZip.moveFileTo(request.destination))
    {
        tempZip.deleteFile();
        res.status = SessionIoStatus::CannotWrite;
        res.errorCode = "DESTINATION_MOVE_FAILED";
        res.message = "Failed to move temporary container to destination.";
        return res;
    }

    res.status = SessionIoStatus::Success;
    res.message = "Session package saved successfully.";
    return res;
}

SessionLoadResult SessionPersistenceService::load(const SessionLoadRequest& request)
{
    SessionLoadResult res;

    // 1. File existence check
    if (!request.source.existsAsFile())
    {
        res.status = SessionIoStatus::FileNotFound;
        res.errorCode = "FILE_NOT_FOUND";
        res.message = "Session container file does not exist: " + request.source.getFullPathName();
        return res;
    }

    // 2. Read package contents via temporary folder
    juce::ZipFile zip(request.source);
    if (zip.getNumEntries() == 0)
    {
        res.status = SessionIoStatus::InvalidPackage;
        res.errorCode = "EMPTY_OR_CORRUPT_ARCHIVE";
        res.message = "Corrupted or empty .abdlabtest package archive.";
        return res;
    }

    SessionSerializer serializer;
    juce::String serializerError;
    SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> points;

    bool ok = serializer.loadSessionFromPackage(request.source, manifest, points, serializerError);

    if (!ok)
    {
        if (serializerError.containsIgnoreCase("missing session_manifest.json"))
        {
            res.status = SessionIoStatus::InvalidPackage;
            res.errorCode = "MISSING_MANIFEST";
        }
        else if (serializerError.containsIgnoreCase("JSON parsing error"))
        {
            res.status = SessionIoStatus::InvalidJson;
            res.errorCode = "MALFORMED_JSON";
        }
        else if (serializerError.containsIgnoreCase("Failed to extract package archive"))
        {
            res.status = SessionIoStatus::InvalidPackage;
            res.errorCode = "EXTRACT_FAILED";
        }
        else
        {
            res.status = SessionIoStatus::ValidationFailed;
            res.errorCode = "VALIDATION_FAILED";
        }
        res.message = serializerError;
        return res;
    }

    // 3. Schema & version validation
    if (manifest.formatVersion.empty())
    {
        res.status = SessionIoStatus::ValidationFailed;
        res.errorCode = "EMPTY_FORMAT_VERSION";
        res.message = "Manifest formatVersion is missing.";
        return res;
    }

    // Example future version guard (supported versions up to 1.x)
    if (manifest.formatVersion.rfind("1.", 0) != 0)
    {
        res.status = SessionIoStatus::UnsupportedVersion;
        res.errorCode = "UNSUPPORTED_VERSION";
        res.message = "Unsupported session manifest format version: " + juce::String(manifest.formatVersion);
        return res;
    }

    res.status = SessionIoStatus::Success;
    res.manifest = std::move(manifest);
    res.points = std::move(points);
    res.message = "Session package loaded and validated successfully.";
    return res;
}

} // namespace abdaudiolab::core
