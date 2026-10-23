#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <string>
#include <complex>
#include <memory>

namespace abdaudiolab::math
{

enum class SnrMeasurementMethod
{
    PhysicalNoiseBaseline,
    LegacyAssumedNoiseFloor,
    NotAvailable
};

[[nodiscard]] inline const char* snrMeasurementMethodToString(SnrMeasurementMethod method) noexcept
{
    switch (method)
    {
        case SnrMeasurementMethod::PhysicalNoiseBaseline: return "PhysicalNoiseBaseline";
        case SnrMeasurementMethod::LegacyAssumedNoiseFloor: return "LegacyAssumedNoiseFloor";
        case SnrMeasurementMethod::NotAvailable: return "NotAvailable";
    }
    return "NotAvailable";
}

[[nodiscard]] inline SnrMeasurementMethod snrMeasurementMethodFromString(const std::string& str) noexcept
{
    if (str == "PhysicalNoiseBaseline") return SnrMeasurementMethod::PhysicalNoiseBaseline;
    if (str == "LegacyAssumedNoiseFloor") return SnrMeasurementMethod::LegacyAssumedNoiseFloor;
    return SnrMeasurementMethod::NotAvailable;
}

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
    SnrMeasurementMethod snrMethod { SnrMeasurementMethod::NotAvailable };
    float frequencyFlatnessDb { 0.1f }; // Max delta across 20Hz - 20kHz
    float flatnessMinMagDb { 0.0f };
    float flatnessMaxMagDb { 0.0f };
    float flatnessMinFreqHz { 0.0f };
    float flatnessMaxFreqHz { 0.0f };
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
    float minMagDb { 0.0f };
    float maxMagDb { 0.0f };
    float minFreqHz { 0.0f };
    float maxFreqHz { 0.0f };
};

/**
 * @brief Synthesized causal minimum-phase FIR filter and coupled power/headroom metrics (HITO-DSP-24.2).
 */
struct MinimumPhaseFirResult
{
    std::vector<float> filterIr;       /**< Causal minimum-phase FIR kernel windowed with Kaiser taper. */
    float averagePowerDb { 0.0f };     /**< Average filter power across [20 Hz, 20 kHz] in dB. */
    float maxBoostDb { 0.0f };         /**< Maximum spectral boost across bins in dB. */
    float safeHeadroomLinear { 1.0f }; /**< Linear attenuation factor 10^(-maxBoostDb/20) to prevent clipping. */
    bool isValid { false };            /**< True if FIR synthesis succeeded. */
    std::vector<float> impulseResponse; /**< Backward-compatible alias for filterIr. */
};

/**
 * @brief Natural Cubic Spline (C2 continuous) 1D interpolator over strictly increasing knots (HITO-DSP-24.3).
 */
class NaturalCubicSpline1D
{
public:
    NaturalCubicSpline1D() = default;
    NaturalCubicSpline1D(const std::vector<float>& xKnots, const std::vector<float>& yKnots);

    bool build(const std::vector<float>& xKnots, const std::vector<float>& yKnots);
    [[nodiscard]] float evaluate(float x) const noexcept;
    [[nodiscard]] bool isValid() const noexcept { return !x_.empty() && x_.size() == a_.size(); }

private:
    std::vector<double> x_;
    std::vector<double> a_;
    std::vector<double> b_;
    std::vector<double> c_;
    std::vector<double> d_;
};

/**
 * @brief High-precision DAC -> ADC Loopback Sound Card Calibration & Compensation engine.
 */
class LoopbackCalibrator
{
public:
    /**
     * @brief Evaluates zero-order modified Bessel function of the first kind I_0(x)
     * using Abramowitz & Stegun (9.8.1 / 9.8.2) Chebyshev polynomial approximation.
     */
    [[nodiscard]] static double besselI0(double x) noexcept;

    /**
     * @brief Generates a symmetric Kaiser window of length M with shape parameter beta.
     */
    [[nodiscard]] static std::vector<float> generateKaiserWindow(size_t length, double beta = 8.0);

    /**
     * @brief Glasberg & Moore (1990) Equivalent Rectangular Bandwidth ERB(f) in Hz.
     * ERB(f) = 24.7 * (0.00437 * f + 1.0)
     */
    [[nodiscard]] static double freqToErbBandwidthHz(double freqHz) noexcept;

    /**
     * @brief Glasberg & Moore (1990) ERB-rate scale E(f) in Cams.
     * E(f) = 21.4 * log10(0.00437 * f + 1.0)
     */
    [[nodiscard]] static double freqToErbRate(double freqHz) noexcept;

    /**
     * @brief Inverse Glasberg & Moore ERB-rate to frequency in Hz.
     */
    [[nodiscard]] static double erbRateToFreq(double erbRate) noexcept;

    /**
     * @brief Applies psychoacoustic ERB power smoothing across a magnitude spectrum (in dB).
     */
    [[nodiscard]] static std::vector<float> smoothSpectrumErb(
        const std::vector<float>& frequenciesHz,
        const std::vector<float>& magnitudesDb,
        float erbBandwidthFactor = 0.5f);

    /**
     * @brief Synthesizes a causal Minimum-Phase FIR filter via Real Cepstrum (discrete Hilbert)
     * and Kaiser window truncation, computing coupled average power and anti-clipping headroom.
     */
    [[nodiscard]] static MinimumPhaseFirResult synthesizeMinimumPhaseFir(
        const LoopbackCalibrationData& calData,
        double sampleRate = 0.0,
        size_t firLength = 1024,
        double kaiserBeta = 8.0);

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
                                                  float targetDbfs = -3.0f,
                                                  std::optional<float> measuredNoiseFloorRmsDb = std::nullopt);

    /**
     * @brief Saves calibration data to JSON format.
     */
    static bool saveCalibrationToJson(const LoopbackCalibrationData& data, const juce::File& file);

    /**
     * @brief Loads calibration data from JSON format.
     */
    static LoopbackCalibrationData loadCalibrationFromJson(const juce::File& file);

    /**
     * @brief Applies de-coloring inverse filter H_inv(f) to an audio buffer using Minimum-Phase FIR
     * synthesis and Overlap-Add (OLA) fast linear convolution.
     * @param audio Buffer to process in-place.
     * @param calData Validated calibration dataset containing inverseCorrectionDb and freqsHz.
     * @param sampleRate Audio sampling rate in Hz.
     * @param avoidClippingEnabled If true, scales output by safeHeadroomLinear when maxBoostDb > 0.
     */
    static void applyInverseCompensation(std::vector<float>& audio,
                                         const LoopbackCalibrationData& calData,
                                         double sampleRate,
                                         bool avoidClippingEnabled = false);
};

/**
 * @brief Configuration for Dead-Time Compensated PID Auto-Trim controller (HITO-DSP-24.5).
 */
struct DeadTimePidConfig
{
    float targetDbfs { -12.0f };        /**< Target input peak level in dBFS. */
    float toleranceDb { 0.5f };         /**< Convergence tolerance band (+/- dB). */
    float kp { 0.65f };                 /**< Proportional gain. */
    float ki { 0.20f };                 /**< Integral gain. */
    float kd { 0.05f };                 /**< Derivative gain. */
    float minGainDb { -36.0f };         /**< Minimum allowed digital trim gain in dB. */
    float maxGainDb { 18.0f };          /**< Maximum allowed digital trim gain in dB. */
    float clipThresholdDbfs { -0.3f };  /**< Saturation ceiling triggering [LIMIT-PID RESET]. */
    int stabilityMinChunks { 4 };       /**< Consecutive in-tolerance chunks required for convergence. */
};

/**
 * @brief Telemetry output of a single discrete PID update step (HITO-DSP-24.5).
 */
struct DeadTimePidStepResult
{
    float gainDb { 0.0f };
    float gainLinear { 1.0f };
    float errorDb { 0.0f };
    float deadTimeAttenuationEta { 1.0f };
    int stableChunksCount { 0 };
    bool isStable { false };
    bool limitPidResetTriggered { false };
    float currentGainDb { 0.0f };
};

/**
 * @brief Telemetry alias for DSP engine compatibility.
 */
using DeadTimePidState = DeadTimePidStepResult;

/**
 * @brief Closed-loop discrete PID gain controller with round-trip dead-time compensation
 * and [LIMIT-PID RESET] anti-windup protection (HITO-DSP-24.5).
 */
class DeadTimeCompensatedPidController
{
public:
    DeadTimeCompensatedPidController() = default;
    explicit DeadTimeCompensatedPidController(const DeadTimePidConfig& cfg,
                                              double chunkDurationSec = 0.05,
                                              double roundTripDeadTimeSec = 0.01);

    void configure(const DeadTimePidConfig& cfg,
                   double chunkDurationSec = 0.05,
                   double roundTripDeadTimeSec = 0.01) noexcept;

    void reset(float initialGainDb = 0.0f) noexcept;

    [[nodiscard]] DeadTimePidStepResult update(float measuredPeakDbfs,
                                               bool hardwareClipped = false) noexcept;

    [[nodiscard]] DeadTimePidStepResult step(float targetHeadroomDbfs,
                                             float measuredPeakDbfs,
                                             bool saturationDetected,
                                             double chunkDurationSec = 0.05,
                                             double roundTripDeadTimeSec = 0.01) noexcept
    {
        cfg_.targetDbfs = targetHeadroomDbfs;
        const double dtc = std::max(1e-4, chunkDurationSec);
        const double tau = std::max(0.0, roundTripDeadTimeSec);
        eta_ = static_cast<float>(dtc / (dtc + tau));
        return update(measuredPeakDbfs, saturationDetected);
    }

    [[nodiscard]] float getCurrentGainDb() const noexcept { return currentGainDb_; }
    [[nodiscard]] float getCurrentGainLinear() const noexcept;
    [[nodiscard]] float getDeadTimeEta() const noexcept { return eta_; }
    [[nodiscard]] int getStableChunksCount() const noexcept { return stableChunksCount_; }
    [[nodiscard]] bool isConverged() const noexcept { return stableChunksCount_ >= cfg_.stabilityMinChunks; }

private:
    DeadTimePidConfig cfg_ {};
    float eta_ { 1.0f };
    float currentGainDb_ { 0.0f };
    float integralError_ { 0.0f };
    float prevError_ { 0.0f };
    int stableChunksCount_ { 0 };
    bool hasPrevError_ { false };
};

/**
 * @brief Hybrid Zero-Latency Real-Time Partitioned FIR Convolver (Gardner 1995) (HITO-DSP-24.7).
 *
 * Combines a direct time-domain FIR head (partitionSize taps, default 64) with a uniform
 * Frequency-Domain Delay Line (FDL) Overlap-Add tail for the remaining partitions.
 * Guarantees 0 samples of algorithmic latency and zero heap allocations in processBlock().
 */
class ZeroLatencyPartitionedConvolver
{
public:
    ZeroLatencyPartitionedConvolver() = default;

    /**
     * @brief Pre-allocates all FFT and ring-buffer state for the given FIR kernel.
     * @param firKernel Causal FIR impulse response h[0..M-1].
     * @param partitionSize Block size B (rounded up to power of 2, default 64).
     */
    bool prepare(const std::vector<float>& firKernel, size_t partitionSize = 64);

    /**
     * @brief Clears all delay-line state without deallocating memory (real-time safe).
     */
    void reset() noexcept;

    /**
     * @brief Processes an arbitrary-length audio block in-place with 0 algorithmic latency
     * and zero heap allocations.
     */
    void processBlock(float* inPlaceAudio, size_t numSamples) noexcept;

    /**
     * @brief Processes an audio block from input to output with 0 algorithmic latency
     * and zero heap allocations. Handles out-of-place and in-place safely.
     */
    void processBlock(const float* input, float* output, size_t numSamples) noexcept;

    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] size_t getPartitionSize() const noexcept { return partSize_; }
    [[nodiscard]] size_t getNumTailPartitions() const noexcept { return tailPartitionsSpec_.size(); }
    [[nodiscard]] int getLatencySamples() const noexcept { return 0; }

private:
    void stepFrequencyDomainTail() noexcept;

    std::unique_ptr<juce::dsp::FFT> fft_;
    size_t partSize_ { 0 };
    size_t fftSize_ { 0 };
    size_t headWritePos_ { 0 };
    size_t blockPos_ { 0 };
    size_t fdlWriteIdx_ { 0 };
    bool prepared_ { false };

    std::vector<float> headCoeffs_;
    std::vector<float> headHistory_;
    std::vector<float> inputBlock_;
    std::vector<float> tailCurrentBlock_;
    std::vector<float> tailOverlap_;

    std::vector<std::vector<std::complex<float>>> tailPartitionsSpec_;
    std::vector<std::vector<std::complex<float>>> fdl_;
    std::vector<std::complex<float>> fftTimeScratch_;
    std::vector<std::complex<float>> accumSpecScratch_;
};

} // namespace abdaudiolab::math


