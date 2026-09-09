/**
 * @file ModulationAnalytics.h
 * @brief Analysis algorithms for CyclicModulators (LFO, Chorus, Flanger, Phaser)
 *        and delay impulse responses.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include "FilterAnalytics.h"

namespace abdaudiolab::math::analytics
{

struct CyclicModulatorAnalysisData
{
    FilterStatisticalPair rateHz;
    FilterStatisticalPair depthPercent;
    FilterStatisticalPair asymmetry;
};

class ModulationAnalytics
{
public:
    static CyclicModulatorAnalysisData analyzeCyclicModulator(const std::vector<std::vector<float>>& recordedPasses,
                                                             double sampleRate);
};

} // namespace abdaudiolab::math::analytics
