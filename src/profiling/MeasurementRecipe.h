#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <algorithm>

namespace abdaudiolab::profiling
{

enum class AssistanceLevel
{
    Quick,
    Configurable,
    Advanced
};

[[nodiscard]] inline std::string assistanceLevelToString(AssistanceLevel level) noexcept
{
    switch (level)
    {
        case AssistanceLevel::Quick:        return "Quick";
        case AssistanceLevel::Configurable: return "Configurable";
        case AssistanceLevel::Advanced:     return "Advanced";
    }
    return "Quick";
}

[[nodiscard]] inline std::optional<AssistanceLevel> assistanceLevelFromString(const std::string& str) noexcept
{
    if (str == "Quick")        return AssistanceLevel::Quick;
    if (str == "Configurable") return AssistanceLevel::Configurable;
    if (str == "Advanced")     return AssistanceLevel::Advanced;
    return std::nullopt;
}

struct TargetConstraints
{
    std::vector<std::string> targetKinds;
    std::vector<std::string> requiredCapabilities;
    std::vector<int> allowedSampleRatesHz;
    int channels { 2 };
};

struct NoteExcitationConfig
{
    int midiNote { 60 };
    double velocity { 0.5 };
    double gateMs { 250.0 };
    double settlingMs { 50.0 };
};

struct ExcitationConfig
{
    std::vector<NoteExcitationConfig> notes;
    int repetitions { 3 };
    std::optional<int> seed;
};

struct MeasurementPointConfig
{
    std::string parameter;
    double normalizedValue { 0.0 };
    std::string semanticId;

    [[nodiscard]] const std::string& getSemanticId() const noexcept
    {
        return !semanticId.empty() ? semanticId : parameter;
    }
};

struct MeasurementConfig
{
    std::vector<MeasurementPointConfig> points;
    std::string calibrationPolicy { "Required" }; // "Required", "Optional", "None"
    std::string analysisPolicy { "CanonicalV1" };  // "CanonicalV1", "HarmonicFull", "LinearTransferOnly"
};

struct EvaluationPolicyConfig
{
    double minimumSnrDb { 50.0 };
    double maximumThdPercent { 1.0 };
    double f0ToleranceCents { 10.0 };
};

struct RecipeProvenance
{
    std::string authoringSource { "builtin" }; // "builtin", "operator_manual", "llm_generated", "migrated", "promoted_from_exploration"
    std::string documentationRef;
    std::string migratedFromSchemaVersion;
    std::string migrationToolVersion;
    std::string sourceRecipeDocumentHash; // Reservado exclusivamente para derivación de otra MeasurementRecipe
    std::string sourceKind;               // "recipe", "exploration", "execution_record"
    std::string sourceExplorationId;
    std::string sourceExplorationHash;    // SHA-256 canónico del ExplorationRecord
    std::string promotionToolVersion;
};

struct MeasurementRecipe
{
    std::string schemaVersion { "1.0" };
    std::string kind { "abd.measurement-recipe" };
    std::string recipeId;
    std::string displayName;
    std::string description;
    AssistanceLevel assistanceLevel { AssistanceLevel::Quick };
    int revision { 1 };

    TargetConstraints targetConstraints;
    ExcitationConfig excitation;
    MeasurementConfig measurement;
    EvaluationPolicyConfig evaluationPolicy;
    RecipeProvenance provenance;
};

enum class DiagnosticSeverity
{
    Error,
    Warning
};

struct ValidationDiagnostic
{
    DiagnosticSeverity severity { DiagnosticSeverity::Error };
    std::string jsonPointer;
    std::string code;
    std::string message;

    [[nodiscard]] std::string toString() const
    {
        std::string s = (severity == DiagnosticSeverity::Error) ? "[ERROR] " : "[WARN] ";
        if (!jsonPointer.empty())
            s += jsonPointer + ": ";
        s += "(" + code + ") " + message;
        return s;
    }
};

struct RecipeLoadResult
{
    MeasurementRecipe recipe;
    std::string recipeDocumentHash;
    std::vector<ValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return std::none_of(diagnostics.begin(), diagnostics.end(), [](const ValidationDiagnostic& d) {
            return d.severity == DiagnosticSeverity::Error;
        });
    }

    [[nodiscard]] bool hasErrors() const noexcept { return !isSuccess(); }
};

} // namespace abdaudiolab::profiling
