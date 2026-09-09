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

struct SingleTakeAnalysisResult
{
    float transientPeakLevel { 0.0f };   // Peak amplitude in transient window
    float transientThd { 0.0f };         // Saturation / harmonic ratio proxy
    float sustainedRmsDb { -100.0f };    // RMS level in sustained window (dB)
    float peakResonanceHz { 1000.0f };   // Dominant frequency in sustained window (Hz)
    float decayTimeMs { 0.0f };          // Estimated decay time (ms)
    bool earlyStopped { false };         // Cut off by dynamic silence detector
};

struct PreScanPoint
{
    float controlValue  { 0.0f };   // MIDI value (0..127) or normalized (0.0..1.0)
    float timeSec       { 0.0f };   // Time position in the pre-scan sweep
    float primaryMetric { 0.0f };   // Measured metric (Magnitude dB, Cutoff Hz, or I/O amplitude)
    float thdPercent    { 0.0f };   // Instantaneous harmonic distortion
};

struct PreScanResult
{
    bool isLinear { false };                    // Flag from fast linear subtraction filter
    float residualRmsDb { -100.0f };            // Residual error after subtracting scaled stimulus
    float maxDerivative { 0.0f };               // Maximum first derivative
    std::vector<PreScanPoint> trajectory;       // Continuous measured trajectory
    std::vector<int> recommendedSteps;          // Precision adaptive sampling roadmap
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

    /**
     * @brief FASE B: Fast Linear Bypass Subtraction Filter.
     * Evaluates whether the device/submodule behaves purely linearly via least-squares scale fitting.
     */
    static PreScanResult evaluateLinearBypass(const std::vector<float>& recordedBuffer,
                                              const std::vector<float>& stimulusBuffer,
                                              float noiseFloorDb = -90.0f);

    /**
     * @brief FASE C: Gradient Analyzer for Adaptive Sampling.
     * Computes the derivative of pre-scan trajectory to inject high point density in non-linear corners/knees.
     */
    static void computeAdaptiveRoadmap(PreScanResult& result, float gradientThreshold = 0.05f);

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

    /**
     * @brief Single-Take Multi-Analysis: Extracts transient saturation and sustained spectral response
     *        from a single sample-accurate aligned recording.
     */
    static SingleTakeAnalysisResult analyzeSingleTakeMultiplexed(const std::vector<float>& alignedAudio,
                                                                 double sampleRate,
                                                                 float transientDurationSec = 0.2f);

    /**
     * @brief FASE B (Manual): Filtro de Colapso de Mesetas Temporales.
     * Escanea una trayectoria manual inestable, elimina las ventanas estáticas de parón humano
     * basándose en un umbral dinámico de ruido, y re-mapea uniformemente el tiempo.
     * 
     * @param rawTrajectory Trayectoria cruda grabada durante el barrido manual continuo.
     * @param noiseFloorVariance Desviación estándar (sigma) actual del ruido del sistema para calcular el umbral 3-sigma.
     * @return PreScanResult Contiene la trayectoria purificada y re-mapeada lista para interpolación 2D.
     */
    static PreScanResult applyPlateauCollapseFilter(const std::vector<PreScanPoint>& rawTrajectory, 
                                                    float noiseFloorVariance) noexcept;
};

} // namespace abdaudiolab::math
