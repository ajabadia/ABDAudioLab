/**
 * @file ModelHoldoutValidator.h
 * @brief Out-of-sample holdout A/B validation runner and FAIR persistence.
 * @author ABDSynths
 * @date 2026
 *
 * Coordinates full empirical model evaluation:
 * - Stimulus excitation through Target (plugin/hardware adapter).
 * - Stimulus excitation through GeneratedAcousticModel.
 * - Latency alignment via AudioABComparator cross-correlation.
 * - Dual metric recording: pre-alignment and post-alignment.
 * - Exact aligned residual calculation: r[n] = y_target[n + sampleOffset] - y_model[n].
 * - Versioned policy evaluation ("audio-ab-v1").
 * - Generation of validation/ artifacts (WAVs, JSONs) and manifest.json indexing with SHA-256 fixity.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include "GeneratedAcousticModel.h"
#include "HoldoutSequence.h"
#include "../math/AudioABComparator.h"
#include "../math/AudioABVerdictEngine.h"

namespace abdaudiolab::core
{

/**
 * @struct AudioSignalMetadata
 * @brief Technical metadata describing rendered audio signals.
 */
struct AudioSignalMetadata
{
    double sampleRate { 48000.0 };
    int numChannels { 1 };
    int bitDepth { 32 };
    int numSamples { 0 };
    double durationSeconds { 0.0 };
    std::string format { "PCM IEEE 32-bit float" };

    [[nodiscard]] nlohmann::json toJson() const;
};

/**
 * @struct AlignmentAndErrorMetrics
 * @brief Metrological metrics captured before and after cross-correlation alignment.
 */
struct AlignmentAndErrorMetrics
{
    float rmse { 0.0f };
    float correlationPeak { 0.0f };
    float rmsDeltaDb { 0.0f };
    float peakAbsoluteError { 0.0f };
    float esrDb { 0.0f };
    float spectralDeltaDb { 0.0f };

    [[nodiscard]] nlohmann::json toJson() const;
};

/**
 * @struct ValidationReport
 * @brief Complete validation report dataset persisted in validation_report.json.
 */
struct ValidationReport
{
    std::string schemaVersion { "audio-validation-report-1.0" };
    std::string schemaUri { "urn:abdaudio:audio-validation-report:1.0" };
    std::string reportType { "model-holdout-validation" };
    std::string reportId;
    std::string timestampUtc;
    std::string status { "completed" };
    std::string errorCode;
    std::string errorMessage;

    std::string verdictPolicy { "audio-ab-v1" };
    std::string policyDescription { "PASS within declared domain and tolerances, not universal acoustic perfection" };
    std::string verdict { "FAIL" };
    std::string reasonCode { "UNINITIALIZED" };

    int sampleOffset { 0 };
    AlignmentAndErrorMetrics preAlignment;
    AlignmentAndErrorMetrics postAlignment;

    AudioSignalMetadata audioMetadata;

    std::string trainingPlanHash;
    std::string holdoutPlanHash;
    std::string sequenceDefinitionHash;

    std::string targetWavSha256;
    std::string modelWavSha256;
    std::string residualWavSha256;
    std::string holdoutManifestSha256;

    [[nodiscard]] nlohmann::json toJson() const;
};

/**
 * @class ModelHoldoutValidator
 * @brief Automated runner executing holdout A/B validation against targets.
 */
class ModelHoldoutValidator
{
public:
    struct Config
    {
        int blockSize { 64 };
        int bitDepth { 32 };
        float passEsrDbMax { -28.0f };
        float passCorrelationMin { 0.98f };
        int passMaxLatencySamples { 256 };

        float passLimEsrDbMax { -18.0f };
        float passLimCorrelationMin { 0.92f };
        int passLimMaxLatencySamples { 1024 };
    };

    ModelHoldoutValidator();
    explicit ModelHoldoutValidator(Config config);
    ~ModelHoldoutValidator() = default;

    /**
     * @brief Executes out-of-sample holdout validation comparing target to model.
     *
     * @param targetRenderer Callable that renders the target plugin given stimulus audio.
     * @param model Prepared or configurable GeneratedAcousticModel.
     * @param sequence Canonical out-of-sample holdout sequence.
     * @param experimentFolder Destination experiment folder (where validation/ and manifest.json live).
     * @param outReport Output validation report with metrology and hashes.
     * @param outError Error message if validation fails.
     * @return true on successful execution and FAIR persistence, false on failure.
     */
    bool validate(std::function<bool(const float* inStim, float* outTarget, int numSamples, double sr, const HoldoutSequence& seq)> targetRenderer,
                  GeneratedAcousticModel& model,
                  const HoldoutSequence& sequence,
                  const juce::File& experimentFolder,
                  ValidationReport& outReport,
                  std::string& outError);

private:
    Config config_;

    static bool writeWavFile(const juce::File& destinationFile,
                             const float* channelData,
                             int numSamples,
                             double sampleRate,
                             int bitDepth);

    static float computeEsrDb(const float* target, const float* residual, int numSamples) noexcept;
};

} // namespace abdaudiolab::core
