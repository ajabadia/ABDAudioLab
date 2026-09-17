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

#include "../../measurement/MeasurementContracts.h"

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

/**
 * @struct FilterDetailedAnalysis
 * @brief Complete metrological characterization of filter magnitude, phase, Q, and harmonic response.
 */
struct FilterDetailedAnalysis
{
    measurement::FilterTopology topology { measurement::FilterTopology::unknown };
    measurement::MeasurementDomain domain { measurement::MeasurementDomain::directTransferFunction };

    double passbandGainDbfs { 0.0 };
    bool passbandObservable { false };

    measurement::CutoffMetrics cutoff;
    measurement::MeasurementMetric cutoffFrequencyHz; /**< Compatibility alias */

    measurement::MeasurementMetric resonancePeakHz;
    measurement::MeasurementMetric resonanceGainDb;

    measurement::MeasurementMetric rollOffSlopeDbPerOct;
    measurement::SlopeFitMetadata slopeFit;

    measurement::MeasurementMetric qFactor;

    measurement::MeasurementMetric passbandLatencyMs;
    measurement::MeasurementMetric thdPercent;
    measurement::MeasurementMetric h2Percent;
    measurement::MeasurementMetric h3Percent;

    std::vector<double> frequencyBinsHz;
    std::vector<double> magnitudeDb;
    std::vector<double> phaseRad;
    std::vector<double> groupDelayMs;
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

    /**
     * @brief Detailed metrological analysis of a deconvolved filter frequency response.
     * 
     * Guarantees:
     * - Does NOT force -3 dB cutoff on topologies where it is not applicable.
     * - Determines cutoff against passband plateau, not against resonance peak.
     * - Calculates roll-off slope in an asymptotic stopband region using log2 linear regression.
     * - Calculates Q factor only when two valid -3 dB crossings exist around resonance peak.
     * - Labels domain and metrics honestly (directTransferFunction vs synthesizedSpectralResponse).
     */
    static FilterDetailedAnalysis analyzeDeconvolvedFilter(
        const math::DeconvolutionResult& deco,
        double sampleRate,
        measurement::FilterTopology declaredTopology = measurement::FilterTopology::unknown,
        measurement::MeasurementDomain domain = measurement::MeasurementDomain::directTransferFunction);

    /**
     * @brief Detects nominal passband gain G_pass in dBFS.
     */
    static double detectPassbandGain(const std::vector<float>& freqs,
                                     const std::vector<float>& magsDb,
                                     measurement::FilterTopology topology,
                                     bool& outObservable);

    /**
     * @brief Extracts topologically-aware cutoff metrics.
     */
    static measurement::CutoffMetrics extractCutoffMetrics(
        const std::vector<float>& freqs,
        const std::vector<float>& magsDb,
        double passbandGainDb,
        measurement::FilterTopology topology);

    /**
     * @brief Extracts resonance peak frequency and boost in dB relative to passband.
     */
    static void extractResonancePeak(const std::vector<float>& freqs,
                                     const std::vector<float>& magsDb,
                                     double passbandGainDb,
                                     measurement::MeasurementMetric& outPeakHz,
                                     measurement::MeasurementMetric& outGainDb);

    /**
     * @brief Calculates asymptotic stopband roll-off slope (dB/oct) via log2 linear regression.
     */
    static measurement::MeasurementMetric calculateAsymptoticSlope(
        const std::vector<float>& freqs,
        const std::vector<float>& magsDb,
        double cutoffHz,
        double resonancePeakHz,
        double sampleRate,
        measurement::FilterTopology topology,
        measurement::SlopeFitMetadata& outFit);

    /**
     * @brief Calculates Q factor from -3 dB bandwidth around resonance peak.
     */
    static measurement::MeasurementMetric calculateQFactor(
        const std::vector<float>& freqs,
        const std::vector<float>& magsDb,
        double resonancePeakHz,
        double resonancePeakDb);
};

} // namespace abdaudiolab::math::analytics
