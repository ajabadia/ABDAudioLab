#pragma once

#include "../audio/LabStimulusGenerator.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace abdaudiolab::gui
{

struct ControlStepConfig
{
    juce::String id;        // Unique ID per control
    juce::String name;
    juce::String type { "Knob" };
    int steps { 1 };        // 1 = Fixed, 3, 5, 8, 16, 32, 64
    float minPct { 0.0f };  // Start % (Default: 0%)
    float maxPct { 100.0f };// End % (Default: 100%)
    int sortOrder { 0 };    // Sort priority weight (0 = highest/first)
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
};

} // namespace abdaudiolab::gui
