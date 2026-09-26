#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <string_view>
#include "MeasurementRecipe.h"

namespace abdaudiolab::profiling
{

/**
 * @class MeasurementRecipeService
 * @brief Servicio central de ingestión, validación estricta por capas y cálculo canónico RFC 8785 de MeasurementRecipe.
 */
class MeasurementRecipeService
{
public:
    MeasurementRecipeService() = default;
    ~MeasurementRecipeService() = default;

    /**
     * @brief Carga y valida exhaustivamente una receta metrológica desde un archivo en disco.
     */
    [[nodiscard]] RecipeLoadResult loadAndValidate(const juce::File& recipeFile) const;

    /**
     * @brief Valida exhaustivamente una receta metrológica desde una cadena de texto JSON.
     */
    [[nodiscard]] RecipeLoadResult loadAndValidateJson(std::string_view jsonString) const;

    /**
     * @brief Genera la representación canónica RFC 8785 de una receta parseada.
     */
    [[nodiscard]] static std::string canonicalizeJsonRfc8785(const std::string& rawJsonString);

    /**
     * @brief Calcula el SHA-256 canónico RFC 8785 del documento.
     */
    [[nodiscard]] static std::string computeRecipeDocumentHash(std::string_view jsonString);

    /**
     * @brief Serializa un objeto MeasurementRecipe a cadena JSON estructurada según el esquema.
     */
    [[nodiscard]] static std::string serializeRecipeToJson(const MeasurementRecipe& recipe);
};

} // namespace abdaudiolab::profiling
