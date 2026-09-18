/**
 * @file MeasurementDspUtils.h
 * @brief Consolidated DSP primitives for measurement: RMS, windowing, FFT magnitudes,
 *        and sub-bin parabolic peak refinement.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <span>
#include <vector>
#include <string>
#include <optional>
#include <cmath>

namespace abdaudiolab::measurement
{

/**
 * @brief Window function selection for spectral analysis.
 */
enum class MeasurementWindow
{
    Hann,        /**< Raised cosine; optimal general-purpose trade-off between resolution and leakage. Coherent gain = 0.5 */
    Blackman,    /**< 3-term Blackman; higher side-lobe suppression (-58 dB). Coherent gain = 0.42 */
    FlatTop,     /**< Specialized amplitude calibration window with flat passband. Coherent gain ≈ 0.2155 */
    Rectangular  /**< No windowing; uniform weights. Coherent gain = 1.0 (used strictly with coherent sampling) */
};

[[nodiscard]] inline std::string measurementWindowToString(MeasurementWindow window) noexcept
{
    switch (window)
    {
        case MeasurementWindow::Hann:        return "Hann";
        case MeasurementWindow::Blackman:    return "Blackman";
        case MeasurementWindow::FlatTop:     return "FlatTop";
        case MeasurementWindow::Rectangular: return "Rectangular";
        default:                             return "Unknown";
    }
}

/**
 * @brief Configuration for one-sided FFT magnitude extraction and peak detection.
 */
struct SpectralAnalysisConfig
{
    double sampleRateHz { 48000.0 };
    int fftSize { 4096 };
    MeasurementWindow window { MeasurementWindow::Hann };
    bool coherentSampling { false };
    int harmonicIntegrationBins { 2 }; /**< Bins integrated on each side of peak [center - N, center + N] */
};

/**
 * @brief Sub-bin peak refinement result.
 */
struct SubbinPeak
{
    bool valid { false };
    double bin { 0.0 };              /**< Refined fractional bin index */
    double frequencyHz { 0.0 };      /**< Frequency in Hertz */
    double magnitude { 0.0 };        /**< Refined peak linear amplitude (coherent-gain corrected) */
    double magnitudeDbfs { -120.0 };  /**< Magnitude in dBFS relative to full-scale (1.0 = 0 dBFS) */
    double uncertaintyBins { 0.05 }; /**< Estimated uncertainty bound in bins */
};

/**
 * @brief Computes Root-Mean-Square (RMS) of an audio sample span.
 * @param samples Input audio samples.
 * @return Linear RMS value in range [0.0, +inf).
 */
[[nodiscard]] float computeRms(std::span<const float> samples) noexcept;

/**
 * @brief Computes RMS in decibels relative to full-scale (dBFS).
 * @param samples Input audio samples.
 * @param fullScale Full-scale peak amplitude (default: 1.0).
 * @param floorDbfs Minimum floor returned for pure silence (default: -120.0 dBFS).
 * @return Decibel value in range [floorDbfs, 0.0].
 */
[[nodiscard]] double computeRmsDbfs(std::span<const float> samples,
                                   double fullScale = 1.0,
                                   double floorDbfs = -120.0) noexcept;

/**
 * @brief Returns the coherent gain (CG) for a declared window type.
 *        CG = (1 / N) * sum(w[n]). Dividing FFT peak bins by (N * CG) recovers true peak sinusoid amplitude.
 */
[[nodiscard]] double getWindowCoherentGain(MeasurementWindow window) noexcept;

/**
 * @brief Generates window weights for a given length.
 * @param window Target window function.
 * @param length Number of samples.
 * @return Vector of normalized window coefficients.
 */
[[nodiscard]] std::vector<float> generateWindow(MeasurementWindow window, size_t length);

/**
 * @brief Applies a window function to input samples and stores the result in output.
 * @param input Source samples.
 * @param output Destination buffer (must be at least input.size()).
 * @param window Window type.
 */
void applyWindow(std::span<const float> input,
                 std::span<float> output,
                 MeasurementWindow window);

/**
 * @brief Refines a discrete spectrum peak using 3-point parabolic interpolation on log magnitudes.
 * @param previous Magnitude of bin k - 1.
 * @param peak Magnitude of bin k.
 * @param next Magnitude of bin k + 1.
 * @param centerBin Nominal integer bin index k.
 * @param binWidthHz Width of one FFT bin in Hz (sampleRate / fftSize).
 * @return SubbinPeak with refined fractional bin, frequency, and amplitude.
 */
[[nodiscard]] SubbinPeak refineSubbinPeak(double previous,
                                         double peak,
                                         double next,
                                         double centerBin = 0.0,
                                         double binWidthHz = 1.0) noexcept;

/**
 * @brief Computes normalized one-sided magnitude spectrum.
 *
 * Normalization guarantees:
 * - One-sided spectrum: bins 0..fftSize/2.
 * - Coherent-gain corrected: A pure sinusoid of amplitude A yields peak bin magnitude = A.
 * - DC (bin 0) and Nyquist (bin N/2) are scaled without factor of 2.
 * - Intermediate bins 1..(N/2 - 1) include energy doubling for one-sided representation.
 *
 * @param samples Input time-domain samples.
 * @param config Spectral analysis configuration.
 * @param output Destination buffer for one-sided magnitudes (must have size >= fftSize / 2 + 1).
 * @return True on success, false if parameters or buffer sizes are invalid.
 */
bool performFftMagnitudes(std::span<const float> samples,
                         const SpectralAnalysisConfig& config,
                         std::span<float> output);

/**
 * @brief Integrates energy around a peak bin over [-integrationBins, +integrationBins].
 * @param oneSidedMags One-sided FFT magnitude buffer.
 * @param centerBin Integer bin index of peak.
 * @param integrationBins Bins to integrate on each side.
 * @return Total integrated RMS amplitude over the band.
 */
[[nodiscard]] double integrateBandEnergy(std::span<const float> oneSidedMags,
                                         int centerBin,
                                         int integrationBins = 2) noexcept;

} // namespace abdaudiolab::measurement
