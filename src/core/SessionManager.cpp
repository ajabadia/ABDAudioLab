#include "SessionManager.h"

namespace abdaudiolab::core
{

SessionManager::SessionManager()
{
    resetSession();
}

void SessionManager::resetSession()
{
    manifest = SessionManifest();
    manifest.sessionTitle = "New Profiling Session";
    manifest.hardwareId = "AIRA_S1";
    manifest.hardwareName = "AIRA Compact S-1 Tweak Synth";
    manifest.activeFunctionId = "FILTER_RESONANCE_SWEEP";
    manifest.activeFunctionName = "Resonant Lowpass Filter Sweep";
    manifest.targetModule = "FLT_BLOCK";

    measuredPoints.clear();
    serializer.setActiveSessionFile(juce::File());
    isSessionDirty = false;
}

bool SessionManager::saveSessionToPackage(const juce::File& file)
{
    manifest.totalMeasuredPoints = static_cast<int>(measuredPoints.size());
    manifest.totalPointsMeasured = manifest.totalMeasuredPoints;

    bool ok = serializer.saveSessionToPackage(file, manifest, measuredPoints);
    if (ok)
    {
        serializer.setActiveSessionFile(file);
        isSessionDirty = false;
    }
    return ok;
}

bool SessionManager::loadSessionFromPackage(const juce::File& file, juce::String& outErrorMessage)
{
    SessionManifest loadedManifest;
    std::vector<exporting::MeasuredPoint> loadedPoints;

    bool ok = serializer.loadSessionFromPackage(file, loadedManifest, loadedPoints, outErrorMessage);
    if (ok)
    {
        manifest = loadedManifest;
        measuredPoints = loadedPoints;
        serializer.setActiveSessionFile(file);
        isSessionDirty = false;
    }
    return ok;
}

void SessionManager::triggerAutoSave()
{
    manifest.totalMeasuredPoints = static_cast<int>(measuredPoints.size());
    manifest.totalPointsMeasured = manifest.totalMeasuredPoints;
    serializer.triggerIncrementalAutoSave(manifest, measuredPoints);
}

} // namespace abdaudiolab::core
