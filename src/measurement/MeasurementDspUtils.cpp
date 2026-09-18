/**
 * @file MeasurementDspUtils.cpp
 * @brief Implementation of consolidated DSP primitives for measurement.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementDspUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <numeric>
#include <numbers>

namespace abdaudiolab::measurement
{

float computeRms(std::span<const float> samples) noexcept
{
    if (samples.empty()) return 0.0f;

    double sumSq = 0.0;
    for (float s : samples)
    {
        sumSq += static_cast<double>(s) * static_cast<double>(s);
    }
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(samples.size())));
}

double computeRmsDbfs(std::span<const float> samples, double fullScale, double floorDbfs) noexcept
{
    const float rms = computeRms(samples);
    if (rms <= 1e-9f || fullScale <= 1e-9)
    {
        return floorDbfs;
    }

    const double ratio = static_cast<double>(rms) / fullScale;
    const double db = 20.0 * std::log10(ratio);
    return std::max(db, floorDbfs);
}

double getWindowCoherentGain(MeasurementWindow window) noexcept
{
    switch (window)
    {
        case MeasurementWindow::Hann:        return 0.50;
        case MeasurementWindow::Blackman:    return 0.42;
        case MeasurementWindow::FlatTop:     return 0.21557895;
        case MeasurementWindow::Rectangular: return 1.00;
        default:                             return 1.00;
    }
}

std::vector<float> generateWindow(MeasurementWindow window, size_t length)
{
    std::vector<float> w(length, 1.0f);
    if (length <= 1 || window == MeasurementWindow::Rectangular)
    {
        return w;
    }

    const double nMinusOne = static_cast<double>(length - 1);
    constexpr double twoPi = 2.0 * std::numbers::pi;

    if (window == MeasurementWindow::Hann)
    {
        for (size_t n = 0; n < length; ++n)
        {
            w[n] = static_cast<float>(0.5 * (1.0 - std::cos(twoPi * static_cast<double>(n) / nMinusOne)));
        }
    }
    else if (window == MeasurementWindow::Blackman)
    {
        for (size_t n = 0; n < length; ++n)
        {
            const double phase = twoPi * static_cast<double>(n) / nMinusOne;
            w[n] = static_cast<float>(0.42 - 0.50 * std::cos(phase) + 0.08 * std::cos(2.0 * phase));
        }
    }
    else if (window == MeasurementWindow::FlatTop)
    {
        // Standard ISO Flat-Top window coefficients
        constexpr double a0 = 0.21557895;
        constexpr double a1 = 0.41663158;
        constexpr double a2 = 0.277263158;
        constexpr double a3 = 0.083578947;
        constexpr double a4 = 0.006947368;

        for (size_t n = 0; n < length; ++n)
        {
            const double phase = twoPi * static_cast<double>(n) / nMinusOne;
            w[n] = static_cast<float>(a0 - a1 * std::cos(phase)
                                         + a2 * std::cos(2.0 * phase)
                                         - a3 * std::cos(3.0 * phase)
                                         + a4 * std::cos(4.0 * phase));
        }
    }

    return w;
}

void applyWindow(std::span<const float> input,
                 std::span<float> output,
                 MeasurementWindow window)
{
    const size_t len = std::min(input.size(), output.size());
    if (window == MeasurementWindow::Rectangular)
    {
        std::copy(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(len), output.begin());
        return;
    }

    const auto win = generateWindow(window, len);
    for (size_t i = 0; i < len; ++i)
    {
        output[i] = input[i] * win[i];
    }
}

SubbinPeak refineSubbinPeak(double previous,
                           double peak,
                           double next,
                           double centerBin,
                           double binWidthHz) noexcept
{
    SubbinPeak res;
    res.bin = centerBin;
    res.frequencyHz = centerBin * binWidthHz;
    res.magnitude = peak;
    res.magnitudeDbfs = (peak > 1e-9) ? 20.0 * std::log10(peak) : -120.0;

    // A valid local peak must be strictly greater than its neighbors
    if (peak <= previous || peak <= next || previous <= 0.0 || next <= 0.0)
    {
        res.valid = false;
        res.uncertaintyBins = 0.5;
        return res;
    }

    // Parabolic interpolation on log magnitudes (Smith & Serra)
    const double alpha = std::log(std::max(previous, 1e-12));
    const double beta  = std::log(std::max(peak, 1e-12));
    const double gamma = std::log(std::max(next, 1e-12));

    const double denom = alpha - 2.0 * beta + gamma;
    if (std::abs(denom) < 1e-12)
    {
        res.valid = false;
        res.uncertaintyBins = 0.5;
        return res;
    }

    const double delta = std::clamp(0.5 * (alpha - gamma) / denom, -0.5, 0.5);
    const double interpolatedLogMag = beta - 0.25 * (alpha - gamma) * delta;

    res.bin = centerBin + delta;
    res.frequencyHz = res.bin * binWidthHz;
    res.magnitude = std::exp(interpolatedLogMag);
    res.magnitudeDbfs = (res.magnitude > 1e-9) ? 20.0 * std::log10(res.magnitude) : -120.0;
    res.uncertaintyBins = std::clamp(1.0 / (10.0 * std::abs(denom)), 0.01, 0.5);
    res.valid = true;

    return res;
}

bool performFftMagnitudes(std::span<const float> samples,
                         const SpectralAnalysisConfig& config,
                         std::span<float> output)
{
    const int n = config.fftSize;
    if (n < 64 || (n & (n - 1)) != 0)
    {
        return false; // fftSize must be power of 2
    }

    const size_t numOneSidedBins = static_cast<size_t>(n / 2 + 1);
    if (output.size() < numOneSidedBins)
    {
        return false;
    }

    const int fftOrder = static_cast<int>(std::round(std::log2(static_cast<double>(n))));
    juce::dsp::FFT fft(fftOrder);

    std::vector<float> timeData(static_cast<size_t>(n) * 2, 0.0f);

    // Apply window or copy with coherent sampling
    const size_t samplesToCopy = std::min(samples.size(), static_cast<size_t>(n));
    if (config.coherentSampling || config.window == MeasurementWindow::Rectangular)
    {
        std::copy(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(samplesToCopy), timeData.begin());
    }
    else
    {
        const auto win = generateWindow(config.window, static_cast<size_t>(n));
        for (size_t i = 0; i < samplesToCopy; ++i)
        {
            timeData[i] = samples[i] * win[i];
        }
    }

    fft.performRealOnlyForwardTransform(timeData.data());

    // Normalization: Coherent gain correction
    // For a pure cosine x[n] = A * cos(omega * n), the one-sided magnitude should recover A.
    const double cg = (config.coherentSampling || config.window == MeasurementWindow::Rectangular)
                      ? 1.0
                      : getWindowCoherentGain(config.window);
    const double normFactor = static_cast<double>(n) * cg;

    // Bin 0: DC (no doubling)
    output[0] = static_cast<float>(std::abs(timeData[0]) / normFactor);

    // Bins 1 .. N/2 - 1: One-sided spectrum (energy doubled from negative frequencies)
    const int halfN = n / 2;
    for (int b = 1; b < halfN; ++b)
    {
        const double re = static_cast<double>(timeData[static_cast<size_t>(2 * b)]);
        const double im = static_cast<double>(timeData[static_cast<size_t>(2 * b + 1)]);
        const double mag = (2.0 * std::sqrt(re * re + im * im)) / normFactor;
        output[static_cast<size_t>(b)] = static_cast<float>(mag);
    }

    // Bin N/2: Nyquist (no doubling)
    const double nyqRe = static_cast<double>(timeData[static_cast<size_t>(n)]);
    const double nyqIm = static_cast<double>(timeData[static_cast<size_t>(n + 1)]);
    output[static_cast<size_t>(halfN)] = static_cast<float>(std::sqrt(nyqRe * nyqRe + nyqIm * nyqIm) / normFactor);

    return true;
}

double integrateBandEnergy(std::span<const float> oneSidedMags,
                          int centerBin,
                          int integrationBins) noexcept
{
    if (oneSidedMags.empty()) return 0.0;

    const int totalBins = static_cast<int>(oneSidedMags.size());
    const int startBin = std::max(0, centerBin - integrationBins);
    const int endBin = std::min(totalBins - 1, centerBin + integrationBins);

    // Integrate energy in quadrature (sum of squared magnitudes)
    // Note: Since magnitudes are peak-normalized, peak RMS = peak_amplitude / sqrt(2)
    double sumSq = 0.0;
    for (int b = startBin; b <= endBin; ++b)
    {
        const double m = static_cast<double>(oneSidedMags[static_cast<size_t>(b)]);
        sumSq += m * m;
    }

    return std::sqrt(sumSq / 2.0);
}

} // namespace abdaudiolab::measurement
