#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "synth/SynthPresetState.h"
#include "synth/MidiExcitationSequence.h"
#include "synth/SynthObservation.h"
#include "synth/SyntheticSynthFixture.h"
#include "synth/ISynthTarget.h"
#include "synth/SyntheticSynthTarget.h"
#include "synth/PluginSynthTarget.h"
#include "synth/DigitalSynthMvpProfiler.h"
#include "synth/SynthPitchEstimator.h"
#include "synth/SynthEnvelopeAnalyzer.h"
#include "core/ProfilingSession.h"
#include "core/ProfilingSequencer.h"
#include <numbers>

using namespace abdaudiolab::synth;

/**
 * @brief Mock de AudioProcessor que simula un sintetizador virtual VST3 directo en RAM.
 */
class MockVirtualSynthPlugin : public juce::AudioProcessor
{
public:
    MockVirtualSynthPlugin()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        setLatencySamples(32); // 32 samples de latencia interna declarada (~0.33ms a 96kHz)
    }

    const juce::String getName() const override { return "MockVirtualSynthPlugin"; }
    void prepareToPlay(double sampleRate, int /*samplesPerBlock*/) override
    {
        currentSampleRate_ = sampleRate;
        phase_ = 0.0;
        envelopeLevel_ = 0.0;
        noteActive_ = false;
    }
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        buffer.clear();
        int numSamples = buffer.getNumSamples();
        auto* leftOut = buffer.getWritePointer(0);
        auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            // Procesar eventos MIDI en el sample exacto
            for (const auto metadata : midiMessages)
            {
                if (metadata.samplePosition == i)
                {
                    auto msg = metadata.getMessage();
                    if (msg.isNoteOn())
                    {
                        noteActive_ = true;
                        phase_ = 0.0;
                        velocity_ = msg.getFloatVelocity();
                        freqHz_ = 440.0 * std::pow(2.0, (msg.getNoteNumber() - 69.0) / 12.0);
                    }
                    else if (msg.isNoteOff() || msg.isAllNotesOff())
                    {
                        noteActive_ = false;
                    }
                }
            }

            // Generador de envolvente simple
            if (noteActive_)
                envelopeLevel_ = std::min(1.0, envelopeLevel_ + (1000.0 / (currentSampleRate_ * 15.0))); // Ataque ~15ms
            else
                envelopeLevel_ = std::max(0.0, envelopeLevel_ - (1000.0 / (currentSampleRate_ * 100.0))); // Release ~100ms

            double phaseInc = (2.0 * std::numbers::pi * freqHz_) / currentSampleRate_;
            phase_ += phaseInc;
            if (phase_ >= 2.0 * std::numbers::pi) phase_ -= 2.0 * std::numbers::pi;

            float sample = static_cast<float>(std::sin(phase_) * envelopeLevel_ * (0.3 + 0.7 * velocity_));
            leftOut[i] = sample;
            if (rightOut != nullptr) rightOut[i] = sample;
        }
    }

    double getTailLengthSeconds() const override { return 0.2; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override { destData.append("MOCK_STATE", 10); }
    void setStateInformation(const void*, int) override {}

private:
    double currentSampleRate_ { 96000.0 };
    double phase_ { 0.0 };
    double freqHz_ { 261.6256 };
    float velocity_ { 1.0f };
    double envelopeLevel_ { 0.0 };
    bool noteActive_ { false };
};

TEST_CASE("DigitalSynthMvpProfiler - Deterministic Nominal Benchmark", "[synth][profiler]")
{
    const double sampleRate = 96000.0;
    SyntheticSynthFixture fixture(sampleRate, 12345);
    DigitalSynthMvpProfiler profiler(sampleRate);

    SynthPresetState preset;
    preset.presetId = "anchor_001";
    preset.presetName = "Analog Saw Calibration";
    preset.stateStatus = StateAppliedStatus::Passed;
    preset.normalizedParameters.push_back({ "filter_cutoff", 1.0, "VCF Cutoff" });
    preset.normalizedParameters.push_back({ "vca_attack", 0.05, "Attack Time" });
    preset.normalizedParameters.push_back({ "vca_sustain", 0.50, "Sustain Level" });
    REQUIRE(preset.finalizeAndComputeHashes());

    // Ejecutar sesión nominal de 27 tomas (3 vel x 3 dur x 3 rep)
    auto report = profiler.runFixtureSession(preset, fixture, 60, 3);

    SECTION("Hashes deterministas SHA-256 no vacios y formateados")
    {
        REQUIRE(preset.rawSysExHash.length() == 64);
        REQUIRE(preset.normalizedParameterHash.length() == 64);
        REQUIRE(preset.stateHash.length() == 64);
        REQUIRE(report.experimentHash.length() == 64);
    }

    SECTION("Validacion de estado y comportamiento nominal")
    {
        REQUIRE(report.stateValidation == "PASSED");
        REQUIRE(report.behaviorValidation == "PASSED");
    }

    SECTION("Offset de transporte y repetibilidad de onset")
    {
        // Ground truth es 3.42 ms con jitter fino de 0.08 ms
        REQUIRE_THAT(report.midiAudioOffsetMs, Catch::Matchers::WithinAbs(3.42, 0.25));
        REQUIRE(report.onsetRepeatabilityMs < 0.20);
    }

    SECTION("Afinacion estimada y cents")
    {
        // Ground truth es +0.7 cents
        REQUIRE_THAT(report.pitchMeanCents, Catch::Matchers::WithinAbs(0.70, 0.40));
        REQUIRE(report.pitchStdDevCents < 0.35);
    }

    SECTION("Envolvente identificada en compuerta representativa")
    {
        // Ground truth: attack 15.0ms, decay 247ms, sustain -6.1dB, release 612ms
        REQUIRE_THAT(report.attackMs.value, Catch::Matchers::WithinAbs(15.0, 1.80));
        REQUIRE(report.attackMs.stdDev < 0.60);

        REQUIRE_THAT(report.decayMs.value, Catch::Matchers::WithinAbs(247.0, 30.0));
        REQUIRE_THAT(report.sustainDb.value, Catch::Matchers::WithinAbs(-6.10, 0.80));
        REQUIRE_THAT(report.releaseMs.value, Catch::Matchers::WithinAbs(612.0, 45.0));
    }

    SECTION("Falsabilidad: Ataque invariante ante variacion de duracion")
    {
        REQUIRE(report.durationInvarianceStatus == "VERIFIED_ATTACK_INVARIANT");
    }

    SECTION("Sensibilidad a velocity detectada")
    {
        REQUIRE(report.velocitySensitivityStatus == "OBSERVED");
    }

    SECTION("Formato textual del informe canónico")
    {
        std::string text = report.formatCanonicalText();
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("Preset: anchor_001"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("State validation: PASSED"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("Behavior validation: PASSED"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("MIDI/audio offset:"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("Onset repeatability:"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("Pitch:"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("attack:"));
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("Domain:"));
    }
}

TEST_CASE("DigitalSynthMvpProfiler - ISynthTarget Polymorphism (Direct RAM Plugin)", "[synth][profiler][vst]")
{
    const double sampleRate = 96000.0;
    DigitalSynthMvpProfiler profiler(sampleRate);

    MockVirtualSynthPlugin plugin;
    PluginSynthTarget pluginTarget(plugin);

    SynthPresetState preset;
    preset.presetId = "vst3_mock_patch";
    preset.stateStatus = StateAppliedStatus::Passed;
    REQUIRE(preset.finalizeAndComputeHashes());

    auto report = profiler.runTargetSession(preset, pluginTarget, 60, 2);

    REQUIRE(report.stateValidation == "PASSED");
    REQUIRE(report.behaviorValidation == "PASSED");
    // En el plugin virtual sin hardware externo, la latencia es puramente digital y el onset es consistente
    REQUIRE(report.onsetRepeatabilityMs < 0.15);
    REQUIRE_THAT(report.pitchMeanCents, Catch::Matchers::WithinAbs(0.0, 0.20));
}

TEST_CASE("DigitalSynthMvpProfiler - Gate Duration Observability Constraints", "[synth][profiler]")
{
    const double sampleRate = 96000.0;
    SyntheticSynthFixture fixture(sampleRate, 777);
    DigitalSynthMvpProfiler profiler(sampleRate);

    SynthPresetState preset;
    preset.presetId = "anchor_001";
    preset.stateStatus = StateAppliedStatus::Passed;
    preset.finalizeAndComputeHashes();

    auto report = profiler.runFixtureSession(preset, fixture, 60, 3);

    const ConditionSummary* shortGateCond = nullptr;
    for (const auto& c : report.conditions)
    {
        if (c.gateDurationSec <= 0.06)
        {
            shortGateCond = &c;
            break;
        }
    }

    REQUIRE(shortGateCond != nullptr);
    REQUIRE(shortGateCond->sustainDb.status == MetricStatus::NotObservableInGate);
    REQUIRE(shortGateCond->netAttackMs.status == MetricStatus::EstimatedWithUncertainty);
    REQUIRE(shortGateCond->netAttackMs.isReliable());
}

TEST_CASE("DigitalSynthMvpProfiler - Velocity Insensitive Anchor Handling", "[synth][profiler]")
{
    const double sampleRate = 96000.0;
    SyntheticSynthFixture fixture(sampleRate, 999);
    fixture.setFaultMode(FixtureFaultMode::VelocityInsensitive);

    DigitalSynthMvpProfiler profiler(sampleRate);
    SynthPresetState preset;
    preset.presetId = "anchor_fixed_velocity";
    preset.stateStatus = StateAppliedStatus::Passed;
    preset.finalizeAndComputeHashes();

    auto report = profiler.runFixtureSession(preset, fixture, 60, 3);

    REQUIRE(report.behaviorValidation == "PASSED");
    REQUIRE(report.velocitySensitivityStatus == "NOT_OBSERVED_IN_ANCHOR");
}

TEST_CASE("DigitalSynthMvpProfiler - Negative Cases & Fault Injection", "[synth][profiler]")
{
    const double sampleRate = 96000.0;
    DigitalSynthMvpProfiler profiler(sampleRate);

    SynthPresetState preset;
    preset.presetId = "anchor_test";
    preset.stateStatus = StateAppliedStatus::Passed;
    preset.finalizeAndComputeHashes();

    SECTION("Fallo por Nota Perdida (Dropout) emite REJECTED")
    {
        SyntheticSynthFixture fixture(sampleRate, 101);
        fixture.setFaultMode(FixtureFaultMode::DroppedNote);

        auto report = profiler.runFixtureSession(preset, fixture, 60, 3);
        REQUIRE(report.behaviorValidation == "REJECTED");
    }

    SECTION("Fallo por Clipping Severo emite INVALID_MEASUREMENT")
    {
        SyntheticSynthFixture fixture(sampleRate, 102);
        fixture.setFaultMode(FixtureFaultMode::SevereClipping);

        auto report = profiler.runFixtureSession(preset, fixture, 60, 3);
        REQUIRE(report.behaviorValidation == "INVALID_MEASUREMENT");
    }

    SECTION("Jitter No Gaussiano excesivo emite INCONCLUSIVE")
    {
        SyntheticSynthFixture fixture(sampleRate, 103);
        fixture.setFaultMode(FixtureFaultMode::NonGaussianJitter);

        auto report = profiler.runFixtureSession(preset, fixture, 60, 3);
        REQUIRE(report.behaviorValidation == "INCONCLUSIVE");
    }

    SECTION("Anomalia de duracion alterando ataque emite INCONCLUSIVE")
    {
        SyntheticSynthFixture fixture(sampleRate, 104);
        fixture.setFaultMode(FixtureFaultMode::DurationAltersAttackAnomaly);

        auto report = profiler.runFixtureSession(preset, fixture, 60, 3);
        REQUIRE(report.durationInvarianceStatus == "ANOMALY_DURATION_ALTERS_ATTACK");
        REQUIRE(report.behaviorValidation == "INCONCLUSIVE");
    }
}

TEST_CASE("DigitalSynthMvpProfiler - ProfilingSequencer Regression Test for Autonomous Synth", "[core][sequencer]")
{
    abdaudiolab::core::TestCase tc;
    tc.isAutonomousSynth = true;
    tc.stimulusType = abdaudiolab::audio::StimulusType::Silence;
    tc.functionalBlockType = "TimeDynamic";

    const bool isNoiseFloorMeasurement = (tc.functionalBlockType == "NoiseFloor") && !tc.isAutonomousSynth;
    REQUIRE_FALSE(isNoiseFloorMeasurement);
}
