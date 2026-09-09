/**
 * @file VoiceDispersionModel.h
 * @brief Re-exports ABDSharedCode::LutDSP::VoiceDispersionModel for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <LutDSP/VoiceDispersionModel.h>

namespace abdaudiolab::dsp
{

template <size_t NumVoices = 8>
using VoiceDispersionModel = abd::lutdsp::VoiceDispersionModel<NumVoices>;

} // namespace abdaudiolab::dsp
