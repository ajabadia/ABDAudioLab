#pragma once

#include "../audio/LabStimulusGenerator.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace abdaudiolab::gui
{

enum class ResolutionPreset
{
    Fixed = 1,
    Coarse = 3,
    Standard = 5,
    Detailed = 8,
    Fine = 16,
    UltraFine = 32,
    Extreme = 64,
    Custom = 99
};

struct ControlStepConfig
{
    juce::String id;        // Unique ID per control
    juce::String name;
    juce::String type { "Knob" };
    int steps { 1 };        // 1 = Fixed, 3, 5, 8, 16, 32, 64 or custom count
    float minPct { 0.0f };  // Start % (Default: 0%)
    float maxPct { 100.0f };// End % (Default: 100%)
    int sortOrder { 0 };    // Sort priority weight (0 = highest/first)
    bool isCustom { false }; // Explicitly distinguishes Custom from standard presets
};

struct ProfilingTimingConfig
{
    float secondsPerManualControlAdjustment { 2.0f };
    float secondsPerManualOverhead { 0.5f };
    float secondsPerAutomatedOverhead { 0.05f };
};

struct ProfilingTimeEstimate
{
    int measurementStates { 0 };
    juce::String dimensionalFormula;
    bool isManualProfiling { false };
    int manualControlAdjustmentEvents { 0 };
    float audioCaptureSeconds { 0.0f };
    float estimatedTotalSeconds { 0.0f };
};

struct TestConfiguration
{
    juce::String testName { "Custom Test" };
    audio::StimulusType stimulusType { audio::StimulusType::LogFarinaSweep };
    juce::String captureMode { "FIXED_TIME" }; // "FIXED_TIME", "ADAPTIVE_ENVELOPE"
    float burstDurationSec { 1.0f };
    float maxTimeoutSec { 60.0f };
    float silenceThresholdDb { -60.0f };
    std::vector<ControlStepConfig> controls;

    [[nodiscard]] int getTotalMeasurementPoints() const noexcept
    {
        int total = 1;
        bool hasAny = false;
        for (const auto& c : controls)
        {
            if (c.steps > 1)
            {
                total *= c.steps;
                hasAny = true;
            }
        }
        return hasAny ? total : (controls.empty() ? 1 : controls[0].steps);
    }

    [[nodiscard]] ProfilingTimeEstimate calculateEstimate(bool isManual,
                                                         const ProfilingTimingConfig& timing = {}) const
    {
        ProfilingTimeEstimate est;
        est.isManualProfiling = isManual;

        if (controls.empty())
        {
            est.measurementStates = 1;
            est.dimensionalFormula = "1 state";
            est.audioCaptureSeconds = burstDurationSec;
            est.manualControlAdjustmentEvents = 0;
            est.estimatedTotalSeconds = burstDurationSec + (isManual ? timing.secondsPerManualOverhead : timing.secondsPerAutomatedOverhead);
            return est;
        }

        // 1. Calculate measurement states & dimensional formula
        int totalStates = 1;
        juce::StringArray formulaParts;

        for (const auto& c : controls)
        {
            int s = std::max(1, c.steps);
            totalStates *= s;
            formulaParts.add(juce::String(s) + " " + c.name);
        }

        if (formulaParts.isEmpty())
        {
            if (!controls.empty())
                formulaParts.add(controls[0].name);
            else
                formulaParts.add("1 state");
        }

        est.measurementStates = totalStates;
        est.dimensionalFormula = formulaParts.joinIntoString(" \u00d7 "); // '×' symbol

        est.audioCaptureSeconds = static_cast<float>(totalStates) * burstDurationSec;

        // 2. Calculate manual control adjustment events across consecutive states
        int adjustmentEvents = 0;
        if (isManual && totalStates > 1)
        {
            const size_t numControls = controls.size();
            std::vector<int> prevStepIndices(numControls, 0);

            for (int p = 1; p < totalStates; ++p)
            {
                int temp = p;
                std::vector<int> currStepIndices(numControls, 0);
                for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
                {
                    int sCount = std::max(1, controls[static_cast<size_t>(k)].steps);
                    currStepIndices[static_cast<size_t>(k)] = temp % sCount;
                    temp /= sCount;
                }

                // Count how many controls physically changed between state (p - 1) and state (p)
                for (size_t k = 0; k < numControls; ++k)
                {
                    if (currStepIndices[k] != prevStepIndices[k])
                    {
                        ++adjustmentEvents;
                    }
                }
                prevStepIndices = currStepIndices;
            }
        }

        est.manualControlAdjustmentEvents = isManual ? adjustmentEvents : 0;

        if (isManual)
        {
            est.estimatedTotalSeconds = est.audioCaptureSeconds
                                      + (static_cast<float>(est.manualControlAdjustmentEvents) * timing.secondsPerManualControlAdjustment)
                                      + (static_cast<float>(totalStates) * timing.secondsPerManualOverhead);
        }
        else
        {
            est.estimatedTotalSeconds = est.audioCaptureSeconds
                                      + (static_cast<float>(totalStates) * timing.secondsPerAutomatedOverhead);
        }

        return est;
    }
};

} // namespace abdaudiolab::gui
