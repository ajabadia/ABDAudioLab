#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstddef>
#include <algorithm>

namespace abdaudiolab::math
{

/**
 * @enum PreflightStatus
 * @brief Metrological outcome of the acoustic preflight ping probe.
 */
enum class PreflightStatus
{
    Pass,
    Warning_LowLevel,
    Warning_HighLevel,
    Fail_NoSignal,
    Fail_Clipping,
    Fail_GroundLoopHum
};

/**
 * @struct PreflightMetrics
 * @brief Telemetry metrics collected during the 1.5-second preflight ping probe.
 */
struct PreflightMetrics
{
    PreflightStatus status { PreflightStatus::Fail_NoSignal };
    float peakDbfs { -100.0f };
    float rmsDbfs { -100.0f };
    float humEnergyRatioDbc { -100.0f }; // 50/60 Hz power relative to 1 kHz fundamental (dBc)
    float fundamentalFreqHz { 1000.0f };
    std::string diagnosticMessage;
};

/**
 * @class PreflightPingProbe
 * @brief Autonomous acoustic continuity, headroom, and ground-loop diagnostics engine.
 *
 * Implements Section 4 of HARDWARE_PROFILING_BY_CONTRACT_DESIGN:
 * - Generates a 1.0 kHz sine test tone at -12.0 dBFS with 50 ms fade-in/fade-out.
 * - Evaluates audio return continuity (RMS > -60 dBFS).
 * - Evaluates converter headroom and saturation (Peak < -0.5 dBFS).
 * - Evaluates spectral hum / ground loop coherence (50 Hz and 60 Hz mains harmonics vs 1 kHz).
 * - Validates optimal SNR return window (-24 dBFS to -6 dBFS).
 *
 * Designed strictly as pure headless DSP with zero heap allocation in real-time execution.
 */
class PreflightPingProbe
{
public:
    /**
     * @brief Generates the preflight probe stimulus signal (1.0 kHz sine @ -12.0 dBFS).
     * @param sampleRate Sampling frequency in Hz.
     * @param durationSec Total duration in seconds (default 1.5 s).
     * @param freqHz Fundamental test frequency in Hz (default 1000.0 Hz).
     * @param targetDbfs Target amplitude in dBFS (default -12.0 dBFS).
     * @param rampMs Duration of cosine fade-in and fade-out in ms (default 50.0 ms).
     */
    static std::vector<float> generateStimulus (double sampleRate,
                                                float durationSec = 1.5f,
                                                float freqHz = 1000.0f,
                                                float targetDbfs = -12.0f,
                                                float rampMs = 50.0f);

    /**
     * @brief Analyzes the raw recorded audio response from the hardware device.
     * @param buffer Pointer to contiguous float samples.
     * @param numSamples Number of samples to analyze.
     * @param sampleRate Sampling rate in Hz.
     * @param expectedFreqHz Fundamental frequency of the stimulus (default 1000.0 Hz).
     */
    static PreflightMetrics analyzeResponse (const float* buffer,
                                             size_t numSamples,
                                             double sampleRate,
                                             float expectedFreqHz = 1000.0f);

    /**
     * @brief Convenience overload for vector buffer.
     */
    static PreflightMetrics analyzeResponse (const std::vector<float>& buffer,
                                             double sampleRate,
                                             float expectedFreqHz = 1000.0f)
    {
        return analyzeResponse (buffer.data(), buffer.size(), sampleRate, expectedFreqHz);
    }

    /**
     * @brief Computes single-frequency energy using the Goertzel algorithm with Hann windowing.
     */
    static double computeGoertzelPower (const float* buffer,
                                        size_t numSamples,
                                        double sampleRate,
                                        double targetFreqHz);
};

} // namespace abdaudiolab::math
