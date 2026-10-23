/**
 * @file FarinaDeconvolver.h
 * @brief Angelo Farina (2000) Logarithmic Swept-Sine deconvolution and harmonic distortion analysis.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace abdaudiolab::math
{

/**
 * @struct DeconvolutionResult
 * @brief Holds the output of the Farina log sweep deconvolution process, including IR and THD metrics.
 */
struct DeconvolutionResult
{
    std::vector<float> fullDeconvolvedIR;          /**< Complete deconvolved impulse response time-series. */
    std::vector<float> linearIR;                   /**< Extracted linear impulse response (fundamental). */
    std::vector<float> frequencyResponseMagnitudeDb; /**< Frequency response magnitude spectrum in dBFS. */
    std::vector<float> frequenciesHz;              /**< Frequency axis bins in Hertz. */
    std::vector<float> phaseResponseRad;           /**< Unwrapped phase response spectrum in radians. */
    std::vector<float> groupDelaySamples;          /**< Group delay in samples: -d(phi)/d(omega). */
    float peakFrequencyHz { 0.0f };                /**< Frequency bin corresponding to maximum peak response. */
    float resonancePeakDb { 0.0f };                /**< Resonance peak magnitude in dB. */
    float thdPercent { 0.0f };                     /**< Total Harmonic Distortion percentage (THD %). */
    std::vector<float> h2IR;                       /**< 2nd harmonic impulse response. */
    std::vector<float> h3IR;                       /**< 3rd harmonic impulse response. */
    std::vector<float> h4IR;                       /**< 4th harmonic impulse response. */
    std::vector<float> h5IR;                       /**< 5th harmonic impulse response. */
    std::vector<float> h2MagnitudeDb;              /**< 2nd harmonic magnitude spectrum in dBFS. */
    std::vector<float> h3MagnitudeDb;              /**< 3rd harmonic magnitude spectrum in dBFS. */
    std::vector<float> h4MagnitudeDb;              /**< 4th harmonic magnitude spectrum in dBFS. */
    std::vector<float> h5MagnitudeDb;              /**< 5th harmonic magnitude spectrum in dBFS. */
    std::vector<float> thdVsFreqPercent;           /**< THD(f) curve across frequency bins in percent. */
    float h2Percent { 0.0f };                     /**< 2nd harmonic distortion percentage (H2 %). */
    float h3Percent { 0.0f };                     /**< 3rd harmonic distortion percentage (H3 %). */
    float h4Percent { 0.0f };                     /**< 4th harmonic distortion percentage (H4 %). */
    float h5Percent { 0.0f };                     /**< 5th harmonic distortion percentage (H5 %). */
};

/**
 * @struct FarinaSweepWindowConfig
 * @brief Asymmetric Half-Hann (Tukey) windowing and guard silence configuration for Farina sweeps.
 */
struct FarinaSweepWindowConfig
{
    double outputFadeInMs { 30.0 };         /**< Asymmetric Half-Hann fade-in duration (ms) at startFreqHz. */
    double outputFadeOutMs { 5.0 };         /**< Asymmetric Half-Hann fade-out duration (ms) at endFreqHz. */
    double inputFadeInMs { 10.0 };          /**< Capture window fade-in duration (ms) prior to FFT deconvolution. */
    double inputFadeOutMs { 10.0 };         /**< Capture window fade-out duration (ms) prior to FFT deconvolution. */
    double safeSilenceMs { 15.0 };          /**< Pre-roll DMA guard silence duration (ms). */
    double interSignalSilenceMs { 650.0 };  /**< Post-roll guard silence duration (ms) for tail decay < -90 dBFS. */
};

/**
 * @struct PilotPulseSweepConfig
 * @brief Configuration for dual-pilot pulse clock-drift and ASIO dropout detection (HITO-DSP-24.7).
 */
struct PilotPulseSweepConfig
{
    double preSilenceMs { 15.0 };          /**< Guard silence before pre-sweep pilot pulse (ms). */
    double pilotDurationMs { 4.0 };        /**< Duration of each wideband FM pilot pulse (ms). */
    double pilotToSweepGuardMs { 25.0 };   /**< Silence between pre-pilot and sweep start (ms). */
    double sweepToPostPilotGuardMs { 50.0 }; /**< Silence between sweep end and post-pilot (ms). */
    double postSilenceMs { 50.0 };         /**< Guard silence after post-sweep pilot pulse (ms). */
    float pilotAmplitude { 0.5f };         /**< Peak linear amplitude of pilot pulses. */
    double maxAllowedDriftPpm { 150.0 };   /**< Maximum acceptable sample-rate clock drift in ppm. */
    double dropoutThresholdSamples { 1.5 }; /**< Sample discrepancy threshold indicating buffer dropout. */
};

/**
 * @struct PilotSweepLayout
 * @brief Generated composite stimulus (pre-pilot + Farina sweep + post-pilot) and nominal landmarks.
 */
struct PilotSweepLayout
{
    std::vector<float> compositeSignal;
    std::vector<float> referencePilot;
    size_t prePilotStartSample { 0 };
    size_t sweepStartSample { 0 };
    size_t sweepLengthSamples { 0 };
    size_t postPilotStartSample { 0 };
    double nominalPilotDistanceSamples { 0.0 };
};

/**
 * @struct ClockDriftPilotResult
 * @brief Sub-sample clock-drift and ASIO dropout telemetry from dual pilot pulses (HITO-DSP-24.7).
 */
struct ClockDriftPilotResult
{
    double measuredPrePilotSample { 0.0 };
    double measuredPostPilotSample { 0.0 };
    double nominalDistanceSamples { 0.0 };
    double measuredDistanceSamples { 0.0 };
    double sampleDrift { 0.0 };
    double driftPpm { 0.0 };
    bool dropoutDetected { false };
    bool clockDriftExcessive { false };
    bool valid { false };
};

/**
 * @struct KirkebyRegularizationConfig
 * @brief Band-limited frequency-dependent Kirkeby regularization parameters (HITO-DSP-24.7).
 */
struct KirkebyRegularizationConfig
{
    float passbandEpsilon { 1e-5f };    /**< In-band regularization factor epsilon_in within [f1, f2]. */
    float outOfBandEpsilon { 1e-1f };   /**< Out-of-band regularization factor epsilon_out at DC and Nyquist. */
    float transitionOctaves { 0.25f };  /**< Log-frequency raised-cosine transition width in octaves. */
};

/**
 * @class FarinaDeconvolver
 * @brief Performs synchronized exponential sine sweep generation and FFT-based deconvolution.
 * 
 * Implements Angelo Farina's seminal method (AES 2000) for simultaneous measurement of impulse
 * response, magnitude/phase frequency response, and harmonic distortion components (H2..H5).
 */
class FarinaDeconvolver
{
public:
    FarinaDeconvolver();
    ~FarinaDeconvolver() = default;

    /**
     * @brief Applies an in-place asymmetric Half-Hann (Tukey) fade-in and fade-out window to a buffer.
     * @param buffer Audio buffer to window in-place.
     * @param sampleRate Sampling rate in Hz.
     * @param fadeInMs Fade-in duration in milliseconds.
     * @param fadeOutMs Fade-out duration in milliseconds.
     */
    static void applyAsymmetricHalfHannWindow(std::vector<float>& buffer,
                                              double sampleRate,
                                              double fadeInMs,
                                              double fadeOutMs) noexcept;

    /**
     * @brief Generates an exponential logarithmic sine sweep signal with asymmetric anti-Gibbs fade-in/out.
     * @param sampleRate Sampling rate in Hz (e.g. 96000.0).
     * @param durationSec Sweep burst duration in seconds.
     * @param startFreqHz Sweep start frequency in Hz (e.g. 20.0f).
     * @param endFreqHz Sweep end frequency in Hz (e.g. 20000.0f).
     * @param outputFadeInMs Fade-in ramp in milliseconds (default 30.0 ms, capped proportionally on short bursts).
     * @param outputFadeOutMs Fade-out ramp in milliseconds (default 5.0 ms, capped proportionally on short bursts).
     * @return std::vector<float> Time-series audio samples of the generated sweep.
     */
    static std::vector<float> generateLogFarinaSweep(double sampleRate,
                                                     double durationSec,
                                                     float startFreqHz,
                                                     float endFreqHz,
                                                     double outputFadeInMs = 30.0,
                                                     double outputFadeOutMs = 5.0);

    /**
     * @brief Generates a Farina sweep bracketed by pre-roll (safeSilenceMs) and post-roll (interSignalSilenceMs) guard silence.
     */
    static std::vector<float> generateGuardedLogFarinaSweep(double sampleRate,
                                                            double durationSec,
                                                            float startFreqHz,
                                                            float endFreqHz,
                                                            const FarinaSweepWindowConfig& config = {});

    /**
     * @brief Generates a Farina sweep bracketed by pre- and post-sweep wideband FM pilot pulses
     * for sub-sample clock drift and ASIO buffer dropout detection (HITO-DSP-24.7).
     */
    static PilotSweepLayout generatePilotGuardedSweep(double sampleRate,
                                                      double durationSec,
                                                      float startFreqHz,
                                                      float endFreqHz,
                                                      const PilotPulseSweepConfig& config = {});

    /**
     * @brief Analyzes a recorded dual-pilot sweep response to measure sub-sample clock drift (ppm)
     * and detect ASIO buffer dropouts (HITO-DSP-24.7).
     */
    static ClockDriftPilotResult analyzePilotClockDrift(const std::vector<float>& recordedSignal,
                                                        const PilotSweepLayout& layout,
                                                        double sampleRate,
                                                        const PilotPulseSweepConfig& config = {});

    /**
     * @brief Computes a frequency-dependent Kirkeby regularization profile epsilon(f) across FFT bins (HITO-DSP-24.7).
     */
    static std::vector<float> computeKirkebyRegularizationProfile(
        size_t fftSize,
        double sampleRate,
        float startFreqHz,
        float endFreqHz,
        const KirkebyRegularizationConfig& config = {});

    /**
     * @brief Performs frequency-dependent Kirkeby regularized deconvolution H(k) = Y(k)*X*(k) / (|X(k)|^2 + eps(f)*P_max) (HITO-DSP-24.7).
     */
    static DeconvolutionResult deconvolveKirkeby(const std::vector<float>& recordedResponse,
                                                 const std::vector<float>& referenceStimulus,
                                                 double sampleRate,
                                                 float startFreqHz,
                                                 float endFreqHz,
                                                 const KirkebyRegularizationConfig& config = {});

    /**
     * @brief Convenience overload accepting sweep duration parameter.
     */
    static DeconvolutionResult deconvolveKirkeby(const std::vector<float>& recordedResponse,
                                                 const std::vector<float>& referenceStimulus,
                                                 double sampleRate,
                                                 double /*durationSec*/,
                                                 float startFreqHz,
                                                 float endFreqHz,
                                                 const KirkebyRegularizationConfig& config = {})
    {
        return deconvolveKirkeby(recordedResponse, referenceStimulus, sampleRate, startFreqHz, endFreqHz, config);
    }

    /**
     * @brief Generates the inverse filter f(t) for an exponential sine sweep with +6dB/octave time-reversal weighting.
     * @param sampleRate Sampling rate in Hz.
     * @param durationSec Sweep burst duration in seconds.
     * @param startFreqHz Sweep start frequency in Hz.
     * @param endFreqHz Sweep end frequency in Hz.
     * @return std::vector<float> Inverse filter audio samples.
     */
    static std::vector<float> generateInverseFilter(double sampleRate, double durationSec, float startFreqHz, float endFreqHz);

    /**
     * @brief Deconvolves a recorded hardware audio response with an inverse filter.
     * @param recordedResponse Recorded audio buffer from device under test.
     * @param inverseFilter Pre-generated inverse sweep filter.
     * @param sampleRate Audio sampling rate in Hz.
     * @param sweepDurationSec Original sweep duration in seconds.
     * @param startFreqHz Sweep start frequency in Hz.
     * @param endFreqHz Sweep end frequency in Hz.
     * @param inputFadeInMs Capture window fade-in duration in ms (default 10.0 ms).
     * @param inputFadeOutMs Capture window fade-out duration in ms (default 10.0 ms).
     * @return DeconvolutionResult Complete impulse response and frequency metrics.
     */
    static DeconvolutionResult deconvolve(const std::vector<float>& recordedResponse,
                                          const std::vector<float>& inverseFilter,
                                          double sampleRate,
                                          double sweepDurationSec,
                                          float startFreqHz,
                                          float endFreqHz,
                                          double inputFadeInMs = 10.0,
                                          double inputFadeOutMs = 10.0);

    /**
     * @brief Convenience overload to extract impulse response from recorded response and stimulus.
     */
    static std::vector<float> extractImpulseResponse(const std::vector<float>& recordedResponse,
                                                    const std::vector<float>& stimulusSweep,
                                                    double sampleRate,
                                                    double sweepDurationSec,
                                                    float startFreqHz,
                                                    float endFreqHz);

    /**
     * @brief Computes unwrapped phase response (rad) and group delay (samples) from linear impulse response.
     * @param impulseResponse Extracted linear impulse response.
     * @param sampleRate Audio sampling rate in Hz.
     * @param outFreqs Frequency axis output bins.
     * @param outPhaseRad Unwrapped phase response output in radians.
     * @param outGroupDelaySamples Group delay output in samples.
     */
    static void computePhaseAndGroupDelay(const std::vector<float>& impulseResponse,
                                          double sampleRate,
                                          std::vector<float>& outFreqs,
                                          std::vector<float>& outPhaseRad,
                                          std::vector<float>& outGroupDelaySamples);

private:
    static void computeFrequencyResponse(const std::vector<float>& impulseResponse,
                                         double sampleRate,
                                         std::vector<float>& outFreqs,
                                         std::vector<float>& outMagsDb,
                                         float& outPeakFreq,
                                         float& outPeakDb);
};

} // namespace abdaudiolab::math

