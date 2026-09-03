#pragma once

#include <vector>
#include <cmath>
#include <string>
#include "FarinaDeconvolver.h"
#include "WienerHammersteinFitter.h"

namespace abdaudiolab::math
{

struct StatisticalPair
{
    float mean { 0.0f };   // mu: stable control behavior
    float stdDev { 0.0f }; // sigma: thermal/analog drift or ACB chaotic variance
};

struct FilterAnalysisResult
{
    StatisticalPair cutoffHz;
    StatisticalPair resonanceDb;
    StatisticalPair thdPercent;
    std::vector<float> frequencyCurveHz;
    std::vector<float> magnitudeCurveDb;
};

struct TimeDynamicAnalysisResult
{
    StatisticalPair attackTimeMs;
    StatisticalPair decayTimeMs;
    StatisticalPair sustainLevel;
    StatisticalPair releaseTimeMs;
    StatisticalPair delayTimeMs;
};

struct WaveShaperAnalysisResult
{
    StatisticalPair thdPercent;
    std::vector<float> transferCurveInput;
    std::vector<float> transferCurveOutput;
};

struct GainAnalysisResult
{
    StatisticalPair gainDb;
    StatisticalPair snrDb;
};

struct CyclicModulatorAnalysisResult
{
    StatisticalPair rateHz;
    StatisticalPair depthPercent;
    StatisticalPair asymmetry;
};

struct WienerHammersteinAnalysisResult
{
    StatisticalPair nonLinearCoeffA;      /**< 3rd-order nonlinearity 'a' (mean & stddev). */
    StatisticalPair preFilterCentroidHz;  /**< Input filter h1 centroid (Hz). */
    StatisticalPair postFilterCentroidHz; /**< Output filter h2 centroid (Hz). */
    StatisticalPair goodnessOfFitR2;      /**< R^2 model fit (0.0 to 1.0). */
    std::vector<float> representativeH1;  /**< Best-fit input filter impulse response. */
    std::vector<float> representativeH2;  /**< Best-fit output filter impulse response. */
};

/**
 * @brief Core mathematical and statistical analysis engine.
 */
class LabAnalyticEngine
{
public:
    LabAnalyticEngine() = default;
    ~LabAnalyticEngine() = default;

    static StatisticalPair calculateStatistics(const std::vector<float>& dataset);

    static float calculateSignalToNoiseRatioDb(const std::vector<float>& signalBuffer, float baselineNoiseRmsDb = -90.0f);
    static bool isMeasurementConfidenceAcceptable(float snrDb, float minThresholdDb = 18.0f) noexcept
    {
        return snrDb >= minThresholdDb;
    }

    static FilterAnalysisResult analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                   const std::vector<float>& inverseFilter,
                                                   double sampleRate,
                                                   double durationSec,
                                                   float startFreqHz,
                                                   float endFreqHz);

    static TimeDynamicAnalysisResult analyzeAdsrEnvelopes(const std::vector<std::vector<float>>& recordedPasses,
                                                         double sampleRate);

    static TimeDynamicAnalysisResult analyzeDelayImpulses(const std::vector<std::vector<float>>& recordedPasses,
                                                         double sampleRate);

    static WaveShaperAnalysisResult analyzeWaveShaperRamps(const std::vector<std::vector<float>>& recordedPasses,
                                                           double sampleRate);

    static GainAnalysisResult analyzeGainTones(const std::vector<std::vector<float>>& recordedPasses,
                                              double sampleRate);

    static CyclicModulatorAnalysisResult analyzeCyclicModulator(const std::vector<std::vector<float>>& recordedPasses,
                                                               double sampleRate);

    static WienerHammersteinAnalysisResult analyzeWienerHammerstein(const std::vector<std::vector<float>>& recordedPasses,
                                                                   const std::vector<float>& inputStimulus,
                                                                   double sampleRate);
};

} // namespace abdaudiolab::math
