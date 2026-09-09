/**
 * @file JunoBBD.h
 * @brief Re-exports ABDSharedCode::LutDSP::JunoBBD for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <LutDSP/JunoBBD.h>

namespace abdaudiolab::dsp
{

using JunoBBD = abd::lutdsp::JunoBBD;

} // namespace abdaudiolab::dsp
