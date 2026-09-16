/**
 * @file MeasurementStimulusCoordinator.cpp
 * @brief Implementation of MeasurementStimulusCoordinator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementStimulusCoordinator.h"
#include "../math/FarinaDeconvolver.h"
#include "../synth/Sha256.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::measurement
{

std::vector<float> MeasurementStimulusCoordinator::generateAudioStimulus(const StimulusSpec& spec, double sampleRate)
{
    if (sampleRate <= 0.0 || spec.durationSec <= 0.0)
        return {};

    size_t totalSamples = static_cast<size_t>(std::lround(sampleRate * spec.durationSec));
    float gainLinear = std::pow(10.0f, spec.levelDbfs / 20.0f);

    switch (spec.type)
    {
        case StimulusType::logSineSweep:
        {
            auto sweep = math::FarinaDeconvolver::generateLogFarinaSweep(
                sampleRate, spec.durationSec, spec.startFreqHz, spec.endFreqHz);
            for (float& s : sweep)
                s *= gainLinear;
            return sweep;
        }

        case StimulusType::sineTone:
        {
            std::vector<float> buffer(totalSamples);
            double phase = static_cast<double>(spec.phaseRad);
            double phaseInc = 2.0 * std::numbers::pi * static_cast<double>(spec.startFreqHz) / sampleRate;

            for (size_t i = 0; i < totalSamples; ++i)
            {
                buffer[i] = static_cast<float>(std::sin(phase)) * gainLinear;
                phase += phaseInc;
                if (phase >= 2.0 * std::numbers::pi)
                    phase -= 2.0 * std::numbers::pi;
            }
            return buffer;
        }

        case StimulusType::impulse:
        {
            std::vector<float> buffer(totalSamples, 0.0f);
            if (!buffer.empty())
                buffer[0] = gainLinear;
            return buffer;
        }

        case StimulusType::whiteNoise:
        {
            std::vector<float> buffer(totalSamples);
            uint32_t state = spec.seed;

            for (size_t i = 0; i < totalSamples; ++i)
            {
                state = state * 1664525u + 1013904223u;
                float white = (static_cast<float>(state) / 2147483648.0f) - 1.0f;
                buffer[i] = white * gainLinear;
            }
            return buffer;
        }

        case StimulusType::pinkNoise:
        {
            std::vector<float> buffer(totalSamples);
            uint32_t state = spec.seed;
            float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

            for (size_t i = 0; i < totalSamples; ++i)
            {
                state = state * 1664525u + 1013904223u;
                float white = (static_cast<float>(state) / 2147483648.0f) - 1.0f;

                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                float pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
                b6 = white * 0.115926f;

                buffer[i] = pink * gainLinear;
            }
            return buffer;
        }

        case StimulusType::amplitudeRamp:
        {
            std::vector<float> buffer(totalSamples);
            float invTotal = (totalSamples > 1) ? (1.0f / static_cast<float>(totalSamples - 1)) : 1.0f;

            for (size_t i = 0; i < totalSamples; ++i)
            {
                float prog = static_cast<float>(i) * invTotal;
                buffer[i] = prog * gainLinear;
            }
            return buffer;
        }

        default:
            return std::vector<float>(totalSamples, 0.0f);
    }
}

synth::MidiExcitationSequence MeasurementStimulusCoordinator::generateMidiStimulus(const StimulusSpec& spec,
                                                                                  double sampleRate,
                                                                                  double releaseTailDurationSec)
{
    synth::MidiExcitationSequence seq;
    seq.channel = spec.midiChannel;
    seq.noteNumber = spec.midiNoteNumber;
    seq.midiVelocity = juce::jlimit(1, 127, static_cast<int>(std::lround(spec.midiVelocity * 127.0f)));
    seq.normalizedVelocity = spec.midiVelocity;

    size_t noteOnSample = spec.noteOnSample;
    size_t noteOffSample = spec.noteOffSample;
    if (noteOffSample <= noteOnSample)
        noteOffSample = noteOnSample + static_cast<size_t>(sampleRate * spec.durationSec);

    size_t tailSamples = static_cast<size_t>(std::lround(sampleRate * releaseTailDurationSec));
    size_t totalSamples = noteOffSample + tailSamples;

    seq.gateDurationSec = static_cast<double>(noteOffSample - noteOnSample) / sampleRate;
    seq.preSilenceSec = static_cast<double>(noteOnSample) / sampleRate;
    seq.postSilenceSec = releaseTailDurationSec;
    seq.totalDurationSec = static_cast<double>(totalSamples) / sampleRate;

    seq.sequenceId = "STIM_N" + std::to_string(spec.midiNoteNumber)
                   + "_ON" + std::to_string(noteOnSample)
                   + "_OFF" + std::to_string(noteOffSample);

    // Event 1: Initial reset
    seq.events.push_back({
        synth::TimedMidiType::AllNotesOff,
        spec.midiChannel,
        spec.midiNoteNumber,
        0.0f,
        0,
        0.0
    });

    // Event 2: Note On
    double noteOnTimeMs = (static_cast<double>(noteOnSample) / sampleRate) * 1000.0;
    seq.events.push_back({
        synth::TimedMidiType::NoteOn,
        spec.midiChannel,
        spec.midiNoteNumber,
        spec.midiVelocity,
        static_cast<int>(noteOnSample),
        noteOnTimeMs
    });

    // Event 3: Note Off
    double noteOffTimeMs = (static_cast<double>(noteOffSample) / sampleRate) * 1000.0;
    seq.events.push_back({
        synth::TimedMidiType::NoteOff,
        spec.midiChannel,
        spec.midiNoteNumber,
        0.0f,
        static_cast<int>(noteOffSample),
        noteOffTimeMs
    });

    // Compute canonical sequence hash
    synth::Sha256 hasher;
    hasher.update("MIDI_STIMULUS_V1");
    hasher.update(&spec.midiChannel, sizeof(spec.midiChannel));
    hasher.update(&spec.midiNoteNumber, sizeof(spec.midiNoteNumber));
    hasher.update(&spec.midiVelocity, sizeof(spec.midiVelocity));
    hasher.update(&noteOnSample, sizeof(noteOnSample));
    hasher.update(&noteOffSample, sizeof(noteOffSample));
    seq.sequenceHash = hasher.finalHex();

    return seq;
}

std::string MeasurementStimulusCoordinator::computeAudioHash(const std::vector<float>& audioSamples)
{
    if (audioSamples.empty())
        return "";
    return synth::Sha256::computeHex(audioSamples.data(), audioSamples.size() * sizeof(float));
}

std::string MeasurementStimulusCoordinator::computeSpecHash(const StimulusSpec& spec)
{
    synth::Sha256 hasher;
    hasher.update("STIMULUS_SPEC_V1");
    int typeInt = static_cast<int>(spec.type);
    hasher.update(&typeInt, sizeof(typeInt));
    hasher.update(&spec.startFreqHz, sizeof(spec.startFreqHz));
    hasher.update(&spec.endFreqHz, sizeof(spec.endFreqHz));
    hasher.update(&spec.durationSec, sizeof(spec.durationSec));
    hasher.update(&spec.levelDbfs, sizeof(spec.levelDbfs));
    hasher.update(&spec.phaseRad, sizeof(spec.phaseRad));
    hasher.update(&spec.seed, sizeof(spec.seed));
    hasher.update(&spec.midiChannel, sizeof(spec.midiChannel));
    hasher.update(&spec.midiNoteNumber, sizeof(spec.midiNoteNumber));
    hasher.update(&spec.midiVelocity, sizeof(spec.midiVelocity));
    hasher.update(&spec.noteOnSample, sizeof(spec.noteOnSample));
    hasher.update(&spec.noteOffSample, sizeof(spec.noteOffSample));
    return hasher.finalHex();
}

} // namespace abdaudiolab::measurement
