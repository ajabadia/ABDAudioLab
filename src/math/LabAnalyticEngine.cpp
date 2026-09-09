/**
 * @file LabAnalyticEngine.cpp
 * @brief Facade implementation delegating mathematical operations to specialized domain analyzers:
 *        FilterAnalytics, EnvelopeAnalytics, DynamicsAnalytics, and ModulationAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "LabAnalyticEngine.h"
#include "analytics/FilterAnalytics.h"
#include "analytics/EnvelopeAnalytics.h"
#include "analytics/DynamicsAnalytics.h"
#include "analytics/ModulationAnalytics.h"

namespace abdaudiolab::math
{

StatisticalPair LabAnalyticEngine::calculateStatistics(const std::vector<float>& dataset)
{
    auto res = analytics::FilterAnalytics::calculateStatistics(dataset);
    return { res.mean, res.stdDev };
}

float LabAnalyticEngine::calculateSignalToNoiseRatioDb(const std::vector<float>& signalBuffer, float baselineNoiseRmsDb)
{
    return analytics::DynamicsAnalytics::calculateSignalToNoiseRatioDb(signalBuffer, baselineNoiseRmsDb);
}

PreScanResult LabAnalyticEngine::evaluateLinearBypass(const std::vector<float>& recordedBuffer,
                                                      const std::vector<float>& stimulusBuffer,
                                                      float noiseFloorDb)
{
    return analytics::DynamicsAnalytics::evaluateLinearBypass(recordedBuffer, stimulusBuffer, noiseFloorDb);
}

void LabAnalyticEngine::computeAdaptiveRoadmap(PreScanResult& result, float gradientThreshold)
{
    analytics::DynamicsAnalytics::computeAdaptiveRoadmap(result, gradientThreshold);
}

FilterAnalysisResult LabAnalyticEngine::analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                           const std::vector<float>& inverseFilter,
                                                           double sampleRate,
                                                           double durationSec,
                                                           float startFreqHz,
                                                           float endFreqHz)
{
    auto data = analytics::FilterAnalytics::analyzeFilterPasses(recordedPasses, inverseFilter, sampleRate, durationSec, startFreqHz, endFreqHz);
    FilterAnalysisResult res;
    res.cutoffHz = { data.cutoffHz.mean, data.cutoffHz.stdDev };
    res.resonanceDb = { data.resonanceDb.mean, data.resonanceDb.stdDev };
    res.thdPercent = { data.thdPercent.mean, data.thdPercent.stdDev };
    res.frequencyCurveHz = std::move(data.frequencyCurveHz);
    res.magnitudeCurveDb = std::move(data.magnitudeCurveDb);
    return res;
}

TimeDynamicAnalysisResult LabAnalyticEngine::analyzeAdsrEnvelopes(const std::vector<std::vector<float>>& recordedPasses,
                                                                 double sampleRate)
{
    auto data = analytics::EnvelopeAnalytics::analyzeAdsrEnvelopes(recordedPasses, sampleRate);
    TimeDynamicAnalysisResult res;
    res.attackTimeMs = { data.attackTimeMs.mean, data.attackTimeMs.stdDev };
    res.decayTimeMs = { data.decayTimeMs.mean, data.decayTimeMs.stdDev };
    res.sustainLevel = { data.sustainLevel.mean, data.sustainLevel.stdDev };
    res.releaseTimeMs = { data.releaseTimeMs.mean, data.releaseTimeMs.stdDev };
    res.delayTimeMs = { data.delayTimeMs.mean, data.delayTimeMs.stdDev };
    return res;
}

TimeDynamicAnalysisResult LabAnalyticEngine::analyzeDelayImpulses(const std::vector<std::vector<float>>& recordedPasses,
                                                                 double sampleRate)
{
    // Reuse envelope transient analysis for delay impulse response
    return analyzeAdsrEnvelopes(recordedPasses, sampleRate);
}

WaveShaperAnalysisResult LabAnalyticEngine::analyzeWaveShaperRamps(const std::vector<std::vector<float>>& recordedPasses,
                                                                   double sampleRate)
{
    auto data = analytics::DynamicsAnalytics::analyzeWaveShaperRamps(recordedPasses, sampleRate);
    WaveShaperAnalysisResult res;
    res.thdPercent = { data.thdPercent.mean, data.thdPercent.stdDev };
    res.transferCurveInput = std::move(data.transferCurveInput);
    res.transferCurveOutput = std::move(data.transferCurveOutput);
    return res;
}

GainAnalysisResult LabAnalyticEngine::analyzeGainTones(const std::vector<std::vector<float>>& recordedPasses,
                                                      double sampleRate)
{
    auto data = analytics::DynamicsAnalytics::analyzeGainTones(recordedPasses, sampleRate);
    GainAnalysisResult res;
    res.gainDb = { data.gainDb.mean, data.gainDb.stdDev };
    res.snrDb = { data.snrDb.mean, data.snrDb.stdDev };
    return res;
}

CyclicModulatorAnalysisResult LabAnalyticEngine::analyzeCyclicModulator(const std::vector<std::vector<float>>& recordedPasses,
                                                                       double sampleRate)
{
    auto data = analytics::ModulationAnalytics::analyzeCyclicModulator(recordedPasses, sampleRate);
    CyclicModulatorAnalysisResult res;
    res.rateHz = { data.rateHz.mean, data.rateHz.stdDev };
    res.depthPercent = { data.depthPercent.mean, data.depthPercent.stdDev };
    res.asymmetry = { data.asymmetry.mean, data.asymmetry.stdDev };
    return res;
}

WienerHammersteinAnalysisResult LabAnalyticEngine::analyzeWienerHammerstein(
    const std::vector<std::vector<float>>& recordedPasses,
    const std::vector<float>& inputStimulus,
    double sampleRate)
{
    auto data = analytics::DynamicsAnalytics::analyzeWienerHammerstein(recordedPasses, inputStimulus, sampleRate);
    WienerHammersteinAnalysisResult res;
    res.nonLinearCoeffA = { data.nonLinearCoeffA.mean, data.nonLinearCoeffA.stdDev };
    res.preFilterCentroidHz = { data.preFilterCentroidHz.mean, data.preFilterCentroidHz.stdDev };
    res.postFilterCentroidHz = { data.postFilterCentroidHz.mean, data.postFilterCentroidHz.stdDev };
    res.goodnessOfFitR2 = { data.goodnessOfFitR2.mean, data.goodnessOfFitR2.stdDev };
    res.representativeH1 = std::move(data.representativeH1);
    res.representativeH2 = std::move(data.representativeH2);
    return res;
}

SingleTakeAnalysisResult LabAnalyticEngine::analyzeSingleTakeMultiplexed(const std::vector<float>& alignedAudio,
                                                                         double sampleRate,
                                                                         float transientDurationSec)
{
    return analytics::DynamicsAnalytics::analyzeSingleTakeMultiplexed(alignedAudio, sampleRate, transientDurationSec);
}

PreScanResult LabAnalyticEngine::applyPlateauCollapseFilter(const std::vector<PreScanPoint>& rawTrajectory, 
                                                           float noiseFloorVariance) noexcept
{
    return analytics::DynamicsAnalytics::applyPlateauCollapseFilter(rawTrajectory, noiseFloorVariance);
}

} // namespace abdaudiolab::math
