/**
 * @file IntermodulationAnalyzer.h
 * @brief SMPTE and CCIF/DIN Intermodulation Distortion (IMD) stimulus generator and spectral analyzer.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include <cmath>
#include <numbers>

namespace abdaudiolab::math
{

/**
 * @struct ImdResult
 * @brief Holds percentage metrics of intermodulation distortion.
 */
struct ImdResult
{
    float totalImdPercent { 0.0f }; /**< Total IMD percentage (% of carrier / fundamental). */
    float d2Percent { 0.0f };       /**< 2nd order intermodulation products percentage. */
    float d3Percent { 0.0f };       /**< 3rd order intermodulation products percentage. */
};

/**
 * @class IntermodulationAnalyzer
 * @brief High-precision generator and FFT-based analyzer for audio hardware IMD characterization.
 */
class IntermodulationAnalyzer
{
public:
    /**
     * @brief Generates an SMPTE standard dual-tone stimulus (60 Hz + 7 kHz, 4:1 voltage ratio).
     */
    static std::vector<float> generateSmpteStimulus(double sampleRate,
                                                   double durationSec,
                                                   float lowFreqHz = 60.0f,
                                                   float highFreqHz = 7000.0f,
                                                   float amplitudeRatio = 4.0f);

    /**
     * @brief Generates an ITU-R / CCIF twin-tone stimulus (e.g. 19 kHz + 20 kHz, 1:1 voltage ratio).
     */
    static std::vector<float> generateCcifStimulus(double sampleRate,
                                                  double durationSec,
                                                  float f1Hz = 19000.0f,
                                                  float f2Hz = 20000.0f);

    /**
     * @brief Analyzes a recorded SMPTE response and calculates modulation sideband energy around highFreqHz.
     */
    static ImdResult analyzeSmpte(const std::vector<float>& recordedSignal,
                                 double sampleRate,
                                 float lowFreqHz = 60.0f,
                                 float highFreqHz = 7000.0f);

    /**
     * @brief Analyzes a recorded CCIF twin-tone response and calculates difference-frequency products.
     */
    static ImdResult analyzeCcif(const std::vector<float>& recordedSignal,
                                double sampleRate,
                                float f1Hz = 19000.0f,
                                float f2Hz = 20000.0f);
};

} // namespace abdaudiolab::math
