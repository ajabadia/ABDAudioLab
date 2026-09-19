#pragma once

#include "DiagnosticsTelemetrySnapshot.h"
#include <cstddef>
#include <string>

namespace abdaudiolab::gui {

/**
 * @struct TelemetryAudioLevels
 */
struct TelemetryAudioLevels
{
    float inPeakL { 0.0f };
    float inPeakR { 0.0f };
    float inRmsL { 0.0f };
    float inRmsR { 0.0f };
    float outPeakL { 0.0f };
    float outPeakR { 0.0f };
    float outRmsL { 0.0f };
    float outRmsR { 0.0f };
};

/**
 * @struct TelemetryDeviceMetrics
 */
struct TelemetryDeviceMetrics
{
    float cpuUsagePercent { 0.0f };
    double sampleRate { 0.0 };
    int bufferSizeSamples { 0 };
};

/**
 * @struct TelemetryCalibrationData
 */
struct TelemetryCalibrationData
{
    bool isCalibrated { false };
    double sampleRate { 0.0 };
    bool isSkipped { false };
};

/**
 * @struct TelemetrySessionProgress
 */
struct TelemetrySessionProgress
{
    int currentTrial { 0 };
    int totalTrials { 0 };
    float lastPluginOutputRmsDb { -120.0f };
    int activeMidiNoteNumber { -1 };
    int sessionStateCode { 0 }; // 0: ReadyToProfile, 1: Profiling, 2: Paused, 3: Completed, 4: Cancelled
};

/**
 * @class IDiagnosticsTelemetrySource
 * @brief Port supplying raw diagnostics metrics from audio engine, session coordinator, and hardware.
 */
class IDiagnosticsTelemetrySource
{
public:
    virtual ~IDiagnosticsTelemetrySource() = default;

    virtual TelemetryAudioLevels readAudioLevels() const = 0;
    virtual bool isSpectrumReady() const = 0;
    virtual std::size_t readSpectrumMagnitudes(float* destBuffer, std::size_t maxBins) const = 0;
    virtual TelemetryDeviceMetrics readDeviceMetrics() const = 0;
    virtual TelemetryCalibrationData readCalibrationData() const = 0;
    virtual TelemetrySessionProgress readSessionProgress() const = 0;
};

/**
 * @class NullDiagnosticsTelemetrySource
 * @brief Fallback source returning safe idle defaults.
 */
class NullDiagnosticsTelemetrySource : public IDiagnosticsTelemetrySource
{
public:
    TelemetryAudioLevels readAudioLevels() const override { return {}; }
    bool isSpectrumReady() const override { return false; }
    std::size_t readSpectrumMagnitudes(float*, std::size_t) const override { return 0; }
    TelemetryDeviceMetrics readDeviceMetrics() const override { return {}; }
    TelemetryCalibrationData readCalibrationData() const override { return {}; }
    TelemetrySessionProgress readSessionProgress() const override { return {}; }
};

} // namespace abdaudiolab::gui
