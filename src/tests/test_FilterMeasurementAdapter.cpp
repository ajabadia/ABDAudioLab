/**
 * @file test_FilterMeasurementAdapter.cpp
 * @brief Catch2 unit tests for FilterMeasurementAdapter and FilterAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/adapters/FilterMeasurementAdapter.h"
#include "measurement/MeasurementStimulusCoordinator.h"
#include "math/FarinaDeconvolver.h"
#include <cmath>
#include <numbers>
#include <vector>

using namespace abdaudiolab;
using namespace abdaudiolab::measurement;
using namespace abdaudiolab::math::analytics;

namespace
{

struct TestBiquad
{
    float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
    float a1 { 0.0f }, a2 { 0.0f };
    float z1 { 0.0f }, z2 { 0.0f };

    void makeLowPass(double sampleRate, double freqHz, double q)
    {
        double w0 = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosw = std::cos(w0);

        double a0 = 1.0 + alpha;
        b0 = static_cast<float>((1.0 - cosw) / (2.0 * a0));
        b1 = static_cast<float>((1.0 - cosw) / a0);
        b2 = static_cast<float>((1.0 - cosw) / (2.0 * a0));
        a1 = static_cast<float>((-2.0 * cosw) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void makeHighPass(double sampleRate, double freqHz, double q)
    {
        double w0 = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosw = std::cos(w0);

        double a0 = 1.0 + alpha;
        b0 = static_cast<float>((1.0 + cosw) / (2.0 * a0));
        b1 = static_cast<float>(-(1.0 + cosw) / a0);
        b2 = static_cast<float>((1.0 + cosw) / (2.0 * a0));
        a1 = static_cast<float>((-2.0 * cosw) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void makeBandPass(double sampleRate, double freqHz, double q)
    {
        double w0 = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosw = std::cos(w0);

        double a0 = 1.0 + alpha;
        b0 = static_cast<float>(alpha / a0);
        b1 = 0.0f;
        b2 = static_cast<float>(-alpha / a0);
        a1 = static_cast<float>((-2.0 * cosw) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
        z1 = 0.0f;
        z2 = 0.0f;
    }

    float process(float x)
    {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    std::vector<float> processBuffer(const std::vector<float>& in)
    {
        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); ++i)
            out[i] = process(in[i]);
        return out;
    }
};

} // namespace

TEST_CASE("FilterMeasurementAdapter - 2-Pole Butterworth LowPass Filter", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const float startFreq = 20.0f;
    const float endFreq = 20000.0f;

    MeasurementSpec spec;
    spec.measurementId = "meas-filter-butterworth-2pole";
    spec.measurementType = "filter";
    spec.measurementDomain = "directTransferFunction";
    spec.filterTopology = "lowPass";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.startFreqHz = startFreq;
    spec.stimulus.endFreqHz = endFreq;
    spec.stimulus.durationSec = durationSec;
    spec.stimulus.levelDbfs = -6.0f;
    spec.execution.sampleRateHz = sampleRate;

    auto sweep = MeasurementStimulusCoordinator::generateAudioStimulus(spec.stimulus, sampleRate);
    REQUIRE(sweep.size() == static_cast<size_t>(sampleRate * durationSec));

    // Filter sweep through 2-pole Butterworth LP at 1000 Hz (Q = 0.7071)
    TestBiquad filter;
    filter.makeLowPass(sampleRate, 1000.0, 0.7071);
    auto filteredSweep = filter.processBuffer(sweep);

    auto result = FilterMeasurementAdapter::measure(spec, filteredSweep, sampleRate);

    CHECK(result.status == MeasurementStatus::completed);
    CHECK(result.observability.status == "observed");
    CHECK(result.measurementType == "filter");
    CHECK(result.measurementDomain == "directTransferFunction");
    CHECK(result.filterTopology == "lowPass");

    // Cutoff frequency should be ~1000 Hz within 10% tolerance
    auto itCutoff = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "cutoffFrequency"; });
    REQUIRE(itCutoff != result.metrics.end());
    CHECK(itCutoff->status.toStdString() == "observed");
    CHECK(itCutoff->value >= 900.0);
    CHECK(itCutoff->value <= 1150.0);

    // Roll-off slope for 2-pole should be ~ -12 dB/oct (e.g. between -10 and -14 dB/oct)
    auto itSlope = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "rollOffSlope"; });
    REQUIRE(itSlope != result.metrics.end());
    CHECK(itSlope->status.toStdString() == "observed");
    CHECK(itSlope->value <= -9.5);
    CHECK(itSlope->value >= -14.5);

    // Asymptotic slope fit fixity
    REQUIRE(result.slopeFit.has_value());
    CHECK(result.slopeFit->rSquared >= 0.85);
    CHECK(result.slopeFit->selectionReason == "asymptotic_stopband");
    CHECK(result.slopeFit->frequencyStartHz >= 1200.0);

    // Butterworth has no resonance peak (gain ~ 0 dB)
    auto itResGain = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "resonanceGain"; });
    REQUIRE(itResGain != result.metrics.end());
    CHECK(itResGain->value <= 1.0);

    // Q factor should be marked not_applicable or critically damped
    auto itQ = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "qFactor"; });
    REQUIRE(itQ != result.metrics.end());
    CHECK(itQ->status.toStdString() == "not_applicable");
}

TEST_CASE("FilterMeasurementAdapter - 4-Pole Resonant LowPass Filter", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const float startFreq = 20.0f;
    const float endFreq = 20000.0f;

    MeasurementSpec spec;
    spec.measurementId = "meas-filter-resonant-4pole";
    spec.measurementType = "filter";
    spec.measurementDomain = "directTransferFunction";
    spec.filterTopology = "lowPass";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.startFreqHz = startFreq;
    spec.stimulus.endFreqHz = endFreq;
    spec.stimulus.durationSec = durationSec;
    spec.stimulus.levelDbfs = -6.0f;
    spec.execution.sampleRateHz = sampleRate;

    auto sweep = MeasurementStimulusCoordinator::generateAudioStimulus(spec.stimulus, sampleRate);

    // Cascade 2 biquads to simulate 4-pole filter at 1000 Hz with high resonance
    TestBiquad stage1, stage2;
    stage1.makeLowPass(sampleRate, 1000.0, 3.0); // Resonant stage
    stage2.makeLowPass(sampleRate, 1000.0, 0.7071);

    auto pass1 = stage1.processBuffer(sweep);
    auto pass2 = stage2.processBuffer(pass1);

    auto result = FilterMeasurementAdapter::measure(spec, pass2, sampleRate);

    CHECK(result.status == MeasurementStatus::completed);
    CHECK(result.observability.status == "observed");

    // Resonance peak should be ~1000 Hz
    auto itResFreq = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "resonanceFrequency"; });
    REQUIRE(itResFreq != result.metrics.end());
    CHECK(itResFreq->status.toStdString() == "observed");
    CHECK(itResFreq->value >= 900.0);
    CHECK(itResFreq->value <= 1100.0);

    // Resonance gain should be significant (> 3 dB)
    auto itResGain = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "resonanceGain"; });
    REQUIRE(itResGain != result.metrics.end());
    CHECK(itResGain->value >= 3.0);

    // Q factor should be observed
    auto itQ = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "qFactor"; });
    REQUIRE(itQ != result.metrics.end());
    CHECK(itQ->status.toStdString() == "observed");
    CHECK(itQ->value >= 1.0);

    // 4-pole slope should be ~ -24 dB/oct (e.g. between -20 and -28 dB/oct)
    auto itSlope = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "rollOffSlope"; });
    REQUIRE(itSlope != result.metrics.end());
    CHECK(itSlope->status.toStdString() == "observed");
    CHECK(itSlope->value <= -18.0);
    CHECK(itSlope->value >= -30.0);
}

TEST_CASE("FilterMeasurementAdapter - Flat Pass-Through does not invent false cutoff", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.5;

    MeasurementSpec spec;
    spec.measurementId = "meas-filter-passthrough";
    spec.measurementType = "filter";
    spec.measurementDomain = "directTransferFunction";
    spec.filterTopology = "lowPass";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.durationSec = durationSec;
    spec.execution.sampleRateHz = sampleRate;

    auto sweep = MeasurementStimulusCoordinator::generateAudioStimulus(spec.stimulus, sampleRate);

    // Identity pass-through
    auto result = FilterMeasurementAdapter::measure(spec, sweep, sampleRate);

    CHECK(result.status == MeasurementStatus::completed);

    // Cutoff must NOT be invented
    auto itCutoff = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "cutoffFrequency"; });
    REQUIRE(itCutoff != result.metrics.end());
    CHECK(itCutoff->status.toStdString() == "not_observable");
    CHECK(itCutoff->reason.toStdString() == "no_3db_crossing_found");

    // Slope should be ~ 0 dB/oct
    auto itSlope = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "rollOffSlope"; });
    REQUIRE(itSlope != result.metrics.end());
    CHECK(std::abs(itSlope->value) <= 1.0);
}

TEST_CASE("FilterMeasurementAdapter - HighPass and BandPass topology handling", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.5;

    MeasurementSpec spec;
    spec.measurementId = "meas-filter-bandpass";
    spec.measurementType = "filter";
    spec.measurementDomain = "directTransferFunction";
    spec.filterTopology = "bandPass";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.durationSec = durationSec;
    spec.execution.sampleRateHz = sampleRate;

    auto sweep = MeasurementStimulusCoordinator::generateAudioStimulus(spec.stimulus, sampleRate);

    TestBiquad filter;
    filter.makeBandPass(sampleRate, 2000.0, 2.0);
    auto filtered = filter.processBuffer(sweep);

    auto result = FilterMeasurementAdapter::measure(spec, filtered, sampleRate);

    CHECK(result.status == MeasurementStatus::completed);
    CHECK(result.filterTopology == "bandPass");

    // Bandpass provides lowerCutoff and upperCutoff
    auto itLower = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "lowerCutoff"; });
    auto itUpper = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "upperCutoff"; });
    auto itBw = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "bandwidth"; });

    REQUIRE(itLower != result.metrics.end());
    REQUIRE(itUpper != result.metrics.end());
    REQUIRE(itBw != result.metrics.end());

    CHECK(itLower->status.toStdString() == "observed");
    CHECK(itUpper->status.toStdString() == "observed");
    CHECK(itBw->status.toStdString() == "observed");
    CHECK(itUpper->value > itLower->value);
}

TEST_CASE("FilterMeasurementAdapter - Synthesized Spectral Response Domain honest labeling", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.5;

    MeasurementSpec spec;
    spec.measurementId = "meas-synth-spectral";
    spec.measurementType = "filter";
    spec.measurementDomain = "synthesizedSpectralResponse";
    spec.filterTopology = "lowPass";
    spec.stimulus.type = StimulusType::midiNote;
    spec.execution.sampleRateHz = sampleRate;

    StimulusSpec sweepSpec;
    sweepSpec.type = StimulusType::logSineSweep;
    sweepSpec.durationSec = durationSec;
    auto sweep = MeasurementStimulusCoordinator::generateAudioStimulus(sweepSpec, sampleRate);

    TestBiquad filter;
    filter.makeLowPass(sampleRate, 1500.0, 1.5);
    auto filtered = filter.processBuffer(sweep);

    auto result = FilterMeasurementAdapter::measure(spec, filtered, sampleRate);

    CHECK(result.status == MeasurementStatus::completed);
    CHECK(result.measurementDomain == "synthesizedSpectralResponse");

    // Must use honest metric naming: observedSpectralPeak, observedSpectralRolloff
    auto itPeak = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "observedSpectralPeak"; });
    auto itRolloff = std::find_if(result.metrics.begin(), result.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "observedSpectralRolloff"; });

    REQUIRE(itPeak != result.metrics.end());
    REQUIRE(itRolloff != result.metrics.end());
    CHECK(itPeak->status.toStdString() == "observed");
}

TEST_CASE("FilterMeasurementAdapter - Error and edge case rejection", "[filter][measurement]")
{
    const double sampleRate = 48000.0;
    MeasurementSpec spec;
    spec.measurementId = "meas-filter-edge";

    SECTION("Empty buffer returns failed")
    {
        std::vector<float> empty;
        auto result = FilterMeasurementAdapter::measure(spec, empty, sampleRate);
        CHECK(result.status == MeasurementStatus::failed);
        CHECK(result.reason == "empty_audio_buffer");
    }

    SECTION("Non-finite samples return invalid")
    {
        std::vector<float> nanAudio(1024, 0.0f);
        nanAudio[512] = std::numeric_limits<float>::quiet_NaN();
        auto result = FilterMeasurementAdapter::measure(spec, nanAudio, sampleRate);
        CHECK(result.status == MeasurementStatus::invalid);
        CHECK(result.reason == "non_finite_audio_samples");
    }

    SECTION("Silent audio returns unreliable")
    {
        std::vector<float> silent(1024, 0.0f);
        auto result = FilterMeasurementAdapter::measure(spec, silent, sampleRate);
        CHECK(result.status == MeasurementStatus::unreliable);
        CHECK(result.reason == "silent_or_flat_signal");
    }
}
