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
#include "../synth/SysExContracts.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace abdaudiolab::measurement
{

/**
 * @struct CaptureArtifactMetadata
 * @brief Telemetría y trazabilidad criptográfica de la sesión de captura (Fase 20.11 T3.3).
 */
struct CaptureArtifactMetadata
{
    double sampleRateHz { 0.0 };
    int32_t bufferSize { 0 };
    int32_t numChannels { 0 };
    double effectiveLatencySamples { 0.0 };
    double hardwareClockDriftPpm { 0.0 };
    int32_t underruns { 0 };
    int32_t overruns { 0 };
    std::string sha256Audio;
    std::string sha256Midi;
    std::string sha256SysEx;
    std::string sha256State;
    std::string wallClockStartIso;
    std::string wallClockEndIso;

    [[nodiscard]] std::string toJsonString(int indent = 2) const;
};

/**
 * @struct CaptureResult
 * @brief Formal capture outcome with full traceability, latency and SHA-256 hashes.
 */
struct CaptureResult
{
    MeasurementStatus status { MeasurementStatus::failed };
    std::string reason;
    std::vector<float> capturedAudio;
    std::vector<float> stimulusAudio; /**< Input audio stimulus copy for directTransferFunction auditing */
    int64_t numSamples { 0 };
    double sampleRateHz { 48000.0 };
    int latencySamples { 0 };
    std::string stimulusSha256;
    std::string capturedAudioSha256;
    std::string presetStateSha256;
    std::string measurementDomain { "directTransferFunction" };
    CaptureArtifactMetadata telemetryMetadata;
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
     * @brief Executes a synchronous MIDI capture cycle against an ISynthTarget (synthesizedSpectralResponse).
     * 
     * Orden estricto (Fase 20.11 T3.2):
     * 1. prepare(spec)
     * 2. resetState()
     * 3. loadState(state)
     * 4. verifyState()
     * 5. Inyectar estímulo MIDI determinista y renderizar
     * 6. Validar telemetría y generar metadatos FAIR de captura
     * 
     * @param target Pointer to initialized ISynthTarget (virtual plugin or mock).
     * @param spec Measurement specification.
     * @param state Optional preset state to load before capture.
     * @return CaptureResult Structured capture outcome with audio samples and diagnostic metadata.
     */
    static CaptureResult captureSynchronous(synth::ISynthTarget* target,
                                            const MeasurementSpec& spec,
                                            const synth::SynthPresetState* state = nullptr);

    /**
     * @brief Ejecuta un ciclo de captura síncrono inyectando un artefacto SysEx validado.
     */
    static CaptureResult captureSynchronousWithSysEx(synth::ISynthTarget* target,
                                                    const MeasurementSpec& spec,
                                                    const synth::SysExArtifact& sysEx);

    /**
     * @brief Executes a synchronous audio-through capture cycle on an AudioProcessor (directTransferFunction).
     * 
     * Injects a deterministic audio stimulus (e.g. log-sine sweep), calls processBlock in chunks,
     * preserves both stimulus and response with SHA-256 fixity, and measures latency.
     * 
     * @param processor Pointer to initialized juce::AudioProcessor (effect or test fixture).
     * @param spec Measurement specification.
     * @param blockSize Block size for processing (default 512, safely handles partial/arbitrary blocks).
     * @return CaptureResult Structured capture outcome.
     */
    static CaptureResult captureAudioThrough(juce::AudioProcessor* processor,
                                             const MeasurementSpec& spec,
                                             int blockSize = 512);
};

} // namespace abdaudiolab::measurement
