/**
 * @file ComplexEnvelopeOrchestrator.h
 * @brief Central hardware/software capture orchestrator and multi-domain observable comparison coordinator.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeOrchestratorContracts.h"
#include "ComplexEnvelopeCaptureSession.h"
#include "TimingReferenceResolver.h"
#include "ObservableComparisonEngine.h"
#include "ComplexEnvelopeAnalyzer.h"
#include <memory>
#include <optional>

namespace abdaudiolab::measurement
{

class ComplexEnvelopeOrchestrator
{
public:
    struct OrchestratorConfig
    {
        TimingReferenceType preferredTimingType { TimingReferenceType::ProvidedEvent };
        TimingReferenceResolver::Config timingConfig;
        ObservableComparisonEngine::ComparisonConfig comparisonConfig;
        ComplexEnvelopeAnalyzerConfig analyzerConfig;
    };

    /**
     * @brief Executes complete orchestration flow: alignment, analysis, and native-to-observable comparison.
     * @param stimulus Stimulus definition.
     * @param session Capture session with immutable raw audio.
     * @param calibration Optional audio chain calibration.
     * @param nativeProvider Optional native state provider (decoupled abstraction, can be nullptr).
     * @param loopbackReferenceAudio Optional loopback audio buffer for correlation-based timing.
     * @param config Execution parameters.
     * @return ComplexEnvelopeOrchestrationResult.
     */
    [[nodiscard]] static ComplexEnvelopeOrchestrationResult orchestrate(
        const ComplexEnvelopeStimulus& stimulus,
        ComplexEnvelopeCaptureSession& session,
        const std::optional<AudioChainCalibration>& calibration = std::nullopt,
        const INativeStateProvider* nativeProvider = nullptr,
        std::span<const float> loopbackReferenceAudio = {},
        const OrchestratorConfig& config = {}) noexcept;
};

} // namespace abdaudiolab::measurement
