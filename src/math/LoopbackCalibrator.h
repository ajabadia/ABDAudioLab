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
    bool phaseInversionDetected { false };  /**< True if hardware loopback cable or pre-amp inverts signal polarity. */
    float phaseInversionCorrelation { 1.0f }; /**< Cross-correlation peak value (negative indicates inversion). */
    bool clippingDetected { false };        /**< True if input exceeded ADC ceiling / reached saturation. */
    int clippedSamplesCount { 0 };          /**< Total number of clipped audio samples detected. */
    float dcOffsetVolts { 0.0f };           /**< Detected average DC offset level. */
    std::vector<float> freqsHz;
    std::vector<float> magnitudeDb;         // Sound card transfer function H(f)
    std::vector<float> inverseCorrectionDb; // Inverse filter to de-color hardware
    juce::String deviceName;
    juce::String timestamp;
};

/**
 * @brief Evaluation result for frequency flatness across a designated analysis band.
 */
struct FlatnessEvaluation
{
    bool hasValidBins { false };
    float deltaDb { 0.0f };
    size_t validBinCount { 0 };
};

/**
 * @brief High-precision DAC -> ADC Loopback Sound Card Calibration & Compensation engine.
 */
class LoopbackCalibrator
{
public:
    /**
     * @brief Computes safe upper sweep limit for Farina stimulus based on active sample rate.
     * Guaranteed to stay comfortably below Nyquist: min(20000.0f, 0.45f * sampleRate).
     */
    [[nodiscard]] static float computeSafeSweepMaxHz(double sampleRate) noexcept;

    /**
     * @brief Computes passband limits [f_low, f_high] for flatness qualification.
     * Evaluates strictly in [40.0f, min(18000.0f, 0.40f * sampleRate)].
     */
    [[nodiscard]] static std::pair<float, float> computeFlatnessBandHz(double sampleRate) noexcept;

    /**
     * @brief Pure helper evaluating peak-to-peak flatness within specified frequency band.
     * If no valid bins fall inside [lowHz, highHz], returns hasValidBins=false and deltaDb=inf.
     */
    [[nodiscard]] static FlatnessEvaluation evaluateFlatnessInBand(
        const std::vector<float>& frequenciesHz,
        const std::vector<float>& magnitudesDb,
        float lowHz,
        float highHz) noexcept;

    /**
     * @brief Analyzes recorded loopback sweep and extracts soundcard response, trim, latency and SNR.
     * @param endFreqHz If <= 0.0f, automatically resolves via computeSafeSweepMaxHz(sampleRate).
     */
    static LoopbackCalibrationData analyzeLoopback(const std::vector<float>& recordedResponse,
                                                  double sampleRate,
                                                  double sweepDurationSec = 1.0,
                                                  float startFreqHz = 20.0f,
                                                  float endFreqHz = 0.0f,
                                                  float targetDbfs = -3.0f);

    /**
     * @brief Saves calibration data to JSON format.
     */
    static bool saveCalibrationToJson(const LoopbackCalibrationData& data, const juce::File& file);

    /**
     * @brief Loads calibration data from JSON format.
     */
    static LoopbackCalibrationData loadCalibrationFromJson(const juce::File& file);

    /**
     * @brief Applies de-coloring inverse filter H_inv(f) to an audio buffer using frequency-domain compensation.
     * @param audio Buffer to process in-place.
     * @param calData Validated calibration dataset containing inverseCorrectionDb and freqsHz.
     * @param sampleRate Audio sampling rate in Hz.
     */
    static void applyInverseCompensation(std::vector<float>& audio,
                                         const LoopbackCalibrationData& calData,
                                         double sampleRate);
};

} // namespace abdaudiolab::math
