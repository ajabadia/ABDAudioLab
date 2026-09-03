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
    float peakFrequencyHz { 0.0f };                /**< Frequency bin corresponding to maximum peak response. */
    float resonancePeakDb { 0.0f };                /**< Resonance peak magnitude in dB. */
    float thdPercent { 0.0f };                     /**< Total Harmonic Distortion percentage (THD %). */
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
     * @brief Generates an exponential logarithmic sine sweep signal.
     * @param sampleRate Sampling rate in Hz (e.g. 96000.0).
     * @param durationSec Sweep burst duration in seconds.
     * @param startFreqHz Sweep start frequency in Hz (e.g. 20.0f).
     * @param endFreqHz Sweep end frequency in Hz (e.g. 20000.0f).
     * @return std::vector<float> Time-series audio samples of the generated sweep.
     */
    static std::vector<float> generateLogFarinaSweep(double sampleRate, double durationSec, float startFreqHz, float endFreqHz);

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
     * @return DeconvolutionResult Complete impulse response and frequency metrics.
     */
    static DeconvolutionResult deconvolve(const std::vector<float>& recordedResponse,
                                          const std::vector<float>& inverseFilter,
                                          double sampleRate,
                                          double sweepDurationSec,
                                          float startFreqHz,
                                          float endFreqHz);

    /**
     * @brief Convenience overload to extract impulse response from recorded response and stimulus.
     */
    static std::vector<float> extractImpulseResponse(const std::vector<float>& recordedResponse,
                                                    const std::vector<float>& stimulusSweep,
                                                    double sampleRate,
                                                    double sweepDurationSec,
                                                    float startFreqHz,
                                                    float endFreqHz);

private:
    static void computeFrequencyResponse(const std::vector<float>& impulseResponse,
                                         double sampleRate,
                                         std::vector<float>& outFreqs,
                                         std::vector<float>& outMagsDb,
                                         float& outPeakFreq,
                                         float& outPeakDb);
};

} // namespace abdaudiolab::math
