#pragma once

#include <vector>
#include "../AutoTestPresetEngine.h"

namespace abdaudiolab {
namespace core {
namespace presets {

class DynamicsPresetCatalog
{
public:
    /**
     * @brief Devuelve presets de dinámica (VCA, saturadores, fuzz, compresores, ring mod)
     *        y los submódulos AIRA de distorsión y dinámica (07..11, 21, 22, 26..29, 31),
     *        incluyendo el modelado a válvulas Tube Clipper de Roland Torcido.
     */
    static std::vector<ComponentPresetRecommendation> getPresets();
};

} // namespace presets
} // namespace core
} // namespace abdaudiolab
