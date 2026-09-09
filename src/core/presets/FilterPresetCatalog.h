#pragma once

#include <vector>
#include "../AutoTestPresetEngine.h"

namespace abdaudiolab {
namespace core {
namespace presets {

class FilterPresetCatalog
{
public:
    /**
     * @brief Devuelve presets de filtros clásicos (Ladder, SVF, Formantes, EQ, Peine)
     *        y los módulos de filtrado específicos de la serie Roland AIRA (01 al 06).
     */
    static std::vector<ComponentPresetRecommendation> getPresets();
};

} // namespace presets
} // namespace core
} // namespace abdaudiolab
