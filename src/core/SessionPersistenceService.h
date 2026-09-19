/**
 * @file SessionPersistenceService.h
 * @brief Pure decoupled persistence and validation service for .abdlabtest package containers.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SessionSerializer.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::core
{

/**
 * @brief Strongly-typed error classification for session container I/O and validation.
 */
enum class SessionIoStatus
{
    Success,
    FileNotFound,
    CannotRead,
    CannotWrite,
    InvalidPackage,
    InvalidJson,
    UnsupportedVersion,
    ValidationFailed
};

[[nodiscard]] inline std::string sessionIoStatusToString(SessionIoStatus s) noexcept
{
    switch (s)
    {
        case SessionIoStatus::Success:            return "Success";
        case SessionIoStatus::FileNotFound:       return "FileNotFound";
        case SessionIoStatus::CannotRead:          return "CannotRead";
        case SessionIoStatus::CannotWrite:         return "CannotWrite";
        case SessionIoStatus::InvalidPackage:      return "InvalidPackage";
        case SessionIoStatus::InvalidJson:         return "InvalidJson";
        case SessionIoStatus::UnsupportedVersion:  return "UnsupportedVersion";
        case SessionIoStatus::ValidationFailed:    return "ValidationFailed";
    }
    return "Unknown";
}

/**
 * @brief Request object containing complete session data to persist atomically.
 */
struct SessionSaveRequest
{
    SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> points;
    juce::File destination;
};

/**
 * @brief Request object specifying the source package container to load.
 */
struct SessionLoadRequest
{
    juce::File source;
};

/**
 * @brief Result object returned by session save operations.
 */
struct SessionIoResult
{
    SessionIoStatus status { SessionIoStatus::ValidationFailed };
    juce::String errorCode;
    juce::String message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == SessionIoStatus::Success;
    }
};

/**
 * @brief Result object returned by session load operations.
 */
struct SessionLoadResult
{
    SessionIoStatus status { SessionIoStatus::ValidationFailed };
    SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> points;
    juce::String errorCode;
    juce::String message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == SessionIoStatus::Success;
    }
};

/**
 * @brief Pure domain persistence service for reading, writing, validating, and ensuring
 * atomic integrity of .abdlabtest package containers without UI or manager dependencies.
 */
class SessionPersistenceService
{
public:
    [[nodiscard]] static SessionIoResult save(const SessionSaveRequest& request);
    [[nodiscard]] static SessionLoadResult load(const SessionLoadRequest& request);
};

} // namespace abdaudiolab::core
