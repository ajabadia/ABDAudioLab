/**
 * @file CalibrationPanelTypes.h
 * @brief Shared value types for the Step 2 audio-interface calibration panel.
 *        Declared in their own namespace so that the panel, its painters and its
 *        tests can share them without depending on the juce::Component itself.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../math/LoopbackCalibrator.h"

namespace abdaudiolab::gui::calibrationpanel
{

/** @brief Coarse lifecycle state of the whole Step 2 calibration flow. */
enum class State
{
    ReadyToMeasure,
    MeasuringNoiseBaseline,
    NoiseBaselinePassed,
    Measuring,
    Success,
    Failed,
    Skipped
};

/** @brief Which of the two Step 2 navigation cards the operator is looking at. */
enum class SubView
{
    NoiseBaseline_2A,
    PhysicalLoopback_2B
};

/** @brief Outcome state of the Step 2A input noise floor check. */
enum class NoiseBaselineState
{
    NotChecked,
    Checking,
    Passed,
    Contaminated,
    SafetyAborted,
    DeviceStopped,
    Cancelled
};

/** @brief Outcome state of the Step 2B physical loopback sweep. */
enum class LoopbackState
{
    Locked,
    Ready,
    MeasuringPreflight,
    MeasuringSweep,
    Passed,
    SignalTooLow,
    Clipped,
    DeviceStopped,
    Failed,
    Stale,
    Cancelled
};

/** @brief Persistent Step 2A measurement report kept while the user navigates to 2B. */
struct NoiseBaselineReport
{
    float rmsDbfs { -120.0f };
    float peakDbfs { -120.0f };
    bool passed { false };
    juce::String statusText { "NOT CHECKED" };
    juce::String failureReason;
    juce::String inputChannel;
    juce::String deviceName;
    std::array<float, 32> spectrum32Bands {};
    juce::String timestampIso;
};

/** @brief Persistent Step 2B measurement report kept while the user navigates back to 2A. */
struct LoopbackReport
{
    float roundTripLatencyMs { 0.0f };
    int latencySamples { 0 };
    float frequencyFlatnessDb { 0.0f };
    float recommendedTrimGain { 1.0f };
    float snrDb { 0.0f };
    math::SnrMeasurementMethod snrMethod { math::SnrMeasurementMethod::NotAvailable };
    float thdPercent { 0.0f };
    float peakInDbfs { -100.0f };
    bool phaseInverted { false };
    bool clippingDetected { false };
    int clippedSamples { 0 };
    bool passed { false };
    juce::String failureReason;
    juce::String timestampIso;
};

/** @brief Raw capture diagnostics surfaced to the operator when a sweep fails. */
struct CalibrationDiagnostics
{
    juce::String deviceName;
    juce::String driverType;
    juce::String inputChannel;
    juce::String outputChannel;
    int samplesCaptured { 0 };
    int samplesRequired { 0 };
    float peakInDbfs { -100.0f };
    float roundTripLatencyMs { 0.0f };
    int latencySamples { 0 };
    float frequencyFlatnessDb { 0.0f };
    float flatnessMinMagDb { 0.0f };
    float flatnessMaxMagDb { 0.0f };
    float flatnessMinFreqHz { 0.0f };
    float flatnessMaxFreqHz { 0.0f };
    float snrDb { 0.0f };
    float thdPercent { 0.0f };
    bool clippingDetected { false };
    int clippedSamplesCount { 0 };
    bool levelPassed { false };
    bool flatnessPassed { false };
    bool isCalibrated { false };
    juce::String failureReason;
};

} // namespace abdaudiolab::gui::calibrationpanel
