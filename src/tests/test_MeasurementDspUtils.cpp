/**
 * @file test_MeasurementDspUtils.cpp
 * @brief Unit tests for consolidated DSP primitives in MeasurementDspUtils.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "measurement/MeasurementDspUtils.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::measurement;

namespace
{
    // Generates a pure sine wave with given amplitude, frequency, and sample rate
    std::vector<float> generateSineWave(double sampleRate, double freqHz, double amplitude, size_t numSamples, double phaseOffsetRad = 0.0)
    {
        std::vector<float> wave(numSamples, 0.0f);
        const double phaseInc = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double phase = phaseOffsetRad;

        for (size_t i = 0; i < numSamples; ++i)
        {
            wave[i] = static_cast<float>(amplitude * std::sin(phase));
            phase += phaseInc;
        }
        return wave;
    }
}

TEST_CASE("MeasurementDspUtils: RMS and dBFS Precision", "[dsp_utils][rms]")
{
    const double sampleRate = 48000.0;

    SECTION("Pure sine with peak 1.0 has exact RMS = 1/sqrt(2) and ~ -3.01 dBFS")
    {
        // 1000 Hz tone, integer number of cycles (48 samples per cycle => 4800 samples = 100 cycles)
        const auto sine = generateSineWave(sampleRate, 1000.0, 1.0, 4800);

        const float rms = computeRms(sine);
        CHECK(rms == Catch::Approx(0.70710678f).margin(1e-4));

        const double dbfs = computeRmsDbfs(sine);
        CHECK(dbfs == Catch::Approx(-3.0103).margin(0.01));
    }

    SECTION("Pure silence returns zero RMS and declared noise floor")
    {
        std::vector<float> silence(1024, 0.0f);
        CHECK(computeRms(silence) == 0.0f);
        CHECK(computeRmsDbfs(silence, 1.0, -100.0) == -100.0);
    }

    SECTION("Scaled amplitude preserves dBFS linearity")
    {
        // Amplitude 0.1 (-20 dB relative to 1.0)
        const auto sineSmall = generateSineWave(sampleRate, 1000.0, 0.1, 4800);
        const double dbfs = computeRmsDbfs(sineSmall);
        CHECK(dbfs == Catch::Approx(-23.0103).margin(0.01));
    }
}

TEST_CASE("MeasurementDspUtils: Window Functions and Coherent Gain", "[dsp_utils][window]")
{
    const size_t len = 4096;

    SECTION("Hann window coherent gain matches theoretical 0.50")
    {
        const auto win = generateWindow(MeasurementWindow::Hann, len);
        double sum = 0.0;
        for (float w : win) sum += static_cast<double>(w);
        const double empiricalCg = sum / static_cast<double>(len);

        CHECK(getWindowCoherentGain(MeasurementWindow::Hann) == 0.50);
        CHECK(empiricalCg == Catch::Approx(0.50).margin(1e-3));
        CHECK(win.front() == Catch::Approx(0.0f).margin(1e-6));
    }

    SECTION("Blackman window coherent gain matches theoretical 0.42")
    {
        const auto win = generateWindow(MeasurementWindow::Blackman, len);
        double sum = 0.0;
        for (float w : win) sum += static_cast<double>(w);
        const double empiricalCg = sum / static_cast<double>(len);

        CHECK(getWindowCoherentGain(MeasurementWindow::Blackman) == 0.42);
        CHECK(empiricalCg == Catch::Approx(0.42).margin(1e-3));
    }

    SECTION("FlatTop window coherent gain matches ISO definition ~ 0.2156")
    {
        const auto win = generateWindow(MeasurementWindow::FlatTop, len);
        double sum = 0.0;
        for (float w : win) sum += static_cast<double>(w);
        const double empiricalCg = sum / static_cast<double>(len);

        CHECK(getWindowCoherentGain(MeasurementWindow::FlatTop) == Catch::Approx(0.21557895).margin(1e-4));
        CHECK(empiricalCg == Catch::Approx(0.21557895).margin(1e-3));
    }
}

TEST_CASE("MeasurementDspUtils: FFT Magnitude Normalization and Peak Amplitude Preservation", "[dsp_utils][fft]")
{
    const double sampleRate = 48000.0;
    const int fftSize = 4096;
    const double binWidth = sampleRate / fftSize; // 11.71875 Hz per bin

    SECTION("Coherent sampling with Rectangular window recovers exact peak amplitude 1.0 at target bin")
    {
        // Target exact bin 32: f = 32 * (48000 / 4096) = 375.0 Hz
        const double targetFreq = 32.0 * binWidth;
        const auto sine = generateSineWave(sampleRate, targetFreq, 1.0, fftSize);

        SpectralAnalysisConfig cfg;
        cfg.sampleRateHz = sampleRate;
        cfg.fftSize = fftSize;
        cfg.window = MeasurementWindow::Rectangular;
        cfg.coherentSampling = true;

        std::vector<float> mags(fftSize / 2 + 1, 0.0f);
        REQUIRE(performFftMagnitudes(sine, cfg, mags));

        // Bin 32 must recover peak amplitude 1.0 (0 dBFS)
        CHECK(mags[32] == Catch::Approx(1.0f).margin(1e-4));

        // Adjacent bins must have near-zero leakage (< -80 dB)
        CHECK(mags[31] < 1e-4f);
        CHECK(mags[33] < 1e-4f);
    }

    SECTION("Hann window with coherent gain normalization recovers peak amplitude ~ 1.0")
    {
        const double targetFreq = 64.0 * binWidth;
        const auto sine = generateSineWave(sampleRate, targetFreq, 0.8, fftSize);

        SpectralAnalysisConfig cfg;
        cfg.sampleRateHz = sampleRate;
        cfg.fftSize = fftSize;
        cfg.window = MeasurementWindow::Hann;
        cfg.coherentSampling = false;

        std::vector<float> mags(fftSize / 2 + 1, 0.0f);
        REQUIRE(performFftMagnitudes(sine, cfg, mags));

        // Center bin recovers peak amplitude 0.8 with Hann window
        CHECK(mags[64] == Catch::Approx(0.8f).margin(0.01));
    }
}

TEST_CASE("MeasurementDspUtils: Sub-bin Peak Refinement", "[dsp_utils][subbin]")
{
    const double binWidthHz = 10.0;

    SECTION("Symmetric peak centered at integer bin returns zero delta")
    {
        const auto peak = refineSubbinPeak(0.5, 1.0, 0.5, 20.0, binWidthHz);
        REQUIRE(peak.valid);
        CHECK(peak.bin == Catch::Approx(20.0).margin(1e-5));
        CHECK(peak.frequencyHz == Catch::Approx(200.0).margin(1e-5));
        CHECK(peak.magnitude == Catch::Approx(1.0).margin(1e-4));
    }

    SECTION("Fractional peak between bins is recovered accurately")
    {
        // Suppose a peak lies at bin 20.35.
        // Gaussian/parabolic shape: mag(k) = A * exp(- (k - 20.35)^2 / (2 * s^2))
        const double A = 1.0;
        const double s = 0.8;
        auto shape = [&](double k) { return A * std::exp(- (k - 20.35) * (k - 20.35) / (2.0 * s * s)); };

        const double prev = shape(19.0);
        const double center = shape(20.0);
        const double next = shape(21.0);

        const auto peak = refineSubbinPeak(prev, center, next, 20.0, binWidthHz);
        REQUIRE(peak.valid);
        CHECK(peak.bin == Catch::Approx(20.35).margin(0.02));
        CHECK(peak.magnitude == Catch::Approx(A).margin(0.02));
    }
}

TEST_CASE("MeasurementDspUtils: Band Energy Integration", "[dsp_utils][integration]")
{
    // Synthesize one-sided spectrum with main lobe spread over bins 30..34
    std::vector<float> mags(128, 0.0f);
    mags[30] = 0.1f;
    mags[31] = 0.5f;
    mags[32] = 1.0f; // Main peak
    mags[33] = 0.5f;
    mags[34] = 0.1f;

    // Integrating +/- 2 bins around bin 32 captures the entire lobe energy
    const double rmsIntegrated = integrateBandEnergy(mags, 32, 2);
    CHECK(rmsIntegrated > 0.707); // Higher than single-bin peak RMS
    CHECK(rmsIntegrated < 1.5);
}
