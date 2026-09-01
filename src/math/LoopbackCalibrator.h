#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::math
{

struct LoopbackCalibrationData
{
    bool isCalibrated { false };
    double sampleRate { 96000.0 };
    float peakInDbfs { -100.0f };
    float recommendedTrimGain { 1.0f }; // Multiplier to align to -3.0 dBfs target
    float targetHeadroomDbfs { -3.0f };
    float roundTripLatencyMs { 0.0f };
    int latencySamples { 0 };
    float thdPlusNoisePercent { 0.001f };
    float snrDb { 95.0f };
    float frequencyFlatnessDb { 0.1f }; // Max delta across 20Hz - 20kHz
    std::vector<float> freqsHz;
    std::vector<float> magnitudeDb;         // Sound card transfer function H(f)
    std::vector<float> inverseCorrectionDb; // Inverse filter to de-color hardware
    juce::String deviceName;
    juce::String timestamp;
};

/**
 * @brief High-precision DAC -> ADC Loopback Sound Card Calibration & Compensation engine.
 */
class LoopbackCalibrator
{
public:
    /**
     * @brief Analyzes recorded loopback sweep and extracts soundcard response, trim, latency and SNR.
     */
    static LoopbackCalibrationData analyzeLoopback(const std::vector<float>& recordedResponse,
                                                  double sampleRate,
                                                  double sweepDurationSec = 1.0,
                                                  float startFreqHz = 20.0f,
                                                  float endFreqHz = 40000.0f,
                                                  float targetDbfs = -3.0f);

    /**
     * @brief Saves calibration data to JSON format.
     */
    static bool saveCalibrationToJson(const LoopbackCalibrationData& data, const juce::File& file);

    /**
     * @brief Loads calibration data from JSON format.
     */
    static LoopbackCalibrationData loadCalibrationFromJson(const juce::File& file);
};

} // namespace abdaudiolab::math
