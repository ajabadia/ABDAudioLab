/**
 * @file DexedParametricCampaignContracts.h
 * @brief Contracts, specifications, and manifests for offline parametric measurement campaigns.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Explanatory role of a fixture parameter pair in metrological characterization.
 */
enum class FixtureRole
{
    CanonicalExploratoryPair,   /**< Designated baseline pair for exploratory bounds testing */
    ExhaustiveSweepPoint,       /**< Individual point within a complete parameter grid */
    ArbitrarySnapshot           /**< Ad-hoc or unclassified test point */
};

[[nodiscard]] inline std::string fixtureRoleToString(FixtureRole role)
{
    switch (role)
    {
        case FixtureRole::CanonicalExploratoryPair: return "canonical_pair";
        case FixtureRole::ExhaustiveSweepPoint:     return "sweep_point";
        case FixtureRole::ArbitrarySnapshot:        return "arbitrary";
    }
    return "unknown";
}

[[nodiscard]] inline FixtureRole fixtureRoleFromString(const std::string& str)
{
    if (str == "canonical_pair" || str == "canonical_exploratory_pair")
        return FixtureRole::CanonicalExploratoryPair;
    if (str == "sweep_point")
        return FixtureRole::ExhaustiveSweepPoint;
    return FixtureRole::ArbitrarySnapshot;
}

/**
 * @brief Traceable record of a single modified parameter, recording requested vs effective values.
 */
struct ParametricRecord
{
    std::string parameterName;
    double requestedValue { 0.0 };
    double effectiveValue { 0.0 };
    std::string parameterId;
    std::string stateSha256;
    FixtureRole fixtureRole { FixtureRole::CanonicalExploratoryPair };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "parameterName", parameterName },
            { "requestedValue", requestedValue },
            { "effectiveValue", effectiveValue },
            { "parameterId", parameterId },
            { "stateSha256", stateSha256 },
            { "fixtureRole", fixtureRoleToString(fixtureRole) }
        };
    }

    static ParametricRecord fromJson(const nlohmann::json& j)
    {
        ParametricRecord rec;
        rec.parameterName = j.value("parameterName", "");
        rec.requestedValue = j.value("requestedValue", 0.0);
        rec.effectiveValue = j.value("effectiveValue", 0.0);
        rec.parameterId = j.value("parameterId", "");
        rec.stateSha256 = j.value("stateSha256", "");
        rec.fixtureRole = fixtureRoleFromString(j.value("fixtureRole", "canonical_pair"));
        return rec;
    }
};

/**
 * @brief Factorial isolation campaign mode ensuring strict one-variable-at-a-time (OFAT) exploration.
 */
enum class ParametricCampaignType
{
    FactorialAlgorithm,             /**< Campaign A: Algorithm 1 vs 32, Feedback = 0 held constant */
    FactorialFeedback,              /**< Campaign B: Feedback 0 vs 7, Algorithm = 1 held constant */
    FactorialCrossed,               /**< Full factorial grid: Algorithm in {1, 32} x Feedback in {0, 7} */
    FmModulationIndex,              /**< Campaign C: Modulator output level 0..99 with constant carrier and ratio */
    FmFrequencyRatio,               /**< Campaign D: Frequency ratios (1.0 vs 2.0 vs 3.14) with constant level */
    FmTemporalCentroidTrajectory    /**< Campaign E: Time-varying spectral centroid C(t) across note envelope */
};

[[nodiscard]] inline std::string parametricCampaignTypeToString(ParametricCampaignType type)
{
    switch (type)
    {
        case ParametricCampaignType::FactorialAlgorithm:          return "factorial_algorithm";
        case ParametricCampaignType::FactorialFeedback:           return "factorial_feedback";
        case ParametricCampaignType::FactorialCrossed:            return "factorial_crossed";
        case ParametricCampaignType::FmModulationIndex:           return "fm_modulation_index";
        case ParametricCampaignType::FmFrequencyRatio:            return "fm_frequency_ratio";
        case ParametricCampaignType::FmTemporalCentroidTrajectory: return "fm_temporal_centroid_trajectory";
    }
    return "unknown";
}

/**
 * @brief STFT temporal analysis parameters for dynamic spectral characterization.
 */
struct StftAnalysisParameters
{
    int fftSize { 2048 };
    int hopSize { 512 };
    std::string windowFunction { "hann" };
    int windowLengthSamples { 2048 };
    double sampleRateHz { 48000.0 };
    double silenceFloorDbfs { -80.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "fftSize", fftSize },
            { "hopSize", hopSize },
            { "windowFunction", windowFunction },
            { "windowLengthSamples", windowLengthSamples },
            { "sampleRateHz", sampleRateHz },
            { "silenceFloorDbfs", silenceFloorDbfs }
        };
    }
};

/**
 * @brief Single discrete STFT frame in the temporal trajectory of spectral centroid C(t).
 */
struct FmTemporalCentroidFrame
{
    int frameIndex { 0 };
    double measurementWindowStartMs { 0.0 };
    double measurementWindowEndMs { 0.0 };
    double rmsDbfs { -96.0 };
    double silenceFloorDbfs { -80.0 };
    std::optional<double> spectralCentroidHz { std::nullopt }; /**< Absent on silence/unreliable */
    std::string status { "observed" };                         /**< "observed", "unreliable", "silent" */

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j{
            { "frameIndex", frameIndex },
            { "measurementWindowStartMs", measurementWindowStartMs },
            { "measurementWindowEndMs", measurementWindowEndMs },
            { "rmsDbfs", rmsDbfs },
            { "silenceFloorDbfs", silenceFloorDbfs },
            { "status", status }
        };
        if (spectralCentroidHz.has_value())
            j["spectralCentroidHz"] = *spectralCentroidHz;
        else
            j["spectralCentroidHz"] = nullptr;
        return j;
    }
};

/**
 * @brief Formal metrological observation of FM operator modulation, separating control proxies from physical beta.
 */
struct FmModulationObservation
{
    int operatorIndex { 2 };                    /**< Modulator operator index (e.g. OP2) */
    int requestedOutputLevel { 0 };             /**< DX7 integer level 0..99 */
    int effectiveOutputLevel { 0 };             /**< Verified plugin quantized level */
    std::string depthProxy { "operator_output_level" };

    double estimatedBeta { 0.0 };               /**< Physical modulation index beta = Delta f / f_m */
    std::string betaStatus { "not_estimated" }; /**< "not_estimated" or "estimated" */
    std::string betaMethod;                     /**< Declared estimation algorithm if applicable */

    double carrierRatioRequested { 1.0 };
    double carrierRatioEffective { 1.0 };
    double modulatorRatioRequested { 1.0 };
    double modulatorRatioEffective { 1.0 };

    double carrierFrequencyHz { 0.0 };
    double modulatorFrequencyHz { 0.0 };
    double observedRatio { 0.0 };
    std::string ratioClass { "not_observable" }; /**< "harmonic", "inharmonic", "not_observable" */

    double observedSidebandSpread { 0.0 };
    double observedCarrierSuppressionDb { 0.0 };

    MeasurementCurve sidebandCurve;
    MeasurementCurve centroidCurve;
    MeasurementCurve temporalCentroidCurve;     /**< C(t) trajectory across note frames */
    std::vector<FmTemporalCentroidFrame> temporalFrames;
    StftAnalysisParameters stftParameters;

    std::string stateSha256;
    std::string stimulusSha256;

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json tf = nlohmann::json::array();
        for (const auto& f : temporalFrames)
            tf.push_back(f.toJson());

        return nlohmann::json{
            { "operatorIndex", operatorIndex },
            { "requestedOutputLevel", requestedOutputLevel },
            { "effectiveOutputLevel", effectiveOutputLevel },
            { "depthProxy", depthProxy },
            { "estimatedBeta", estimatedBeta },
            { "betaStatus", betaStatus },
            { "betaMethod", betaMethod },
            { "carrierRatioRequested", carrierRatioRequested },
            { "carrierRatioEffective", carrierRatioEffective },
            { "modulatorRatioRequested", modulatorRatioRequested },
            { "modulatorRatioEffective", modulatorRatioEffective },
            { "carrierFrequencyHz", carrierFrequencyHz },
            { "modulatorFrequencyHz", modulatorFrequencyHz },
            { "observedRatio", observedRatio },
            { "ratioClass", ratioClass },
            { "observedSidebandSpread", observedSidebandSpread },
            { "observedCarrierSuppressionDb", observedCarrierSuppressionDb },
            { "stftParameters", stftParameters.toJson() },
            { "temporalFrames", tf },
            { "stateSha256", stateSha256 },
            { "stimulusSha256", stimulusSha256 }
        };
    }
};

/**
 * @brief Specification for a single variant in a parametric sweep campaign.
 */
struct ParametricVariantSpec
{
    std::string variantId;          /**< e.g. "var_algo_01_fb_0", "var_algo_32_fb_0" */
    std::string label;              /**< Human-readable description */
    std::vector<ParametricRecord> parameters;
    std::string fixtureRole { "canonical_pair" };
    std::string expectedStateSha256;
    int velocityPoints { 5 };       /**< Discrete velocity levels tested (e.g. 0, 32, 64, 96, 127) */
};

/**
 * @brief Campaign manifest documenting the complete set of generated variant containers.
 */
struct ParametricCampaignManifest
{
    std::string campaignId;
    ParametricCampaignType campaignType { ParametricCampaignType::FactorialAlgorithm };
    std::string schemaVersion { "abdaudiolab-fair-lnl-1.0" };
    std::string dutName { "Dexed.vst3" };
    std::string basePresetName { "INIT_VOICE" };
    std::string baseStateSha256;
    std::vector<ParametricVariantSpec> variants;
    std::string notes;

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json vars = nlohmann::json::array();
        for (const auto& v : variants)
        {
            nlohmann::json params = nlohmann::json::array();
            for (const auto& p : v.parameters)
                params.push_back(p.toJson());

            vars.push_back({
                { "variantId", v.variantId },
                { "label", v.label },
                { "parameters", params },
                { "fixtureRole", v.fixtureRole },
                { "expectedStateSha256", v.expectedStateSha256 },
                { "velocityPoints", v.velocityPoints }
            });
        }

        return nlohmann::json{
            { "campaignId", campaignId },
            { "campaignType", parametricCampaignTypeToString(campaignType) },
            { "schemaVersion", schemaVersion },
            { "dutName", dutName },
            { "basePresetName", basePresetName },
            { "baseStateSha256", baseStateSha256 },
            { "variants", vars },
            { "notes", notes }
        };
    }
};

} // namespace abdaudiolab::measurement
