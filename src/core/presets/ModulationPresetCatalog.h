#pragma once

#include <vector>
#include "../AutoTestPresetEngine.h"

namespace abdaudiolab {
namespace core {
namespace presets {

class ModulationPresetCatalog
{
public:
    /**
     * @brief Devuelve presets de envolventes y LFOs clásicos
     *        así como los submódulos AIRA de modulación (12 al 15, 24, 25).
     */
    static std::vector<ComponentPresetRecommendation> getPresets();
};

} // namespace presets
} // namespace core
} // namespace abdaudiolab
