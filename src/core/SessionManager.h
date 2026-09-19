/**
 * @file SessionManager.h
 * @brief Manages profiling session state, dirty tracking, auto-save, and package serialization.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SessionSerializer.h"
#include "SessionPersistenceService.h"
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
    void setManifest(const SessionManifest& m) { manifest = m; }

    [[nodiscard]] std::vector<exporting::MeasuredPoint>& getMeasuredPoints() noexcept { return measuredPoints; }
    [[nodiscard]] const std::vector<exporting::MeasuredPoint>& getMeasuredPoints() const noexcept { return measuredPoints; }
    void setMeasuredPoints(const std::vector<exporting::MeasuredPoint>& points) { measuredPoints = points; }
    void clearMeasuredPoints() noexcept { measuredPoints.clear(); }

    void addMeasuredPoint(const exporting::MeasuredPoint& pt)
    {
        measuredPoints.push_back(pt);
        isSessionDirty = true;
    }

    bool patchMeasuredPoint(size_t index, const exporting::MeasuredPoint& pt)
    {
        if (index < measuredPoints.size())
        {
            measuredPoints[index] = pt;
            isSessionDirty = true;
            return true;
        }
        return false;
    }

    void removeMeasuredPoint(size_t index)
    {
        if (index < measuredPoints.size())
        {
            measuredPoints.erase(measuredPoints.begin() + index);
            isSessionDirty = true;
        }
    }

    [[nodiscard]] size_t getPointCount() const noexcept { return measuredPoints.size(); }
    [[nodiscard]] bool hasPoints() const noexcept { return !measuredPoints.empty(); }
    [[nodiscard]] const exporting::MeasuredPoint* getPoint(size_t index) const noexcept
    {
        if (index < measuredPoints.size()) return &measuredPoints[index];
        return nullptr;
    }

    [[nodiscard]] bool isDirty() const noexcept { return isSessionDirty; }
    void setDirty(bool dirty) noexcept { isSessionDirty = dirty; }

    [[nodiscard]] const juce::File& getActiveSessionFile() const noexcept { return serializer.getActiveSessionFile(); }
    void setActiveSessionFile(const juce::File& file) { serializer.setActiveSessionFile(file); }

    [[nodiscard]] SessionSerializer& getSerializer() noexcept { return serializer; }
    [[nodiscard]] const SessionSerializer& getSerializer() const noexcept { return serializer; }

    bool saveSessionToPackage(const juce::File& file);
    bool saveSessionToPackage(const juce::File& file, const SessionManifest& newManifest);
    bool loadSessionFromPackage(const juce::File& file, juce::String& outErrorMessage);

    /**
     * @struct ReanalysisProgress
     * @brief Detailed progress of an offline re-analysis pass.
     */
    struct ReanalysisProgress
    {
        int currentPoint { 0 };
        int totalPoints { 0 };
        std::string currentTestId;
    };
    using ReanalysisCallback = std::function<void(float progress0to1, const ReanalysisProgress& info)>;

    /**
     * @brief Recomputes analytic metrics (mu, sigma, THD, SNR) offline from raw recorded audio.
     * @param progressCb Optional progress reporting callback.
     * @return Number of points successfully re-analyzed.
     */
    int reanalyzeSessionOffline(ReanalysisCallback progressCb = nullptr);

    void triggerAutoSave();
    void triggerAutoSave(const SessionManifest& currentManifest);
    [[nodiscard]] juce::File getRecoverableAutoSaveFile() const { return serializer.getRecoverableAutoSaveFile(); }
    void cleanupTempSession() { serializer.cleanupTempSession(); }

private:
    SessionSerializer serializer;
    SessionManifest manifest;
    std::vector<exporting::MeasuredPoint> measuredPoints;
    bool isSessionDirty { false };
};

} // namespace abdaudiolab::core
