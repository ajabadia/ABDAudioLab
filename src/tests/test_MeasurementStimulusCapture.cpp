/**
 * @file test_MeasurementStimulusCapture.cpp
 * @brief Catch2 unit tests for deterministic stimulus generation and synchronous capture coordinator.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/MeasurementStimulusCoordinator.h"
#include "measurement/MeasurementCaptureCoordinator.h"
#include "synth/ISynthTarget.h"
#include <cmath>
#include <limits>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::synth;
using Catch::Matchers::WithinAbs;

/**
 * @class MockCaptureSynthTarget
 * @brief Mock ISynthTarget implementation for testing deterministic capture cycles.
 */
class MockCaptureSynthTarget : public ISynthTarget
{
public:
    enum class OutputMode
    {
        NormalSine,
        Silence,
        NonFiniteNaN,
        EmptyBuffer,
        FailStateLoad
    };

    OutputMode mode { OutputMode::NormalSine };
    bool wasPrepared { false };
    ProcessingSpec lastPreparedSpec;
    SynthPresetState lastLoadedState;

    bool loadState(const SynthPresetState& state) override
    {
        if (mode == OutputMode::FailStateLoad)
            return false;
        lastLoadedState = state;
        return true;
    }

    StateAppliedStatus verifyState() const override
    {
        return StateAppliedStatus::Passed;
    }

    void prepare(const ProcessingSpec& spec) override
    {
        wasPrepared = true;
        lastPreparedSpec = spec;
    }

    void resetState() override {}

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int repetitionIndex = 0) override
    {
        juce::ignoreUnused(repetitionIndex);

        if (mode == OutputMode::EmptyBuffer)
        {
            destinationAudio.clear();
            return;
        }

        if (mode == OutputMode::NonFiniteNaN)
        {
            for (size_t i = 0; i < destinationAudio.size(); ++i)
                destinationAudio[i] = 0.5f;
            if (destinationAudio.size() > 10)
                destinationAudio[10] = std::numeric_limits<float>::quiet_NaN();
            return;
        }

        if (mode == OutputMode::Silence)
        {
            std::fill(destinationAudio.begin(), destinationAudio.end(), 0.0f);
            return;
        }

        // OutputMode::NormalSine: render pure sine tone during note gate
        int noteOnSample = 0;
        int noteOffSample = static_cast<int>(destinationAudio.size());
        for (const auto& ev : sequence.events)
        {
            if (ev.type == TimedMidiType::NoteOn)
                noteOnSample = ev.sampleOffset;
            else if (ev.type == TimedMidiType::NoteOff)
                noteOffSample = ev.sampleOffset;
        }

        double phase = 0.0;
        double phaseInc = 2.0 * 3.141592653589793 * 440.0 / lastPreparedSpec.sampleRate;

        for (size_t i = 0; i < destinationAudio.size(); ++i)
        {
            if (static_cast<int>(i) >= noteOnSample && static_cast<int>(i) < noteOffSample)
            {
                destinationAudio[i] = static_cast<float>(std::sin(phase)) * 0.5f;
                phase += phaseInc;
            }
            else
            {
                destinationAudio[i] = 0.0f;
            }
        }
    }

    TargetTimingInfo timingInfo() const override
    {
        TargetTimingInfo t;
        t.declaredLatencySamples = 0.0;
        return t;
    }

    bool supportsBinaryState() const override { return false; }
    StateTransferResult getState(std::vector<uint8_t>& stateData) const override { juce::ignoreUnused(stateData); return StateTransferResult{ false, false, "NOT_SUPPORTED", 0, "" }; }
    StateTransferResult setState(const std::vector<uint8_t>& stateData) override { juce::ignoreUnused(stateData); return StateTransferResult{ false, false, "NOT_SUPPORTED", 0, "" }; }
};

TEST_CASE("MeasurementStimulusCoordinator - Audio stimulus determinism", "[measurement][stimulus]")
{
    const double sampleRate = 48000.0;

    SECTION("White noise with identical seed produces bit-for-bit identical samples")
    {
        StimulusSpec s1;
        s1.type = StimulusType::whiteNoise;
        s1.durationSec = 0.5;
        s1.seed = 0xABCDEF01u;

        StimulusSpec s2 = s1;

        auto audio1 = MeasurementStimulusCoordinator::generateAudioStimulus(s1, sampleRate);
        auto audio2 = MeasurementStimulusCoordinator::generateAudioStimulus(s2, sampleRate);

        REQUIRE(audio1.size() == 24000);
        REQUIRE(audio1 == audio2);

        auto hash1 = MeasurementStimulusCoordinator::computeAudioHash(audio1);
        auto hash2 = MeasurementStimulusCoordinator::computeAudioHash(audio2);
        REQUIRE(hash1 == hash2);
        REQUIRE_FALSE(hash1.empty());
    }

    SECTION("Different seeds produce distinct white noise buffers")
    {
        StimulusSpec s1;
        s1.type = StimulusType::whiteNoise;
        s1.durationSec = 0.1;
        s1.seed = 0x11111111u;

        StimulusSpec s2 = s1;
        s2.seed = 0x22222222u;

        auto audio1 = MeasurementStimulusCoordinator::generateAudioStimulus(s1, sampleRate);
        auto audio2 = MeasurementStimulusCoordinator::generateAudioStimulus(s2, sampleRate);

        REQUIRE(audio1 != audio2);
        REQUIRE(MeasurementStimulusCoordinator::computeAudioHash(audio1) !=
                MeasurementStimulusCoordinator::computeAudioHash(audio2));
    }

    SECTION("Log sine sweep is deterministic and non-empty")
    {
        StimulusSpec sweepSpec;
        sweepSpec.type = StimulusType::logSineSweep;
        sweepSpec.durationSec = 0.2;
        sweepSpec.startFreqHz = 20.0f;
        sweepSpec.endFreqHz = 20000.0f;

        auto sweep1 = MeasurementStimulusCoordinator::generateAudioStimulus(sweepSpec, sampleRate);
        auto sweep2 = MeasurementStimulusCoordinator::generateAudioStimulus(sweepSpec, sampleRate);

        REQUIRE(sweep1.size() == sweep2.size());
        REQUIRE(sweep1 == sweep2);
        REQUIRE_FALSE(sweep1.empty());
    }
}

TEST_CASE("MeasurementStimulusCoordinator - MIDI stimulus determinism", "[measurement][stimulus][midi]")
{
    const double sampleRate = 48000.0;

    StimulusSpec spec;
    spec.type = StimulusType::midiNote;
    spec.midiChannel = 2;
    spec.midiNoteNumber = 64; // E4
    spec.midiVelocity = 0.85f;
    spec.noteOnSample = 100;
    spec.noteOffSample = 24100; // 500ms gate

    auto seq = MeasurementStimulusCoordinator::generateMidiStimulus(spec, sampleRate, 0.2);

    REQUIRE(seq.channel == 2);
    REQUIRE(seq.noteNumber == 64);
    REQUIRE_THAT(static_cast<double>(seq.normalizedVelocity), WithinAbs(0.85, 1e-4));
    REQUIRE_FALSE(seq.sequenceHash.empty());

    // Verify exact events
    REQUIRE(seq.events.size() == 3);
    REQUIRE(seq.events[0].type == TimedMidiType::AllNotesOff);

    REQUIRE(seq.events[1].type == TimedMidiType::NoteOn);
    REQUIRE(seq.events[1].sampleOffset == 100);
    REQUIRE(seq.events[1].noteNumber == 64);

    REQUIRE(seq.events[2].type == TimedMidiType::NoteOff);
    REQUIRE(seq.events[2].sampleOffset == 24100);
    REQUIRE(seq.events[2].noteNumber == 64);

    // Verify hash stability across multiple calls
    auto seq2 = MeasurementStimulusCoordinator::generateMidiStimulus(spec, sampleRate, 0.2);
    REQUIRE(seq.sequenceHash == seq2.sequenceHash);
}

TEST_CASE("MeasurementCaptureCoordinator - Synchronous capture execution and failure isolation", "[measurement][capture]")
{
    MeasurementSpec spec;
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.execution.numChannels = 2;
    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.noteOnSample = 0;
    spec.stimulus.noteOffSample = 4800; // 100ms note

    SECTION("Normal successful capture completes with reproducible audio and metadata")
    {
        MockCaptureSynthTarget target;
        target.mode = MockCaptureSynthTarget::OutputMode::NormalSine;

        SynthPresetState state;
        state.presetName = "InitDexed";
        state.normalizedParameters.push_back({ "ALGORITHM", 0.5, "Algorithm" });
        state.finalizeAndComputeHashes();

        auto result = MeasurementCaptureCoordinator::captureSynchronous(&target, spec, &state);

        REQUIRE(result.status == MeasurementStatus::completed);
        REQUIRE(result.reason == "capture_completed");
        REQUIRE_FALSE(result.capturedAudio.empty());
        REQUIRE(result.sampleRateHz == 48000.0);
        REQUIRE(result.latencySamples == 0);
        REQUIRE_FALSE(result.stimulusSha256.empty());
        REQUIRE_FALSE(result.presetStateSha256.empty());
        REQUIRE(result.presetStateSha256 != "PRESET_DEFAULT_UNSPECIFIED");
    }

    SECTION("Null target returns failed status without hanging")
    {
        auto result = MeasurementCaptureCoordinator::captureSynchronous(nullptr, spec);
        REQUIRE(result.status == MeasurementStatus::failed);
        REQUIRE(result.reason == "target_null");
        REQUIRE(result.capturedAudio.empty());
    }

    SECTION("State load failure returns failed status")
    {
        MockCaptureSynthTarget target;
        target.mode = MockCaptureSynthTarget::OutputMode::FailStateLoad;

        SynthPresetState state;
        state.presetName = "BadPreset";

        auto result = MeasurementCaptureCoordinator::captureSynchronous(&target, spec, &state);
        REQUIRE(result.status == MeasurementStatus::failed);
        REQUIRE(result.reason == "state_load_failed");
        REQUIRE(result.capturedAudio.empty());
    }

    SECTION("Incomplete capture returns failed status")
    {
        MockCaptureSynthTarget target;
        target.mode = MockCaptureSynthTarget::OutputMode::EmptyBuffer;

        auto result = MeasurementCaptureCoordinator::captureSynchronous(&target, spec);
        REQUIRE(result.status == MeasurementStatus::failed);
        REQUIRE(result.reason == "capture_incomplete");
    }

    SECTION("Non-finite samples (NaN) return invalid status and clear audio (zero leakage)")
    {
        MockCaptureSynthTarget target;
        target.mode = MockCaptureSynthTarget::OutputMode::NonFiniteNaN;

        auto result = MeasurementCaptureCoordinator::captureSynchronous(&target, spec);
        REQUIRE(result.status == MeasurementStatus::invalid);
        REQUIRE(result.reason == "non_finite_audio_samples");
        REQUIRE(result.capturedAudio.empty()); // Zero leakage
    }

    SECTION("Silent capture returns unreliable status")
    {
        MockCaptureSynthTarget target;
        target.mode = MockCaptureSynthTarget::OutputMode::Silence;

        auto result = MeasurementCaptureCoordinator::captureSynchronous(&target, spec);
        REQUIRE(result.status == MeasurementStatus::unreliable);
        REQUIRE(result.reason == "silent_or_flat_signal");
        REQUIRE_FALSE(result.capturedAudio.empty());
    }
}
