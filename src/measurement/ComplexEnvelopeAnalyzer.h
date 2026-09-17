/**
 * @file ComplexEnvelopeAnalyzer.h
 * @brief Multi-domain envelope analyzer extracting lockstep Pitch, Timbre, and Amplitude observables.
 * @author ABDSynths
 * @date 2026
 *
 * Provides DUT-agnostic acoustic feature extraction across pitch (DCO/VCO),
 * timbre / spectral centroid (DCW/VCF/Phase Distortion proxy), and amplitude (DCA/VCA).
 * Fully decoupled from guided session coordinators: works with live capture,
 * unguided mode, pre-recorded audio, and synthetic control benchmarks.
 */

#pragma once

#include "ComplexEnvelopeContracts.h"
#include <vector>
#include <string>
#include <optional>
#include <memory>

namespace abdaudiolab::measurement
{

/**
 * @brief Channel downmixing policy for multichannel audio input.
 */
enum class ChannelDownmixPolicy
{
    MonoOnly,        /**< Expects mono signal; rejects multi-channel without processing */
    AverageToMono,   /**< Averages all interleaved channels to mono */
    LeftChannelOnly, /**< Takes channel 0 of interleaved stream */
    RightChannelOnly /**< Takes channel 1 of interleaved stream */
};

/**
 * @brief Input parameters and captured audio buffer for multi-domain envelope analysis.
 */
struct ComplexEnvelopeAnalysisInput
{
    std::vector<float> audioBuffer;
    int numChannels { 1 };
    ChannelDownmixPolicy downmixPolicy { ChannelDownmixPolicy::AverageToMono };
    double sampleRate { 48000.0 };

    // Timing markers (optional: if omitted, automatic unguided detection is used)
    std::optional<size_t> noteOnSample;
    std::optional<size_t> noteOffSample;
    std::string noteOnMethod { "provided_midi_event" };   // "provided_midi_event", "energy_onset", "manual_marker", "not_available"
    std::string noteOffMethod { "provided_midi_event" };  // "provided_midi_event", "energy_decay", "manual_marker", "not_available"

    double nominalFrequencyHz { 261.6256 }; // Base reference (default: C4)
    int midiVelocity { 100 };

    // Provenance & container tracking
    DutIdentity dut;
    std::string captureId { "capture_envelope_001" };
    std::string audioSha256;
    std::string stimulusSha256;
    std::string nativePatchStateSha256;
};

/**
 * @brief Configuration parameters for STFT, windowing, and metric extraction.
 */
struct ComplexEnvelopeAnalyzerConfig
{
    double sampleRateHz { 48000.0 };
    int hopSizeSamples { 256 };
    int fftSize { 2048 };
    int windowLengthSamples { 1024 };
    std::string windowFunction { "Hann" };
    double noiseFloorDbfs { -90.0 };
    double silenceThresholdDbfs { -75.0 };
    double onsetEnergyThresholdRatio { 0.05 }; // Fraction of peak for unguided onset detection
    std::string normalizationReference { "peak_observed" }; // "peak_observed", "full_scale", "calibration_reference"

    // Unit for pitch trajectory ("cents" relative to nominal, or "Hz")
    std::string pitchUnit { "cents" };

    // Generic labels (default) or DUT-specific adapter labels (e.g. DCO/DCW/DCA for CZ-101, VCO/VCF/VCA for Minimoog)
    std::string pitchLabel { "PitchObservable" };
    std::string timbreLabel { "TimbreObservable" };
    std::string amplitudeLabel { "AmplitudeObservable" };
};

/**
 * @brief Generator of synthetic control audio signals with mathematically known ground truth.
 */
class SyntheticEnvelopeControlGenerator
{
public:
    struct GenerationParams
    {
        double sampleRate { 48000.0 };
        double noteOnMs { 50.0 };
        double noteOffMs { 800.0 };
        double totalDurationMs { 1200.0 };
        double baseFrequencyHz { 261.6256 };
        int numChannels { 1 };

        // Stages for ground truth definition
        std::vector<EnvelopeStageDescriptor> pitchStages;
        std::vector<EnvelopeStageDescriptor> timbreStages;
        std::vector<EnvelopeStageDescriptor> amplitudeStages;
    };

    struct GenerationResult
    {
        std::vector<float> audioBuffer;
        int numChannels { 1 };
        double sampleRate { 48000.0 };
        size_t noteOnSample { 0 };
        size_t noteOffSample { 0 };
        SyntheticEnvelopeGroundTruth groundTruth;
    };

    /**
     * @brief Generates standard 8-stage synthetic signal with pitch modulation, phase distortion, and amplitude envelope.
     */
    [[nodiscard]] static GenerationResult generateStandard8StageTestSignal(const GenerationParams& params = {});

    /**
     * @brief Generates pitch-swept signal with stationary timbre and amplitude.
     */
    [[nodiscard]] static GenerationResult generatePitchSweepSignal(double sampleRate = 48000.0,
                                                                   double startFreqHz = 220.0,
                                                                   double endFreqHz = 440.0,
                                                                   double durationMs = 800.0);

    /**
     * @brief Generates phase distortion sweep signal where index increases then decreases.
     */
    [[nodiscard]] static GenerationResult generatePhaseDistortionSweepSignal(double sampleRate = 48000.0,
                                                                             double baseFreqHz = 261.6256,
                                                                             double maxIndex = 1.2,
                                                                             double durationMs = 800.0);

    /**
     * @brief Generates shaped noise burst with observable timbre and amplitude but unobservable pitch.
     */
    [[nodiscard]] static GenerationResult generateNoiseBurstSignal(double sampleRate = 48000.0,
                                                                  double durationMs = 500.0);
};

/**
 * @brief Core analyzer producing lockstep MultiDomainEnvelopeCaptureRecord.
 */
class ComplexEnvelopeAnalyzer
{
public:
    /**
     * @brief Analyzes input audio and extracts synchronized Pitch, Timbre, and Amplitude trajectories.
     * 
     * Guarantees:
     * 1. Strict single TemporalGrid: identical originMs, hopMs, frameCount and gridId for all 3 trajectories.
     * 2. Silent or unreliable frames preserve frameIndex and timeMs with value = nullopt.
     * 3. Metrological honesty: nativeEnvelopeReconstruction == "not_claimed", phaseDistortionProxy == "not_claimed".
     * 4. Safe sample rate handling: adapts effective sample rate if input differs from config.
     * 5. Autonomous unguided onset/decay detection when MIDI markers are absent.
     */
    [[nodiscard]] static MultiDomainEnvelopeCaptureRecord analyze(
        const ComplexEnvelopeAnalysisInput& input,
        const ComplexEnvelopeAnalyzerConfig& config = {});

    /**
     * @brief Detects acoustic energy onset sample index without MIDI markers.
     * @return Sample index of onset, or std::nullopt if audio is entirely silent or below threshold.
     */
    [[nodiscard]] static std::optional<size_t> detectEnergyOnset(
        const std::vector<float>& monoAudio,
        double sampleRate,
        double noiseFloorDbfs = -90.0,
        double ratioOfPeak = 0.05);

    /**
     * @brief Detects acoustic energy decay sample index (note-off acoustic release).
     * @return Sample index of decay start, or std::nullopt if signal remains sustained or never drops.
     */
    [[nodiscard]] static std::optional<size_t> detectEnergyDecay(
        const std::vector<float>& monoAudio,
        size_t noteOnSample,
        double sampleRate,
        double noiseFloorDbfs = -90.0);

    /**
     * @brief Downmixes multichannel audio to mono according to policy.
     */
    [[nodiscard]] static std::vector<float> downmixToMono(
        const std::vector<float>& audioBuffer,
        int numChannels,
        ChannelDownmixPolicy policy);
};

} // namespace abdaudiolab::measurement
