/**
 * @file ComplexEnvelopeSvgRenderer.h
 * @brief Pure static, deterministic, and safe multi-series SVG renderer for complex envelope measurements.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeContracts.h"
#include "ComplexEnvelopeOrchestratorContracts.h"
#include <string>
#include <vector>
#include <optional>

namespace abdaudiolab::measurement
{

class ComplexEnvelopeSvgRenderer
{
public:
    struct RenderOptions
    {
        int width { 960 };
        int height { 480 };
        int paddingLeft { 70 };
        int paddingRight { 40 };
        int paddingTop { 50 };
        int paddingBottom { 60 };
        bool showNativeStages { true };
        bool showProjectedReference { true };
        bool showObservedTrajectory { true };
        std::string title { "Complex Envelope Observable Comparison" };
    };

    /**
     * @brief Generates a strictly deterministic, self-contained, offline-safe SVG string.
     * Blocks all scripting, external hrefs, foreignObjects, url() constructs, and NaN/Inf values.
     * Breaks continuous paths when encountering silent/unreliable intervals (no false zeros).
     *
     * @param record Analyzed capture record.
     * @param comparisons Observable comparison reports.
     * @param nativeStages Optional native stage descriptors (for vertical stage boundaries).
     * @param domain Domain to render (Pitch, Timbre, or Amplitude).
     * @param options Layout and dimension options.
     * @return Deterministic SVG string.
     */
    [[nodiscard]] static std::string renderOverlaySvg(
        const MultiDomainEnvelopeCaptureRecord& record,
        const std::vector<ObservableComparisonReport>& comparisons,
        const std::vector<EnvelopeStageDescriptor>& nativeStages = {},
        EnvelopeDomain domain = EnvelopeDomain::Timbre,
        const RenderOptions& options = {}) noexcept;

    /**
     * @brief Validates that an SVG string adheres to all safety and deterministic rules.
     * @param svg Svg content string.
     * @param outError Diagnostic error if validation fails.
     * @return true if valid and safe, false otherwise.
     */
    [[nodiscard]] static bool validateSvgSafety(const std::string& svg, std::string& outError) noexcept;
};

} // namespace abdaudiolab::measurement
