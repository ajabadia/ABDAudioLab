/**
 * @file SysexPresetGenerator.h
 * @brief Re-exports the consolidated hardware driver for DRY modularity.
 *
 * El driver vive en el sistema consolidado de codigo compartido bajo
 * ABDShared/Code/HardwareDrivers. Este header es un trannings adapter local
 * para ABDAudioLab.
 *
 * Inclusion preferida: ruta consolidada bajo ABDShared/Code/HardwareDrivers.
 * Ruta legacy bajo ABDSharedCode se mantiene solo como compatibilidad temporal;
 * se retira cuando el arbol legacy deja de existir.
 *
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#if __has_include(<HardwareDrivers/SysexPresetGenerator.h>)
#include <HardwareDrivers/SysexPresetGenerator.h>
#elif __has_include("../../../ABDShared/Code/HardwareDrivers/SysexPresetGenerator.h")
#include "../../../ABDShared/Code/HardwareDrivers/SysexPresetGenerator.h"
#elif __has_include("../../../ABDSharedCode/HardwareDrivers/SysexPresetGenerator.h")
#include "../../../ABDSharedCode/HardwareDrivers/SysexPresetGenerator.h"
#endif

namespace abdaudiolab::hardware
{

using SysexPresetGenerator = abd::hw::SysexPresetGenerator;

} // namespace abdaudiolab::hardware
