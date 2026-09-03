/**
 * @file SessionManager.h
 * @brief Manages profiling session state, dirty tracking, auto-save, and package serialization.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SessionSerializer.h"
#include "ProfilingSession.h"
#include "../export/LutExporter.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace abdaudiolab::core
{

/**
 * @class SessionManager
 * @brief High-level manager for profiling session state, package save/load operations, and auto-save.
 */
class SessionManager
{
public:
    SessionManager();
    ~SessionManager() = default;

    void resetSession();

    [[nodiscard]] SessionManifest& getManifest() noexcept { return manifest; }
    [[nodiscard]] const SessionManifest& getManifest() const noexcept { return manifest; }

    [[nodiscard]] std::vector<exporting::MeasuredPoint>& getMeasuredPoints() noexcept { return measuredPoints; }
    [[nodiscard]] const std::vector<exporting::MeasuredPoint>& getMeasuredPoints() const noexcept { return measuredPoints; }

    [[nodiscard]] bool isDirty() const noexcept { return isSessionDirty; }
    void setDirty(bool dirty) noexcept { isSessionDirty = dirty; }

    [[nodiscard]] const juce::File& getActiveSessionFile() const noexcept { return serializer.getActiveSessionFile(); }
    void setActiveSessionFile(const juce::File& file) { serializer.setActiveSessionFile(file); }

    bool saveSessionToPackage(const juce::File& file);
    bool loadSessionFromPackage(const juce::File& file, juce::String& outErrorMessage);

    void triggerAutoSave();
    [[nodiscard]] juce::File getRecoverableAutoSaveFile() const { return serializer.getRecoverableAutoSaveFile(); }

private:
    SessionSerializer serializer;
    SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> measuredPoints;
    bool isSessionDirty { false };
};

} // namespace abdaudiolab::core
