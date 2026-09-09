/**
 * @file SysexPresetGenerator.h
 * @brief Re-exports ABDSharedCode::HardwareDrivers::SysexPresetGenerator for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#if __has_include(<HardwareDrivers/SysexPresetGenerator.h>)
#include <HardwareDrivers/SysexPresetGenerator.h>
#elif __has_include("../../../ABDSharedCode/HardwareDrivers/SysexPresetGenerator.h")
#include "../../../ABDSharedCode/HardwareDrivers/SysexPresetGenerator.h"
#endif

namespace abdaudiolab::hardware
{

using SysexPresetGenerator = abd::hw::SysexPresetGenerator;

} // namespace abdaudiolab::hardware
