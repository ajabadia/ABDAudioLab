/**
 * @file EnvelopeAnalytics.h
 * @brief Analysis algorithms for envelopes (ADSR, AR) and transient profiling.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include "FilterAnalytics.h"

namespace abdaudiolab::math::analytics
{

struct EnvelopeAnalysisData
{
    FilterStatisticalPair attackTimeMs;
    FilterStatisticalPair decayTimeMs;
    FilterStatisticalPair sustainLevel;
    FilterStatisticalPair releaseTimeMs;
    FilterStatisticalPair delayTimeMs;
};

class EnvelopeAnalytics
{
public:
    static EnvelopeAnalysisData analyzeAdsrEnvelopes(const std::vector<std::vector<float>>& recordedPasses,
                                                     double sampleRate);
};

} // namespace abdaudiolab::math::analytics
