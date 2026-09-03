#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LabAnalyticEngine.h"
#include <cmath>
#include <vector>
#include <numbers>

TEST_CASE("LabAnalyticEngine ADSR Envelope Analysis", "[dsp][analytic]")
{
    double sampleRate = 48000.0;
    size_t totalSamples = static_cast<size_t>(sampleRate * 1.5); // 1.5s recording
    std::vector<float> syntheticAdsr(totalSamples, 0.0f);

    size_t attackSamples = static_cast<size_t>(sampleRate * 0.05); // 50ms attack
    size_t decaySamples = static_cast<size_t>(sampleRate * 0.10);  // 100ms decay
    size_t sustainSamples = static_cast<size_t>(sampleRate * 0.50); // 500ms sustain
    size_t releaseSamples = static_cast<size_t>(sampleRate * 0.20); // 200ms release

    // 1. Attack phase: 0 -> 1.0
    for (size_t i = 0; i < attackSamples; ++i)
    {
        syntheticAdsr[i] = static_cast<float>(i) / static_cast<float>(attackSamples);
    }

    // 2. Decay phase: 1.0 -> 0.7
    for (size_t i = 0; i < decaySamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(decaySamples);
        syntheticAdsr[attackSamples + i] = 1.0f - 0.3f * t;
    }

    // 3. Sustain phase: 0.7
    for (size_t i = 0; i < sustainSamples; ++i)
    {
        syntheticAdsr[attackSamples + decaySamples + i] = 0.7f;
    }

    // 4. Release phase: 0.7 -> 0.0001 (-70dB)
    size_t releaseStart = attackSamples + decaySamples + sustainSamples;
    for (size_t i = 0; i < releaseSamples && (releaseStart + i) < totalSamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(releaseSamples);
        syntheticAdsr[releaseStart + i] = 0.7f * std::exp(-5.0f * t);
    }

    std::vector<std::vector<float>> passes = { syntheticAdsr };
    auto adsrRes = abdaudiolab::math::LabAnalyticEngine::analyzeAdsrEnvelopes(passes, sampleRate);

    // Verify Attack ~ 50ms (+/- 10ms smoothing window)
    REQUIRE(adsrRes.attackTimeMs.mean > 30.0f);
    REQUIRE(adsrRes.attackTimeMs.mean < 70.0f);

    // Verify Sustain Level ~ 0.7 (+/- 0.1)
    REQUIRE_THAT(adsrRes.sustainLevel.mean, Catch::Matchers::WithinAbs(0.7f, 0.15f));

    // Verify Release Time is measured (> 10ms)
    REQUIRE(adsrRes.releaseTimeMs.mean > 10.0f);
}

TEST_CASE("LabAnalyticEngine WaveShaper THD Calculation", "[dsp][analytic]")
{
    double sampleRate = 48000.0;
    size_t totalSamples = 4800; // 100ms
    std::vector<float> distortedSine(totalSamples, 0.0f);

    double f1 = 1000.0;
    double twoPi = 2.0 * std::numbers::pi;

    // Fundamental (1 kHz) + 10% 2nd Harmonic (2 kHz) + 5% 3rd Harmonic (3 kHz)
    for (size_t i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        distortedSine[i] = static_cast<float>(std::sin(twoPi * f1 * t) + 
                                               0.10 * std::sin(twoPi * (f1 * 2.0) * t) + 
                                               0.05 * std::sin(twoPi * (f1 * 3.0) * t));
    }

    std::vector<std::vector<float>> passes = { distortedSine };
    auto shaperRes = abdaudiolab::math::LabAnalyticEngine::analyzeWaveShaperRamps(passes, sampleRate);

    // Expected THD ~ sqrt(0.10^2 + 0.05^2) / 1.0 = sqrt(0.0125) ~ 11.18%
    REQUIRE(shaperRes.thdPercent.mean > 5.0f);
    REQUIRE(shaperRes.thdPercent.mean < 25.0f);
}

TEST_CASE("LabAnalyticEngine LFO Cyclic Modulator Analysis", "[dsp][analytic]")
{
    double sampleRate = 48000.0;
    size_t totalSamples = static_cast<size_t>(sampleRate * 2.0); // 2 seconds
    std::vector<float> lfoModulated(totalSamples, 0.0f);

    double carrierFreq = 1000.0;
    double lfoRate = 2.0; // 2 Hz LFO
    double twoPi = 2.0 * std::numbers::pi;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double lfoEnv = 0.5 + 0.5 * std::sin(twoPi * lfoRate * t); // 0.0 to 1.0 amplitude modulation
        lfoModulated[i] = static_cast<float>(lfoEnv * std::sin(twoPi * carrierFreq * t));
    }

    std::vector<std::vector<float>> passes = { lfoModulated };
    auto modRes = abdaudiolab::math::LabAnalyticEngine::analyzeCyclicModulator(passes, sampleRate);

    // LFO Rate should detect ~ 2 Hz (+/- 0.5 Hz)
    REQUIRE(modRes.rateHz.mean >= 1.5f);
    REQUIRE(modRes.rateHz.mean <= 2.5f);

    // Depth should be > 50%
    REQUIRE(modRes.depthPercent.mean > 50.0f);
}
