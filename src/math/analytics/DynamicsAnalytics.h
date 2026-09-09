/**
 * @file DynamicsAnalytics.h
 * @brief Analysis algorithms for Waveshapers, Saturation, Gain/SNR, and Linear Bypass fitting.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include "FilterAnalytics.h"
#include "../WienerHammersteinFitter.h"

namespace abdaudiolab::math
{
    struct PreScanPoint;
    struct PreScanResult;
    struct SingleTakeAnalysisResult;
}

namespace abdaudiolab::math::analytics
{

struct WaveShaperAnalysisData
{
    FilterStatisticalPair thdPercent;
    std::vector<float> transferCurveInput;
    std::vector<float> transferCurveOutput;
};

struct GainAnalysisData
{
    FilterStatisticalPair gainDb;
    FilterStatisticalPair snrDb;
};

struct WienerHammersteinAnalysisData
{
    FilterStatisticalPair nonLinearCoeffA;
    FilterStatisticalPair preFilterCentroidHz;
    FilterStatisticalPair postFilterCentroidHz;
    FilterStatisticalPair goodnessOfFitR2;
    std::vector<float> representativeH1;
    std::vector<float> representativeH2;
};

class DynamicsAnalytics
{
public:
    static float calculateSignalToNoiseRatioDb(const std::vector<float>& signalBuffer,
                                              float baselineNoiseRmsDb = -90.0f);

    static WaveShaperAnalysisData analyzeWaveShaperRamps(const std::vector<std::vector<float>>& recordedPasses,
                                                         double sampleRate);

    static GainAnalysisData analyzeGainTones(const std::vector<std::vector<float>>& recordedPasses,
                                            double sampleRate);

    static WienerHammersteinAnalysisData analyzeWienerHammerstein(const std::vector<std::vector<float>>& recordedPasses,
                                                                 const std::vector<float>& inputStimulus,
                                                                 double sampleRate);

    static math::PreScanResult evaluateLinearBypass(const std::vector<float>& recordedBuffer,
                                                    const std::vector<float>& stimulusBuffer,
                                                    float noiseFloorDb = -90.0f);

    static void computeAdaptiveRoadmap(math::PreScanResult& result,
                                       float gradientThreshold = 0.05f);

    static math::SingleTakeAnalysisResult analyzeSingleTakeMultiplexed(const std::vector<float>& alignedAudio,
                                                                      double sampleRate,
                                                                      float transientDurationSec = 0.2f);

    static math::PreScanResult applyPlateauCollapseFilter(const std::vector<math::PreScanPoint>& rawTrajectory,
                                                          float noiseFloorVariance) noexcept;
};

} // namespace abdaudiolab::math::analytics
