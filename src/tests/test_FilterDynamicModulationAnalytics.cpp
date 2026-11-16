#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "math/analytics/FilterDynamicModulationAnalytics.h"

using namespace abdaudiolab::math::analytics::FilterDynamicModulationAnalytics;
using Catch::Matchers::WithinAbs;

TEST_CASE("FilterDynamicModulationAnalytics: Key Tracking Slope estimation", "[filter][analytics][keytrack]")
{
    SECTION("100% Key Tracking (1:1 ratio, 1 semitone per key)")
    {
        // Pivot C4 (MIDI note 60) @ 1000.0 Hz
        // C2 (36) -> 250 Hz (2 octavas abajo)
        // C3 (48) -> 500 Hz (1 octava abajo)
        // C4 (60) -> 1000 Hz
        // C5 (72) -> 2000 Hz (1 octava arriba)
        // C6 (84) -> 4000 Hz (2 octavas arriba)
        std::vector<int> notes = { 36, 48, 60, 72, 84 };
        std::vector<double> cutoffs = { 250.0, 500.0, 1000.0, 2000.0, 4000.0 };

        auto res = estimateKeyTrackSlope(notes, cutoffs, 60);

        REQUIRE(res.valid);
        REQUIRE_THAT(res.keyTrackRatio, WithinAbs(1.0, 1e-4));
        REQUIRE_THAT(res.keyTrackPercent, WithinAbs(100.0, 1e-2));
        REQUIRE_THAT(res.rSquared, WithinAbs(1.0, 1e-4));
        REQUIRE_THAT(res.baseCutoffHz, WithinAbs(1000.0, 1.0));
        REQUIRE(res.maxAbsErrorSemitones < 0.05);
    }

    SECTION("0% Key Tracking (Fixed cutoff across entire keyboard)")
    {
        std::vector<int> notes = { 36, 48, 60, 72, 84 };
        std::vector<double> cutoffs = { 1500.0, 1500.0, 1500.0, 1500.0, 1500.0 };

        auto res = estimateKeyTrackSlope(notes, cutoffs, 60);

        REQUIRE(res.valid);
        REQUIRE_THAT(res.keyTrackRatio, WithinAbs(0.0, 1e-4));
        REQUIRE_THAT(res.keyTrackPercent, WithinAbs(0.0, 1e-2));
        REQUIRE_THAT(res.baseCutoffHz, WithinAbs(1500.0, 0.5));
    }

    SECTION("+50% Key Tracking (Half-rate slope: 0.5 semitones / key)")
    {
        // A +50%, 2 octavas de teclado (24 semitonos) desplazan 1 octava de corte (12 semitonos, ratio 2:1)
        // C2 (36): 1000 * 2^(-1) = 500 Hz
        // C4 (60): 1000 Hz
        // C6 (84): 1000 * 2^(+1) = 2000 Hz
        std::vector<int> notes = { 36, 60, 84 };
        std::vector<double> cutoffs = { 500.0, 1000.0, 2000.0 };

        auto res = estimateKeyTrackSlope(notes, cutoffs, 60);

        REQUIRE(res.valid);
        REQUIRE_THAT(res.keyTrackRatio, WithinAbs(0.5, 1e-4));
        REQUIRE_THAT(res.keyTrackPercent, WithinAbs(50.0, 1e-2));
        REQUIRE_THAT(res.rSquared, WithinAbs(1.0, 1e-4));
    }

    SECTION("-100% Inverted Key Tracking (Cutoff falls as pitch rises)")
    {
        std::vector<int> notes = { 48, 60, 72 };
        std::vector<double> cutoffs = { 2000.0, 1000.0, 500.0 };

        auto res = estimateKeyTrackSlope(notes, cutoffs, 60);

        REQUIRE(res.valid);
        REQUIRE_THAT(res.keyTrackRatio, WithinAbs(-1.0, 1e-4));
        REQUIRE_THAT(res.keyTrackPercent, WithinAbs(-100.0, 1e-2));
    }

    SECTION("Edge Cases (insufficient or degenerate points)")
    {
        std::vector<int> singleNote = { 60 };
        std::vector<double> singleCutoff = { 1000.0 };
        REQUIRE_FALSE(estimateKeyTrackSlope(singleNote, singleCutoff, 60).valid);

        std::vector<int> emptyNotes;
        std::vector<double> emptyCutoffs;
        REQUIRE_FALSE(estimateKeyTrackSlope(emptyNotes, emptyCutoffs, 60).valid);
    }
}

TEST_CASE("FilterDynamicModulationAnalytics: Bipolar Envelope Excursion", "[filter][analytics][envelope]")
{
    SECTION("Positive excursion (Opening filter by 3 octaves = +36 semitones)")
    {
        const double restingHz = 400.0;
        const double peakHz = 3200.0; // 400 * 2^3

        auto res = estimateEnvelopeExcursion(restingHz, peakHz);

        REQUIRE(res.valid);
        REQUIRE_FALSE(res.isInverted);
        REQUIRE_THAT(res.excursionSemitones, WithinAbs(36.0, 1e-4));
        REQUIRE_THAT(res.restingCutoffHz, WithinAbs(400.0, 1e-4));
        REQUIRE_THAT(res.peakCutoffHz, WithinAbs(3200.0, 1e-4));
    }

    SECTION("Negative / Inverted excursion (Closing filter by 2 octaves = -24 semitones)")
    {
        const double restingHz = 2400.0;
        const double peakHz = 600.0; // 2400 / 4

        auto res = estimateEnvelopeExcursion(restingHz, peakHz);

        REQUIRE(res.valid);
        REQUIRE(res.isInverted);
        REQUIRE_THAT(res.excursionSemitones, WithinAbs(-24.0, 1e-4));
    }

    SECTION("Invalid zero or negative inputs")
    {
        REQUIRE_FALSE(estimateEnvelopeExcursion(0.0, 1000.0).valid);
        REQUIRE_FALSE(estimateEnvelopeExcursion(1000.0, 0.0).valid);
    }
}

TEST_CASE("FilterDynamicModulationAnalytics: Self-Oscillation detection and characterization", "[filter][analytics][selfosc]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 2048;

    SECTION("Silence / Below threshold: No oscillation detected")
    {
        std::vector<float> silent(numSamples, 0.0f);
        auto res = analyzeSelfOscillation(silent.data(), numSamples, sampleRate, -60.0);

        REQUIRE_FALSE(res.isOscillating);
        REQUIRE(res.rmsDbfs < -100.0);
    }

    SECTION("Pure self-oscillation tone @ 1000 Hz at -6 dBFS")
    {
        std::vector<float> tone(numSamples, 0.0f);
        const double amp = 0.501187; // approx -6.0 dBFS (peak)
        const double twoPi = 6.28318530717958647692;

        for (int i = 0; i < numSamples; ++i)
        {
            const double phase = twoPi * 1000.0 * (static_cast<double>(i) / sampleRate);
            tone[i] = static_cast<float>(amp * std::sin(phase));
        }

        auto res = analyzeSelfOscillation(tone.data(), numSamples, sampleRate, -60.0);

        REQUIRE(res.isOscillating);
        REQUIRE_THAT(res.oscillationFreqHz, WithinAbs(1000.0, 2.0));
        REQUIRE_THAT(res.peakDbfs, WithinAbs(-6.0, 0.8));
        REQUIRE(res.thdPercent < 0.2); // Seno puro: distorsión armónica despreciable
    }

    SECTION("Non-linear saturated self-oscillation with harmonic saturation (THD > 5%)")
    {
        std::vector<float> saturated(numSamples, 0.0f);
        const double twoPi = 6.28318530717958647692;
        const double f0 = 440.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = static_cast<double>(i) / sampleRate;
            // Fundamental + 12% de H2 + 8% de H3
            const double sig = 0.5 * std::sin(twoPi * f0 * t)
                             + 0.06 * std::sin(twoPi * 2.0 * f0 * t)
                             + 0.04 * std::sin(twoPi * 3.0 * f0 * t);
            saturated[i] = static_cast<float>(sig);
        }

        auto res = analyzeSelfOscillation(saturated.data(), numSamples, sampleRate, -60.0);

        REQUIRE(res.isOscillating);
        REQUIRE_THAT(res.oscillationFreqHz, WithinAbs(440.0, 1.5));
        REQUIRE(res.thdPercent > 5.0); // Se detecta claramente la saturación no lineal
    }
}
