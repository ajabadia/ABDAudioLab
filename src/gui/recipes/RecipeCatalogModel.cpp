#include "RecipeCatalogModel.h"
#include "core/LabResourcePaths.h"
#include <algorithm>

namespace abdaudiolab::gui::recipes
{

RecipeCatalogModel::RecipeCatalogModel(const profiling::MeasurementRecipeService& service)
    : recipeService_(service)
{
}

void RecipeCatalogModel::loadFromDirectory(const juce::File& presetsDirectory)
{
    entries_.clear();

    if (!presetsDirectory.isDirectory())
        return;

    juce::Array<juce::File> files;
    presetsDirectory.findChildFiles(files, juce::File::findFiles, false, "*.json");

    // Orden determinista por nombre de archivo
    files.sort();

    for (const auto& file : files)
    {
        RecipeCatalogEntry entry;
        entry.file = file;
        entry.loadResult = recipeService_.loadAndValidate(file);
        entry.isAvailable = entry.loadResult.isSuccess();
        entries_.push_back(entry);
    }
}

void RecipeCatalogModel::loadDefaultPresets()
{
    // Las recetas son datos del repo: se resuelven contra la raiz del repositorio,
    // no contra el directorio de trabajo desde el que se haya lanzado la app.
    // Variante no lanzante: si el arbol del repo no esta presente, el catalogo
    // queda vacio y la app sigue arrancando, como antes.
    const auto presetsDir = abdaudiolab::core::optionalRepoResource("presets/profiling");

    loadFromDirectory(presetsDir);
}

std::vector<RecipeCatalogEntry> RecipeCatalogModel::getValidEntries() const
{
    std::vector<RecipeCatalogEntry> valid;
    std::copy_if(entries_.begin(), entries_.end(), std::back_inserter(valid), [](const RecipeCatalogEntry& e) {
        return e.isAvailable;
    });
    return valid;
}

std::optional<RecipeCatalogEntry> RecipeCatalogModel::findByRecipeId(const std::string& recipeId) const
{
    for (const auto& entry : entries_)
    {
        if (entry.loadResult.recipe.recipeId == recipeId)
            return entry;
    }
    return std::nullopt;
}

std::optional<RecipeCatalogEntry> RecipeCatalogModel::findByAssistanceLevel(profiling::AssistanceLevel level) const
{
    for (const auto& entry : entries_)
    {
        if (entry.loadResult.recipe.assistanceLevel == level)
            return entry;
    }
    return std::nullopt;
}

size_t RecipeCatalogModel::getValidEntryCount() const noexcept
{
    return static_cast<size_t>(std::count_if(entries_.begin(), entries_.end(), [](const RecipeCatalogEntry& e) {
        return e.isAvailable;
    }));
}

} // namespace abdaudiolab::gui::recipes
