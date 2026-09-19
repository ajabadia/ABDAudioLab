/**
 * @file ReportExportService.h
 * @brief Headless, deterministic domain and infrastructure service for session report export,
 *        metrics calculation, and atomic production package publishing.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

#include "../core/ProfilingSession.h"
#include "../core/SessionSerializer.h"
#include "../export/LutExporter.h"
#include "../dsp/LutEvaluatorSimd.h"

namespace abdaudiolab::exporting {

/**
 * @enum ReportExportStatus
 * @brief Strongly-typed outcomes for report export operations.
 */
enum class ReportExportStatus
{
    Success,
    InvalidRequest,
    MissingSessionData,
    CannotCreateDirectory,
    CannotWriteArtifact,
    IntegrityCheckFailed,
    UnsupportedFormat,
    GenerationFailed
};

/**
 * @enum ReportArtifactKind
 * @brief Categories of exported report artifacts.
 */
enum class ReportArtifactKind
{
    CertificationHtml,
    ProductionPackage,
    TelemetryJson,
    SessionManifestJson,
    CppLutHeader,
    AuditionLutGrid
};

/**
 * @enum PublishStage
 * @brief Discrete sequential states for atomic staging, backup, and publication.
 */
enum class PublishStage
{
    StagingCreated,
    ArtifactsGenerated,
    ArtifactsVerified,
    ExistingTargetBackedUp,
    StagingPublished,
    BackupRemoved
};

/**
 * @struct ReportExportOptions
 * @brief Configurable toggles specifying which artifact types to generate.
 */
struct ReportExportOptions
{
    bool includeHtmlCertification { true };
    bool includeCppLutHeader      { false };
    bool includeTelemetryJson     { false };
    bool includeProductionPackage { false };
    bool includeAuditionData      { false };
};

/**
 * @struct ReportContext
 * @brief Environmental and operational metadata for report export.
 */
struct ReportContext
{
    double sampleRate             { 44100.0 };
    std::string operatorNotes;
    double ambientTemperatureC   { 21.0 };
    double warmupTimeMinutes      { 15.0 };
    float inputTrimDb             { 0.0f };
};

/**
 * @struct ExportFaultInjectionHooks
 * @brief Controlled test injection hooks to deterministically verify rollback paths.
 */
struct ExportFaultInjectionHooks
{
    bool failStagingArtifactGeneration { false };
    bool failBackup                    { false };
    bool failPromote                   { false };
    bool failPostPublishVerification   { false };
    bool failCleanupBackup             { false };
};

/**
 * @struct ReportExportRequest
 * @brief Immutable domain snapshot containing all data required to execute an export.
 */
struct ReportExportRequest
{
    core::SessionManifest manifest;
    std::vector<MeasuredPoint> measuredPoints;
    ReportExportOptions options;
    ReportContext context;
    std::string baseFileName;
    std::filesystem::path destinationDirectory;
    ExportFaultInjectionHooks faultInjection;
};

/**
 * @struct ReportArtifact
 * @brief Metadata describing a successfully verified and published artifact.
 *        Note: publishedPath points strictly to the final published location, never internal staging.
 */
struct ReportArtifact
{
    ReportArtifactKind kind;
    std::filesystem::path publishedPath;
    std::uint64_t byteSize { 0 };
    std::string sha256;
};

/**
 * @struct CalculatedSessionMetrics
 * @brief Pure statistical summary derived from measured points.
 */
struct CalculatedSessionMetrics
{
    float avgSnrDb         { 0.0f };
    float noiseFloorDb     { 0.0f };
    float avgThdPercent    { 0.0f };
    int   validPointCount  { 0 };
    float totalDurationSec { 0.0f };
};

/**
 * @struct ReportExportResult
 * @brief Typed result returned by ReportExportService upon operation completion.
 */
struct ReportExportResult
{
    ReportExportStatus status { ReportExportStatus::InvalidRequest };
    std::vector<ReportArtifact> artifacts;
    CalculatedSessionMetrics metrics;
    std::filesystem::path primaryOutputPath;
    std::string manifestSha256;
    std::string errorCode;
    std::string userMessage;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == ReportExportStatus::Success;
    }
};

/**
 * @class ReportExportService
 * @brief Headless, deterministic domain and infrastructure service for report export.
 *
 * Provides pure mathematical transformations (metrics, audition grids, metadata)
 * alongside headless filesystem staging, fixity verification, and atomic commit.
 * Strictly free of any GUI dependencies.
 */
class ReportExportService
{
public:
    ReportExportService() = delete;

    // --- Pure Transformations (Domain Layer) ---

    /**
     * @brief Computes SNR, THD, noise floor, and duration metrics from measured points.
     */
    [[nodiscard]] static CalculatedSessionMetrics calculateMetrics(
        const std::vector<MeasuredPoint>& points,
        float inputTrimDb = 0.0f);

    /**
     * @brief Computes an 8x8 audition LUT table from measured points.
     */
    [[nodiscard]] static std::vector<dsp::AbdBatchedPoint> buildAuditionGrid(
        const std::vector<MeasuredPoint>& points,
        int gridSize = 8);

    /**
     * @brief Builds profiling metadata from export request.
     */
    [[nodiscard]] static core::ProfilingMetadata buildProfilingMetadata(
        const ReportExportRequest& request);

    /**
     * @brief Builds session manifest data from export request.
     */
    [[nodiscard]] static SessionManifestData buildManifestData(
        const ReportExportRequest& request);

    // --- Individual Artifact Writers (Infrastructure Layer) ---

    /**
     * @brief Writes telemetry JSON report to target path.
     */
    [[nodiscard]] static bool writeTelemetryJson(
        const std::filesystem::path& targetFile,
        const core::ProfilingMetadata& metadata,
        const std::vector<MeasuredPoint>& points);

    /**
     * @brief Writes session manifest JSON to target path.
     */
    [[nodiscard]] static bool writeSessionManifestJson(
        const std::filesystem::path& targetFile,
        const SessionManifestData& manifest,
        const std::vector<MeasuredPoint>& points);

    /**
     * @brief Writes C++ LUT constexpr header file to target path.
     */
    [[nodiscard]] static bool writeCppLutHeader(
        const std::filesystem::path& targetFile,
        const core::ProfilingMetadata& metadata,
        const std::string& tableName,
        const std::vector<MeasuredPoint>& points);

    /**
     * @brief Writes certification HTML report to target path.
     */
    [[nodiscard]] static bool writeCertificationHtml(
        const std::filesystem::path& targetFile,
        const SessionManifestData& manifest,
        const std::vector<MeasuredPoint>& points);

    // --- Headless Export Orchestration (Infrastructure Layer) ---

    /**
     * @brief Executes export according to the immutable request.
     * Uses atomic staging and SHA-256 verification before publishing to destinationDirectory.
     */
    [[nodiscard]] static ReportExportResult exportReport(
        const ReportExportRequest& request);

private:
    /**
     * @brief Sanitizes baseFileName preventing Windows reserved device names and invalid characters.
     */
    [[nodiscard]] static std::string sanitizeBaseFileName(
        const std::string& input,
        const std::string& fallbackDefault = "profile");

    /**
     * @brief Computes SHA-256 hash string for an existing file.
     */
    [[nodiscard]] static std::string computeFileSha256(
        const std::filesystem::path& filePath);
};

} // namespace abdaudiolab::exporting
