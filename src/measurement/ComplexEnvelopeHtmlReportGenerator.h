/**
 * @file ComplexEnvelopeHtmlReportGenerator.h
 * @brief Self-contained, offline-safe, publication-grade HTML report generator with interactive 8-stage envelope viewer.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeContracts.h"
#include "ComplexEnvelopeOrchestratorContracts.h"
#include "ComplexEnvelopeExportContracts.h"
#include <string>
#include <vector>

namespace abdaudiolab::measurement
{

class ComplexEnvelopeHtmlReportGenerator
{
public:
    /**
     * @brief Generates an offline-safe, CSP-compliant HTML report with interactive multi-domain envelope views.
     * Contains zero remote dependencies, zero network requests, and deterministic rendering.
     *
     * @param result Orchestration outcome including trajectories, timing resolution, and comparisons.
     * @param spec Export specification.
     * @param nativeStages Optional 8-stage native parameters.
     * @param relRawAudioPath Relative path to raw audio WAV file (e.g. "../audio/raw_capture.wav").
     * @param relCompAudioPath Relative path to latency-compensated WAV file.
     * @return Deterministic HTML string.
     */
    [[nodiscard]] static std::string generateInteractiveReportHtml(
        const ComplexEnvelopeOrchestrationResult& result,
        const ComplexEnvelopeExportSpec& spec,
        const std::vector<EnvelopeStageDescriptor>& nativeStages = {},
        const std::string& relRawAudioPath = "",
        const std::string& relCompAudioPath = "") noexcept;
};

} // namespace abdaudiolab::measurement
