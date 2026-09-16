/**
 * @file ExperimentRecord.h
 * @brief Canonical data format and contracts for reproducible, immutable experiment records.
 * @author ABDSynths
 * @date 2026
 *
 * Implements FAIR data principles (Findable, Accessible, Interoperable, Reusable)
 * with strict provenance, manifest checksum verification, and anti-corruption detection.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace abdaudiolab::core
{

/**
 * @brief Tipología o intención experimental de la captura.
 */
enum class ExperimentKind
{
    Exploration, /**< Modo no guiado / exploración libre (presets, MIDI, parámetros, audio raw). */
    Measurement, /**< Modo guiado formal (ensayos deterministas, métricas, holdout). */
    Validation   /**< Modo comparativo / benchmark A/B (target vs modelo vs residual). */
};

[[nodiscard]] inline std::string experimentKindToString(ExperimentKind kind)
{
    switch (kind)
    {
        case ExperimentKind::Exploration: return "Exploration";
        case ExperimentKind::Measurement: return "Measurement";
        case ExperimentKind::Validation:  return "Validation";
        default:                          return "Unknown";
    }
}

[[nodiscard]] inline ExperimentKind experimentKindFromString(const std::string& str)
{
    if (str == "Exploration") return ExperimentKind::Exploration;
    if (str == "Measurement") return ExperimentKind::Measurement;
    if (str == "Validation")  return ExperimentKind::Validation;
    return ExperimentKind::Exploration;
}

/**
 * @brief Estado formal de certificación e integridad del experimento.
 */
enum class ExperimentStatus
{
    LoadedForExploration, /**< Captura libre no auditada, admisible para exploración. */
    AuditedApproved,      /**< Aprobado tras superar todas las directivas metrológicas. */
    AuditedWithWarnings,  /**< Aprobado con advertencias operativas documentadas. */
    MeasurementInvalid,   /**< Descartado por inestabilidad, clipping o artefactos. */
    Rejected,             /**< Rechazado por no alcanzar los criterios de calidad. */
    Inconclusive,         /**< Margen entre candidatos inferior a la incertidumbre. */
    Corrupt               /**< Fallo de integridad: discrepancia de hash o archivo faltante. */
};

[[nodiscard]] inline std::string experimentStatusToString(ExperimentStatus status)
{
    switch (status)
    {
        case ExperimentStatus::LoadedForExploration: return "LoadedForExploration";
        case ExperimentStatus::AuditedApproved:      return "AuditedApproved";
        case ExperimentStatus::AuditedWithWarnings:  return "AuditedWithWarnings";
        case ExperimentStatus::MeasurementInvalid:   return "MeasurementInvalid";
        case ExperimentStatus::Rejected:             return "Rejected";
        case ExperimentStatus::Inconclusive:         return "Inconclusive";
        case ExperimentStatus::Corrupt:              return "Corrupt";
        default:                                     return "Unknown";
    }
}

[[nodiscard]] inline ExperimentStatus experimentStatusFromString(const std::string& str)
{
    if (str == "LoadedForExploration") return ExperimentStatus::LoadedForExploration;
    if (str == "AuditedApproved")      return ExperimentStatus::AuditedApproved;
    if (str == "AuditedWithWarnings")  return ExperimentStatus::AuditedWithWarnings;
    if (str == "MeasurementInvalid")   return ExperimentStatus::MeasurementInvalid;
    if (str == "Rejected")             return ExperimentStatus::Rejected;
    if (str == "Inconclusive")         return ExperimentStatus::Inconclusive;
    if (str == "Corrupt")              return ExperimentStatus::Corrupt;
    return ExperimentStatus::LoadedForExploration;
}

/**
 * @brief Perfil de persistencia y profundidad de evidencia.
 */
enum class StorageProfile
{
    Quick,    /**< Metadatos, evaluación y hashes (sin archivos WAV pesados). */
    Standard, /**< Quick + audio WAV de target, modelo y residual. */
    Audit     /**< Standard + logs de sesión, volcados de contratos y eventos completos. */
};

[[nodiscard]] inline std::string storageProfileToString(StorageProfile profile)
{
    switch (profile)
    {
        case StorageProfile::Quick:    return "Quick";
        case StorageProfile::Standard: return "Standard";
        case StorageProfile::Audit:    return "Audit";
        default:                       return "Standard";
    }
}

[[nodiscard]] inline StorageProfile storageProfileFromString(const std::string& str)
{
    if (str == "Quick") return StorageProfile::Quick;
    if (str == "Audit") return StorageProfile::Audit;
    return StorageProfile::Standard;
}

/**
 * @brief Validador de seguridad de rutas relativas para prevenir path traversal.
 */
[[nodiscard]] inline bool isSafeRelativePath(const std::string& relPath)
{
    if (relPath.empty())
        return false;
    if (relPath.find("..") != std::string::npos)
        return false;
    if (relPath.front() == '/' || relPath.front() == '\\')
        return false;
    if (relPath.find(':') != std::string::npos)
        return false;
    return true;
}

/**
 * @struct AudioArtifactMetadata
 * @brief Parámetros técnicos explícitos de archivos de audio (WAV).
 */
struct AudioArtifactMetadata
{
    double sampleRate { 0.0 };
    int channels { 0 };
    int bitsPerSample { 0 };
    int64_t sampleCount { 0 };
    double durationSeconds { 0.0 };
    std::string format { "WAV_PCM" };
    bool isInterleaved { true };
};

/**
 * @struct ExperimentArtifact
 * @brief Entrada en el inventario/manifiesto de evidencia del experimento.
 */
struct ExperimentArtifact
{
    std::string relativePath;
    std::string role;
    uint64_t sizeBytes { 0 };
    std::string sha256;
    std::optional<AudioArtifactMetadata> audio;
};

/**
 * @struct TargetIdentity
 * @brief Identidad física o digital del objetivo caracterizado.
 */
struct TargetIdentity
{
    std::string targetId;
    std::string targetName;
    std::string manufacturer;
    std::string version;
    std::string format;
    std::string binarySha256;
    std::string binaryPath;
    bool isDeterministic { true };
};

/**
 * @struct CaptureSpec
 * @brief Condiciones operativas de captura y excitación.
 */
struct CaptureSpec
{
    double sampleRate { 48000.0 };
    int hostBufferSize { 480 };
    int processingBlockSize { 256 };
    int channels { 2 };
    double durationSeconds { 0.0 };
    std::string presetStateHash;
    std::string excitationPlanHash;
    std::optional<uint64_t> randomSeed;
    StorageProfile storageProfile { StorageProfile::Standard };
};

/**
 * @struct ProvenanceRecord
 * @brief Trazabilidad metrológica y ambiental del ensayo.
 */
struct ProvenanceRecord
{
    std::string appVersion;
    int buildNumber { 0 };
    std::string gitCommit;
    std::string executionMode;
    std::string operatingSystem;
    std::string machineName;
    std::string timestampUtc;
    std::string operatorNotes;
    float ambientTemperatureC { 22.0f };
    int warmupTimeMinutes { 0 };
};

/**
 * @struct EvaluationSummary
 * @brief Métricas físicas y dictamen metrológico.
 */
struct EvaluationSummary
{
    bool hasEvaluation { false };
    std::string recommendedModelType;
    std::string selectionStatus;
    std::string canonicalEvaluationHash;
    double validationEsrDb { 0.0 };
    double validationCorrelation { 0.0 };
    double criteriaCompliancePercent { 0.0 };
    std::string validatedDomain;
    double relativeCpuCost { 1.0 };
    bool hashVerified { false };
};

/**
 * @struct LimitationsRecord
 * @brief Delimitación honesta y explícita del dominio de validez.
 */
struct LimitationsRecord
{
    std::vector<std::string> modeledAspects;
    std::vector<std::string> unmodeledAspects;
    std::string validityDomain;
    std::vector<std::string> extrapolationWarnings;
};

/**
 * @struct ExperimentRecord
 * @brief Contenedor canónico de un experimento completo en ABDAudioLab.
 */
struct ExperimentRecord
{
    int schemaVersion { 1 };
    std::string experimentId;
    uint32_t revision { 1 };
    std::optional<std::string> parentExperimentId;

    ExperimentKind kind { ExperimentKind::Measurement };
    ExperimentStatus status { ExperimentStatus::LoadedForExploration };
    std::string failureOrCorruptionReason;

    TargetIdentity target;
    CaptureSpec capture;
    ProvenanceRecord provenance;
    EvaluationSummary evaluation;
    LimitationsRecord limitations;

    std::vector<ExperimentArtifact> artifacts;

    [[nodiscard]] bool isCorrupt() const noexcept { return status == ExperimentStatus::Corrupt; }
    [[nodiscard]] bool isExportable() const noexcept
    {
        return !isCorrupt() && (status == ExperimentStatus::AuditedApproved || status == ExperimentStatus::AuditedWithWarnings);
    }
};

} // namespace abdaudiolab::core
