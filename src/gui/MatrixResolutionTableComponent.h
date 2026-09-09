#pragma once

#include "TestConfiguration.h"
#include "AppTheme.h"
#include "SoundIdTheme.h"
#include "ControlIcon.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

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

    void updateTheme();
    int getPreferredHeight() const;

    std::function<void()> onControlsChanged;

    void resized() override;

    // juce::TableListBoxModel methods
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

private:
    std::vector<ControlStepConfig> controlList;
    juce::TableListBox matrixTable;

    class OrderButtonsComponent : public juce::Component
    {
    public:
        OrderButtonsComponent();
        void resized() override;
        juce::TextButton btnUp { juce::String::fromUTF8(u8"▲") };
        juce::TextButton btnDown { juce::String::fromUTF8(u8"▼") };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixResolutionTableComponent)
};

} // namespace abdaudiolab::gui
