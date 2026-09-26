#include "RecipeAssistanceLevelSelectorComponent.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::recipes
{

RecipeAssistanceLevelSelectorComponent::RecipeAssistanceLevelSelectorComponent()
{
    auto configureBtn = [this](juce::TextButton& btn, RecipeEditorView view) {
        btn.setClickingTogglesState(false);
        btn.onClick = [this, view]() {
            setSelectedView(view);
            if (onLevelChanged)
                onLevelChanged(view);
        };
        addAndMakeVisible(btn);
    };

    configureBtn(btnQuick_, RecipeEditorView::Quick);
    configureBtn(btnConfigurable_, RecipeEditorView::Configurable);
    configureBtn(btnAdvanced_, RecipeEditorView::Advanced);

    updateButtonStates();
}

void RecipeAssistanceLevelSelectorComponent::setSelectedView(RecipeEditorView view)
{
    if (currentView_ != view)
    {
        currentView_ = view;
        updateButtonStates();
        repaint();
    }
}

void RecipeAssistanceLevelSelectorComponent::updateButtonStates()
{
    auto applyStyle = [](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? SoundIdTheme::accentBlue : SoundIdTheme::bgCard);
        btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : SoundIdTheme::textSecondary);
    };

    applyStyle(btnQuick_, currentView_ == RecipeEditorView::Quick);
    applyStyle(btnConfigurable_, currentView_ == RecipeEditorView::Configurable);
    applyStyle(btnAdvanced_, currentView_ == RecipeEditorView::Advanced);
}

void RecipeAssistanceLevelSelectorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
}

void RecipeAssistanceLevelSelectorComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    int btnWidth = area.getWidth() / 3;

    btnQuick_.setBounds(area.removeFromLeft(btnWidth));
    btnConfigurable_.setBounds(area.removeFromLeft(btnWidth));
    btnAdvanced_.setBounds(area);
}

} // namespace abdaudiolab::gui::recipes
