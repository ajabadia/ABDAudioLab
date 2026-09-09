#pragma once

#include "AppTheme.h"
#include "SoundIdTheme.h"
#include "TestConfiguration.h"
#include "MatrixResolutionTableComponent.h"
#include "TestPlanEstimationCardComponent.h"
#include "../audio/LabStimulusGenerator.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @class TestEditorPanel
 * @brief Reusable component for test parameter editing shared between SlideInDrawer and TestConfigModal.
 * Adheres to Sonarworks SoundID Reference Nordic Light precision aesthetic.
 */
class TestEditorPanel : public juce::Component
{
public:
    TestEditorPanel();
    ~TestEditorPanel() override = default;

    void setConfiguration(const TestConfiguration& config);
    [[nodiscard]] const TestConfiguration& getConfiguration() const noexcept { return currentConfig; }

    void setPresetSelectorVisible(bool visible);
    void setPresetOptions(const std::vector<juce::String>& presetNames);
    void populateWithAutoTestPresets();

    std::function<void(int presetIndex)> onPresetSelected;
    std::function<void()> onConfigChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    int getPreferredHeight() const;
    void updateTheme();

private:
    void updateEstimatedTime();

    TestConfiguration currentConfig;

    // Section 1: Presets & Test Name
    juce::Label lblPresetSelector;
    juce::ComboBox comboPresets;

    juce::Label lblTestName;
    juce::TextEditor txtTestName;

    juce::Label lblStimulusType;
    juce::ComboBox comboStimulusType;
    juce::Label lblStimulusDesc;

    // Section 2: Duration & Capture Mode
    juce::Label lblDurationSection;
    juce::ComboBox comboDurationPreset;
    juce::Label lblManualDuration;
    juce::TextEditor txtManualDuration;
    juce::Label lblSecondsUnit;
    juce::ToggleButton btnAdaptiveTail;

    // Section 3: Matrix Resolution
    juce::Label lblMatrixSection;
    MatrixResolutionTableComponent matrixTableComp;

    // Section 4: Estimation Summary Card
    TestPlanEstimationCardComponent estimationCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestEditorPanel)
};

} // namespace abdaudiolab::gui
