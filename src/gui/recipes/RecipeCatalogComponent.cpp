#include "RecipeCatalogComponent.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::recipes
{

RecipeCatalogComponent::RecipeCatalogComponent()
{
    addAndMakeVisible(groupCatalog_);

    listBox_.setModel(&listBoxModel_);
    listBox_.setRowHeight(36);
    listBox_.setColour(juce::ListBox::backgroundColourId, SoundIdTheme::bgCard);
    addAndMakeVisible(listBox_);
}

void RecipeCatalogComponent::setCatalogModel(const RecipeCatalogModel* model)
{
    catalogModel_ = model;
    rebuildEntries();
}

void RecipeCatalogComponent::rebuildEntries()
{
    displayedEntries_.clear();
    if (catalogModel_ != nullptr)
    {
        displayedEntries_ = catalogModel_->getEntries();
    }
    listBox_.updateContent();
    repaint();
}

void RecipeCatalogComponent::selectRecipeById(const std::string& recipeId)
{
    for (size_t i = 0; i < displayedEntries_.size(); ++i)
    {
        if (displayedEntries_[i].loadResult.recipe.recipeId == recipeId)
        {
            listBox_.selectRow(static_cast<int>(i));
            if (onRecipeSelected_)
            {
                onRecipeSelected_(displayedEntries_[i]);
            }
            break;
        }
    }
}

int RecipeCatalogComponent::RecipeListBoxModel::getNumRows()
{
    return static_cast<int>(owner_.displayedEntries_.size());
}

void RecipeCatalogComponent::RecipeListBoxModel::paintListBoxItem(int rowNumber,
                                                                 juce::Graphics& g,
                                                                 int width,
                                                                 int height,
                                                                 bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(owner_.displayedEntries_.size()))
        return;

    const auto& entry = owner_.displayedEntries_[static_cast<size_t>(rowNumber)];

    if (rowIsSelected)
    {
        g.setColour(SoundIdTheme::accentBlue.withAlpha(0.25f));
        g.fillRoundedRectangle(2.0f, 2.0f, static_cast<float>(width - 4), static_cast<float>(height - 4), 4.0f);
        g.setColour(SoundIdTheme::accentBlue);
        g.drawRoundedRectangle(2.0f, 2.0f, static_cast<float>(width - 4), static_cast<float>(height - 4), 4.0f, 1.0f);
    }
    else
    {
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(2.0f, 2.0f, static_cast<float>(width - 4), static_cast<float>(height - 4), 4.0f);
    }

    // Texto de la fila
    auto r = juce::Rectangle<int>(8, 0, width - 16, height);

    g.setFont(juce::FontOptions(13.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
    g.setColour(entry.isAvailable ? SoundIdTheme::textPrimary : SoundIdTheme::accentRed);

    juce::String nameText = entry.isAvailable
        ? juce::String::fromUTF8(entry.loadResult.recipe.displayName.c_str())
        : "[Error] " + entry.file.getFileName();

    g.drawText(nameText, r.removeFromLeft(width - 110), juce::Justification::centredLeft, true);

    // Nivel de asistencia a la derecha
    if (entry.isAvailable)
    {
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText("[" + juce::String(profiling::assistanceLevelToString(entry.loadResult.recipe.assistanceLevel)) + "]",
                   r, juce::Justification::centredRight, true);
    }
}

void RecipeCatalogComponent::RecipeListBoxModel::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(owner_.displayedEntries_.size()))
    {
        if (owner_.onRecipeSelected_)
        {
            owner_.onRecipeSelected_(owner_.displayedEntries_[static_cast<size_t>(lastRowSelected)]);
        }
    }
}

void RecipeCatalogComponent::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void RecipeCatalogComponent::resized()
{
    auto area = getLocalBounds();
    groupCatalog_.setBounds(area);
    listBox_.setBounds(area.reduced(8, 20));
}

} // namespace abdaudiolab::gui::recipes
