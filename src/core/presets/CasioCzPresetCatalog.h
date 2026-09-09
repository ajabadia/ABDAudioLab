#pragma once

#include <vector>
#include "../AutoTestPresetEngine.h"

namespace abdaudiolab {
namespace core {
namespace presets {

class CasioCzPresetCatalog
{
public:
 /**
 * @brief Devuelve las recetas analiticas especificas para clonar el comportamiento
 * del chip de Distorsion de Fase Casio NZ-1 (DCO, DCW, DCA).
 */
 static std::vector<ComponentPresetRecommendation> getPresets();
};

} // namespace presets
} // namespace core
} // namespace abdaudiolab
