/**
 * @file AudioLabMidiBackend.h
 * @brief Host MIDI Hardware Backend for ABDSharedCode::HardwareMidiDetect.
 * @details Re-exports ABDSharedCode::HardwareMidiDetect::JuceMidiHardwareBackend
 *          under namespace abdaudiolab::hardware for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <HardwareMidiDetect/JuceMidiHardwareBackend.h>

namespace abdaudiolab::hardware
{

using AudioLabMidiBackend = abd::hwid::JuceMidiHardwareBackend;

} // namespace abdaudiolab::hardware
