#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <optional>
#include "../../profiling/MeasurementRecipe.h"
#include "../../profiling/MeasurementRecipeService.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @brief Entrada individual del catálogo con su resultado de carga y metadatos.
 */
struct RecipeCatalogEntry
{
    juce::File file;
    profiling::RecipeLoadResult loadResult;
    bool isAvailable { false };
};

/**
 * @class RecipeCatalogModel
 * @brief Modelo de descubrimiento y carga de recetas normativas integradas.
 * Carga exclusivamente a través de MeasurementRecipeService, asegurando validación estricta y hashes canónicos.
 */
class RecipeCatalogModel
{
public:
    explicit RecipeCatalogModel(const profiling::MeasurementRecipeService& service);
    ~RecipeCatalogModel() = default;

    /**
     * @brief Descubre y carga todas las recetas en el directorio de presets especificado.
     */
    void loadFromDirectory(const juce::File& presetsDirectory);

    /**
     * @brief Carga las recetas normativas por defecto (quick_vcf_3pts, standard_vcf_11pts, exhaustive_synth_full).
     */
    void loadDefaultPresets();

    [[nodiscard]] const std::vector<RecipeCatalogEntry>& getEntries() const noexcept { return entries_; }
    [[nodiscard]] std::vector<RecipeCatalogEntry> getValidEntries() const;
    [[nodiscard]] std::optional<RecipeCatalogEntry> findByRecipeId(const std::string& recipeId) const;
    [[nodiscard]] std::optional<RecipeCatalogEntry> findByAssistanceLevel(profiling::AssistanceLevel level) const;

    [[nodiscard]] size_t getEntryCount() const noexcept { return entries_.size(); }
    [[nodiscard]] size_t getValidEntryCount() const noexcept;

private:
    const profiling::MeasurementRecipeService& recipeService_;
    std::vector<RecipeCatalogEntry> entries_;
};

} // namespace abdaudiolab::gui::recipes
