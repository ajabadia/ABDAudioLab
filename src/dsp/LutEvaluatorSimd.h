/**
 * @file LutEvaluatorSimd.h
 * @brief Re-exports ABDSharedCode::LutDSP::LutEvaluatorSimd for DRY modularity.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <LutDSP/LutEvaluatorSimd.h>

namespace abdaudiolab::dsp
{

using AbdBatchedPoint = abd::lutdsp::AbdBatchedPoint;
using LutMetric = abd::lutdsp::LutMetric;
using LutEvaluatorSimd = abd::lutdsp::LutEvaluatorSimd;
using LutEvaluator1DSimd = abd::lutdsp::LutEvaluator1DSimd;

} // namespace abdaudiolab::dsp
