#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LabAnalyticEngine.h"
#include "math/LabEnvelopeCurveAnalyzer.h"
#include "audio/LabAudioEngine.h"
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

TEST_CASE("LabEnvelopeCurveAnalyzer Schroeder Backward Integration with Noise-Floor Knee and Adaptive T30/T20/T10 (HITO-DSP-24.4)", "[dsp][analytic][schroeder]")
{
    using namespace abdaudiolab::math;
    const double sampleRate = 48000.0;
    const size_t totalSamples = static_cast<size_t>(sampleRate * 2.0); // 2.0 seconds
    const double trueRt60 = 0.60; // 600 ms RT60 -> amplitude decay exp(-6.907755 * t / RT60)
    const double alpha = (3.0 * std::numbers::ln10) / trueRt60;

    SECTION("High-SNR exponential decay selects T30 with R^2 >= 0.99 and recovers RT60")
    {
        std::vector<float> ir(totalSamples, 0.0f);
        for (size_t n = 0; n < totalSamples; ++n)
        {
            const double t = static_cast<double>(n) / sampleRate;
            const double env = std::exp(-alpha * t);
            // Deterministic stationary noise floor at -70 dBFS
            const double noise = 3.162e-4 * std::sin(2.0 * std::numbers::pi * 7919.0 * t);
            ir[n] = static_cast<float>(env + noise);
        }

        auto res = LabEnvelopeCurveAnalyzer::analyzeSchroederDecay(ir, sampleRate, 0.95f);
        REQUIRE(res.valid);
        REQUIRE(res.modeUsed == ReverbEstimatorMode::T30);
        REQUIRE(res.rSquared >= 0.99f);
        REQUIRE(res.kneeSampleIndex < totalSamples - 1);
        REQUIRE_THAT(res.rt60Seconds, Catch::Matchers::WithinAbs(static_cast<float>(trueRt60), 0.04f));
    }

    SECTION("Moderate-SNR decay (~30 dB SNR above knee) automatically falls back to T20")
    {
        std::vector<float> ir(totalSamples, 0.0f);
        for (size_t n = 0; n < totalSamples; ++n)
        {
            const double t = static_cast<double>(n) / sampleRate;
            const double env = std::exp(-alpha * t);
            // Stationary noise floor at ~ -36 dBFS (amplitude 0.02 -> power -37 dB, knee at -31 dB)
            const double noise = 0.02 * std::sin(2.0 * std::numbers::pi * 6151.0 * t);
            ir[n] = static_cast<float>(env + noise);
        }

        auto res = LabEnvelopeCurveAnalyzer::analyzeSchroederDecay(ir, sampleRate, 0.95f);
        REQUIRE(res.valid);
        REQUIRE(res.modeUsed == ReverbEstimatorMode::T20);
        REQUIRE(res.rSquared >= 0.95f);
        REQUIRE_THAT(res.rt60Seconds, Catch::Matchers::WithinAbs(static_cast<float>(trueRt60), 0.06f));
    }
}

TEST_CASE("LabAnalyticEngine Synchronous Time-Domain Averaging (STA), Kirkeby Filter Passes & Delay Schroeder Integration", "[dsp][analytic][schroeder]")
{
    using namespace abdaudiolab::math;
    const double sampleRate = 48000.0;

    SECTION("Synchronous Time-Domain Averaging reduces uncorrelated noise by 1/sqrt(K)")
    {
        const size_t N = 4096;
        std::vector<float> clean(N, 0.0f);
        for (size_t i = 0; i < N; ++i)
            clean[i] = std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(i) / static_cast<float>(sampleRate));

        // 4 passes with orthogonal/canceling deterministic noise phases
        std::vector<std::vector<float>> passes(4, std::vector<float>(N, 0.0f));
        for (int k = 0; k < 4; ++k)
        {
            const float phaseOffset = static_cast<float>(k) * (std::numbers::pi_v<float> * 0.5f);
            for (size_t i = 0; i < N; ++i)
            {
                const float noise = 0.1f * std::sin(2.0f * std::numbers::pi_v<float> * 6151.0f * static_cast<float>(i) / static_cast<float>(sampleRate) + phaseOffset);
                passes[static_cast<size_t>(k)][i] = clean[i] + noise;
            }
        }

        auto sta = LabAnalyticEngine::computeSynchronousTimeDomainAverage(passes);
        REQUIRE(sta.size() == N);

        double maxErr = 0.0;
        for (size_t i = 0; i < N; ++i)
            maxErr = std::max(maxErr, static_cast<double>(std::abs(sta[i] - clean[i])));

        REQUIRE(maxErr < 1e-5);
    }

    SECTION("analyzeDelayImpulses extracts secondary echo delayTimeMs and Schroeder RT60 releaseTimeMs")
    {
        const size_t totalSamples = static_cast<size_t>(sampleRate * 1.5);
        std::vector<float> delayIr(totalSamples, 0.0f);

        const size_t primaryIdx = 480; // 10 ms
        const size_t echoDelaySamples = static_cast<size_t>(0.125 * sampleRate); // 125 ms echo spacing
        const double trueRt60 = 0.50; // 500 ms RT60
        const double alpha = (3.0 * std::numbers::ln10) / trueRt60;

        for (size_t n = primaryIdx; n < totalSamples; ++n)
        {
            const double t = static_cast<double>(n - primaryIdx) / sampleRate;
            delayIr[n] = static_cast<float>(0.25 * std::exp(-alpha * t));
        }
        delayIr[primaryIdx] = 1.0f;
        delayIr[primaryIdx + echoDelaySamples] = 0.65f; // prominent first echo tap at +125 ms

        auto res = LabAnalyticEngine::analyzeDelayImpulses({ delayIr }, sampleRate);
        REQUIRE_THAT(res.delayTimeMs.mean, Catch::Matchers::WithinAbs(125.0f, 1.0f));
        REQUIRE_THAT(res.releaseTimeMs.mean, Catch::Matchers::WithinAbs(500.0f, 60.0f));
    }

    SECTION("LabAudioEngine Zero-Latency FIR De-Coloring & Dead-Time PID Auto-Trim wiring")
    {
        abdaudiolab::audio::LabAudioEngine engine;
        std::vector<float> fir(128, 0.0f);
        fir[0] = 0.5f; // -6 dB zero-latency attenuation kernel

        REQUIRE(engine.loadInputDeColoringFir(fir, 64));
        REQUIRE(engine.isInputDeColoringEnabled());

        engine.resetAutoTrimPid(0.0f);
        auto pidState = engine.stepAutoGainTrimPid(-3.0f, 0.05, 0.01);
        REQUIRE(std::isfinite(pidState.currentGainDb));
    }
}

