/**
 * @file test_DynamicsMeasurementAdapter.cpp
 * @brief Catch2 unit tests for DynamicsMeasurementAdapter (Campaña 20.10.3-D).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/adapters/DynamicsMeasurementAdapter.h"
#include <cmath>
#include <vector>

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

namespace
{

/**
 * @brief Helper generating deterministic sine tone with exponential attack.
 */
std::vector<float> generateSynthTone(double sampleRate,
                                     double durationSec,
                                     double freqHz,
                                     float peakGain,
                                     double attackSec = 0.02)
{
    size_t totalSamples = static_cast<size_t>(sampleRate * durationSec);
    std::vector<float> buffer(totalSamples, 0.0f);

    size_t attackSamples = static_cast<size_t>(sampleRate * attackSec);

    for (size_t i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double env = 1.0;
        if (i < attackSamples && attackSamples > 0)
            env = static_cast<double>(i) / static_cast<double>(attackSamples);

        double s = peakGain * env * std::sin(2.0 * 3.14159265358979323846 * freqHz * t);
        buffer[i] = static_cast<float>(s);
    }

    return buffer;
}

} // namespace

TEST_CASE("DynamicsMeasurementAdapter - Default velocity grid adheres to approved campaign", "[measurement][dynamics][adapter]")
{
    auto grid = DynamicsMeasurementAdapter::getDefaultVelocityGrid();
    std::vector<int> expected = { 0, 1, 8, 16, 24, 32, 48, 64, 80, 96, 112, 120, 127 };
    REQUIRE(grid == expected);
}

TEST_CASE("DynamicsMeasurementAdapter - Single point analysis", "[measurement][dynamics][adapter]")
{
    double sampleRate = 48000.0;

    SECTION("Velocity 0 special case")
    {
        std::vector<float> silence(4800, 0.0f);
        DynamicPoint pt = DynamicsMeasurementAdapter::analyzeSinglePoint(
            0, silence, sampleRate, 0, 4800, 50.0, 90.0, "preset_hash_0", "audio_hash_0");

        REQUIRE(pt.velocity == 0);
        REQUIRE(pt.status == "skipped");
        REQUIRE(pt.reason == "midi_note_on_velocity_zero");
        REQUIRE_THAT(pt.rmsDbfs, WithinAbs(-96.0, 1e-4));
        REQUIRE_THAT(pt.peakDbfs, WithinAbs(-96.0, 1e-4));
        REQUIRE_THAT(pt.spectralCentroidHz, WithinAbs(0.0, 1e-4));
        REQUIRE(pt.presetStateHash == "preset_hash_0");
        REQUIRE(pt.audioArtifactHash == "audio_hash_0");
    }

    SECTION("Signal below noise floor returns unreliable without invented spectral values")
    {
        std::vector<float> noise(4800, 1e-6f);
        DynamicPoint pt = DynamicsMeasurementAdapter::analyzeSinglePoint(
            32, noise, sampleRate, 0, 4800);

        REQUIRE(pt.velocity == 32);
        REQUIRE(pt.status == "unreliable");
        REQUIRE(pt.reason == "signal_below_noise_floor_or_too_short");
        REQUIRE_THAT(pt.spectralCentroidHz, WithinAbs(0.0, 1e-4)); // No invented numbers
    }

    SECTION("Normal tone at 440 Hz with 20ms attack")
    {
        auto audio = generateSynthTone(sampleRate, 0.5, 440.0, 0.5f, 0.020);
        DynamicPoint pt = DynamicsMeasurementAdapter::analyzeSinglePoint(
            64, audio, sampleRate, 0, audio.size(), 100.0, 400.0, "preset_v64", "audio_v64");

        REQUIRE(pt.velocity == 64);
        REQUIRE(pt.status == "observed");
        REQUIRE(pt.reason.empty());

        // 0.5 peak gain = -6.02 dBFS
        REQUIRE_THAT(pt.peakDbfs, WithinAbs(-6.02, 0.5));
        // Sine RMS = peak - 3.01 dB = -9.03 dBFS
        REQUIRE_THAT(pt.rmsDbfs, WithinAbs(-9.03, 0.8));

        // Attack time should be around 16-20 ms (10% to 90% of 20ms)
        REQUIRE(pt.attackTimeMs >= 10.0);
        REQUIRE(pt.attackTimeMs <= 25.0);

        // Centroid of pure 440 Hz tone should be close to 440 Hz
        REQUIRE(pt.spectralCentroidHz >= 420.0);
        REQUIRE(pt.spectralCentroidHz <= 480.0);

        // Rolloff must be >= fundamental
        REQUIRE(pt.spectralRolloffHz >= 420.0);

        REQUIRE_THAT(pt.measurementWindowStartMs, WithinAbs(100.0, 1e-4));
        REQUIRE_THAT(pt.measurementWindowEndMs, WithinAbs(400.0, 1e-4));
        REQUIRE(pt.presetStateHash == "preset_v64");
        REQUIRE(pt.audioArtifactHash == "audio_v64");
    }
}

TEST_CASE("DynamicsMeasurementAdapter - Fit curve model decouples R^2 from assumed linearity", "[measurement][dynamics][adapter]")
{
    SECTION("Linear dataset yields high R^2 with linear model")
    {
        std::vector<double> x = { 1.0, 32.0, 64.0, 96.0, 127.0 };
        std::vector<double> y = { 100.0, 200.0, 300.0, 400.0, 500.0 };

        CurveFitMetadata fit = DynamicsMeasurementAdapter::fitCurveModel(x, y, "velocity", "spectralCentroidHz");
        REQUIRE(fit.model == "linear");
        REQUIRE(fit.rSquared >= 0.99);
        REQUIRE(fit.xVariable == "velocity");
        REQUIRE(fit.yVariable == "spectralCentroidHz");
    }

    SECTION("Logarithmic volume taper is identified as logarithmic without penalty")
    {
        std::vector<double> x = { 1.0, 8.0, 16.0, 32.0, 64.0, 96.0, 127.0 };
        std::vector<double> y;
        for (double v : x)
            y.push_back(-40.0 + 20.0 * std::log(v)); // pure logarithmic taper

        CurveFitMetadata fit = DynamicsMeasurementAdapter::fitCurveModel(x, y, "velocity", "rmsDbfs");
        REQUIRE(fit.model == "logarithmic");
        REQUIRE(fit.rSquared >= 0.98);
        REQUIRE(fit.xVariable == "velocity");
        REQUIRE(fit.yVariable == "rmsDbfs");
    }
}

TEST_CASE("DynamicsMeasurementAdapter - Discontinuity observation detects jumps without assuming layers", "[measurement][dynamics][adapter]")
{
    SECTION("Smooth monotonic series reports no discontinuity")
    {
        std::vector<int> velocities = { 0, 1, 8, 16, 32, 64, 96, 127 };
        std::vector<double> values = { -96.0, -32.0, -28.0, -23.0, -17.5, -12.0, -6.0, 0.0 };

        DiscontinuityObservation disc = DynamicsMeasurementAdapter::detectDiscontinuity(velocities, values, 6.0);
        REQUIRE_FALSE(disc.detected);
        REQUIRE(disc.confidence == 0.0);
    }

    SECTION("Step jump of 8.5 dB between velocity 64 and 80 is observed")
    {
        std::vector<int> velocities = { 0, 1, 32, 64, 80, 96, 127 };
        std::vector<double> values = { -96.0, -30.0, -22.0, -18.0, -9.5, -6.0, -1.0 }; // 8.5 dB jump at 64->80

        DiscontinuityObservation disc = DynamicsMeasurementAdapter::detectDiscontinuity(velocities, values, 6.0);
        REQUIRE(disc.detected);
        REQUIRE(disc.lowerVelocity == 64);
        REQUIRE(disc.upperVelocity == 80);
        REQUIRE_THAT(disc.jumpDb, WithinAbs(8.5, 1e-2));
        REQUIRE(disc.confidence >= 0.70);
        REQUIRE(disc.reason.contains("discontinuity observed"));
        REQUIRE_FALSE(disc.reason.contains("layer switching confirmed"));
    }
}

TEST_CASE("DynamicsMeasurementAdapter - Full campaign analysis across velocity takes", "[measurement][dynamics][adapter]")
{
    double sampleRate = 48000.0;
    MeasurementSpec spec;
    spec.measurementId = "meas-dyn-campaign-001";
    spec.measurementType = "dynamics";
    spec.execution.sampleRateHz = sampleRate;
    spec.presetStateHash = "preset_sha256_full_test";

    std::vector<DynamicsMeasurementAdapter::VelocityTake> takes;
    std::vector<int> grid = { 0, 16, 32, 64, 96, 127 };

    for (int v : grid)
    {
        DynamicsMeasurementAdapter::VelocityTake take;
        take.velocity = v;
        take.sampleRateHz = sampleRate;
        take.presetStateHash = spec.presetStateHash;
        take.audioArtifactHash = "audio_hash_v_" + std::to_string(v);

        if (v == 0)
        {
            take.audio = std::vector<float>(2400, 0.0f);
        }
        else
        {
            float gain = static_cast<float>(v) / 127.0f;
            double freq = 300.0 + 5.0 * v; // Centroid scales with velocity
            take.audio = generateSynthTone(sampleRate, 0.3, freq, gain, 0.015);
        }
        take.noteOnSample = 0;
        take.noteOffSample = take.audio.size();
        takes.push_back(std::move(take));
    }

    MeasurementResult res = DynamicsMeasurementAdapter::analyzeTakes(spec, takes);

    REQUIRE(res.status == MeasurementStatus::completed);
    REQUIRE(res.measurementType == "dynamics");
    REQUIRE(res.measurementDomain == "synthesizedSpectralResponse");
    REQUIRE(res.dynamicResult.has_value());

    const auto& dyn = *res.dynamicResult;
    REQUIRE(dyn.points.size() == 6);
    REQUIRE(dyn.points[0].velocity == 0);
    REQUIRE(dyn.points[0].status == "skipped");

    REQUIRE(dyn.points[5].velocity == 127);
    REQUIRE(dyn.points[5].status == "observed");

    REQUIRE(dyn.amplitudeCurve.x.size() == 6);
    REQUIRE(dyn.amplitudeCurve.yUnit == "dBFS");

    REQUIRE(dyn.brightnessCurve.x.size() == 6);
    REQUIRE(dyn.brightnessCurve.yUnit == "Hz");

    REQUIRE(dyn.dynamicRangeDb > 15.0);
    REQUIRE(dyn.spectralMetadata.has_value());
    REQUIRE(dyn.spectralMetadata->fftSize == 2048);

    // Summary metrics check
    bool foundDr = false;
    for (const auto& m : res.metrics)
    {
        if (m.name == "dynamic_range")
        {
            foundDr = true;
            REQUIRE(m.value > 15.0);
            REQUIRE(m.unit == "dB");
        }
    }
    REQUIRE(foundDr);
}
