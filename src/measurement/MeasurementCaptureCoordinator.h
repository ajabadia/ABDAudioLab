/**
 * @file MeasurementCaptureCoordinator.h
 * @brief Synchronous audio and MIDI capture coordinator for hardware and plugin targets.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include "MeasurementStimulusCoordinator.h"
#include "../synth/ISynthTarget.h"
#include <string>
#include <vector>

namespace abdaudiolab::measurement
{

/**
 * @struct CaptureResult
 * @brief Formal capture outcome with full traceability, latency and SHA-256 hashes.
 */
struct CaptureResult
{
    MeasurementStatus status { MeasurementStatus::failed };
    std::string reason;
    std::vector<float> capturedAudio;
    int64_t numSamples { 0 };
    double sampleRateHz { 48000.0 };
    int latencySamples { 0 };
    std::string stimulusSha256;
    std::string presetStateSha256;
};

/**
 * @class MeasurementCaptureCoordinator
 * @brief Executes synchronous closed-loop stimulus excitation and response capture.
 * 
 * Guarantees formal failure isolation: unhandled errors, uninitialized targets, or NaN corruptions
 * return explicit failed/invalid states without hanging, preventing partial artifact leakage.
 */
class MeasurementCaptureCoordinator
{
public:
    /**
     * @brief Executes a synchronous capture cycle against an ISynthTarget.
     * 
     * @param target Pointer to initialized ISynthTarget (virtual plugin or mock).
     * @param spec Measurement specification.
     * @param state Optional preset state to load before capture.
     * @return CaptureResult Structured capture outcome with audio samples and diagnostic metadata.
     */
    static CaptureResult captureSynchronous(synth::ISynthTarget* target,
                                            const MeasurementSpec& spec,
                                            const synth::SynthPresetState* state = nullptr);
};

} // namespace abdaudiolab::measurement
