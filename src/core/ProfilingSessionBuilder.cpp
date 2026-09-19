/**
 * @file ProfilingSessionBuilder.cpp
 * @brief Implementation of pure decoupled builder for ProfilingSession and TestCase cartesian orchestration.
 * @author ABDSynths
 * @date 2026
 */

#include "ProfilingSessionBuilder.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::core
{

std::string ProfilingSessionBuilder::mapBadgeToBlockType(const juce::String& badgeText)
{
    if (badgeText == "FLT") return "SpectrumFilter";
    if (badgeText == "ENV") return "TimeDynamic";
    if (badgeText == "SAT") return "WaveShaper";
    if (badgeText == "MOD") return "CyclicModulator";
    if (badgeText == "WNH") return "WienerHammerstein";
    if (badgeText == "NAM") return "NeuralCalibration";
    return "AmplitudeGain";
}

static void populateMetadata(ProfilingSession& profSession, const SessionMetadataConfig& config)
{
    ProfilingMetadata meta;
    meta.hardwareName = config.hardwareName;
    meta.targetModule = config.targetModule;
    meta.operatorMode = config.operatorMode;
    meta.sampleRate = config.sampleRate;
    meta.bitDepth = config.bitDepth;
    meta.timestamp = config.timestampIso8601;
    meta.operatorNotes = config.operatorNotes;
    meta.ambientTemperatureC = config.ambientTemperatureC;
    meta.warmupTimeMinutes = config.warmupTimeMinutes;
    profSession.setMetadata(meta);
}

static MeasurementPresetRecipe resolveRecipe(const gui::QueueItem& item, const HardwareContractSnapshot& hardware)
{
    MeasurementPresetRecipe itemRecipe;
    const HardwareContract* itemContract = nullptr;

    if (item.hwId.isNotEmpty())
    {
        for (const auto& c : hardware.contracts)
        {
            if (c.id == item.hwId.toStdString())
            {
                itemContract = &c;
                break;
            }
        }
    }

    if (itemContract == nullptr && !hardware.contracts.empty())
    {
        itemContract = &hardware.contracts.front();
    }

    if (itemContract != nullptr)
    {
        for (const auto& fn : itemContract->functions)
        {
            if (fn.id == item.funcId.toStdString() || fn.name == item.title.toStdString())
            {
                itemRecipe = fn.measurementRecipe;
                break;
            }
        }
    }

    return itemRecipe;
}

BuildResult ProfilingSessionBuilder::buildFromQueue(
    const std::vector<gui::QueueItem>& queue,
    const HardwareContractSnapshot& hardware,
    const SessionMetadataConfig& config)
{
    BuildResult res;

    if (config.sampleRate <= 0.0)
    {
        res.status = BuildStatus::InvalidSampleRate;
        res.errorCode = "INVALID_SAMPLE_RATE";
        res.message = "Sample rate must be strictly positive.";
        return res;
    }

    if (queue.empty())
    {
        res.status = BuildStatus::EmptyQueue;
        res.errorCode = "EMPTY_QUEUE";
        res.message = "Test queue is empty.";
        return res;
    }

    populateMetadata(res.session, config);

    int globalPointCounter = 0;

    for (int qIdx = 0; qIdx < static_cast<int>(queue.size()); ++qIdx)
    {
        const auto& item = queue[static_cast<size_t>(qIdx)];
        if (item.isSkipped) continue;

        if (item.stimulusType == audio::StimulusType::Silence)
        {
            TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = 1;
            tc.totalPointsInTest = 1;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = "NoiseFloor";
            tc.stimulusType = audio::StimulusType::Silence;
            tc.stimulusDurationSec = (item.burstDurationSec > 0.1f) ? item.burstDurationSec : 0.8;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            res.session.addTestCase(tc);
            continue;
        }

        size_t numControls = item.controls.size();
        MeasurementPresetRecipe itemRecipe = resolveRecipe(item, hardware);

        if (numControls == 0)
        {
            TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = 1;
            tc.totalPointsInTest = 1;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            ExcitationMode excMode = itemRecipe.excitationMode;
            if (hardware.isAutonomousSynth) excMode = ExcitationMode::MidiNotes;
            tc.excitationMode = excMode;
            tc.isAutonomousSynth = (excMode == ExcitationMode::MidiNotes);
            tc.stimulusType = (excMode == ExcitationMode::MidiNotes) ? audio::StimulusType::Silence : item.stimulusType;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            res.session.addTestCase(tc);
            continue;
        }

        std::vector<int> stepsPerControl(numControls);
        std::vector<std::string> controlNames(numControls);
        std::vector<std::string> controlTypes(numControls);
        std::vector<float> minNorms(numControls);
        std::vector<float> maxNorms(numControls);

        int totalTestPoints = 1;
        for (size_t k = 0; k < numControls; ++k)
        {
            const auto& c = item.controls[k];
            stepsPerControl[k] = std::max(1, c.steps);
            controlNames[k] = c.name.toStdString();
            controlTypes[k] = c.type.isEmpty() ? "Knob" : c.type.toStdString();
            minNorms[k] = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
            maxNorms[k] = std::clamp(c.maxPct / 100.0f, minNorms[k], 1.0f);
            totalTestPoints *= stepsPerControl[k];
        }

        for (int p = 0; p < totalTestPoints; ++p)
        {
            int temp = p;
            std::vector<int> stepIndices(numControls);
            for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
            {
                stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
                temp /= stepsPerControl[static_cast<size_t>(k)];
            }

            TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = p + 1;
            tc.totalPointsInTest = totalTestPoints;
            tc.globalPointIndex = globalPointCounter++;
            tc.pointId = "P_" + juce::String::formatted("%03d", tc.globalPointIndex + 1).toStdString();
            tc.testId = item.title.toStdString();

            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            ExcitationMode excMode = itemRecipe.excitationMode;
            if (hardware.isAutonomousSynth) excMode = ExcitationMode::MidiNotes;
            tc.excitationMode = excMode;
            tc.isAutonomousSynth = (excMode == ExcitationMode::MidiNotes);
            tc.stimulusType = (excMode == ExcitationMode::MidiNotes) ? audio::StimulusType::Silence : item.stimulusType;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;

            for (size_t k = 0; k < numControls; ++k)
            {
                int stepIdx = stepIndices[k];
                int sCount = stepsPerControl[k];
                float minN = minNorms[k];
                float maxN = maxNorms[k];

                float normVal = (sCount > 1)
                    ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                    : (minN + maxN) * 0.5f;

                int rawVal = static_cast<int>(std::round(normVal * 127.0f));

                ParameterStep ps;
                ps.paramIndex = static_cast<int>(k) + 1;
                ps.paramName = controlNames[k];
                ps.controlType = controlTypes[k];
                ps.minNormalized = minN;
                ps.maxNormalized = maxN;
                ps.normalizedValue = normVal;
                ps.rawValue = rawVal;
                ps.id = item.controls[k].id.isNotEmpty() ? item.controls[k].id.toStdString() : ("ctrl_" + std::to_string(k + 1));
                ps.sortOrder = item.controls[k].sortOrder;
                tc.parameterSteps.push_back(ps);
            }

            res.session.addTestCase(tc);
        }
    }

    res.status = BuildStatus::Success;
    res.message = "Profiling session successfully constructed.";
    return res;
}

BuildResult ProfilingSessionBuilder::buildPatch(
    const std::vector<PatchPoint>& pointsToPatch,
    const std::vector<gui::QueueItem>& queue,
    const HardwareContractSnapshot& hardware,
    const SessionMetadataConfig& config)
{
    BuildResult res;

    if (config.sampleRate <= 0.0)
    {
        res.status = BuildStatus::InvalidSampleRate;
        res.errorCode = "INVALID_SAMPLE_RATE";
        res.message = "Sample rate must be strictly positive.";
        return res;
    }

    if (pointsToPatch.empty())
    {
        res.status = BuildStatus::InvalidPatch;
        res.errorCode = "EMPTY_PATCH_POINTS";
        res.message = "Points to patch list is empty.";
        return res;
    }

    populateMetadata(res.session, config);
    res.session.setIsPatchSession(true);

    int validPointsAdded = 0;

    for (const auto& target : pointsToPatch)
    {
        int qIdx = target.testIndex;
        int pIdx = target.pointIndex;

        if (qIdx < 0 || qIdx >= static_cast<int>(queue.size()))
            continue;

        const auto& item = queue[static_cast<size_t>(qIdx)];
        if (pIdx < 0 || pIdx >= item.totalPoints)
            continue;

        // Calculate global point index in full session
        int sessionOffset = 0;
        for (int k = 0; k < qIdx; ++k)
            sessionOffset += queue[static_cast<size_t>(k)].totalPoints;
        int globalPointIdx = sessionOffset + pIdx;

        if (item.stimulusType == audio::StimulusType::Silence)
        {
            TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = pIdx + 1;
            tc.totalPointsInTest = item.totalPoints;
            tc.globalPointIndex = globalPointIdx;
            tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = "NoiseFloor";
            tc.stimulusType = audio::StimulusType::Silence;
            tc.stimulusDurationSec = (item.burstDurationSec > 0.1f) ? item.burstDurationSec : 0.8;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            res.session.addTestCase(tc);
            validPointsAdded++;
            continue;
        }

        size_t numControls = item.controls.size();
        MeasurementPresetRecipe itemRecipe = resolveRecipe(item, hardware);

        if (numControls == 0)
        {
            TestCase tc;
            tc.queueItemIndex = qIdx;
            tc.pointIndexInTest = pIdx + 1;
            tc.totalPointsInTest = item.totalPoints;
            tc.globalPointIndex = globalPointIdx;
            tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
            tc.testId = item.title.toStdString();
            tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
            tc.presetRecipe = itemRecipe;
            tc.stimulusType = hardware.isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
            tc.isAutonomousSynth = hardware.isAutonomousSynth;
            tc.midiNoteNumber = 60;
            tc.midiVelocity = 0.8f;
            tc.noteGateDurationSec = item.burstDurationSec;
            tc.stimulusDurationSec = item.burstDurationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 1;
            tc.stabilizationWaitMs = 50.0;
            res.session.addTestCase(tc);
            validPointsAdded++;
            continue;
        }

        std::vector<int> stepsPerControl(numControls);
        std::vector<std::string> controlNames(numControls);
        std::vector<std::string> controlTypes(numControls);
        std::vector<float> minNorms(numControls);
        std::vector<float> maxNorms(numControls);

        int totalTestPoints = 1;
        for (size_t k = 0; k < numControls; ++k)
        {
            const auto& c = item.controls[k];
            stepsPerControl[k] = std::max(1, c.steps);
            controlNames[k] = c.name.toStdString();
            controlTypes[k] = c.type.isEmpty() ? "Knob" : c.type.toStdString();
            minNorms[k] = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
            maxNorms[k] = std::clamp(c.maxPct / 100.0f, minNorms[k], 1.0f);
            totalTestPoints *= stepsPerControl[k];
        }

        int temp = pIdx;
        std::vector<int> stepIndices(numControls);
        for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
        {
            stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
            temp /= stepsPerControl[static_cast<size_t>(k)];
        }

        TestCase tc;
        tc.queueItemIndex = qIdx;
        tc.pointIndexInTest = pIdx + 1;
        tc.totalPointsInTest = totalTestPoints;
        tc.globalPointIndex = globalPointIdx;
        tc.pointId = "P_" + juce::String::formatted("%03d", globalPointIdx + 1).toStdString();
        tc.testId = item.title.toStdString();
        tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
        tc.presetRecipe = itemRecipe;
        tc.stimulusType = hardware.isAutonomousSynth ? audio::StimulusType::Silence : item.stimulusType;
        tc.isAutonomousSynth = hardware.isAutonomousSynth;
        tc.midiNoteNumber = 60;
        tc.midiVelocity = 0.8f;
        tc.noteGateDurationSec = item.burstDurationSec;
        tc.stimulusDurationSec = item.burstDurationSec;
        tc.startFreqHz = 20.0f;
        tc.endFreqHz = 20000.0f;
        tc.numPasses = 1;
        tc.stabilizationWaitMs = 50.0;

        for (size_t k = 0; k < numControls; ++k)
        {
            int stepIdx = stepIndices[k];
            int sCount = stepsPerControl[k];
            float minN = minNorms[k];
            float maxN = maxNorms[k];

            float normVal = (sCount > 1)
                ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                : (minN + maxN) * 0.5f;

            int rawVal = static_cast<int>(std::round(normVal * 127.0f));

            ParameterStep ps;
            ps.paramIndex = static_cast<int>(k) + 1;
            ps.paramName = controlNames[k];
            ps.controlType = controlTypes[k];
            ps.minNormalized = minN;
            ps.maxNormalized = maxN;
            ps.normalizedValue = normVal;
            ps.rawValue = rawVal;
            ps.id = item.controls[k].id.isNotEmpty() ? item.controls[k].id.toStdString() : ("ctrl_" + std::to_string(k + 1));
            ps.sortOrder = item.controls[k].sortOrder;
            tc.parameterSteps.push_back(ps);
        }

        res.session.addTestCase(tc);
        validPointsAdded++;
    }

    if (validPointsAdded == 0)
    {
        res.status = BuildStatus::InvalidPatch;
        res.errorCode = "NO_VALID_PATCH_POINTS";
        res.message = "None of the requested patch coordinates matched valid tests or points.";
        return res;
    }

    res.status = BuildStatus::Success;
    res.message = "Patch session successfully constructed.";
    return res;
}

} // namespace abdaudiolab::core
