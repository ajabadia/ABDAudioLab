/**
 * @file VoiceAllocator.h
 * @brief Re-exports ABDSharedCode::LutDSP::VoiceAllocator for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <LutDSP/VoiceAllocator.h>

namespace abdaudiolab::dsp
{

template <size_t MaxVoices = 8>
using VoiceAllocator = abd::lutdsp::VoiceAllocator<MaxVoices>;
using PolyMode = abd::lutdsp::PolyMode;
using VoiceState = abd::lutdsp::VoiceState;

} // namespace abdaudiolab::dsp
