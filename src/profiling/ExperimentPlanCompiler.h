#pragma once

#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <cstdint>
#include "MeasurementRecipe.h"
#include "TargetProfile.h"
#include "../synth/ExperimentPlan.h"
#include "../gui/session/ProfilingSessionContracts.h"
#include "../core/ProfilingSession.h"

namespace abdaudiolab::profiling
{

/**
 * @brief Opciones y valores por defecto normativos para la compilación científica.
 */
struct CompilationDefaults
{
    int defaultMidiChannel { 1 };
    double defaultSampleRate { 96000.0 };
    double preSilenceSec { 0.05 };
    double postSilenceSec { 0.05 };
    uint32_t randomSeed { 42 };
};

/**
 * @brief Solicitud operativa desacoplada de la receta y del entorno físico (Ajuste 2).
 */
struct ExecutionRequest
{
    std::string schemaVersion { "1.0" };
    std::string requestId;
    std::string recipeDocumentHash;
    std::optional<MeasurementRecipe> embeddedRecipe;
    std::string targetLogicalId;
    std::optional<gui::session::TargetSelectionState> targetSelection;
    std::string executionMode { "Automatic" }; // "Manual", "Automatic", "Headless"
    double timeoutSeconds { 60.0 };
    std::string operatorId;
    std::string evidenceDestinationDirectory;
    std::vector<std::string> allowedOverrides;
};

/**
 * @brief Entorno físico resuelto en tiempo de ejecución (Ajuste 2).
 */
struct ExecutionEnvironment
{
    std::string schemaVersion { "1.0" };
    std::string driver { "MockAudioEngine" }; // "WASAPI", "ASIO", "MockAudioEngine"
    double sampleRate { 48000.0 };
    int blockSize { 256 };
    int channels { 2 };
    std::string deviceName { "VirtualLoopback" };
    std::string pluginBinary;
    std::string pluginVersion;
    int latencySamples { 0 };
    double latencyMs { 0.0 };
    std::vector<std::string> discoveredCapabilities;
};

/**
 * @brief Realización concreta ejecutable que liga el plan científico al entorno físico (Ajuste 1 y 2).
 */
struct ResolvedExecutionPlan
{
    synth::ExperimentPlan experimentPlan;
    ExecutionEnvironment environment;
    std::string resolvedExecutionPlanHash;
    int64_t totalSamples { 0 };
    double totalDurationSec { 0.0 };
};

/**
 * @brief Registro inmutable de ejecución y trazabilidad completa de 4 hashes canónicos (Ajuste 1 y 2).
 */
struct ExecutionRecord
{
    std::string schemaVersion { "1.0" };
    std::string recordId;
    ExecutionRequest request;
    ExecutionEnvironment rawEnvironment;
    ExecutionEnvironment canonicalEnvironment;
    std::string recipeDocumentHash;
    std::string experimentPlanHash;
    std::string resolvedExecutionPlanHash;
    std::string audioEvidenceHash;
    std::string resultStatus { "Completed" }; // "Completed", "Aborted", "Failed"
    std::string evidencePath;
    uint64_t timestampMs { 0 };
};

/**
 * @brief Resultado estructurado de resolución física con diagnósticos RFC 6901.
 */
struct ResolveExecutionPlanResult
{
    std::optional<ResolvedExecutionPlan> resolvedPlan;
    std::vector<ValidationDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return resolvedPlan.has_value()
            && std::none_of(diagnostics.begin(), diagnostics.end(), [](const ValidationDiagnostic& d) {
                return d.severity == DiagnosticSeverity::Error;
            });
    }
};

/**
 * @class ExperimentPlanCompiler
 * @brief Compilador unidireccional y determinista de MeasurementRecipe a ExperimentPlan,
 * resolución de entorno y puente compatible hacia core::ProfilingSession.
 */
class ExperimentPlanCompiler
{
public:
    ExperimentPlanCompiler() = default;
    ~ExperimentPlanCompiler() = default;

    /**
     * @brief Compila una receta metrológica declarativa a un ExperimentPlan científico puro (Ajustes 1 y 5).
     * El hash generado (experimentPlanHash) depende exclusivamente de la receta y de los defaults normativos,
     * siendo completamente agnóstico al hardware o entorno físico.
     */
    [[nodiscard]] static synth::ExperimentPlan compileToExperimentPlan(
        const MeasurementRecipe& recipe,
        const CompilationDefaults& defaults = {});

    /**
     * @brief Resuelve el plan científico frente a un entorno físico y genera el ResolvedExecutionPlan estructurado.
     * Valida compatibilidad de sampleRate y capacidades requeridas emitiendo ValidationDiagnostics normativos.
     * Computa resolvedExecutionPlanHash.
     */
    [[nodiscard]] static ResolveExecutionPlanResult resolveExecutionPlan(
        const synth::ExperimentPlan& plan,
        const ExecutionEnvironment& env,
        const MeasurementRecipe* sourceRecipe = nullptr);

    /**
     * @brief Resuelve declarativamente la combinación MeasurementRecipe + TargetProfile + ExecutionEnvironment.
     * Valida correspondencia de semanticIds, capacidades de transporte y entorno físico emitiendo ValidationDiagnostics RFC 6901.
     */
    [[nodiscard]] static ResolveExecutionPlanResult resolveExecutionPlan(
        const MeasurementRecipe& recipe,
        const TargetProfile& profile,
        const ExecutionEnvironment& environment,
        const CompilationDefaults& defaults = {});

    /**
     * @brief Puente compatible hacia core::ProfilingSession para ejecución mediante ProfilingSequencer / MockAudioEngine.
     */
    [[nodiscard]] static core::ProfilingSession createProfilingSession(
        const ResolvedExecutionPlan& resolvedPlan,
        const gui::session::TargetSelectionState& targetState,
        const TargetProfile* profile = nullptr);

    /**
     * @brief Calcula el SHA-256 canónico de un ExperimentPlan puro.
     */
    [[nodiscard]] static std::string computeExperimentPlanHash(const synth::ExperimentPlan& plan);

    /**
     * @brief Calcula el SHA-256 canónico de un ResolvedExecutionPlan.
     */
    [[nodiscard]] static std::string computeResolvedExecutionPlanHash(
        const std::string& experimentPlanHash,
        const ExecutionEnvironment& env);
};

} // namespace abdaudiolab::profiling
