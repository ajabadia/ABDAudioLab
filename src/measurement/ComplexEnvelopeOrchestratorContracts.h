/**
 * @file ComplexEnvelopeOrchestratorContracts.h
 * @brief Canonical data structures and interfaces for complex envelope capture orchestration and observable comparison.
 * @author ABDSynths
 * @date 2026
 *
 * Enforces strict metrological honesty:
 * 1. Sincronización medida con confianza y detección de ambigüedad.
 * 2. Captura inmutable en 2 capas (raw intocable y compensated).
 * 3. Comparación exclusivamente en espacio observable común (unidades compatibles).
 * 4. Etiquetado obligatorio "observable_agreement" (prohibido afirmar reconstrucción nativa de hardware).
 * 5. Desacoplamiento total del núcleo mediante la interfaz abstracta INativeStateProvider.
 */

#pragma once

#include "ComplexEnvelopeContracts.h"
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Explicit transform applied to align trajectories or buffers.
 */
enum class AlignmentTransform
{
    None,
    IntegerSampleShift,
    FractionalSampleShift,
    Interpolation,
    Rejected
};

[[nodiscard]] inline std::string alignmentTransformToString(AlignmentTransform t) noexcept
{
    switch (t)
    {
        case AlignmentTransform::None:                  return "None";
        case AlignmentTransform::IntegerSampleShift:    return "IntegerSampleShift";
        case AlignmentTransform::FractionalSampleShift: return "FractionalSampleShift";
        case AlignmentTransform::Interpolation:         return "Interpolation";
        case AlignmentTransform::Rejected:              return "Rejected";
    }
    return "Rejected";
}

/**
 * @brief Mechanism used to resolve the temporal origin of an envelope.
 */
enum class TimingReferenceType
{
    ProvidedEvent,       /**< Direct timestamp or sample offset from MIDI/host event */
    AudioOnset,          /**< Autonomous energy onset detected on acoustic waveform */
    LoopbackCorrelation, /**< Measured cross-correlation peak between stimulus and reference loopback */
    ManualMarker,        /**< Explicit user-supplied reference marker */
    Unresolved           /**< Insufficient signal or failed alignment */
};

[[nodiscard]] inline std::string timingReferenceTypeToString(TimingReferenceType t) noexcept
{
    switch (t)
    {
        case TimingReferenceType::ProvidedEvent:       return "provided_event";
        case TimingReferenceType::AudioOnset:          return "audio_onset";
        case TimingReferenceType::LoopbackCorrelation: return "loopback_correlation";
        case TimingReferenceType::ManualMarker:        return "manual_marker";
        case TimingReferenceType::Unresolved:          return "unresolved";
    }
    return "unresolved";
}

/**
 * @brief Specifies the domain where latency compensation is applied.
 */
enum class CompensationDomain
{
    TimeAxisOnly, /**< Latency shifts only the timestamp/frame grid without mutating audio samples */
    AudioSamples, /**< Latency shifts the sample index/buffer in an explicit compensated container */
    Both,         /**< Both time axis and compensated audio container are provided */
    NotApplied    /**< No compensation applied (zero latency or uncalibrated) */
};

[[nodiscard]] inline std::string compensationDomainToString(CompensationDomain d) noexcept
{
    switch (d)
    {
        case CompensationDomain::TimeAxisOnly: return "time_axis_only";
        case CompensationDomain::AudioSamples: return "audio_samples";
        case CompensationDomain::Both:         return "both";
        case CompensationDomain::NotApplied:   return "not_applied";
    }
    return "not_applied";
}

/**
 * @brief Structured outcome of temporal alignment resolution.
 */
struct TimingResolution
{
    TimingReferenceType type { TimingReferenceType::Unresolved };
    std::optional<int> offsetSamples;
    std::optional<double> offsetFractionalSamples;
    std::optional<double> confidence;
    std::optional<double> ambiguityMargin; /**< (R1 - R2) / R1. Ambiguous if <= 0.15 */
    std::optional<double> peakRatio;       /**< R2 / R1. Ambiguous if >= 0.85 */
    std::string status { "not_available" }; /**< "resolved", "ambiguous", "insufficient_signal", "not_available" */
    std::string methodVersion { "1.0.0" };
    std::string resolutionDetails;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        if (ambiguityMargin.has_value()) j["ambiguityMargin"] = *ambiguityMargin;
        else j["ambiguityMargin"] = nullptr;
        if (confidence.has_value()) j["confidence"] = *confidence;
        else j["confidence"] = nullptr;
        j["methodVersion"] = methodVersion;
        if (offsetFractionalSamples.has_value()) j["offsetFractionalSamples"] = *offsetFractionalSamples;
        else j["offsetFractionalSamples"] = nullptr;
        if (offsetSamples.has_value()) j["offsetSamples"] = *offsetSamples;
        else j["offsetSamples"] = nullptr;
        if (peakRatio.has_value()) j["peakRatio"] = *peakRatio;
        else j["peakRatio"] = nullptr;
        j["resolutionDetails"] = resolutionDetails;
        j["status"] = status;
        j["type"] = timingReferenceTypeToString(type);
        return j;
    }
};

/**
 * @brief Characterization of the audio interface / loopback chain latency and uncertainty.
 */
struct AudioChainCalibration
{
    double sampleRateHz { 48000.0 };
    int inputLatencySamples { 0 };
    int outputLatencySamples { 0 };
    int roundTripLatencySamples { 0 };
    double roundTripLatencyMs { 0.0 };
    std::optional<double> latencyUncertaintySamples;
    std::string uncertaintyStatus { "not_estimated" }; /**< "not_estimated", "evaluated" */
    std::string measurementMethod { "loopback_correlation" }; /**< "loopback_correlation", "driver_reported", "manual_override" */
    CompensationDomain compensationDomain { CompensationDomain::AudioSamples };
    std::string calibrationAudioSha256;
    std::string deviceIdentity;
    std::string driverIdentity;
    std::string calibrationId;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["calibrationAudioSha256"] = calibrationAudioSha256;
        j["calibrationId"] = calibrationId;
        j["compensationDomain"] = compensationDomainToString(compensationDomain);
        j["deviceIdentity"] = deviceIdentity;
        j["driverIdentity"] = driverIdentity;
        j["inputLatencySamples"] = inputLatencySamples;
        if (latencyUncertaintySamples.has_value()) j["latencyUncertaintySamples"] = *latencyUncertaintySamples;
        else j["latencyUncertaintySamples"] = nullptr;
        j["measurementMethod"] = measurementMethod;
        j["outputLatencySamples"] = outputLatencySamples;
        j["roundTripLatencyMs"] = roundTripLatencyMs;
        j["roundTripLatencySamples"] = roundTripLatencySamples;
        j["sampleRateHz"] = sampleRateHz;
        j["uncertaintyStatus"] = uncertaintyStatus;
        return j;
    }
};

/**
 * @brief Canonical description of an excitation stimulus.
 */
struct ComplexEnvelopeStimulus
{
    std::string stimulusId { "stimulus_001" };
    double nominalFrequencyHz { 261.6256 }; // C4
    int midiVelocity { 100 };
    double noteDurationMs { 1000.0 };
    double totalDurationMs { 2000.0 };
    size_t expectedNoteOnSample { 2400 };   // at 48 kHz = 50 ms
    size_t expectedNoteOffSample { 50400 }; // at 48 kHz = 1050 ms
    int repetitions { 1 };
    double intervalBetweenTakesMs { 500.0 };
    std::string stimulusAudioSha256;
    std::string triggerMode { "provided_event" }; // "provided_event", "audio_trigger", "manual_marker", "external_clock", "no_trigger"

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["expectedNoteOffSample"] = expectedNoteOffSample;
        j["expectedNoteOnSample"] = expectedNoteOnSample;
        j["intervalBetweenTakesMs"] = intervalBetweenTakesMs;
        j["midiVelocity"] = midiVelocity;
        j["nominalFrequencyHz"] = nominalFrequencyHz;
        j["noteDurationMs"] = noteDurationMs;
        j["repetitions"] = repetitions;
        j["stimulusAudioSha256"] = stimulusAudioSha256;
        j["stimulusId"] = stimulusId;
        j["totalDurationMs"] = totalDurationMs;
        j["triggerMode"] = triggerMode;
        return j;
    }
};

/**
 * @brief Detailed report comparing an observed trajectory with native intent in a compatible space.
 */
struct ObservableComparisonReport
{
    std::string observableDomain;            /**< "Pitch", "Timbre", "Amplitude" */
    std::string nativeParameterPath;         /**< e.g. "line1.dcw.envelope" */
    std::string comparisonStatus { "not_compared" }; /**< "not_compared", "compared", "inconclusive", "not_observable", "insufficient_alignment" */

    std::string comparisonSpace { "normalized_0_1" }; /**< Common compatible space */
    std::string normalizationMethod { "min_max" };    /**< "min_max", "peak_relative", "none" */
    std::string nativeTrajectoryDerivation;           /**< e.g. "declared_proxy", "direct_binding" */

    std::optional<double> onsetErrorMs;
    std::optional<double> peakTimeErrorMs;
    std::optional<double> durationErrorMs;

    std::optional<size_t> validPairCount;
    std::optional<double> coverageRatio;
    std::optional<double> meanAbsoluteError;
    std::optional<double> maxAbsoluteError;
    std::optional<double> trajectoryRmse;
    std::optional<double> correlation;       /**< Pearson r; nullopt if trajectory is constant (variance == 0) */

    AlignmentTransform alignmentTransform { AlignmentTransform::None };
    std::string sourceGridId;
    std::string targetGridId;
    int shiftSamples { 0 };
    std::string interpolationMethod { "linear" };

    std::string phaseDistortionProxy { "not_claimed" };
    std::string limitations;
    std::string comparisonLabel { "observable_agreement" }; /**< Strictly observable_agreement. PROHIBIDO: native_parameter_reconstruction */

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["alignmentTransform"] = alignmentTransformToString(alignmentTransform);
        j["comparisonLabel"] = comparisonLabel;
        j["comparisonSpace"] = comparisonSpace;
        j["comparisonStatus"] = comparisonStatus;
        if (correlation.has_value()) j["correlation"] = *correlation;
        else j["correlation"] = nullptr;
        if (coverageRatio.has_value()) j["coverageRatio"] = *coverageRatio;
        else j["coverageRatio"] = nullptr;
        if (durationErrorMs.has_value()) j["durationErrorMs"] = *durationErrorMs;
        else j["durationErrorMs"] = nullptr;
        j["interpolationMethod"] = interpolationMethod;
        j["limitations"] = limitations;
        if (maxAbsoluteError.has_value()) j["maxAbsoluteError"] = *maxAbsoluteError;
        else j["maxAbsoluteError"] = nullptr;
        if (meanAbsoluteError.has_value()) j["meanAbsoluteError"] = *meanAbsoluteError;
        else j["meanAbsoluteError"] = nullptr;
        j["nativeParameterPath"] = nativeParameterPath;
        j["nativeTrajectoryDerivation"] = nativeTrajectoryDerivation;
        j["normalizationMethod"] = normalizationMethod;
        j["observableDomain"] = observableDomain;
        if (onsetErrorMs.has_value()) j["onsetErrorMs"] = *onsetErrorMs;
        else j["onsetErrorMs"] = nullptr;
        if (peakTimeErrorMs.has_value()) j["peakTimeErrorMs"] = *peakTimeErrorMs;
        else j["peakTimeErrorMs"] = nullptr;
        j["phaseDistortionProxy"] = phaseDistortionProxy;
        j["shiftSamples"] = shiftSamples;
        j["sourceGridId"] = sourceGridId;
        j["targetGridId"] = targetGridId;
        if (trajectoryRmse.has_value()) j["trajectoryRmse"] = *trajectoryRmse;
        else j["trajectoryRmse"] = nullptr;
        if (validPairCount.has_value()) j["validPairCount"] = *validPairCount;
        else j["validPairCount"] = nullptr;
        return j;
    }
};

/**
 * @brief Abstract pure interface allowing the orchestrator to query native DUT bindings without coupling to any hardware or format.
 */
class INativeStateProvider
{
public:
    virtual ~INativeStateProvider() = default;

    /**
     * @brief Gets model identifier (e.g. "CZ-101", "Dexed", "Minimoog").
     */
    [[nodiscard]] virtual std::string getModelIdentifier() const = 0;

    /**
     * @brief Gets SHA-256 fixity hash of source patch or parameters.
     */
    [[nodiscard]] virtual std::string getStateSha256() const = 0;

    /**
     * @brief Returns registered bindings linking native parameter paths to observable domains.
     */
    [[nodiscard]] virtual std::vector<NativeEnvelopeBinding> getBindings() const = 0;

    /**
     * @brief Returns native stage descriptors for an envelope (e.g. 8 stages with Rate/Level).
     */
    [[nodiscard]] virtual std::vector<EnvelopeStageDescriptor> getNativeStageDescriptors(
        const std::string& nativePath) const = 0;

    /**
     * @brief Produces a physically justified observable reference trajectory in a declared space.
     * If no unique physically justifiable projection exists, returns std::nullopt.
     */
    [[nodiscard]] virtual std::optional<EnvelopeTrajectory> getObservableReference(
        const std::string& nativePath) const = 0;
};

/**
 * @brief Complete result of complex envelope orchestration and comparison.
 */
struct ComplexEnvelopeOrchestrationResult
{
    std::string orchestrationId;
    std::string status { "success" }; /**< "success", "partial", "insufficient_alignment", "failed" */
    TimingResolution timingResolution;
    std::optional<AudioChainCalibration> appliedCalibration;
    MultiDomainEnvelopeCaptureRecord captureRecord;
    std::vector<ObservableComparisonReport> comparisons;
    std::string sourceRawAudioSha256;
    std::string compensatedAudioSha256;
};

} // namespace abdaudiolab::measurement
