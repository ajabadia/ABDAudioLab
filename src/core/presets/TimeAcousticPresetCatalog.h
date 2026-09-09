#pragma once

#include <vector>
#include "../AutoTestPresetEngine.h"

namespace abdaudiolab {
namespace core {
namespace presets {

class TimeAcousticPresetCatalog
{
public:
    /**
     * @brief Devuelve presets de tiempo, espacio y acústica:
     *        - Delays (BBD, Cinta, Digital), Chorus/Flanger/Phaser, Reverberación.
     *        - Osciladores autónomos (VCO/DCO, Wavetable, Ruido).
     *        - Recintos y acústica (Cabinet IR, Preamp EIN).
     *        - Submódulos AIRA espaciales (16 Delay corto, 17 Cinta, 18 BBD Chorus,
     *          19 Flanger, 20 Phaser, 23 Ruido, 30 Pitch Transposer).
     */
    static std::vector<ComponentPresetRecommendation> getPresets();
};

} // namespace presets
} // namespace core
} // namespace abdaudiolab
