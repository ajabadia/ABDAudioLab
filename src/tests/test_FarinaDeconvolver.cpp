#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/FarinaDeconvolver.h"
#include <cmath>

TEST_CASE("FarinaDeconvolver Log Sweep Generation", "[dsp][farina]")
{
    abdaudiolab::math::FarinaDeconvolver deconvolver;
    
    double sampleRate = 96000.0;
    double durationSec = 1.0;
    float startFreq = 20.0f;
    float endFreq = 20000.0f;

    auto sweep = deconvolver.generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);

    size_t expectedSamples = static_cast<size_t>(sampleRate * durationSec);
    REQUIRE(sweep.size() == expectedSamples);

    // Check non-zero amplitude within peak bounds
    float maxAmp = 0.0f;
    for (float sample : sweep)
    {
        maxAmp = std::max(maxAmp, std::abs(sample));
    }

    REQUIRE(maxAmp > 0.5f);
    REQUIRE(maxAmp <= 1.0f);
}

TEST_CASE("FarinaDeconvolver Impulse Response Extraction", "[dsp][farina]")
{
    abdaudiolab::math::FarinaDeconvolver deconvolver;
    
    double sampleRate = 48000.0;
    double durationSec = 0.5;
    float startFreq = 20.0f;
    float endFreq = 20000.0f;

    auto sweep = deconvolver.generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);
    
    // Simulate ideal identity system (response == sweep)
    auto ir = deconvolver.extractImpulseResponse(sweep, sweep, sampleRate, durationSec, startFreq, endFreq);

    REQUIRE_FALSE(ir.empty());

    // Identity system should produce an impulse peak
    float maxPeak = 0.0f;
    for (float sample : ir)
    {
        maxPeak = std::max(maxPeak, std::abs(sample));
    }

    REQUIRE(maxPeak > 0.0001f);
}

TEST_CASE("FarinaDeconvolver Asymmetric Half-Hann Window and Guard Silences (HITO-DSP-24.1)", "[dsp][farina][window]")
{
    using namespace abdaudiolab::math;
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const float startFreq = 20.0f;
    const float endFreq = 20000.0f;

    SECTION("Asymmetric fade-in (30 ms) and fade-out (5 ms) suppress onset/termination clicks")
    {
        auto windowedSweep = FarinaDeconvolver::generateLogFarinaSweep(
            sampleRate, durationSec, startFreq, endFreq, 30.0, 5.0);
        auto unwindowedSweep = FarinaDeconvolver::generateLogFarinaSweep(
            sampleRate, durationSec, startFreq, endFreq, 0.0, 0.0);

        REQUIRE(windowedSweep.size() == 48000);
        REQUIRE(std::abs(windowedSweep.front()) < 1e-6f);
        REQUIRE(std::abs(windowedSweep.back()) < 1e-5f);

        // Unwindowed sweep terminates at non-zero amplitude (causing a step discontinuity)
        // whereas the 5 ms Half-Hann fade-out smoothly decays the last 240 samples to 0.
        const size_t fadeInSamples = static_cast<size_t>(std::lround(0.030 * sampleRate)); // 1440
        const size_t midIdx = windowedSweep.size() / 2;
        REQUIRE_THAT(windowedSweep[midIdx], Catch::Matchers::WithinAbs(unwindowedSweep[midIdx], 1e-6f));
        REQUIRE(std::abs(windowedSweep[10]) < std::abs(unwindowedSweep[10]));
        REQUIRE(fadeInSamples == 1440);
    }

    SECTION("Guarded sweep prepends safeSilence (15 ms) and appends interSignalSilence (650 ms)")
    {
        FarinaSweepWindowConfig cfg;
        cfg.outputFadeInMs = 30.0;
        cfg.outputFadeOutMs = 5.0;
        cfg.safeSilenceMs = 15.0;
        cfg.interSignalSilenceMs = 650.0;

        auto guarded = FarinaDeconvolver::generateGuardedLogFarinaSweep(
            sampleRate, durationSec, startFreq, endFreq, cfg);

        const size_t preSamples = static_cast<size_t>(std::lround(0.015 * sampleRate));  // 720
        const size_t coreSamples = static_cast<size_t>(std::lround(1.0 * sampleRate));   // 48000
        const size_t postSamples = static_cast<size_t>(std::lround(0.650 * sampleRate)); // 31200

        REQUIRE(guarded.size() == preSamples + coreSamples + postSamples);

        for (size_t i = 0; i < preSamples; ++i)
            REQUIRE(guarded[i] == 0.0f);

        for (size_t i = preSamples + coreSamples; i < guarded.size(); ++i)
            REQUIRE(guarded[i] == 0.0f);
    }

    SECTION("Dual-Pilot Pulse Clock Drift & ASIO Buffer Dropout Detection (HITO-DSP-24.7)")
    {
        PilotPulseSweepConfig pilotCfg;
        auto layout = FarinaDeconvolver::generatePilotGuardedSweep(
            sampleRate, 0.25, startFreq, endFreq, pilotCfg);
        REQUIRE(!layout.compositeSignal.empty());
        REQUIRE(!layout.referencePilot.empty());

        // 1. Nominal zero-drift recording
        auto cleanDrift = FarinaDeconvolver::analyzePilotClockDrift(
            layout.compositeSignal, layout, sampleRate, pilotCfg);
        REQUIRE(cleanDrift.valid);
        REQUIRE_THAT(cleanDrift.sampleDrift, Catch::Matchers::WithinAbs(0.0, 0.05));
        REQUIRE_THAT(cleanDrift.driftPpm, Catch::Matchers::WithinAbs(0.0, 5.0));
        REQUIRE_FALSE(cleanDrift.dropoutDetected);
        REQUIRE_FALSE(cleanDrift.clockDriftExcessive);

        // 2. Simulate a 4-sample ASIO dropout in the middle of the sweep
        std::vector<float> droppedSignal = layout.compositeSignal;
        const size_t dropPos = layout.sweepStartSample + layout.sweepLengthSamples / 2;
        droppedSignal.erase(droppedSignal.begin() + static_cast<std::ptrdiff_t>(dropPos),
                            droppedSignal.begin() + static_cast<std::ptrdiff_t>(dropPos + 4));
        droppedSignal.resize(layout.compositeSignal.size(), 0.0f);

        auto dropRes = FarinaDeconvolver::analyzePilotClockDrift(
            droppedSignal, layout, sampleRate, pilotCfg);
        REQUIRE(dropRes.valid);
        REQUIRE_THAT(dropRes.sampleDrift, Catch::Matchers::WithinAbs(-4.0, 0.1));
        REQUIRE(dropRes.dropoutDetected);
    }

    SECTION("Kirkeby Frequency-Dependent Band-Limited Regularization (HITO-DSP-24.7)")
    {
        KirkebyRegularizationConfig kCfg;
        kCfg.passbandEpsilon = 1e-5f;
        kCfg.outOfBandEpsilon = 1e-1f;
        kCfg.transitionOctaves = 0.25f;

        const size_t fftSize = 4096;
        auto profile = FarinaDeconvolver::computeKirkebyRegularizationProfile(
            fftSize, sampleRate, 100.0f, 10000.0f, kCfg);
        REQUIRE(profile.size() == fftSize / 2 + 1);

        // DC bin (0 Hz) and Nyquist bin (24 kHz) must equal outOfBandEpsilon
        REQUIRE_THAT(profile.front(), Catch::Matchers::WithinAbs(1e-1f, 1e-6f));
        REQUIRE_THAT(profile.back(), Catch::Matchers::WithinAbs(1e-1f, 1e-6f));

        // 1 kHz bin (inside passband [100 Hz, 10 kHz]) must equal passbandEpsilon
        const size_t bin1k = static_cast<size_t>(std::lround(1000.0 * static_cast<double>(fftSize) / sampleRate));
        REQUIRE_THAT(profile[bin1k], Catch::Matchers::WithinAbs(1e-5f, 1e-8f));

        // Deconvolve unity system using Kirkeby inversion
        auto stim = FarinaDeconvolver::generateLogFarinaSweep(sampleRate, 0.2, 40.0f, 18000.0f);
        auto res = FarinaDeconvolver::deconvolveKirkeby(stim, stim, sampleRate, 40.0f, 18000.0f, kCfg);
        REQUIRE(!res.linearIR.empty());
        REQUIRE(std::abs(res.linearIR[0]) > 0.2f);
    }
}


