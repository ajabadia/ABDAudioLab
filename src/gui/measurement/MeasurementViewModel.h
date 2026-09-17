/**
 * @file MeasurementViewModel.h
 * @brief Decoupled view model for measurement presentation in ABDAudioLab GUI.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../../measurement/MeasurementContracts.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::gui::measurement
{

enum class UiIntegrityStatus
{
    Unchecked,
    Verifying,
    Verified,
    Corrupt
};

struct MeasurementViewModel
{
    juce::String measurementId;
    juce::String measurementType; // "envelope", "filter", etc.
    juce::String dutName;
    juce::String dutFormat;

    // Metrological observability (Never PASS)
    abdaudiolab::measurement::MeasurementStatus measurementStatus { abdaudiolab::measurement::MeasurementStatus::failed };
    juce::String statusText { "FAILED" };
    juce::String statusIcon { "[!]" };
    juce::String diagnosticReason;

    // Metrological metadata
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    int latencySamples { 0 };
    juce::String analyzerName;
    juce::String analyzerVersion;

    juce::String filterTopology;
    juce::String measurementDomain;
    std::optional<abdaudiolab::measurement::SlopeFitMetadata> slopeFit;

    std::optional<abdaudiolab::measurement::DynamicResponseResult> dynamicsResult;
    std::optional<abdaudiolab::measurement::ModulationResultData> modulationResult;

    // Metrics & Curve
    std::vector<abdaudiolab::measurement::MeasurementMetric> metrics;
    abdaudiolab::measurement::MeasurementCurve curve;

    // FAIR Artifact paths
    juce::File containerDirectory;
    juce::File audioFile;               // Primary captured audio
    juce::File stimulusAudioFile;       // Sweep stimulus audio
    juce::File impulseResponseFile;     // Deconvolved IR audio
    juce::File curveFile;               // Primary curve JSON
    juce::File secondaryCurveFile;      // Secondary curve JSON (timbre or modulation spectrum)
    juce::File htmlReportFile;
    juce::File specFile;
    juce::File resultFile;

    // Integrity state
    UiIntegrityStatus integrityStatus { UiIntegrityStatus::Unchecked };
    juce::String expectedAudioSha256;
    juce::String expectedStimulusAudioSha256;
    juce::String expectedImpulseResponseSha256;
    juce::String integrityDiagnostic;

    [[nodiscard]] bool isPlaybackAllowed() const noexcept
    {
        return integrityStatus != UiIntegrityStatus::Corrupt &&
               measurementStatus != abdaudiolab::measurement::MeasurementStatus::invalid &&
               measurementStatus != abdaudiolab::measurement::MeasurementStatus::failed &&
               audioFile.existsAsFile();
    }

    [[nodiscard]] bool isStimulusPlaybackAllowed() const noexcept
    {
        return integrityStatus != UiIntegrityStatus::Corrupt &&
               stimulusAudioFile.existsAsFile();
    }

    [[nodiscard]] bool isImpulseResponsePlaybackAllowed() const noexcept
    {
        return integrityStatus != UiIntegrityStatus::Corrupt &&
               impulseResponseFile.existsAsFile();
    }
};

} // namespace abdaudiolab::gui::measurement
