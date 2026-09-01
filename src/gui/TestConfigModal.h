#pragma once

#include "SoundIdTheme.h"
#include "../audio/LabStimulusGenerator.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace abdaudiolab::gui
{

struct ControlStepConfig
{
    juce::String name;
    juce::String type { "Knob" };
    int steps { 1 }; // 1 = Fixed, 3, 5, 8, 16, 32, 64
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

/**
 * @brief Spacious Nordic light-themed modal for advanced test configuration & dynamic control step matrix.
 */
class TestConfigModal : public juce::Component
{
public:
    TestConfigModal();
    ~TestConfigModal() override = default;

    void showDialog(juce::Component* parent, const TestConfiguration& initialConfig);
    void dismissDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

    [[nodiscard]] const TestConfiguration& getConfiguration() const noexcept { return currentConfig; }

    std::function<void(const TestConfiguration&)> onConfigurationConfirmed;

private:
    void rebuildControlRows();
    void updateEstimatedTime();

    TestConfiguration currentConfig;

    juce::Component panel;
    juce::TextButton btnClose { "X" };

    // Section 1: Test Identity & Stimulus
    juce::Label lblSectionStimulus;
    juce::Label lblTestName;
    juce::TextEditor txtTestName;
    juce::Label lblStimulusType;
    juce::ComboBox comboStimulusType;
    juce::Label lblStimulusDesc;

    // Section 2: Burst Duration & Manual Seconds
    juce::Label lblSectionDuration;
    juce::ComboBox comboDurationPreset;
    juce::Label lblManualDuration;
    juce::TextEditor txtManualDuration;
    juce::Label lblSecondsUnit;
    juce::ToggleButton btnAdaptiveTail { "Adaptive Auto-Tail Silence Cutoff (for ADSR / Reverb)" };

    // Section 3: Dynamic Matrix Grid Controls
    juce::Label lblSectionMatrix;
    
    struct ControlRowWidgets
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextEditor> txtCustomSteps;
    };
    std::vector<ControlRowWidgets> controlRowWidgets;

    // Summary & Actions
    juce::Label lblSummaryBanner;
    juce::TextButton btnCancel { "Cancel" };
    juce::TextButton btnApply { "Apply Configuration" };

    bool isUpdatingFromPreset { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestConfigModal)
};

} // namespace abdaudiolab::gui
