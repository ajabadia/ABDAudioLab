/**
 * @file FineLatencyAnalyzer.h
 * @brief Algorithmic analyzer for sub-sample latency, linear clock drift modeling,
 *        discontinuity detection, and multichannel skew characterization.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "FineLatencyContracts.h"
#include <span>
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class FineLatencyAnalyzer
 * @brief Implements sub-sample cross-correlation, multi-window drift regression,
 *        inter-channel skew analysis, and non-destructive compensation view factory.
 */
class FineLatencyAnalyzer
{
public:
    struct SubsampleRefinementResult
    {
        double delta { 0.0 };              /**< Fractional shift in samples [-0.5, 0.5] */
        double interpolatedPeak { 0.0 };   /**< Refined peak correlation value */
        double curvature { 0.0 };          /**< |R(k-1) - 2R(k) + R(k+1)| */
        double estimatedErrorBound { 0.05 }; /**< Error bound in samples based on SNR, curvature & confidence */
        bool valid { false };
    };

    /**
     * @brief Refines an integer peak index using 3-point parabolic interpolation.
     * @param rPrev Correlation value at peakIndex - 1.
     * @param rPeak Correlation value at peakIndex.
     * @param rNext Correlation value at peakIndex + 1.
     * @param snrDbfs Estimated SNR of the signal in dBFS.
     * @return SubsampleRefinementResult with fractional delta and bounded uncertainty.
     */
    [[nodiscard]] static SubsampleRefinementResult refineParabolicThreePoint(
        double rPrev,
        double rPeak,
        double rNext,
        double snrDbfs = 60.0) noexcept;

    /**
     * @brief Performs full fine latency and clock drift analysis between a reference stimulus and captured response.
     * @param stimulus Reference stimulus audio samples.
     * @param capture Captured response audio samples (Channel 0 / Primary loopback).
     * @param sampleRate Nominal sampling rate in Hz.
     * @param config Analysis parameters and thresholds.
     * @param calibrationId Unique identifier or empty for auto-generated.
     * @return FineLatencyCalibrationRecord with complete metrological characterization.
     */
    [[nodiscard]] static FineLatencyCalibrationRecord analyzeFineLatencyAndDrift(
        std::span<const float> stimulus,
        std::span<const float> capture,
        double sampleRate,
        const FineLatencyConfig& config = {},
        const std::string& calibrationId = "");

    /**
     * @brief Analyzes multichannel inter-channel timing skew relative to reference Channel 0.
     * @param multichannelCapture Vector of spans for each input channel [0..N-1].
     * @param sampleRate Nominal sampling rate in Hz.
     * @param config Analysis parameters.
     * @return Vector of ChannelSkewObservation relative to Channel 0.
     */
    [[nodiscard]] static std::vector<ChannelSkewObservation> analyzeMultichannelSkew(
        const std::vector<std::span<const float>>& multichannelCapture,
        double sampleRate,
        const FineLatencyConfig& config = {});

    /**
     * @brief Performs linear regression on local offset observations over time.
     * @param observations Vector of extracted time-offset observations.
     * @param sampleRate Nominal sample rate in Hz.
     * @param topology Declared clock architecture topology.
     * @param maxJumpThreshold Samples jump threshold to detect discontinuities.
     * @param outRecord Reference to record to populate with drift & regression parameters.
     */
    static void fitLinearDrift(
        std::vector<ClockDriftObservation>& observations,
        double sampleRate,
        ClockTopology topology,
        double maxJumpThreshold,
        FineLatencyCalibrationRecord& outRecord);

    /**
     * @brief Factory for non-destructive latency compensation views.
     * @param rawCapture Raw, immutable capture buffer.
     * @param type Transformation mode (TimeAxisOnly, IntegerShift, FractionalResampling).
     * @param latencyRecord Validated calibration record.
     * @return LatencyCompensationView wrapping the buffers safely.
     */
    [[nodiscard]] static LatencyCompensationView createCompensationView(
        std::span<const float> rawCapture,
        CompensationTransformationType type,
        const FineLatencyCalibrationRecord& latencyRecord);

    /**
     * @brief Computes deterministic SHA-256 hash string for an audio sample span.
     */
    [[nodiscard]] static std::string computeAudioSha256(std::span<const float> audio);
};

} // namespace abdaudiolab::measurement
