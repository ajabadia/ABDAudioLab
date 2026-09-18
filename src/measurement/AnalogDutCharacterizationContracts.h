/**
 * @file AnalogDutCharacterizationContracts.h
 * @brief Canonical contracts for Analog DUT Characterization: Frequency Response,
 *        Harmonic Distortion (THD IEEE/IEC), Two-Tone IMD, and Level Sweep / Clipping Thresholds.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "FineLatencyContracts.h"
#include "MeasurementDspUtils.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <optional>
#include <cmath>

namespace abdaudiolab::measurement
{

/**
 * @brief Total Harmonic Distortion (THD) denominator reference convention.
 */
enum class ThdConvention
{
    FundamentalReferenced, /**< IEEE standard: Denominator is pure fundamental amplitude V1 */
    TotalRmsReferenced     /**< IEC standard: Denominator is total RMS of fundamental + harmonics */
};

[[nodiscard]] inline std::string thdConventionToString(ThdConvention conv) noexcept
{
    switch (conv)
    {
        case ThdConvention::FundamentalReferenced: return "fundamental_referenced_ieee";
        case ThdConvention::TotalRmsReferenced:     return "total_rms_referenced_iec";
        default:                                   return "unknown";
    }
}

/**
 * @brief Intermodulation Distortion (IMD) standard excitation and analysis strategy.
 */
enum class ImdConvention
{
    Smpte, /**< SMPTE standard: 60 Hz + 7 kHz (4:1 amplitude ratio). Analyzes f2 ± f1, f2 ± 2f1 */
    Din,   /**< DIN standard: 250 Hz + 8 kHz (4:1 amplitude ratio) */
    Ccif,  /**< CCIF / ITU-R twin-tone: 19 kHz + 20 kHz (1:1 amplitude ratio). Analyzes f2 - f1, 2f1 - f2, 2f2 - f1 */
    ItuR,  /**< Equivalent to CCIF twin-tone */
    Custom /**< Arbitrary user-defined two-tone excitation */
};

[[nodiscard]] inline std::string imdConventionToString(ImdConvention conv) noexcept
{
    switch (conv)
    {
        case ImdConvention::Smpte:  return "smpte";
        case ImdConvention::Din:    return "din";
        case ImdConvention::Ccif:   return "ccif";
        case ImdConvention::ItuR:   return "itu_r";
        case ImdConvention::Custom: return "custom";
        default:                    return "unknown";
    }
}

/**
 * @brief Individual harmonic component measured relative to the fundamental.
 */
struct HarmonicComponent
{
    int harmonicOrder { 2 };
    double expectedFrequencyHz { 0.0 };
    double measuredFrequencyHz { 0.0 };
    double measuredBin { 0.0 };
    double linearMagnitude { 0.0 };
    double rmsAmplitude { 0.0 };
    double levelDbc { -120.0 }; /**< Decibels relative to carrier fundamental */
    double phaseRad { 0.0 };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["expectedFrequencyHz"] = expectedFrequencyHz;
        j["harmonicOrder"] = harmonicOrder;
        j["levelDbc"] = levelDbc;
        j["linearMagnitude"] = linearMagnitude;
        j["measuredBin"] = measuredBin;
        j["measuredFrequencyHz"] = measuredFrequencyHz;
        j["phaseRad"] = phaseRad;
        j["rmsAmplitude"] = rmsAmplitude;
        return j;
    }
};

/**
 * @brief Complete structured outcome of a harmonic distortion analysis.
 */
struct HarmonicDistortionResult
{
    double fundamentalFrequencyHz { 1000.0 };
    double measuredFundamentalHz { 1000.0 };
    double fundamentalRms { 0.0 };
    double fundamentalMagnitudeDbfs { -120.0 };
    double totalHarmonicsRms { 0.0 };

    ThdConvention convention { ThdConvention::FundamentalReferenced };
    double thdRatio { 0.0 };
    double thdPercent { 0.0 };
    double thdDb { -120.0 };

    // Parallel metrics for cross-norm transparency
    double thdFundamentalReferencedPercent { 0.0 };
    double thdTotalRmsReferencedPercent { 0.0 };

    std::string windowType { "Hann" };
    double coherentGain { 0.50 };
    int integrationBandwidthBins { 2 };
    std::string spectrumConvention { "one_sided_coherent_gain_corrected" };

    std::vector<HarmonicComponent> harmonics;
    std::string status { "resolved" }; /**< "resolved", "insufficient_signal", "ambiguous_fundamental" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["coherentGain"] = coherentGain;
        j["convention"] = thdConventionToString(convention);
        j["fundamentalFrequencyHz"] = fundamentalFrequencyHz;
        j["fundamentalMagnitudeDbfs"] = fundamentalMagnitudeDbfs;
        j["fundamentalRms"] = fundamentalRms;

        nlohmann::ordered_json hArr = nlohmann::ordered_json::array();
        for (const auto& h : harmonics)
            hArr.push_back(h.toCanonicalJson());
        j["harmonics"] = hArr;

        j["integrationBandwidthBins"] = integrationBandwidthBins;
        j["measuredFundamentalHz"] = measuredFundamentalHz;
        j["spectrumConvention"] = spectrumConvention;
        j["status"] = status;
        j["thdDb"] = thdDb;
        j["thdFundamentalReferencedPercent"] = thdFundamentalReferencedPercent;
        j["thdPercent"] = thdPercent;
        j["thdRatio"] = thdRatio;
        j["thdTotalRmsReferencedPercent"] = thdTotalRmsReferencedPercent;
        j["totalHarmonicsRms"] = totalHarmonicsRms;
        j["windowType"] = windowType;
        return j;
    }
};

/**
 * @brief Discrete intermodulation product component.
 */
struct IntermodulationProduct
{
    double frequencyHz { 0.0 };
    int order { 2 }; /**< 2nd order (e.g. f2 - f1) or 3rd order (e.g. 2f1 - f2) */
    double linearMagnitude { 0.0 };
    double rmsAmplitude { 0.0 };
    double levelDbc { -120.0 }; /**< Decibels relative to reference tone */
    std::string productLabel; /**< e.g. "f2 - f1", "f2 + f1", "2f1 - f2", "2f2 - f1" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["frequencyHz"] = frequencyHz;
        j["levelDbc"] = levelDbc;
        j["linearMagnitude"] = linearMagnitude;
        j["order"] = order;
        j["productLabel"] = productLabel;
        j["rmsAmplitude"] = rmsAmplitude;
        return j;
    }
};

/**
 * @brief Complete structured outcome of a two-tone IMD analysis.
 */
struct IntermodulationResult
{
    ImdConvention convention { ImdConvention::Smpte };
    double f1Hz { 60.0 };
    double f2Hz { 7000.0 };
    double f1Rms { 0.0 };
    double f2Rms { 0.0 };
    std::string referenceTone { "f2_high_carrier" }; /**< "f2_high_carrier" for SMPTE, "equal_split_power" for CCIF */

    double totalImdPercent { 0.0 };
    double totalImdDb { -120.0 };
    double d2Percent { 0.0 }; /**< 2nd order products energy percentage */
    double d3Percent { 0.0 }; /**< 3rd order products energy percentage */

    std::vector<IntermodulationProduct> products;
    std::string status { "resolved" }; /**< "resolved", "insufficient_signal", "tones_unresolved" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["convention"] = imdConventionToString(convention);
        j["d2Percent"] = d2Percent;
        j["d3Percent"] = d3Percent;
        j["f1Hz"] = f1Hz;
        j["f1Rms"] = f1Rms;
        j["f2Hz"] = f2Hz;
        j["f2Rms"] = f2Rms;

        nlohmann::ordered_json pArr = nlohmann::ordered_json::array();
        for (const auto& p : products)
            pArr.push_back(p.toCanonicalJson());
        j["products"] = pArr;

        j["referenceTone"] = referenceTone;
        j["status"] = status;
        j["totalImdDb"] = totalImdDb;
        j["totalImdPercent"] = totalImdPercent;
        return j;
    }
};

/**
 * @brief Discrete point measured during an input-output level sweep.
 */
struct LevelSweepPoint
{
    double inputLevelDbfs { -40.0 };
    double outputLevelDbfs { -40.0 };
    double gainDb { 0.0 };
    double thdPercent { 0.0 };
    bool hardClipDetectedInCapture { false };
    bool nonlinearDistortionDetected { false };
    bool gainCompressionDetected { false };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["gainCompressionDetected"] = gainCompressionDetected;
        j["gainDb"] = gainDb;
        j["hardClipDetectedInCapture"] = hardClipDetectedInCapture;
        j["inputLevelDbfs"] = inputLevelDbfs;
        j["nonlinearDistortionDetected"] = nonlinearDistortionDetected;
        j["outputLevelDbfs"] = outputLevelDbfs;
        j["thdPercent"] = thdPercent;
        return j;
    }
};

/**
 * @brief Structured outcome of level sweep and clipping threshold extraction.
 */
struct ClippingThresholdResult
{
    std::optional<double> thd1PercentInputDbfs;
    std::optional<double> thd3PercentInputDbfs;
    std::optional<double> p1dbInputDbfs;
    double smallSignalGainDb { 0.0 };
    bool hardClippingObserved { false };
    std::string interpolationMethod { "linear_monotone" };
    std::vector<LevelSweepPoint> sweepPoints;
    std::string status { "resolved" }; /**< "resolved", "insufficient_points", "no_compression_observed" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["hardClippingObserved"] = hardClippingObserved;
        j["interpolationMethod"] = interpolationMethod;
        if (p1dbInputDbfs.has_value()) j["p1dbInputDbfs"] = *p1dbInputDbfs;
        else j["p1dbInputDbfs"] = nullptr;
        j["smallSignalGainDb"] = smallSignalGainDb;
        j["status"] = status;

        nlohmann::ordered_json pts = nlohmann::ordered_json::array();
        for (const auto& pt : sweepPoints)
            pts.push_back(pt.toCanonicalJson());
        j["sweepPoints"] = pts;

        if (thd1PercentInputDbfs.has_value()) j["thd1PercentInputDbfs"] = *thd1PercentInputDbfs;
        else j["thd1PercentInputDbfs"] = nullptr;
        if (thd3PercentInputDbfs.has_value()) j["thd3PercentInputDbfs"] = *thd3PercentInputDbfs;
        else j["thd3PercentInputDbfs"] = nullptr;
        return j;
    }
};

/**
 * @brief Linear frequency response extracted via Farina logarithmic sweep deconvolution.
 */
struct FrequencyResponseResult
{
    double sampleRateHz { 48000.0 };
    double sweepDurationSec { 1.0 };
    double startFrequencyHz { 20.0 };
    double endFrequencyHz { 20000.0 };

    double peakFrequencyHz { 0.0 };
    double peakMagnitudeDb { 0.0 };
    double deconvolutionThdPercent { 0.0 };

    std::vector<float> frequenciesHz;
    std::vector<float> magnitudeDb;
    std::vector<float> phaseRad;
    std::vector<float> groupDelaySamples;

    // Harmonic impulse response energy fractions
    double h2FractionPercent { 0.0 };
    double h3FractionPercent { 0.0 };
    double h4FractionPercent { 0.0 };
    double h5FractionPercent { 0.0 };

    std::string method { "farina_swept_sine_deconvolution" };
    std::string status { "resolved" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["deconvolutionThdPercent"] = deconvolutionThdPercent;
        j["endFrequencyHz"] = endFrequencyHz;
        j["h2FractionPercent"] = h2FractionPercent;
        j["h3FractionPercent"] = h3FractionPercent;
        j["h4FractionPercent"] = h4FractionPercent;
        j["h5FractionPercent"] = h5FractionPercent;
        j["method"] = method;
        j["peakFrequencyHz"] = peakFrequencyHz;
        j["peakMagnitudeDb"] = peakMagnitudeDb;
        j["pointCount"] = frequenciesHz.size();
        j["sampleRateHz"] = sampleRateHz;
        j["startFrequencyHz"] = startFrequencyHz;
        j["status"] = status;
        j["sweepDurationSec"] = sweepDurationSec;
        return j;
    }
};

/**
 * @brief Comprehensive characterization record for Analog Device Under Test.
 */
struct AnalogDutCharacterizationRecord
{
    std::string characterizationId;
    std::string dutName;
    std::string channelSetup { "stereo_loopback_and_dut" };
    double sampleRateHz { 48000.0 };

    // Latency & Time alignment (from T20.12-1)
    FineLatencyCalibrationRecord latencyRecord;

    // Analog Metrology Results
    FrequencyResponseResult frequencyResponse;
    HarmonicDistortionResult harmonicDistortion;
    IntermodulationResult intermodulation;
    ClippingThresholdResult clippingThresholds;

    // Fixity & Integrity
    std::string rawStimulusSha256;
    std::string rawResponseSha256;
    std::string status { "resolved" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["channelSetup"] = channelSetup;
        j["characterizationId"] = characterizationId;
        j["clippingThresholds"] = clippingThresholds.toCanonicalJson();
        j["dutName"] = dutName;
        j["frequencyResponse"] = frequencyResponse.toCanonicalJson();
        j["harmonicDistortion"] = harmonicDistortion.toCanonicalJson();
        j["intermodulation"] = intermodulation.toCanonicalJson();
        j["latencyRecord"] = latencyRecord.toCanonicalJson();
        j["rawResponseSha256"] = rawResponseSha256;
        j["rawStimulusSha256"] = rawStimulusSha256;
        j["sampleRateHz"] = sampleRateHz;
        j["status"] = status;
        return j;
    }
};

} // namespace abdaudiolab::measurement
