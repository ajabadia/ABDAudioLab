/**
 * @file TestEditorPanel.h
 * @brief Reusable test parameter configuration panel (stimulus, duration, and matrix controls).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SoundIdTheme.h"
#include "ControlIcon.h"
#include "TestConfiguration.h"
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

    std::function<void(int presetIndex)> onPresetSelected;
    std::function<void()> onConfigChanged;

    void resized() override;
    int getPreferredHeight() const;

private:
    void rebuildControlRows();
    void updateEstimatedTime();

    TestConfiguration currentConfig;

    juce::Label lblPresetSelector;
    juce::ComboBox comboPresets;

    juce::Label lblTestName;
    juce::TextEditor txtTestName;

    juce::Label lblStimulusType;
    juce::ComboBox comboStimulusType;
    juce::Label lblStimulusDesc;

    juce::Label lblDurationSection;
    juce::ComboBox comboDurationPreset;
    juce::Label lblManualDuration;
    juce::TextEditor txtManualDuration;
    juce::Label lblSecondsUnit;
    juce::ToggleButton btnAdaptiveTail;

    juce::Label lblMatrixSection;
    juce::Label lblHeaderParam;
    juce::Label lblHeaderResolution;
    juce::Label lblHeaderCustom;
    juce::Label lblHeaderMin;
    juce::Label lblHeaderMax;
    juce::Label lblHeaderOrder;

    struct RowWidgets
    {
        std::unique_ptr<ControlIconComponent> icon;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::TextButton> btnUp;
        std::unique_ptr<juce::TextButton> btnDown;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextEditor> txtCustomSteps;
        std::unique_ptr<juce::TextEditor> txtMinPct;
        std::unique_ptr<juce::TextEditor> txtMaxPct;
    };
    std::vector<RowWidgets> rowWidgets;

    juce::Label lblTestSummary;
};

} // namespace abdaudiolab::gui
