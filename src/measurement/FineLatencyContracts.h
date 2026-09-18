/**
 * @file FineLatencyContracts.h
 * @brief Canonical contracts for fine sub-sample latency, clock drift, multichannel skew,
 *        and non-destructive compensation views for Phase 20.12 (T20.12-1).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <vector>
#include <optional>
#include <cmath>

namespace abdaudiolab::measurement
{

/**
 * @brief Sub-sample latency refinement method.
 */
enum class SubsampleRefinementMethod
{
    ParabolicThreePoint, /**< 3-point parabolic peak interpolation (method-dependent estimate) */
    PhaseSlope,          /**< Spectral phase slope fitting across passband (reserved) */
    SincLocalFit,        /**< Local bandlimited sinc interpolation (reserved) */
    NotApplied           /**< Integer sample resolution only */
};

[[nodiscard]] inline std::string subsampleRefinementMethodToString(SubsampleRefinementMethod method) noexcept
{
    switch (method)
    {
        case SubsampleRefinementMethod::ParabolicThreePoint: return "parabolic_three_point";
        case SubsampleRefinementMethod::PhaseSlope:          return "phase_slope";
        case SubsampleRefinementMethod::SincLocalFit:        return "sinc_local_fit";
        case SubsampleRefinementMethod::NotApplied:          return "not_applied";
        default:                                             return "unknown";
    }
}

/**
 * @brief Clock architecture between emission (stimulus) and reception (capture).
 */
enum class ClockTopology
{
    SharedClock,        /**< Shared converter master clock (loopback or common audio interface) */
    IndependentClocks,  /**< Independent asynchronous clock sources (DUT with dedicated clock / separate devices) */
    Unknown             /**< Clock topology not asserted or determined */
};

[[nodiscard]] inline std::string clockTopologyToString(ClockTopology topology) noexcept
{
    switch (topology)
    {
        case ClockTopology::SharedClock:       return "shared_clock";
        case ClockTopology::IndependentClocks: return "independent_clocks";
        case ClockTopology::Unknown:           return "unknown";
        default:                               return "unknown";
    }
}

/**
 * @brief Interpretation assigned to observed clock drift.
 */
enum class DriftInterpretation
{
    RelativeClockDrift, /**< Real physical clock rate mismatch between independent clocks */
    ResidualJitter,     /**< Residual converter/host phase jitter around nominal 0 ppm drift */
    NotIdentifiable     /**< Insufficient duration, low coherence or invalid fit */
};

[[nodiscard]] inline std::string driftInterpretationToString(DriftInterpretation interp) noexcept
{
    switch (interp)
    {
        case DriftInterpretation::RelativeClockDrift: return "relative_clock_drift";
        case DriftInterpretation::ResidualJitter:     return "residual_jitter";
        case DriftInterpretation::NotIdentifiable:    return "not_identifiable";
        default:                                      return "not_identifiable";
    }
}

/**
 * @brief Fit quality status for linear clock drift modeling.
 */
enum class LinearFitStatus
{
    LinearFit,            /**< Stable linear trajectory with validated residuals */
    InsufficientSpan,     /**< Time span too short to resolve ppm drift over jitter floor */
    NonlinearDrift,       /**< Non-linear time drift trajectory detected */
    OutlierContaminated,  /**< Outliers detected and handled; fit degraded */
    Invalid               /**< Fit failed, excessive discontinuities or unresolvable */
};

[[nodiscard]] inline std::string linearFitStatusToString(LinearFitStatus status) noexcept
{
    switch (status)
    {
        case LinearFitStatus::LinearFit:           return "linear_fit";
        case LinearFitStatus::InsufficientSpan:    return "insufficient_span";
        case LinearFitStatus::NonlinearDrift:      return "nonlinear_drift";
        case LinearFitStatus::OutlierContaminated: return "outlier_contaminated";
        case LinearFitStatus::Invalid:             return "invalid";
        default:                                   return "invalid";
    }
}

/**
 * @brief Non-destructive compensation transformation type.
 */
enum class CompensationTransformationType
{
    TimeAxisOnly,         /**< Virtual time origin shift; raw audio samples unaltered */
    IntegerShift,         /**< Sample index shift */
    FractionalResampling  /**< Bandlimited fractional sinc/polyphase resampling */
};

[[nodiscard]] inline std::string compensationTransformationTypeToString(CompensationTransformationType type) noexcept
{
    switch (type)
    {
        case CompensationTransformationType::TimeAxisOnly:        return "time_axis_only";
        case CompensationTransformationType::IntegerShift:        return "integer_shift";
        case CompensationTransformationType::FractionalResampling: return "fractional_resampling";
        default:                                                  return "unknown";
    }
}

/**
 * @brief Discontinuity or dropout detected along the capture timeline.
 */
struct TimingDiscontinuity
{
    double elapsedTimeSeconds { 0.0 };
    double jumpSamples { 0.0 };
    std::string type { "dropout" }; /**< "dropout", "discontinuity", "clock_reset", "unknown" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["elapsedTimeSeconds"] = elapsedTimeSeconds;
        j["jumpSamples"] = jumpSamples;
        j["type"] = type;
        return j;
    }
};

/**
 * @brief Single local temporal offset observation extracted within a specific time window.
 */
struct ClockDriftObservation
{
    double elapsedTimeSeconds { 0.0 };
    int integerOffsetSamples { 0 };
    double localOffsetFractionalSamples { 0.0 };
    double localOffsetSamples { 0.0 }; /**< integer + fractional local offset */
    double correlationConfidence { 0.0 }; /**< Normalized cross-correlation peak magnitude [0.0, 1.0] */
    double estimatedErrorBoundSamples { 0.05 }; /**< Bounded uncertainty for this window */
    bool isOutlier { false };
    std::string method { "parabolic_three_point" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["correlationConfidence"] = correlationConfidence;
        j["elapsedTimeSeconds"] = elapsedTimeSeconds;
        j["estimatedErrorBoundSamples"] = estimatedErrorBoundSamples;
        j["integerOffsetSamples"] = integerOffsetSamples;
        j["isOutlier"] = isOutlier;
        j["localOffsetFractionalSamples"] = localOffsetFractionalSamples;
        j["localOffsetSamples"] = localOffsetSamples;
        j["method"] = method;
        return j;
    }
};

/**
 * @brief Inter-channel alignment and phase skew relative to a reference channel (Channel 0).
 */
struct ChannelSkewObservation
{
    int channelIndex { 1 };
    int referenceChannelIndex { 0 };
    double skewSamples { 0.0 };
    double skewNanoseconds { 0.0 };
    std::optional<double> phaseAngleRad;
    double phaseReferenceFrequencyHz { 1000.0 };
    std::string phaseConvention { "lag_positive" };
    bool polarityInverted { false };
    double confidence { 0.0 };
    std::string status { "resolved" }; /**< "resolved", "low_coherence", "ambiguous_skew", "polarity_inverted" */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["channelIndex"] = channelIndex;
        j["confidence"] = confidence;
        if (phaseAngleRad.has_value()) j["phaseAngleRad"] = *phaseAngleRad;
        else j["phaseAngleRad"] = nullptr;
        j["phaseConvention"] = phaseConvention;
        j["phaseReferenceFrequencyHz"] = phaseReferenceFrequencyHz;
        j["polarityInverted"] = polarityInverted;
        j["referenceChannelIndex"] = referenceChannelIndex;
        j["skewNanoseconds"] = skewNanoseconds;
        j["skewSamples"] = skewSamples;
        j["status"] = status;
        return j;
    }
};

/**
 * @brief Comprehensive calibration record for fine latency, clock drift, and multichannel alignment.
 */
struct FineLatencyCalibrationRecord
{
    std::string calibrationId;
    double nominalSampleRateHz { 48000.0 };
    
    // Latency modeling
    int integerLatencySamples { 0 };
    double fractionalLatencySamples { 0.0 };
    double totalLatencySamples { 0.0 };
    double fixedLatencySeconds { 0.0 };
    
    // Sub-sample refinement
    SubsampleRefinementMethod refinementMethod { SubsampleRefinementMethod::ParabolicThreePoint };
    double estimatedErrorBoundSamples { 0.05 };
    std::string uncertaintyMethod { "curvature_and_snr_bounded" };
    
    // Clock drift modeling
    ClockTopology clockTopology { ClockTopology::Unknown };
    DriftInterpretation driftInterpretation { DriftInterpretation::NotIdentifiable };
    double slopeSamplesPerSecond { 0.0 };
    double fittedDriftRatePpm { 0.0 };
    double driftRSquared { 0.0 };
    double residualMedianAbsoluteDeviation { 0.0 };
    double maxResidualSamples { 0.0 };
    double shortTermJitterSamples { 0.0 };
    LinearFitStatus fitStatus { LinearFitStatus::LinearFit };
    int inlierCount { 0 };
    int outlierCount { 0 };
    
    // Ambiguity & Overall Status
    double peakRatio { 0.0 };
    double ambiguityMargin { 1.0 };
    double globalConfidence { 1.0 };
    std::string status { "resolved" }; /**< "resolved", "ambiguous", "insufficient_signal", "degraded", "calibration_invalid" */
    
    // Units metadata (explicit and deterministic)
    std::string offsetUnit { "samples" };
    std::string timeUnit { "seconds" };
    std::string slopeUnit { "samples_per_second" };
    
    // Provenance & Fixity
    std::string rawReferenceSha256;
    std::string rawCaptureSha256;
    
    // Time-series observations & Discontinuities
    std::vector<ClockDriftObservation> observations;
    std::vector<ChannelSkewObservation> channelSkews;
    std::vector<TimingDiscontinuity> discontinuities;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["ambiguityMargin"] = ambiguityMargin;
        j["calibrationId"] = calibrationId;

        nlohmann::ordered_json skewsJson = nlohmann::ordered_json::array();
        for (const auto& sk : channelSkews)
            skewsJson.push_back(sk.toCanonicalJson());
        j["channelSkews"] = skewsJson;

        j["clockTopology"] = clockTopologyToString(clockTopology);

        nlohmann::ordered_json discJson = nlohmann::ordered_json::array();
        for (const auto& d : discontinuities)
            discJson.push_back(d.toCanonicalJson());
        j["discontinuities"] = discJson;

        j["driftInterpretation"] = driftInterpretationToString(driftInterpretation);
        j["driftRSquared"] = driftRSquared;
        j["estimatedErrorBoundSamples"] = estimatedErrorBoundSamples;
        j["fitStatus"] = linearFitStatusToString(fitStatus);
        j["fittedDriftRatePpm"] = fittedDriftRatePpm;
        j["fixedLatencySeconds"] = fixedLatencySeconds;
        j["fractionalLatencySamples"] = fractionalLatencySamples;
        j["globalConfidence"] = globalConfidence;
        j["inlierCount"] = inlierCount;
        j["integerLatencySamples"] = integerLatencySamples;
        j["maxResidualSamples"] = maxResidualSamples;
        j["nominalSampleRateHz"] = nominalSampleRateHz;

        nlohmann::ordered_json obsJson = nlohmann::ordered_json::array();
        for (const auto& obs : observations)
            obsJson.push_back(obs.toCanonicalJson());
        j["observations"] = obsJson;

        j["offsetUnit"] = offsetUnit;
        j["outlierCount"] = outlierCount;
        j["peakRatio"] = peakRatio;
        j["rawCaptureSha256"] = rawCaptureSha256;
        j["rawReferenceSha256"] = rawReferenceSha256;
        j["refinementMethod"] = subsampleRefinementMethodToString(refinementMethod);
        j["residualMedianAbsoluteDeviation"] = residualMedianAbsoluteDeviation;
        j["shortTermJitterSamples"] = shortTermJitterSamples;
        j["slopeSamplesPerSecond"] = slopeSamplesPerSecond;
        j["slopeUnit"] = slopeUnit;
        j["status"] = status;
        j["timeUnit"] = timeUnit;
        j["totalLatencySamples"] = totalLatencySamples;
        j["uncertaintyMethod"] = uncertaintyMethod;
        return j;
    }
};

/**
 * @brief Configuration parameters for fine latency, drift, and skew analysis.
 */
struct FineLatencyConfig
{
    int maxSearchLagSamples { 48000 };
    int analysisWindowSizeSamples { 8192 };
    int analysisHopSizeSamples { 4096 };
    SubsampleRefinementMethod refinementMethod { SubsampleRefinementMethod::ParabolicThreePoint };
    ClockTopology clockTopology { ClockTopology::Unknown };
    double ambiguityMarginThreshold { 0.15 };
    double maxPeakRatio { 0.85 };
    double minCorrelationConfidence { 0.30 };
    double minSnrDbfs { -80.0 };
    double maxAcceptableDropoutJumpSamples { 4.0 };
    double phaseReferenceFrequencyHz { 1000.0 };
};

/**
 * @brief Read-only, non-destructive view providing raw and compensated representations
 *        without mutating the underlying acquisition buffers.
 */
class LatencyCompensationView
{
public:
    LatencyCompensationView(std::span<const float> rawSamples,
                            CompensationTransformationType type,
                            double timeOffsetSeconds,
                            double sampleRateHz,
                            std::vector<float> compensatedBuffer = {})
        : rawSamples_(rawSamples),
          type_(type),
          timeOffsetSeconds_(timeOffsetSeconds),
          sampleRateHz_(sampleRateHz),
          compensatedBuffer_(std::move(compensatedBuffer))
    {
    }

    [[nodiscard]] std::span<const float> rawSamples() const noexcept
    {
        return rawSamples_;
    }

    [[nodiscard]] std::span<const float> compensatedSamples() const noexcept
    {
        if (type_ == CompensationTransformationType::TimeAxisOnly)
        {
            return rawSamples_;
        }
        return compensatedBuffer_;
    }

    [[nodiscard]] CompensationTransformationType transformationType() const noexcept
    {
        return type_;
    }

    [[nodiscard]] double timeOffsetSeconds() const noexcept
    {
        return timeOffsetSeconds_;
    }

    [[nodiscard]] double sampleRateHz() const noexcept
    {
        return sampleRateHz_;
    }

private:
    std::span<const float> rawSamples_;
    CompensationTransformationType type_;
    double timeOffsetSeconds_ { 0.0 };
    double sampleRateHz_ { 48000.0 };
    std::vector<float> compensatedBuffer_;
};

} // namespace abdaudiolab::measurement
