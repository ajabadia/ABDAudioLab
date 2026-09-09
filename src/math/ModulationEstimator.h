#pragma once

#include "ModulationMatrixProfile.h"

namespace abdaudiolab {
namespace math {

class ModulationEstimator {
public:
    /**
     * Calcula mediante mínimos cuadrados analíticos los parámetros K, c y R^2
     * Garantiza cero allocations dinámicas en su núcleo estadístico.
     */
    static ModulationNode calculateNode (int srcID, int dstID, 
                                         const std::vector<ModulationProbePoint>& points);
};

} // namespace math
} // namespace abdaudiolab
