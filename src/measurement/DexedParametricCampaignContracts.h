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
    FmTemporalCentroidTrajectory,   /**< Campaign E: Time-varying spectral centroid C(t) across note envelope */
    KeyboardScaling                 /**< Campaign F: Segregated Keyboard Level and Rate Scaling sweeps */
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
        case ParametricCampaignType::KeyboardScaling:             return "keyboard_scaling";
    }
    return "unknown";
}

/**
 * @brief Method declared for physical FM modulation index (beta) estimation.
 */
enum class BetaEstimationMethod
{
    None,
    CarrierNullBesselJ0,
    SpectralEnergyRatio
};

[[nodiscard]] inline std::string betaEstimationMethodToString(BetaEstimationMethod m)
{
    switch (m)
    {
        case BetaEstimationMethod::None:                return "none";
        case BetaEstimationMethod::CarrierNullBesselJ0: return "carrier_null_bessel_j0";
        case BetaEstimationMethod::SpectralEnergyRatio: return "spectral_energy_ratio";
    }
    return "none";
}

[[nodiscard]] inline BetaEstimationMethod betaEstimationMethodFromString(const std::string& str)
{
    if (str == "carrier_null_bessel_j0") return BetaEstimationMethod::CarrierNullBesselJ0;
    if (str == "spectral_energy_ratio")  return BetaEstimationMethod::SpectralEnergyRatio;
    return BetaEstimationMethod::None;
}

/**
 * @brief 3-point local neighborhood evidencing a local carrier minimum for physical validation.
 */
struct CarrierNullNeighborhood
{
    double controlBefore { 0.0 };
    double carrierBeforeDbfs { 0.0 };

    double controlAtNull { 0.0 };
    double carrierAtNullDbfs { 0.0 };

    double controlAfter { 0.0 };
    double carrierAfterDbfs { 0.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "controlBefore", controlBefore },
            { "carrierBeforeDbfs", carrierBeforeDbfs },
            { "controlAtNull", controlAtNull },
            { "carrierAtNullDbfs", carrierAtNullDbfs },
            { "controlAfter", controlAfter },
            { "carrierAfterDbfs", carrierAfterDbfs }
        };
    }

    static CarrierNullNeighborhood fromJson(const nlohmann::json& j)
    {
        CarrierNullNeighborhood n;
        n.controlBefore = j.value("controlBefore", 0.0);
        n.carrierBeforeDbfs = j.value("carrierBeforeDbfs", 0.0);
        n.controlAtNull = j.value("controlAtNull", 0.0);
        n.carrierAtNullDbfs = j.value("carrierAtNullDbfs", 0.0);
        n.controlAfter = j.value("controlAfter", 0.0);
        n.carrierAfterDbfs = j.value("carrierAfterDbfs", 0.0);
        return n;
    }
};

/**
 * @brief Metrological observation of Bessel carrier null for physical FM beta inference.
 */
struct BetaNullObservation
{
    std::string measurand { "modulation_index_beta" };
    std::string referenceModel { "bessel_J0_carrier_null" };
    BetaEstimationMethod method { BetaEstimationMethod::None };
    std::string status { "not_estimated" }; /**< "estimated", "not_estimated", "unreliable" */

    double estimatedBeta { 0.0 };
    int nullOrder { 0 };
    double carrierSuppressionDb { 0.0 };
    double carrierLevelBeforeDbfs { 0.0 };
    double carrierLevelAtNullDbfs { 0.0 };
    double carrierLevelAfterDbfs { 0.0 };
    CarrierNullNeighborhood localNeighborhood;
    double nullConfidence { 0.0 };
    double resolution { 0.01 };
    std::string uncertaintyStatus { "not_estimated" };
    double uncertaintyOrResolution { 0.01 };

    int controlValueAtNull { 0 };
    double observedCarrierFrequencyHz { 0.0 };
    double observedModulationFrequencyHz { 0.0 };
    double modulationFrequencyHz { 0.0 };
    std::string reason;

    bool betaEstimatedFromObservedNull { false };
    double carrierNullThresholdDb { 24.0 };
    double carrierNullSearchToleranceDb { 3.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "measurand", measurand },
            { "referenceModel", referenceModel },
            { "method", betaEstimationMethodToString(method) },
            { "betaMethod", betaEstimationMethodToString(method) },
            { "status", status },
            { "betaStatus", status },
            { "estimatedBeta", estimatedBeta },
            { "nullOrder", nullOrder },
            { "carrierSuppressionDb", carrierSuppressionDb },
            { "carrierLevelBeforeDbfs", carrierLevelBeforeDbfs },
            { "carrierLevelAtNullDbfs", carrierLevelAtNullDbfs },
            { "carrierLevelAfterDbfs", carrierLevelAfterDbfs },
            { "localNeighborhood", localNeighborhood.toJson() },
            { "nullConfidence", nullConfidence },
            { "resolution", resolution },
            { "uncertaintyStatus", uncertaintyStatus },
            { "uncertaintyOrResolution", uncertaintyOrResolution },
            { "controlValueAtNull", controlValueAtNull },
            { "nullControlValue", controlValueAtNull },
            { "observedCarrierFrequencyHz", observedCarrierFrequencyHz },
            { "observedModulationFrequencyHz", observedModulationFrequencyHz },
            { "modulationFrequencyHz", modulationFrequencyHz },
            { "reason", reason },
            { "betaEstimatedFromObservedNull", betaEstimatedFromObservedNull },
            { "carrierNullThresholdDb", carrierNullThresholdDb },
            { "carrierNullSearchToleranceDb", carrierNullSearchToleranceDb }
        };
    }

    static BetaNullObservation fromJson(const nlohmann::json& j)
    {
        BetaNullObservation o;
        o.measurand = j.value("measurand", "modulation_index_beta");
        o.referenceModel = j.value("referenceModel", "bessel_J0_carrier_null");
        std::string mStr = j.value("betaMethod", j.value("method", "none"));
        o.method = betaEstimationMethodFromString(mStr);
        o.status = j.value("betaStatus", j.value("status", "not_estimated"));
        o.estimatedBeta = j.value("estimatedBeta", 0.0);
        o.nullOrder = j.value("nullOrder", 0);
        o.carrierSuppressionDb = j.value("carrierSuppressionDb", 0.0);
        o.carrierLevelBeforeDbfs = j.value("carrierLevelBeforeDbfs", 0.0);
        o.carrierLevelAtNullDbfs = j.value("carrierLevelAtNullDbfs", 0.0);
        o.carrierLevelAfterDbfs = j.value("carrierLevelAfterDbfs", 0.0);
        if (j.contains("localNeighborhood"))
            o.localNeighborhood = CarrierNullNeighborhood::fromJson(j["localNeighborhood"]);
        o.nullConfidence = j.value("nullConfidence", 0.0);
        o.resolution = j.value("resolution", 0.01);
        o.uncertaintyStatus = j.value("uncertaintyStatus", "not_estimated");
        o.uncertaintyOrResolution = j.value("uncertaintyOrResolution", o.resolution);
        o.controlValueAtNull = j.value("nullControlValue", j.value("controlValueAtNull", 0));
        o.observedCarrierFrequencyHz = j.value("observedCarrierFrequencyHz", 0.0);
        o.observedModulationFrequencyHz = j.value("observedModulationFrequencyHz", j.value("modulationFrequencyHz", 0.0));
        o.modulationFrequencyHz = o.observedModulationFrequencyHz;
        o.reason = j.value("reason", "");
        o.betaEstimatedFromObservedNull = j.value("betaEstimatedFromObservedNull", false);
        o.carrierNullThresholdDb = j.value("carrierNullThresholdDb", 24.0);
        o.carrierNullSearchToleranceDb = j.value("carrierNullSearchToleranceDb", 3.0);
        return o;
    }
};

/**
 * @brief DUT parameter binding mapping abstract measurement properties to native target IDs.
 */
struct TargetParameterBinding
{
    std::string logicalName;
    std::string nativeId;
    std::string unit;
    std::string mappingVersion { "1.0" };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "logicalName", logicalName },
            { "nativeId", nativeId },
            { "unit", unit },
            { "mappingVersion", mappingVersion }
        };
    }
};

[[nodiscard]] inline nlohmann::json dutIdentityToJson(const DutIdentity& dut)
{
    return nlohmann::json{
        { "name", dut.name },
        { "format", dut.format },
        { "version", dut.version },
        { "type", dut.type },
        { "dutType", dut.dutType },
        { "vendor", dut.vendor },
        { "model", dut.model },
        { "instanceId", dut.instanceId },
        { "binarySha256", dut.binarySha256 },
        { "firmwareSha256", dut.firmwareSha256 },
        { "stateSha256", dut.stateSha256 },
        { "interfaceId", dut.interfaceId }
    };
}

/**
 * @brief Discrete point in a carrier level sweep across modulator output levels.
 */
struct CarrierSweepPoint
{
    int controlValue { 0 };
    double carrierLevelDbfs { 0.0 };
    bool sidebandsObservable { true };
    double modulationFrequencyHz { 0.0 };
    bool ratioCompatible { true };
    bool hasClipping { false };
    bool hasInsufficientSignal { false };
    bool spectrumContaminated { false };
};

/**
 * @brief Configuration parameters for Bessel carrier null estimation.
 */
struct CarrierNullEstimationConfig
{
    double carrierNullThresholdDb { 24.0 };
    double carrierNullSearchToleranceDb { 3.0 };
    int targetNullOrder { 1 };
    double baselineCarrierDbfs { -6.0 };
};

/**
 * @brief Keyboard level scaling component data isolating breakpoint, curves, depths and output level.
 */
struct KeyboardLevelScalingRecord
{
    int breakpoint { 60 };
    std::string leftCurve { "-LIN" };
    std::string rightCurve { "-LIN" };
    int leftDepth { 0 };
    int rightDepth { 0 };
    int effectiveOutputLevel { 0 };
    double spectralCentroidHz { 0.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "breakpoint", breakpoint },
            { "leftCurve", leftCurve },
            { "rightCurve", rightCurve },
            { "leftDepth", leftDepth },
            { "rightDepth", rightDepth },
            { "effectiveOutputLevel", effectiveOutputLevel },
            { "spectralCentroidHz", spectralCentroidHz }
        };
    }
};

/**
 * @brief Keyboard rate scaling component data isolating rate scaling factor and envelope duration.
 */
struct KeyboardRateScalingRecord
{
    int rateScaling { 0 };
    double attackTimeMs { 0.0 };
    double releaseTimeMs { 0.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "rateScaling", rateScaling },
            { "attackTimeMs", attackTimeMs },
            { "releaseTimeMs", releaseTimeMs }
        };
    }
};

/**
 * @brief Single observation point in a keyboard scaling campaign, holding both segregated models and direct fields.
 */
struct KeyboardScalingPointRecord
{
    int note { 60 };
    int breakpoint { 60 };
    std::string leftCurve { "-LIN" };
    std::string rightCurve { "-LIN" };
    int leftDepth { 0 };
    int rightDepth { 0 };
    int rateScaling { 0 };
    double attackTimeMs { 0.0 };
    double releaseTimeMs { 0.0 };
    int effectiveOutputLevel { 0 };
    double spectralCentroidHz { 0.0 };

    KeyboardLevelScalingRecord keyboardLevelScaling;
    KeyboardRateScalingRecord keyboardRateScaling;

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "note", note },
            { "breakpoint", breakpoint },
            { "leftCurve", leftCurve },
            { "rightCurve", rightCurve },
            { "leftDepth", leftDepth },
            { "rightDepth", rightDepth },
            { "rateScaling", rateScaling },
            { "attackTimeMs", attackTimeMs },
            { "releaseTimeMs", releaseTimeMs },
            { "effectiveOutputLevel", effectiveOutputLevel },
            { "spectralCentroidHz", spectralCentroidHz },
            { "keyboardLevelScaling", keyboardLevelScaling.toJson() },
            { "keyboardRateScaling", keyboardRateScaling.toJson() }
        };
    }
};

/**
 * @brief Results collection for a complete Keyboard Scaling sweep.
 */
struct KeyboardScalingCampaignResult
{
    std::string campaignId;
    std::vector<KeyboardScalingPointRecord> points;
    std::string schemaVersion { "abdaudiolab-fair-lnl-1.0" };

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json pts = nlohmann::json::array();
        for (const auto& p : points)
            pts.push_back(p.toJson());

        return nlohmann::json{
            { "campaignId", campaignId },
            { "points", pts },
            { "schemaVersion", schemaVersion }
        };
    }
};

/**
 * @brief Descriptive item for deterministic batch sorting of variants.
 */
struct BatchVariantItem
{
    std::string variantId;
    int note { 60 };
    int operatorLevel { 0 };
    double ratio { 1.0 };
    std::string stateHash;
    std::string stimulusHash;
    std::string containerPath;
};

/**
 * @brief Comparator enforcing deterministic ordering: keyboard note ascending -> operator level ascending -> ratio ascending.
 */
inline bool compareBatchVariantItems(const BatchVariantItem& a, const BatchVariantItem& b)
{
    if (a.note != b.note)
        return a.note < b.note;
    if (a.operatorLevel != b.operatorLevel)
        return a.operatorLevel < b.operatorLevel;
    if (std::abs(a.ratio - b.ratio) > 1e-6)
        return a.ratio < b.ratio;
    return a.variantId < b.variantId;
}

/**
 * @brief Top-level campaign manifest for batch container exports.
 */
struct BatchCampaignManifest
{
    std::string campaignId;
    std::string campaignType;
    std::vector<std::string> orderedVariantIds;
    std::vector<std::string> variantStateHashes;
    std::vector<std::string> variantStimulusHashes;
    std::vector<std::string> containerPaths;
    std::string schemaVersion { "abdaudiolab-fair-lnl-1.0" };
    std::string rulesVersion { "20.11.5" };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "campaignId", campaignId },
            { "campaignType", campaignType },
            { "orderedVariantIds", orderedVariantIds },
            { "variantStateHashes", variantStateHashes },
            { "variantStimulusHashes", variantStimulusHashes },
            { "containerPaths", containerPaths },
            { "schemaVersion", schemaVersion },
            { "rulesVersion", rulesVersion }
        };
    }
};

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
    std::string betaStatus { "not_estimated" }; /**< "not_estimated", "estimated", "unreliable" */
    std::string betaMethod;                     /**< Declared estimation algorithm if applicable */
    BetaNullObservation betaNullObservation;    /**< Physical Bessel null observation details */

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
            { "betaNullObservation", betaNullObservation.toJson() },
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
