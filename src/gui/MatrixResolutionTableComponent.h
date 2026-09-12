#pragma once

#include "TestConfiguration.h"
#include "AppTheme.h"
#include "SoundIdTheme.h"
#include "ControlIcon.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <optional>

namespace abdaudiolab::gui
{

/**
 * @class MatrixResolutionTableComponent
 * @brief Standalone table component that renders and manages measurement matrix resolution,
 * step counts, min/max percentages, and execution order for physical controls.
 */
class MatrixResolutionTableComponent : public juce::Component,
                                       public juce::TableListBoxModel
{
public:
    MatrixResolutionTableComponent();
    ~MatrixResolutionTableComponent() override = default;

    void setControls(const std::vector<ControlStepConfig>& controls);
    [[nodiscard]] const std::vector<ControlStepConfig>& getControls() const noexcept { return controlList; }

    /** Set the catalogue of available parameters the user can pick from (plugin mode).
     *  When non-empty, an "Add Parameter" button appears below the table. */
    void setAvailableParams(const std::vector<ControlStepConfig>& availableParams);
    void clearAvailableParams();
    [[nodiscard]] bool hasAvailableParams() const noexcept { return !availableParamsList.empty(); }

    void updateTheme();
    int getPreferredHeight() const;

    std::function<void()> onControlsChanged;

    void resized() override;

    // juce::TableListBoxModel methods
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void toggleAdvancedRow(int rowIndex);
    [[nodiscard]] int getActiveAdvancedRow() const noexcept { return activeAdvancedRow; }

private:
    void showAddParamMenu();
    void removeRow(int rowIndex);
    void moveRow(int fromIndex, int toIndex);

    std::vector<ControlStepConfig> controlList;
    std::vector<ControlStepConfig> availableParamsList;
    juce::TableListBox matrixTable;
    int activeAdvancedRow { -1 };

    // Dedicated contextual Advanced Settings panel below the compact table
    class AdvancedSettingsPanel : public juce::Component
    {
    public:
        AdvancedSettingsPanel();
        void setTargetControl(int index, ControlStepConfig* config, int totalControls);
        void resized() override;
        void paint(juce::Graphics& g) override;

        std::function<void()> onChanged;
        std::function<void(int dir)> onMoveOrder;
        std::function<void()> onRemove;

    private:
        int targetIndex { -1 };
        ControlStepConfig* targetConfig { nullptr };

        juce::Label lblTitle;
        juce::Label lblRange;
        juce::TextEditor txtMin;
        juce::Label lblRangeDash;
        juce::TextEditor txtMax;
        juce::Label lblUnitMin;
        juce::Label lblUnitMax;

        juce::Label lblOrder;
        juce::TextButton btnMoveEarlier { juce::String::fromUTF8(u8"\u25b2 Move Earlier") };
        juce::TextButton btnMoveLater { juce::String::fromUTF8(u8"\u25bc Move Later") };

        // Custom resolution inputs (only visible when isCustom == true)
        juce::Label lblCustomSteps;
        juce::TextEditor txtCustomSteps;

        juce::TextButton btnRemove { "Remove Control" };
    };

    AdvancedSettingsPanel advancedPanel;

    // "Add Parameter" button — only visible when availableParamsList is set
    juce::TextButton btnAddParam { "+ Add Parameter" };

    // Toggle button for the table column
    class AdvancedToggleComponent : public juce::Component
    {
    public:
        std::function<void()> onToggle;
        AdvancedToggleComponent()
        {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
            btn.onClick = [this] { if (onToggle) onToggle(); };
            addAndMakeVisible(btn);
        }
        void setState(bool isOpen)
        {
            btn.setButtonText(isOpen ? juce::String::fromUTF8(u8"\u25b4") : juce::String::fromUTF8(u8"\u25be"));
            btn.setTooltip(isOpen ? "Close advanced settings" : "Open advanced settings");
        }
        void resized() override { btn.setBounds(getLocalBounds().reduced(2)); }
    private:
        juce::TextButton btn;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixResolutionTableComponent)
};

} // namespace abdaudiolab::gui
