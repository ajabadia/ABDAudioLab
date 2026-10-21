#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LoopbackCalibrator.h"
#include <vector>
#include <cmath>

using namespace abdaudiolab::math;

TEST_CASE("LoopbackCalibrator De-Coloring Active Inverse Compensation", "[math][loopback][decolor]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 4096;

    // Create a mock calibration profile with known attenuation (-3 dB at high frequencies)
    LoopbackCalibrationData calData;
    calData.isCalibrated = true;
    calData.sampleRate = sampleRate;

    const int numBins = 1024;
    calData.freqsHz.resize(numBins);
    calData.magnitudeDb.resize(numBins);
    calData.inverseCorrectionDb.resize(numBins);

    for (int i = 0; i < numBins; ++i)
    {
        float f = static_cast<float>(i) * (static_cast<float>(sampleRate) / 2048.0f);
        calData.freqsHz[i] = f;
        // Simulate high-frequency roll-off (-3dB at 10kHz)
        float rollOff = (f > 1000.0f) ? -3.0f * (f / 10000.0f) : 0.0f;
        calData.magnitudeDb[i] = rollOff;
        calData.inverseCorrectionDb[i] = -rollOff; // Exact inverse
    }

    // Audio signal: 10 kHz pure sine wave
    std::vector<float> audio(numSamples, 0.0f);
    const float testFreq = 10000.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        audio[i] = 0.5f * std::sin(2.0f * 3.14159265f * testFreq * static_cast<float>(i) / static_cast<float>(sampleRate));
    }

    // Apply inverse compensation
    LoopbackCalibrator::applyInverseCompensation(audio, calData, sampleRate);

    // Peak amplitude of compensated 10 kHz wave should have increased by ~+3 dB (factor ~1.41)
    float maxCompensated = 0.0f;
    for (size_t i = 1000; i < 3000; ++i)
    {
        float a = std::abs(audio[i]);
        if (a > maxCompensated) maxCompensated = a;
    }

    // Original peak was 0.5, with +3dB compensation it should be near 0.707f
    REQUIRE(maxCompensated > 0.55f);
}

TEST_CASE("Minimum-Phase FIR Synthesis, Kaiser Window I0(x), ERB Scale and Cubic Spline (HITO-DSP-24.2 & 24.3)",
          "[math][loopback][decolor][minphase]")
{
    SECTION("Abramowitz & Stegun 9.8.1 / 9.8.2 Bessel I0(x) and Kaiser Window")
    {
        REQUIRE_THAT(LoopbackCalibrator::besselI0(0.0), Catch::Matchers::WithinAbs(1.0, 1e-7));
        REQUIRE_THAT(LoopbackCalibrator::besselI0(2.0), Catch::Matchers::WithinAbs(2.27958530, 1e-6));
        REQUIRE_THAT(LoopbackCalibrator::besselI0(5.0), Catch::Matchers::WithinAbs(27.23987182, 1e-4));
        REQUIRE_THAT(LoopbackCalibrator::besselI0(8.0), Catch::Matchers::WithinAbs(427.56411572, 1e-3));

        auto win = LoopbackCalibrator::generateKaiserWindow(513, 8.0);
        REQUIRE(win.size() == 513);
        REQUIRE_THAT(win[256], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(win.front(), Catch::Matchers::WithinAbs(win.back(), 1e-6f));
        REQUIRE(win.front() < 0.005f);
    }

    SECTION("Glasberg & Moore (1990) ERB Bandwidth, ERB-Rate Invertibility and ERB Smoothing")
    {
        // ERB(1000 Hz) = 24.7 * (4.37 + 1) = 132.639 Hz
        REQUIRE_THAT(LoopbackCalibrator::freqToErbBandwidthHz(1000.0), Catch::Matchers::WithinAbs(132.639, 1e-3));
        for (double f : { 50.0, 440.0, 1000.0, 4000.0, 12000.0 })
        {
            const double erb = LoopbackCalibrator::freqToErbRate(f);
            const double fRec = LoopbackCalibrator::erbRateToFreq(erb);
            REQUIRE_THAT(fRec, Catch::Matchers::WithinRel(f, 1e-6));
        }

        std::vector<float> freqs(200), mags(200, 0.0f);
        for (size_t i = 0; i < freqs.size(); ++i)
            freqs[i] = 100.0f + static_cast<float>(i) * 50.0f;
        mags[100] = 6.0f; // Isolated narrow comb spike at 5100 Hz

        auto smoothed = LoopbackCalibrator::smoothSpectrumErb(freqs, mags, 1.0f);
        REQUIRE(smoothed.size() == mags.size());
        REQUIRE(smoothed[100] < 3.0f); // Spike attenuated by psychoacoustic ERB integration
        REQUIRE(smoothed[100] > 0.1f);
    }

    SECTION("NaturalCubicSpline1D C2 Continuity and Knot Interpolation")
    {
        std::vector<float> x = { 20.0f, 100.0f, 1000.0f, 5000.0f, 20000.0f };
        std::vector<float> y = { -2.0f, 0.0f, 1.5f, -1.0f, -4.0f };
        NaturalCubicSpline1D spline(x, y);
        REQUIRE(spline.isValid());

        for (size_t i = 0; i < x.size(); ++i)
            REQUIRE_THAT(spline.evaluate(x[i]), Catch::Matchers::WithinAbs(y[i], 1e-5f));

        // On uniform/log-frequency knots, midpoint interpolation stays smoothly bounded between neighbors
        std::vector<float> logX = { 1.0f, 2.0f, 3.0f, 4.0f };
        std::vector<float> logY = { -2.0f, 0.0f, 1.5f, -1.0f };
        NaturalCubicSpline1D logSpline(logX, logY);
        const float midVal = logSpline.evaluate(2.5f);
        REQUIRE(std::isfinite(midVal));
        REQUIRE(midVal > 0.0f);
        REQUIRE(midVal < 1.5f);
    }

    SECTION("Minimum-Phase FIR via Real Cepstrum Concentrates Energy Causally at t >= 0")
    {
        LoopbackCalibrationData calData;
        calData.isCalibrated = true;
        calData.sampleRate = 48000.0;

        const int numBins = 512;
        calData.freqsHz.resize(numBins);
        calData.inverseCorrectionDb.resize(numBins);
        for (int i = 0; i < numBins; ++i)
        {
            const float f = static_cast<float>(i) * (24000.0f / static_cast<float>(numBins));
            calData.freqsHz[i] = f;
            calData.inverseCorrectionDb[i] = (f > 2000.0f) ? 3.0f : 0.0f;
        }

        auto firRes = LoopbackCalibrator::synthesizeMinimumPhaseFir(calData, 48000.0, 1024, 8.0);
        REQUIRE(firRes.filterIr.size() == 1024);
        REQUIRE_THAT(firRes.maxBoostDb, Catch::Matchers::WithinAbs(3.0f, 0.25f));
        REQUIRE(firRes.safeHeadroomLinear < 0.75f);
        REQUIRE(firRes.averagePowerDb > 1.5f);

        // Causal minimum-phase property: > 90% of impulse energy is concentrated in the first 64 samples
        double earlyEnergy = 0.0;
        double totalEnergy = 0.0;
        for (size_t n = 0; n < firRes.filterIr.size(); ++n)
        {
            const double e = static_cast<double>(firRes.filterIr[n]) * firRes.filterIr[n];
            totalEnergy += e;
            if (n < 64)
                earlyEnergy += e;
        }
        REQUIRE(totalEnergy > 0.1);
        REQUIRE((earlyEnergy / totalEnergy) > 0.90);
    }

    SECTION("DeadTimeCompensatedPidController Auto-Trim Convergence and [LIMIT-PID RESET] (HITO-DSP-24.5)")
    {
        DeadTimePidConfig cfg;
        cfg.targetDbfs = -12.0f;
        cfg.toleranceDb = 0.5f;
        cfg.kp = 0.65f;
        cfg.ki = 0.20f;
        cfg.kd = 0.05f;
        cfg.stabilityMinChunks = 4;

        // Chunk = 50 ms, round-trip dead-time = 50 ms -> eta = 0.5
        DeadTimeCompensatedPidController pid(cfg, 0.05, 0.05);
        REQUIRE_THAT(pid.getDeadTimeEta(), Catch::Matchers::WithinAbs(0.5f, 1e-4f));

        // Simulate hardware chain with intrinsic -20 dBFS base peak level (needs +8 dB trim to reach -12 dBFS)
        const float plantBaseDbfs = -20.0f;
        DeadTimePidStepResult step {};
        for (int k = 0; k < 25; ++k)
        {
            const float measuredPeak = plantBaseDbfs + pid.getCurrentGainDb();
            step = pid.update(measuredPeak, false);
            if (step.isStable)
                break;
        }

        REQUIRE(step.isStable);
        REQUIRE(step.stableChunksCount >= 4);
        REQUIRE_THAT(step.gainDb, Catch::Matchers::WithinAbs(8.0f, 0.5f));

        // Trigger [LIMIT-PID RESET] via saturation/clipping
        const float gainBeforeClip = pid.getCurrentGainDb();
        auto clipStep = pid.update(-0.1f, true);
        REQUIRE(clipStep.limitPidResetTriggered);
        REQUIRE_FALSE(clipStep.isStable);
        REQUIRE(clipStep.stableChunksCount == 0);
        REQUIRE_THAT(clipStep.gainDb, Catch::Matchers::WithinAbs(gainBeforeClip - 6.0f, 1e-4f));
    }

    SECTION("ZeroLatencyPartitionedConvolver Matches Direct Linear Convolution with 0 Latency (HITO-DSP-24.7)")
    {
        const size_t firLen = 512;
        std::vector<float> kernel(firLen, 0.0f);
        for (size_t k = 0; k < firLen; ++k)
        {
            const double t = static_cast<double>(k) / 64.0;
            kernel[k] = static_cast<float>(std::exp(-t) * std::cos(0.35 * static_cast<double>(k)));
        }

        ZeroLatencyPartitionedConvolver conv;
        REQUIRE(conv.prepare(kernel, 64));
        REQUIRE(conv.isPrepared());
        REQUIRE(conv.getLatencySamples() == 0);
        REQUIRE(conv.getPartitionSize() == 64);
        REQUIRE(conv.getNumTailPartitions() == 7); // (512 - 64) / 64 = 7 tail partitions

        const size_t signalLen = 1000;
        std::vector<float> input(signalLen, 0.0f);
        for (size_t n = 0; n < signalLen; ++n)
            input[n] = static_cast<float>(std::sin(0.11 * static_cast<double>(n)) + 0.4 * std::cos(0.73 * static_cast<double>(n)));

        // Direct reference convolution y[n] = sum_{m=0}^{M-1} h[m] * x[n - m]
        std::vector<float> expected(signalLen, 0.0f);
        for (size_t n = 0; n < signalLen; ++n)
        {
            double sum = 0.0;
            const size_t maxM = std::min(n + 1, firLen);
            for (size_t m = 0; m < maxM; ++m)
                sum += static_cast<double>(kernel[m]) * static_cast<double>(input[n - m]);
            expected[n] = static_cast<float>(sum);
        }

        // Process in irregular chunk sizes (e.g., 37, 64, 113) to verify arbitrary host buffer invariance
        std::vector<float> actual = input;
        size_t pos = 0;
        const size_t chunkPattern[] = { 37, 64, 113, 19, 128 };
        size_t patIdx = 0;
        while (pos < signalLen)
        {
            const size_t step = std::min(chunkPattern[patIdx % 5], signalLen - pos);
            conv.processBlock(actual.data() + pos, step);
            pos += step;
            ++patIdx;
        }

        for (size_t n = 0; n < signalLen; ++n)
            REQUIRE_THAT(actual[n], Catch::Matchers::WithinAbs(expected[n], 1e-4f));
    }
}



