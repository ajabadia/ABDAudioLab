#include "SessionManager.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <cmath>

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

    SessionSaveRequest req;
    req.manifest = manifest;
    req.points = measuredPoints;
    req.destination = file;

    auto result = SessionPersistenceService::save(req);
    if (result.succeeded())
    {
        serializer.setActiveSessionFile(file);
        isSessionDirty = false;
        return true;
    }
    return false;
}

bool SessionManager::saveSessionToPackage(const juce::File& file, const SessionManifest& newManifest)
{
    manifest = newManifest;
    return saveSessionToPackage(file);
}

bool SessionManager::loadSessionFromPackage(const juce::File& file, juce::String& outErrorMessage)
{
    SessionLoadRequest req;
    req.source = file;

    auto result = SessionPersistenceService::load(req);
    if (result.succeeded())
    {
        manifest = std::move(result.manifest);
        measuredPoints = std::move(result.points);
        serializer.setActiveSessionFile(file);
        isSessionDirty = false;
        return true;
    }

    outErrorMessage = result.message;
    return false;
}

void SessionManager::triggerAutoSave()
{
    manifest.totalMeasuredPoints = static_cast<int>(measuredPoints.size());
    manifest.totalPointsMeasured = manifest.totalMeasuredPoints;
    serializer.triggerIncrementalAutoSave(manifest, measuredPoints);
}

void SessionManager::triggerAutoSave(const SessionManifest& currentManifest)
{
    manifest = currentManifest;
    manifest.totalMeasuredPoints = static_cast<int>(measuredPoints.size());
    manifest.totalPointsMeasured = manifest.totalMeasuredPoints;
    serializer.triggerIncrementalAutoSave(manifest, measuredPoints);
}

int SessionManager::reanalyzeSessionOffline(ReanalysisCallback progressCb)
{
    if (measuredPoints.empty()) return 0;

    auto rawDir = serializer.getWorkingTempDirectory().getChildFile("raw_audio");
    double sampleRate = manifest.sampleRate > 8000.0 ? manifest.sampleRate : 96000.0;
    int reanalyzedCount = 0;
    int totalCount = static_cast<int>(measuredPoints.size());

    // Map test ID to test configuration for duration / frequency limits
    std::unordered_map<std::string, const gui::TestConfiguration*> testConfigMap;
    for (const auto& t : manifest.tests)
    {
        testConfigMap[t.testName.toStdString()] = &t;
    }

    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();

    for (int idx = 0; idx < totalCount; ++idx)
    {
        auto& pt = measuredPoints[static_cast<size_t>(idx)];

        if (progressCb)
        {
            ReanalysisProgress pInfo;
            pInfo.currentPoint = idx + 1;
            pInfo.totalPoints = totalCount;
            pInfo.currentTestId = pt.testId;
            float prog = static_cast<float>(idx) / static_cast<float>(totalCount);
            progressCb(prog, pInfo);
        }

        // 1. Gather recorded audio passes (from memory pt.irSamples or disk WAV)
        std::vector<std::vector<float>> recordedPasses;

        if (rawDir.isDirectory())
        {
            juce::String cleanTestId = juce::File::createLegalFileName(pt.testId);
            auto passFiles = rawDir.findChildFiles(juce::File::findFiles, false, "*" + cleanTestId + "*pass*.wav");
            std::sort(passFiles.begin(), passFiles.end(), [](const juce::File& a, const juce::File& b) {
                return a.getFileName() < b.getFileName();
            });

            for (const auto& wavFile : passFiles)
            {
                std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(wavFile));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    int samplesToRead = static_cast<int>(reader->lengthInSamples);
                    std::vector<float> passData(static_cast<size_t>(samplesToRead));
                    juce::AudioBuffer<float> tempBuf(1, samplesToRead);
                    reader->read(tempBuf.getArrayOfWritePointers(), 1, 0, samplesToRead);
                    std::memcpy(passData.data(), tempBuf.getReadPointer(0), sizeof(float) * static_cast<size_t>(samplesToRead));
                    recordedPasses.push_back(std::move(passData));
                }
            }
        }

        // Fallback: If no raw WAVs found, use existing irSamples from memory
        if (recordedPasses.empty() && !pt.irSamples.empty())
        {
            recordedPasses.push_back(pt.irSamples);
        }

        if (recordedPasses.empty()) continue;

        // 2. Derive test parameters
        double stimDuration = 2.0;
        float startFreq = 20.0f;
        float endFreq = 20000.0f;

        auto confIt = testConfigMap.find(pt.testId);
        if (confIt != testConfigMap.end() && confIt->second != nullptr)
        {
            if (confIt->second->burstDurationSec > 0.05f)
                stimDuration = confIt->second->burstDurationSec;
        }

        // 3. Dispatch to LabAnalyticEngine based on blockType
        if (pt.blockType == "NoiseFloor" || pt.stimulusType == "Silence")
        {
            float noiseRms = 0.0f;
            if (!recordedPasses[0].empty())
            {
                double sumSq = 0.0;
                for (float s : recordedPasses[0]) sumSq += static_cast<double>(s * s);
                noiseRms = static_cast<float>(std::sqrt(sumSq / recordedPasses[0].size()));
            }
            float noiseDb = (noiseRms > 1e-6f) ? (20.0f * std::log10(noiseRms)) : -96.0f;
            pt.muSigmaValue = { noiseDb, 0.2f };
            pt.secondaryValue = { -noiseDb, 0.2f };
            pt.thdValue = { 0.0f, 0.0f };
            pt.thdPercent = 0.0f;
        }
        else if (pt.blockType == "SpectrumFilter")
        {
            auto invFilter = math::FarinaDeconvolver::generateInverseFilter(sampleRate, stimDuration, startFreq, endFreq);
            auto filterRes = math::LabAnalyticEngine::analyzeFilterPasses(recordedPasses, invFilter, sampleRate, stimDuration, startFreq, endFreq);
            pt.muSigmaValue = filterRes.cutoffHz;
            pt.secondaryValue = filterRes.resonanceDb;
            pt.thdValue = filterRes.thdPercent;
            pt.thdPercent = filterRes.thdPercent.mean;
        }
        else if (pt.blockType == "TimeDynamic")
        {
            auto timeRes = math::LabAnalyticEngine::analyzeAdsrEnvelopes(recordedPasses, sampleRate);
            pt.muSigmaValue = timeRes.attackTimeMs;
            pt.secondaryValue = timeRes.sustainLevel;
            pt.thdValue = { 0.0f, 0.0f };
            pt.thdPercent = 0.0f;
        }
        else if (pt.blockType == "WaveShaper")
        {
            if (pt.stimulusType == "LogFarinaSweep")
            {
                auto invFilter = math::FarinaDeconvolver::generateInverseFilter(sampleRate, stimDuration, startFreq, endFreq);
                auto filterRes = math::LabAnalyticEngine::analyzeFilterPasses(recordedPasses, invFilter, sampleRate, stimDuration, startFreq, endFreq);
                pt.muSigmaValue = filterRes.cutoffHz;
                pt.secondaryValue = filterRes.resonanceDb;
                pt.thdValue = filterRes.thdPercent;
                pt.thdPercent = filterRes.thdPercent.mean;
            }
            else
            {
                auto wsRes = math::LabAnalyticEngine::analyzeWaveShaperRamps(recordedPasses, sampleRate);
                pt.muSigmaValue = wsRes.thdPercent;
                pt.secondaryValue = { 1.0f, 0.0f };
                pt.thdValue = wsRes.thdPercent;
                pt.thdPercent = wsRes.thdPercent.mean;
            }
        }
        else if (pt.blockType == "CyclicModulator")
        {
            auto modRes = math::LabAnalyticEngine::analyzeCyclicModulator(recordedPasses, sampleRate);
            pt.muSigmaValue = modRes.rateHz;
            pt.secondaryValue = modRes.depthPercent;
            pt.thdValue = modRes.asymmetry;
            pt.thdPercent = modRes.asymmetry.mean;
        }
        else if (pt.blockType == "WienerHammerstein")
        {
            auto inputStimulus = math::FarinaDeconvolver::generateLogFarinaSweep(sampleRate, stimDuration, 20.0f, 20000.0f);
            auto whRes = math::LabAnalyticEngine::analyzeWienerHammerstein(recordedPasses, inputStimulus, sampleRate);
            pt.muSigmaValue = whRes.nonLinearCoeffA;
            pt.secondaryValue = whRes.postFilterCentroidHz;
            pt.thdValue = { 1.0f - whRes.goodnessOfFitR2.mean, whRes.goodnessOfFitR2.stdDev };
            pt.thdPercent = pt.thdValue.mean;
            pt.irSamples = whRes.representativeH1;
        }
        else
        {
            auto gainRes = math::LabAnalyticEngine::analyzeGainTones(recordedPasses, sampleRate);
            pt.muSigmaValue = gainRes.gainDb;
            pt.secondaryValue = gainRes.snrDb;
            pt.thdValue = { 0.0f, 0.0f };
            pt.thdPercent = 0.0f;
        }

        // Recompute SNR
        if (!recordedPasses.empty() && !recordedPasses[0].empty())
        {
            pt.snrDb = math::LabAnalyticEngine::calculateSignalToNoiseRatioDb(recordedPasses[0], -90.0f);
        }

        reanalyzedCount++;
    }

    if (reanalyzedCount > 0)
    {
        isSessionDirty = true;
    }

    if (progressCb)
    {
        ReanalysisProgress pDone;
        pDone.currentPoint = totalCount;
        pDone.totalPoints = totalCount;
        pDone.currentTestId = "Complete";
        progressCb(1.0f, pDone);
    }

    return reanalyzedCount;
}

} // namespace abdaudiolab::core

