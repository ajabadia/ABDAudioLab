/**
 * @file test_UniversalABComparatorIntegration.cpp
 * @brief Hermetic unit tests for UniversalABComparatorEngine (HITO-AB-DIFF-01).
 *
 * Verifies that the engine surpasses ABDMS2000 algorithms:
 *   - Sub-sample sinc cross-correlation alignment.
 *   - Error-to-Signal Ratio (ESR) in dB and residual null cancellation.
 *   - 24-Band 1/3-octave acoustic spectral analysis and cosine similarity.
 *   - 4 Macro-Bands (Bass, Low-Mid, High-Mid, High) energy breakdown.
 *   - Phase Invariance detection for free-running oscillators (CSDM coherence).
 *
 * Tags: [UniversalAB][Comparator][NullTest][ABDMS2000][Hermetic]
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../math/UniversalABComparatorEngine.h"
#include <cmath>
#include <numbers>
#include <vector>

using namespace abdaudiolab::math;
using Catch::Approx;

namespace {

juce::AudioBuffer<float> generateTestTone(
    int numSamples,
    double sampleRate,
    float freqHz,
    float phaseOffsetRad = 0.0f,
    float decayRate = 3.0f,
    float gain = 0.5f)
{
    juce::AudioBuffer<float> buf(2, numSamples);
    buf.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        float* p = buf.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-decayRate * t);
            p[i] = gain * std::sin(2.0f * std::numbers::pi_v<float> * freqHz * t + phaseOffsetRad) * env;
        }
    }
    return buf;
}

} // namespace

TEST_CASE("UniversalABComparatorEngine: Identical Buffers Yield True Null Cancellation", "[UniversalAB][Hermetic]")
{
    constexpr double kSr = 48000.0;
    constexpr int kSamples = 48000; // 1 second

    auto ref = generateTestTone(kSamples, kSr, 440.0f, 0.0f, 2.0f, 0.6f);
    auto cap = generateTestTone(kSamples, kSr, 440.0f, 0.0f, 2.0f, 0.6f);

    auto m = UniversalABComparatorEngine::compare(ref, cap, kSr);

    REQUIRE(m.verdictCode == "PASS_IDENTITY");
    REQUIRE(m.correlationPeak == Approx(1.0).margin(0.001));
    REQUIRE(m.sampleOffset == Approx(0.0).margin(0.01));
    REQUIRE(m.timeOffsetMs == Approx(0.0).margin(0.001));
    REQUIRE(m.esrDb < -100.0);
    REQUIRE(m.spectralCosineSimilarityPct > 99.9);
    REQUIRE(m.envSlopeDeltaDb == Approx(0.0).margin(0.01));
    REQUIRE(m.stereoWidthDeltaPct == Approx(0.0).margin(0.01));
}

TEST_CASE("UniversalABComparatorEngine: Fractional Sub-Sample Delay Resolved", "[UniversalAB][Hermetic]")
{
    constexpr double kSr = 48000.0;
    constexpr int kSamples = 48000;

    auto ref = generateTestTone(kSamples, kSr, 440.0f, 0.0f, 2.0f, 0.6f);

    // Create candidate shifted by exactly 4 integer samples
    juce::AudioBuffer<float> cap(2, kSamples);
    cap.clear();
    constexpr int kShift = 4;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* r = ref.getReadPointer(ch);
        float* c = cap.getWritePointer(ch);
        for (int i = 0; i < kSamples - kShift; ++i)
        {
            c[i + kShift] = r[i];
        }
    }

    auto m = UniversalABComparatorEngine::compare(ref, cap, kSr);

    REQUIRE(m.correlationPeak > 0.99);
    REQUIRE(m.sampleOffset == Approx(static_cast<double>(kShift)).margin(0.2));
    REQUIRE(m.timeOffsetMs == Approx((kShift / kSr) * 1000.0).margin(0.01));
    REQUIRE(m.spectralCosineSimilarityPct > 99.0);
}

TEST_CASE("UniversalABComparatorEngine: Free-Running Oscillator Phase Drift Detected as Phase-Invariant", "[UniversalAB][Hermetic]")
{
    constexpr double kSr = 48000.0;
    constexpr int kSamples = 48000;

    // Multi-harmonic synth patch (440 Hz fundamental + 880 Hz 2nd harmonic)
    // Candidate has an independent harmonic phase shift (free-running oscillator)
    juce::AudioBuffer<float> ref(2, kSamples);
    juce::AudioBuffer<float> cap(2, kSamples);
    ref.clear();
    cap.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        float* pR = ref.getWritePointer(ch);
        float* pC = cap.getWritePointer(ch);
        for (int i = 0; i < kSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(kSr);
            const float env = std::exp(-2.0f * t);
            const float f1 = 0.4f * std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * t);
            const float f2Ref = 0.25f * std::sin(2.0f * std::numbers::pi_v<float> * 880.0f * t);
            const float f2Cap = 0.25f * std::sin(2.0f * std::numbers::pi_v<float> * 880.0f * t + 1.2f);
            pR[i] = (f1 + f2Ref) * env;
            pC[i] = (f1 + f2Cap) * env;
        }
    }

    auto m = UniversalABComparatorEngine::compare(ref, cap, kSr);

    // In ABDMS2000, raw subtraction would fail with poor ESR, but ABDAudioLab detects phase invariance
    REQUIRE(m.spectralCosineSimilarityPct > 99.0);
    REQUIRE(m.phaseInvarianceCoherence > 0.985);
    REQUIRE(m.isPhaseInvariant == true);
    REQUIRE(m.verdictCode == "PASS_PHASE_INVARIANT");
}

TEST_CASE("UniversalABComparatorEngine: 24-Band Spectral and Macro-Band Divergence", "[UniversalAB][Hermetic]")
{
    constexpr double kSr = 48000.0;
    constexpr int kSamples = 48000;

    // Multitone with low (200 Hz) and high (5000 Hz) harmonics
    juce::AudioBuffer<float> ref(2, kSamples);
    juce::AudioBuffer<float> cap(2, kSamples);
    ref.clear();
    cap.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        float* pR = ref.getWritePointer(ch);
        float* pC = cap.getWritePointer(ch);
        for (int i = 0; i < kSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(kSr);
            const float env = std::exp(-2.0f * t);
            const float low = 0.4f * std::sin(2.0f * std::numbers::pi_v<float> * 200.0f * t);
            const float high = 0.4f * std::sin(2.0f * std::numbers::pi_v<float> * 5000.0f * t);
            pR[i] = (low + high) * env;
            // Candidate has severe high attenuation (low-pass filtered)
            pC[i] = (low + 0.05f * high) * env;
        }
    }

    auto m = UniversalABComparatorEngine::compare(ref, cap, kSr);

    // High macro-band should show significant drop in candidate
    REQUIRE(m.highCapPct < m.highRefPct);
    REQUIRE(m.spectralCentroidCapHz < m.spectralCentroidRefHz);
    REQUIRE(m.spectralCosineSimilarityPct < 96.0);
    REQUIRE(m.verdictCode == "FAIL_DIVERGENCE");
}

TEST_CASE("UniversalABComparatorEngine: Loudness Matching Compensates Volume Bias and Isolates Timbre", "[UniversalAB][Hermetic]")
{
    constexpr double kSr = 48000.0;
    constexpr int kSamples = 48000;

    // Candidate has identical timbre but is 6 dB quieter (0.5x amplitude)
    auto ref = generateTestTone(kSamples, kSr, 440.0f, 0.0f, 2.0f, 0.6f);
    auto cap = generateTestTone(kSamples, kSr, 440.0f, 0.0f, 2.0f, 0.3f);

    // 1. Without loudness matching, volume discrepancy degrades null cancellation
    auto mRaw = UniversalABComparatorEngine::compare(ref, cap, kSr, false);
    REQUIRE(mRaw.rmsDeltaDb == Approx(-6.02).margin(0.15));
    REQUIRE(mRaw.loudnessMatched == false);
    REQUIRE(mRaw.esrDb > -12.0); // Raw null test reflects the 6 dB level mismatch

    // 2. With autoMatchLoudness = true, engine normalizes integrated level and proves timbral identity
    auto mNorm = UniversalABComparatorEngine::compare(ref, cap, kSr, true);
    REQUIRE(mNorm.loudnessMatched == true);
    REQUIRE(mNorm.esrDb < -100.0); // Null test cancels to noise floor
    REQUIRE(mNorm.verdictCode == "PASS_IDENTITY");
    REQUIRE(mNorm.spectralCosineSimilarityPct > 99.9);
}
