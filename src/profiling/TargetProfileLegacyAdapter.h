#pragma once

#include <variant>
#include "profiling/TargetProfile.h"
#include "core/HardwareContractRegistry.h"

namespace abdaudiolab::profiling
{

/**
 * @class TargetProfileLegacyAdapter
 * @brief Adaptador unidireccional de compatibilidad de solo lectura:
 *        TargetProfile canónico validado -> abdaudiolab::core::HardwareContract legacy.
 * 
 * Cumple los requerimientos de la Fase E3 de HITO-10E:
 * - Proyecta TargetProfile canónicos hacia consumidores legacy sin modificar código de producción.
 * - Dirección estrictamente unidireccional (TargetProfile -> HardwareContract).
 * - Queda terminantemente prohibida la proyección inversa en runtime para no generar
 *   TargetProfiles aparentes con semántica degradada o no verificada.
 */
class TargetProfileLegacyAdapter
{
public:
    /**
     * @brief Proyecta un TargetProfile canónico a su vista de compatibilidad HardwareContract legacy.
     */
    [[nodiscard]] static core::HardwareContract toLegacyHardwareContract(const TargetProfile& profile);
};

} // namespace abdaudiolab::profiling
