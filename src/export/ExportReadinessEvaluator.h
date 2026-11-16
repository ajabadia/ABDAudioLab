/**
 * @file ExportReadinessEvaluator.h
 * @brief Pre-export validation gate and traceable metadata generator (Phase 19.6).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../synth/ModelEvaluationTypes.h"

namespace abdaudiolab::exporting
{

/**
 * @class ExportReadinessEvaluator
 * @brief Evaluates model acceptance status, decides export permission, and renders C++ preprocessor guards and JSON metadata.
 */
class ExportReadinessEvaluator
{
public:
    ExportReadinessEvaluator() = default;
    ~ExportReadinessEvaluator() = default;

    /**
     * @brief Evaluates export readiness from a canonical ModelEvaluation object.
     * @param evaluation Full evaluation record.
     * @param allowUnvalidatedOverride If true, allows writing blocked models with fatal #error compile guards.
     * @return ExportReadiness report with verdict, reason, certified limits, and permission flag.
     */
    [[nodiscard]] static synth::ExportReadiness evaluate(const synth::ModelEvaluation& evaluation,
                                                         bool allowUnvalidatedOverride = false);

    /**
     * @brief Generates C++ preprocessor macros, #pragma message warnings, or #error blocking guards.
     * @param readiness Evaluated readiness record.
     * @param sampleRate Certified sample rate.
     * @param minDbfs Minimum tested input level in dBFS.
     * @param maxDbfs Maximum tested input level in dBFS.
     * @return Formatted C++ preprocessor directives block.
     */
    [[nodiscard]] static std::string generatePreprocessorGuards(const synth::ExportReadiness& readiness,
                                                                double sampleRate = 96000.0,
                                                                double minDbfs = -36.0,
                                                                double maxDbfs = -3.0);

    /**
     * @brief Generates an embedded JSON comment block for C++ headers.
     * @param readiness Evaluated readiness record.
     * @param evaluation Full evaluation record.
     * @return Single or multi-line comment containing canonical JSON metadata.
     */
    [[nodiscard]] static std::string generateEmbeddedJsonBlock(const synth::ExportReadiness& readiness,
                                                               const synth::ModelEvaluation& evaluation);

    /**
     * @brief Generates the structured "metrologicalCertification" JSON object for telemetry and manifest exports.
     * @param readiness Evaluated readiness record.
     * @param evaluation Full evaluation record.
     * @return nlohmann::json object conforming to export_metadata.schema.json v1.0.
     */
    [[nodiscard]] static nlohmann::json toMetrologicalCertificationJson(const synth::ExportReadiness& readiness,
                                                                        const synth::ModelEvaluation& evaluation);
};

} // namespace abdaudiolab::exporting
