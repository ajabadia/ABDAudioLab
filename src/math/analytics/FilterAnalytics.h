/**
 * @file FilterAnalytics.h
 * @brief Analysis algorithms for analog and digital filters (VCF, EQ, Comb)
 *        via Farina log-sine sweep deconvolution.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include "../FarinaDeconvolver.h"

namespace abdaudiolab::math::analytics
{

struct FilterStatisticalPair
{
    float mean { 0.0f };
    float stdDev { 0.0f };
};

struct FilterAnalysisData
{
    FilterStatisticalPair cutoffHz;
    FilterStatisticalPair resonanceDb;
    FilterStatisticalPair thdPercent;
    std::vector<float> frequencyCurveHz;
    std::vector<float> magnitudeCurveDb;
};

class FilterAnalytics
{
public:
    static FilterStatisticalPair calculateStatistics(const std::vector<float>& dataset);

    static FilterAnalysisData analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                  const std::vector<float>& inverseFilter,
                                                  double sampleRate,
                                                  double durationSec,
                                                  float startFreqHz,
                                                  float endFreqHz);
};

} // namespace abdaudiolab::math::analytics
