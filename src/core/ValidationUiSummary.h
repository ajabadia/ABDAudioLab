/**
 * @file ValidationUiSummary.h
 * @brief Structured, typed UI contract for holdout validation results and FAIR experiment audit.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <optional>

namespace abdaudiolab::core
{

// Forward declaration
struct ValidationReport;

/**
 * @struct ValidationUiSummary
 * @brief Decoupled, typed summary consumed by UI components (e.g. SoundIdResultsSummaryView).
 *        The UI never reads raw JSON or recalculates ESR, correlation, or latency offsets.
 */
struct ValidationUiSummary
{
    enum class Status
    {
        completed,
        error,
        corrupt,
        notExecuted
    };

    enum class Verdict
    {
        pass,
        passWithLimitations,
        fail,
        notAvailable
    };

    Status status { Status::notExecuted };
    Verdict verdict { Verdict::notAvailable };

    juce::String policy { "audio-ab-v1" };
    juce::String reason;
    juce::String errorCode;

    double esrDb { 0.0 };
    double correlation { 0.0 };
    int sampleOffset { 0 };

    juce::File targetFile;
    juce::File modelFile;
    juce::File residualFile;
    juce::File htmlReportFile;

    bool targetAvailable { false };
    bool modelAvailable { false };
    bool residualAvailable { false };
    bool htmlReportAvailable { false };
    bool integrityVerified { false };

    /**
     * @brief Parses an experiment directory, checking manifest integrity,
     *        SHA-256 fixity of validation artifacts, schema compatibility,
     *        and path boundaries within the experiment.
     */
    [[nodiscard]] static ValidationUiSummary fromExperimentFolder(const juce::File& experimentDir);

    /**
     * @brief Constructs a summary directly from an in-memory ValidationReport.
     */
    [[nodiscard]] static ValidationUiSummary fromValidationReport(const ValidationReport& report,
                                                                  const juce::File& experimentDir,
                                                                  bool integrityVerified);

    [[nodiscard]] static juce::String statusToString(Status s);
    [[nodiscard]] static juce::String verdictToString(Verdict v);
};

} // namespace abdaudiolab::core
