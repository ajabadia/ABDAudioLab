/**
 * @file TestEditorPanel.h
 * @brief Reusable test parameter configuration panel (stimulus, duration, and matrix controls).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "AppTheme.h"
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
 * Adheres to Sonarworks SoundID Reference Nordic Light precision aesthetic.
 */
class TestEditorPanel : public juce::Component,
                        public juce::TableListBoxModel
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

    void paint(juce::Graphics& g) override;
    void resized() override;
    int getPreferredHeight() const;

    // juce::TableListBoxModel methods
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

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
    juce::TableListBox matrixTable;

    // Section 4: Estimation Summary Card
    class EstimationCardComponent : public juce::Component
    {
    public:
        EstimationCardComponent();
        void paint(juce::Graphics& g) override;
        void setEstimation(int totalPoints, float totalSeconds);
    private:
        int points { 0 };
        float seconds { 0.0f };
    };
    EstimationCardComponent estimationCard;

    // Helper inner component for row order buttons
    class OrderButtonsComponent : public juce::Component
    {
    public:
        OrderButtonsComponent();
        void resized() override;
        juce::TextButton btnUp { juce::String::fromUTF8(u8"▲") };
        juce::TextButton btnDown { juce::String::fromUTF8(u8"▼") };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestEditorPanel)
};

} // namespace abdaudiolab::gui
