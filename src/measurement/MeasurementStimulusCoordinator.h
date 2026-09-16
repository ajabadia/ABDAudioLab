/**
 * @file MeasurementStimulusCoordinator.h
 * @brief Deterministic excitation stimulus generator for audio and MIDI measurements.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include "../synth/MidiExcitationSequence.h"
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class MeasurementStimulusCoordinator
 * @brief Produces sample-accurate, reproducible excitation stimuli (audio & MIDI).
 * 
 * Guarantees that identical StimulusSpec configurations produce identical bit-for-bit audio
 * or identical MIDI timestamped event sequences, computing cryptographic SHA-256 fixity.
 */
class MeasurementStimulusCoordinator
{
public:
    /**
     * @brief Generates deterministic time-series audio samples according to the StimulusSpec.
     * 
     * Supports:
     * - logSineSweep (via FarinaDeconvolver)
     * - sineTone
     * - impulse
     * - whiteNoise (deterministic seed)
     * - pinkNoise (deterministic Voss-McCartney filter)
     * - amplitudeRamp
     * 
     * @param spec Stimulus specification.
     * @param sampleRate Operating sample rate in Hz.
     * @return std::vector<float> Sample-accurate audio buffer.
     */
    static std::vector<float> generateAudioStimulus(const StimulusSpec& spec, double sampleRate);

    /**
     * @brief Generates deterministic sample-accurate MIDI excitation sequence according to the StimulusSpec.
     * 
     * Configures NoteOn and NoteOff events at exact sample offsets with deterministic velocities and channels.
     * 
     * @param spec Stimulus specification.
     * @param sampleRate Operating sample rate in Hz.
     * @param releaseTailDurationSec Post-note tail margin (default 0.3s).
     * @return synth::MidiExcitationSequence Fully populated MIDI sequence.
     */
    static synth::MidiExcitationSequence generateMidiStimulus(const StimulusSpec& spec,
                                                              double sampleRate,
                                                              double releaseTailDurationSec = 0.3);

    /**
     * @brief Computes canonical SHA-256 hash of generated audio samples.
     */
    static std::string computeAudioHash(const std::vector<float>& audioSamples);

    /**
     * @brief Computes canonical SHA-256 hash of a StimulusSpec configuration.
     */
    static std::string computeSpecHash(const StimulusSpec& spec);
};

} // namespace abdaudiolab::measurement
