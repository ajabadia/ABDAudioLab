/**
 * @file test_EnvelopeMeasurementAdapter.cpp
 * @brief Catch2 unit tests for EnvelopeMeasurementAdapter.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/adapters/EnvelopeMeasurementAdapter.h"
#include "measurement/MeasurementSerialization.h"
#include <cmath>
#include <numbers>
#include <limits>

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

/**
 * @brief Helper creating a controlled ADSR audio buffer with a 440 Hz carrier tone.
 */
static std::vector<float> generateSyntheticAdsr(double sampleRate,
                                                double attackSec,
                                                double decaySec,
                                                float sustainLevelLinear,
                                                double sustainSec,
                                                double releaseSec,
                                                size_t& outNoteOn,
                                                size_t& outNoteOff)
{
    outNoteOn = static_cast<size_t>(sampleRate * 0.02); // 20ms pre-roll silence
    size_t attackSamples = static_cast<size_t>(sampleRate * attackSec);
    size_t decaySamples = static_cast<size_t>(sampleRate * decaySec);
    size_t sustainSamples = static_cast<size_t>(sampleRate * sustainSec);
    size_t releaseSamples = static_cast<size_t>(sampleRate * releaseSec);

    outNoteOff = outNoteOn + attackSamples + decaySamples + sustainSamples;
    size_t totalSamples = outNoteOff + releaseSamples + static_cast<size_t>(sampleRate * 0.05);

    std::vector<float> buffer(totalSamples, 0.0f);
    double phase = 0.0;
    double phaseInc = 2.0 * std::numbers::pi * 440.0 / sampleRate;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        float carrier = static_cast<float>(std::sin(phase));
        phase += phaseInc;

        float env = 0.0f;
        if (i < outNoteOn)
        {
            env = 0.0f;
        }
        else if (i < outNoteOn + attackSamples)
        {
            float prog = static_cast<float>(i - outNoteOn) / static_cast<float>(std::max<size_t>(1, attackSamples));
            env = prog;
        }
        else if (i < outNoteOn + attackSamples + decaySamples)
        {
            float prog = static_cast<float>(i - (outNoteOn + attackSamples)) / static_cast<float>(std::max<size_t>(1, decaySamples));
            env = 1.0f - prog * (1.0f - sustainLevelLinear);
        }
        else if (i < outNoteOff)
        {
            env = sustainLevelLinear;
        }
        else if (i < outNoteOff + releaseSamples)
        {
            float prog = static_cast<float>(i - outNoteOff) / static_cast<float>(std::max<size_t>(1, releaseSamples));
            env = sustainLevelLinear * (1.0f - prog);
        }
        else
        {
            env = 0.0f;
        }

        buffer[i] = carrier * env;
    }

    return buffer;
}

TEST_CASE("EnvelopeMeasurementAdapter - Normal observable signal", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    size_t noteOn = 0, noteOff = 0;
    auto audio = generateSyntheticAdsr(sampleRate, 0.05, 0.10, 0.5f, 0.30, 0.15, noteOn, noteOff);

    MeasurementSpec spec;
    spec.measurementId = "test-env-normal";
    spec.measurementType = "envelope";
    spec.dutType = DeviceUnderTest::instrument;
    spec.parameterName = "Dexed";

    auto result = EnvelopeMeasurementAdapter::measure(spec, audio, sampleRate, noteOn, noteOff);

    REQUIRE(result.status == MeasurementStatus::completed);
    REQUIRE(result.observability.status == "observed");
    REQUIRE_FALSE(result.observability.reason.has_value());
    REQUIRE(result.analyzer.name == "SynthEnvelopeAnalyzer");

    // Metrics verification
    REQUIRE(result.metrics.size() == 5);

    auto findMetric = [&](const juce::String& name) -> const MeasurementMetric* {
        for (const auto& m : result.metrics)
            if (m.name == name) return &m;
        return nullptr;
    };

    auto* mAttack = findMetric("attackTime");
    REQUIRE(mAttack != nullptr);
    REQUIRE(mAttack->unit == "ms");
    REQUIRE(mAttack->status == "observed");
    REQUIRE(mAttack->value >= 40.0);
    REQUIRE(mAttack->value <= 60.0);

    auto* mDecay = findMetric("decayTime");
    REQUIRE(mDecay != nullptr);
    REQUIRE(mDecay->unit == "ms");
    REQUIRE(mDecay->status == "observed");
    REQUIRE(mDecay->value > 0.0);

    auto* mSustain = findMetric("sustainLevel");
    REQUIRE(mSustain != nullptr);
    REQUIRE(mSustain->unit == "dBFS");
    REQUIRE(mSustain->status == "observed");
    // 0.5 linear ~ -6.02 dBFS
    REQUIRE_THAT(mSustain->value, WithinAbs(-6.02, 1.5));

    auto* mRelease = findMetric("releaseTime");
    REQUIRE(mRelease != nullptr);
    REQUIRE(mRelease->unit == "ms");
    REQUIRE(mRelease->status == "observed");
    REQUIRE(mRelease->value > 50.0);

    // Curve verification
    REQUIRE_FALSE(result.curve.x.empty());
    REQUIRE(result.curve.x.size() == result.curve.y.size());
    REQUIRE(result.curve.xName == "time");
    REQUIRE(result.curve.xUnit == "ms");
    REQUIRE(result.curve.yName == "amplitude");
    REQUIRE(result.curve.yUnit == "dBFS");

    // Full round-trip validation
    std::string jsonStr = MeasurementSerialization::serializeResult(result);
    REQUIRE_FALSE(jsonStr.empty());

    MeasurementResult deserialized;
    std::string err;
    bool ok = MeasurementSerialization::deserializeResult(jsonStr, deserialized, err);
    REQUIRE(ok);
    REQUIRE(err.empty());
    REQUIRE(deserialized.status == MeasurementStatus::completed);
}

TEST_CASE("EnvelopeMeasurementAdapter - Note too short to measure decay", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    size_t noteOn = 0, noteOff = 0;
    // NoteOff cut immediately after 50ms attack peak: only 5ms post peak margin
    auto audio = generateSyntheticAdsr(sampleRate, 0.05, 0.005, 0.5f, 0.0, 0.10, noteOn, noteOff);

    MeasurementSpec spec;
    spec.measurementId = "test-env-short-gate";
    spec.measurementType = "envelope";

    auto result = EnvelopeMeasurementAdapter::measure(spec, audio, sampleRate, noteOn, noteOff);

    REQUIRE(result.status == MeasurementStatus::completed);
    REQUIRE(result.observability.status == "unreliable");
    REQUIRE(result.reason == "decay_not_observable_gate_too_short");
    REQUIRE(result.observability.reason.has_value());

    for (const auto& m : result.metrics)
    {
        if (m.name == "decayTime")
        {
            REQUIRE(m.status == "unreliable");
        }
    }
}

TEST_CASE("EnvelopeMeasurementAdapter - Silent or flat signal", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    std::vector<float> silent(4800, 0.0f);

    MeasurementSpec spec;
    spec.measurementId = "test-env-silent";

    auto result = EnvelopeMeasurementAdapter::measure(spec, silent, sampleRate, 100, 2000);

    REQUIRE(result.status == MeasurementStatus::unreliable);
    REQUIRE(result.reason == "silent_or_flat_signal");
    REQUIRE(result.observability.status == "unreliable");
    REQUIRE(result.observability.reason == "Signal peak is below noise floor (-100 dBFS)");
    REQUIRE(result.curve.x.empty()); // empty curve allowed for unreliable
}

TEST_CASE("EnvelopeMeasurementAdapter - Rejection of NaN or Infinite samples", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    std::vector<float> badAudio(1000, 0.1f);
    badAudio[500] = std::numeric_limits<float>::quiet_NaN();

    MeasurementSpec spec;
    spec.measurementId = "test-env-nan";

    auto result = EnvelopeMeasurementAdapter::measure(spec, badAudio, sampleRate, 100, 800);

    REQUIRE(result.status == MeasurementStatus::invalid);
    REQUIRE(result.reason == "non_finite_audio_samples");
    REQUIRE(result.observability.status == "invalid");
}

TEST_CASE("EnvelopeMeasurementAdapter - Empty buffer guard", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    std::vector<float> emptyBuf;

    MeasurementSpec spec;
    spec.measurementId = "test-env-empty";

    auto result = EnvelopeMeasurementAdapter::measure(spec, emptyBuf, sampleRate, 0, 100);

    REQUIRE(result.status == MeasurementStatus::failed);
    REQUIRE(result.reason == "empty_audio_buffer");
}

TEST_CASE("EnvelopeMeasurementAdapter - Absence of onset within gate", "[measurement][envelope]")
{
    const double sampleRate = 48000.0;
    // Audio has silence during the gate [100, 500], then sound appears much later at sample 1000
    std::vector<float> delayedAudio(2000, 0.0f);
    for (size_t i = 1000; i < 1500; ++i)
        delayedAudio[i] = 0.5f;

    MeasurementSpec spec;
    spec.measurementId = "test-env-late-onset";

    auto result = EnvelopeMeasurementAdapter::measure(spec, delayedAudio, sampleRate, 100, 500);

    REQUIRE(result.status == MeasurementStatus::unreliable);
    REQUIRE(result.reason == "onset_not_detected");
    REQUIRE(result.observability.status == "unreliable");
}
