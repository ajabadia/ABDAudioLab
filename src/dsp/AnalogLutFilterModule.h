/**
 * @file AnalogLutFilterModule.h
 * @brief Re-exports ABDSharedCode::LutDSP::AnalogLutFilterModule for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <LutDSP/AnalogLutFilterModule.h>
#include "dsp/LutEvaluatorSimd.h"

namespace abdaudiolab::dsp
{

using AnalogLutFilterModule = abd::lutdsp::AnalogLutFilterModule;

} // namespace abdaudiolab::dsp
