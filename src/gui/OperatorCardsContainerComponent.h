#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include "../core/ProfilingSession.h"

namespace abdaudiolab::gui
{

/**
 * @class OperatorCardsContainerComponent
 * @brief Standalone visual container for physical control cards and parameter telemetry.
 *
 * Supports:
 * - Dual/Single card view (<= 2 controls): Large control render with side telemetry card.
 * - Multi-control grid view (>= 3 controls): Horizontal micro-cards scroll with click selection
 *   and full-width telemetry specification card for the selected parameter.
 */
class OperatorCardsContainerComponent : public juce::Component
{
public:
    OperatorCardsContainerComponent();
    ~OperatorCardsContainerComponent() override = default;

    /**
     * @brief Sets the parameter steps and optional guidance prompt message to display.
     */
    void setStepData(const std::vector<core::ParameterStep>& steps, const juce::String& promptMsg = {});

    /**
     * @brief Selects the active parameter index in multi-control mode.
     */
    void setSelectedParamIndex(int index);

    /**
     * @brief Returns the currently selected parameter index.
     */
    [[nodiscard]] int getSelectedParamIndex() const noexcept { return selectedParamIndex; }

    /**
     * @brief Returns the active parameter steps.
     */
    [[nodiscard]] const std::vector<core::ParameterStep>& getParameterSteps() const noexcept { return parameterSteps; }

    /**
     * @brief Returns the prompt message.
     */
    [[nodiscard]] const juce::String& getPromptMessage() const noexcept { return promptMessage; }

    /**
     * @brief Callback invoked when user selects a different parameter card in multi-control mode.
     */
    std::function<void(int newIndex)> onSelectionChanged;

    void mouseDown(const juce::MouseEvent& e) override;
    void paint(juce::Graphics& g) override;

private:
    void layoutDualCardView(juce::Graphics& g, float w, float areaH);
    void layoutMultiControlGridView(juce::Graphics& g, float w, float areaH);
    static void drawControl(juce::Graphics& g, juce::Rectangle<float> ctrlArea, const core::ParameterStep& ps);

    int selectedParamIndex { 0 };
    std::vector<core::ParameterStep> parameterSteps;
    juce::String promptMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorCardsContainerComponent)
};

} // namespace abdaudiolab::gui
