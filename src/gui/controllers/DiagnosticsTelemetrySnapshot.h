#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstddef>
#include <cmath>
#include <string>

namespace abdaudiolab::gui {

static constexpr std::size_t kMaxTelemetryFftBins = 1024;

/**
 * @struct TelemetryPollerConfig
 * @brief Tunable configuration for DiagnosticsTelemetryPoller.
 */
struct TelemetryPollerConfig
{
    int calibrationPeriodTicks { 15 };
    int fftCapacity { static_cast<int>(kMaxTelemetryFftBins) };
};

/**
 * @struct TelemetrySnapshot
 * @brief Immutable value snapshot capturing periodic audio, FFT, session, and hardware diagnostics.
 *
 * Guarantees zero heap allocation on periodic sampling via fixed-size pre-allocated array.
 */
struct TelemetrySnapshot
{
    float inputPeakL { 0.0f };
    float inputPeakR { 0.0f };
    float inputRmsL { 0.0f };
    float inputRmsR { 0.0f };

    float outputPeakL { 0.0f };
    float outputPeakR { 0.0f };
    float outputRmsL { 0.0f };
    float outputRmsR { 0.0f };

    std::array<float, kMaxTelemetryFftBins> fftMagnitudes {};
    std::size_t fftBinCount { 0 };
    bool spectrumReady { false };

    float cpuUsagePercent { 0.0f };
    double sampleRate { 0.0 };
    int bufferSizeSamples { 0 };

    int activeMidiNoteNumber { -1 };
    juce::String activeMidiNoteName { "No MIDI note" };

    int currentTrial { 0 };
    int totalTrials { 0 };
    float progressPercent { 0.0f };

    float lastPluginOutputRmsDb { -120.0f };
    std::string stimulusDescription;
    int sessionStateCode { 0 }; // 0: ReadyToProfile, 1: Profiling, 2: Paused, 3: Completed, 4: Cancelled

    bool isCalibrated { false };
    double calibrationSampleRate { 0.0 };
    bool isCalibrationSkipped { false };
    bool calibrationTickDue { false };
};

/**
 * @brief Helper to format MIDI note number to standard note representation.
 * Maps 0 -> "C-1", 60 -> "C4", 61 -> "C#4", 127 -> "G9". Any out of range or -1 -> "No MIDI note".
 */
inline juce::String formatMidiNoteName(int noteNumber)
{
    if (noteNumber < 0 || noteNumber > 127)
        return "No MIDI note";

    static const char* const noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    int noteIndex = noteNumber % 12;
    int octave = (noteNumber / 12) - 1; // MIDI 0 -> C-1, MIDI 60 -> C4

    return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

/**
 * @brief Ensures a float value is finite; otherwise returns a fallback.
 */
inline float sanitizeFloat(float val, float fallback = 0.0f) noexcept
{
    return std::isfinite(val) ? val : fallback;
}

/**
 * @brief Ensures a double value is finite; otherwise returns a fallback.
 */
inline double sanitizeDouble(double val, double fallback = 0.0) noexcept
{
    return std::isfinite(val) ? val : fallback;
}

} // namespace abdaudiolab::gui
