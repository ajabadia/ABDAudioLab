/**
 * @file FilterAnalytics.cpp
 * @brief Implementation of FilterAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "FilterAnalytics.h"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace abdaudiolab::math::analytics
{

FilterStatisticalPair FilterAnalytics::calculateStatistics(const std::vector<float>& dataset)
{
    FilterStatisticalPair res;
    if (dataset.empty())
        return res;

    if (dataset.size() == 1)
    {
        res.mean = dataset[0];
        res.stdDev = 0.0f;
        return res;
    }

    double sum = 0.0;
    for (float v : dataset)
        sum += v;

    double mean = sum / static_cast<double>(dataset.size());
    res.mean = static_cast<float>(mean);

    double varSum = 0.0;
    for (float v : dataset)
    {
        double diff = static_cast<double>(v) - mean;
        varSum += diff * diff;
    }

    double variance = varSum / static_cast<double>(dataset.size() - 1);
    res.stdDev = static_cast<float>(std::sqrt(std::max(0.0, variance)));

    return res;
}

FilterAnalysisData FilterAnalytics::analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                       const std::vector<float>& inverseFilter,
                                                       double sampleRate,
                                                       double durationSec,
                                                       float startFreqHz,
                                                       float endFreqHz)
{
    FilterAnalysisData result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> cutoffs;
    std::vector<float> resonances;
    std::vector<float> thds;

    cutoffs.reserve(recordedPasses.size());
    resonances.reserve(recordedPasses.size());
    thds.reserve(recordedPasses.size());

    for (const auto& pass : recordedPasses)
    {
        auto deco = FarinaDeconvolver::deconvolve(pass, inverseFilter, sampleRate, durationSec, startFreqHz, endFreqHz);
        cutoffs.push_back(deco.peakFrequencyHz);
        resonances.push_back(deco.resonancePeakDb);
        thds.push_back(deco.thdPercent);

        if (result.frequencyCurveHz.empty())
        {
            result.frequencyCurveHz = deco.frequenciesHz;
            result.magnitudeCurveDb = deco.frequencyResponseMagnitudeDb;
        }
    }

    result.cutoffHz = calculateStatistics(cutoffs);
    result.resonanceDb = calculateStatistics(resonances);
    result.thdPercent = calculateStatistics(thds);

    return result;
}

} // namespace abdaudiolab::math::analytics
