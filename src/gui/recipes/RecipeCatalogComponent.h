#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "RecipeCatalogModel.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @class RecipeCatalogComponent
 * @brief Componente visual que lista las recetas normativas integradas y emite la selección.
 * No realiza parsing ni construcción de planes; solo interacción de presentación.
 */
class RecipeCatalogComponent : public juce::Component
{
public:
    using RecipeSelectedCallback = std::function<void(const RecipeCatalogEntry& entry)>;

    RecipeCatalogComponent();
    ~RecipeCatalogComponent() override = default;

    /**
     * @brief Asocia el modelo de catálogo y refresca los elementos visuales.
     */
    void setCatalogModel(const RecipeCatalogModel* model);

    /**
     * @brief Registra el callback invocado cuando el usuario selecciona una receta.
     */
    void setOnRecipeSelected(RecipeSelectedCallback cb) { onRecipeSelected_ = std::move(cb); }

    /**
     * @brief Selecciona programáticamente la primera receta disponible o la especificada por ID.
     */
    void selectRecipeById(const std::string& recipeId);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    const RecipeCatalogModel* catalogModel_ { nullptr };
    RecipeSelectedCallback onRecipeSelected_;

    juce::GroupComponent groupCatalog_ { {}, "Catalogo de Recetas Metrologicas" };
    juce::ListBox listBox_;

    class RecipeListBoxModel : public juce::ListBoxModel
    {
    public:
        explicit RecipeListBoxModel(RecipeCatalogComponent& owner) : owner_(owner) {}

        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override;

    private:
        RecipeCatalogComponent& owner_;
    };

    RecipeListBoxModel listBoxModel_ { *this };

    std::vector<RecipeCatalogEntry> displayedEntries_;

    void rebuildEntries();
};

} // namespace abdaudiolab::gui::recipes
