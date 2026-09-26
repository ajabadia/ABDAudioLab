#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include <optional>
#include "../../profiling/MeasurementRecipe.h"
#include "../../profiling/ExperimentPlanCompiler.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @class RecipeSummaryCardComponent
 * @brief Componente visual de inspección estructurada de una MeasurementRecipe ya validada.
 * Expone claramente la intención científica y operativa, los hashes canónicos y el estado de compatibilidad.
 * No muta la receta ni inventa valores por defecto.
 */
class RecipeSummaryCardComponent : public juce::Component
{
public:
    RecipeSummaryCardComponent();
    ~RecipeSummaryCardComponent() override = default;

    /**
     * @brief Actualiza la tarjeta con los datos de una receta validada y su hash documental.
     */
    void setRecipe(const profiling::MeasurementRecipe& recipe,
                   const std::string& recipeDocumentHash,
                   const std::optional<std::string>& experimentPlanHash = std::nullopt);

    /**
     * @brief Actualiza el estado de resolución y compatibilidad frente al entorno actual.
     */
    void setResolutionStatus(const profiling::ResolveExecutionPlanResult& resolveResult);

    /**
     * @brief Limpia la vista cuando no hay ninguna receta seleccionada.
     */
    void clear();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::optional<profiling::MeasurementRecipe> currentRecipe_;
    std::string recipeDocumentHash_;
    std::optional<std::string> experimentPlanHash_;
    std::optional<profiling::ResolveExecutionPlanResult> resolutionResult_;

    juce::Label lblTitle_;
    juce::Label lblAssistanceBadge_;
    juce::Label lblRecipeId_;

    juce::Label lblExcitationSummary_;
    juce::Label lblMeasurementSummary_;
    juce::Label lblEnvironmentSummary_;
    juce::Label lblEvaluationSummary_;

    juce::Label lblDocumentHash_;
    juce::Label lblPlanHash_;
    juce::Label lblCompatibilityBanner_;

    void updateLabels();
};

} // namespace abdaudiolab::gui::recipes
