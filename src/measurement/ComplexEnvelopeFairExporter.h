/**
 * @file ComplexEnvelopeFairExporter.h
 * @brief Transactional FAIR/LNL experiment container packager for complex envelope characterization.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeExportContracts.h"
#include "ComplexEnvelopeCaptureSession.h"
#include <juce_core/juce_core.h>
#include <string>
#include <vector>

namespace abdaudiolab::measurement
{

class ComplexEnvelopeFairExporter
{
public:
    /**
     * @brief Exports an immutable FAIR/LNL measurement container with full transactional staging and rollback.
     *
     * Roles registered:
     * - envelope_spec: spec.json
     * - raw_audio: audio/raw_capture.wav (optional)
     * - compensated_audio: audio/compensated_capture.wav (optional)
     * - envelope_record: data/envelope_record.json
     * - observable_comparison: data/comparison_report.json
     * - vector_svg_overlay: reports/complex_envelope_overlay.svg
     * - interactive_report: reports/envelope_report.html
     *
     * @param targetDir Destination container directory.
     * @param result Orchestration outcome.
     * @param spec Export specification.
     * @param nativeStages Optional 8-stage native parameters.
     * @param rawAudio Optional raw audio samples for WAV creation.
     * @param compensatedAudio Optional compensated audio samples for WAV creation.
     * @param outError Diagnostic error string if export fails.
     * @return true on success, false with rollback on failure.
     */
    static bool exportContainer(
        const juce::File& targetDir,
        const ComplexEnvelopeOrchestrationResult& result,
        const ComplexEnvelopeExportSpec& spec,
        const std::vector<EnvelopeStageDescriptor>& nativeStages = {},
        std::span<const float> rawAudio = {},
        std::span<const float> compensatedAudio = {},
        juce::String& outError = *(juce::String*)nullptr) noexcept;

    /**
     * @brief Validates that a container is valid and non-corrupted by recomputing all artifact SHA-256 hashes.
     * @param containerDir Container directory to audit.
     * @param outError Diagnostic error string if validation fails.
     * @return true if valid and authentic, false otherwise.
     */
    static bool validateContainerIntegrity(
        const juce::File& containerDir,
        juce::String& outError) noexcept;
};

} // namespace abdaudiolab::measurement
