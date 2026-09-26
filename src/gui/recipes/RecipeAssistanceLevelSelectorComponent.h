#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "RecipeExecutionController.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @class RecipeAssistanceLevelSelectorComponent
 * @brief Selector visual segmentado con tres botones estilizados: [Rápido], [Configurable], [Avanzado].
 * Modifica el RecipeEditorView sin alterar la receta ni los hashes canónicos.
 */
class RecipeAssistanceLevelSelectorComponent : public juce::Component
{
public:
    RecipeAssistanceLevelSelectorComponent();
    ~RecipeAssistanceLevelSelectorComponent() override = default;

    void setSelectedView(RecipeEditorView view);
    [[nodiscard]] RecipeEditorView getSelectedView() const noexcept { return currentView_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(RecipeEditorView)> onLevelChanged;

private:
    void updateButtonStates();

    juce::TextButton btnQuick_ { "Rápido" };
    juce::TextButton btnConfigurable_ { "Configurable" };
    juce::TextButton btnAdvanced_ { "Avanzado" };

    RecipeEditorView currentView_ { RecipeEditorView::Quick };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeAssistanceLevelSelectorComponent)
};

} // namespace abdaudiolab::gui::recipes
