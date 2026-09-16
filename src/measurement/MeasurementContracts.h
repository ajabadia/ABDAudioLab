/**
 * @file MeasurementContracts.h
 * @brief Canonical data contracts for acoustic and response measurement framework.
 * @author ABDSynths
 * @date 2026
 *
 * Implements canonical schemas for Phase 20.10 Response Measurement Framework,
 * separating device type, deterministic stimulus, execution metadata,
 * formal observability status, strongly-typed metrics and curves, and FAIR traceability.
 */

#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace abdaudiolab::measurement
{

/**
 * @brief Device Under Test (DUT) classification for appropriate stimulus routing.
 */
enum class DeviceUnderTest
{
    audioEffect,        /**< Audio effect plugin: audio in -> audio out */
    instrument,         /**< Virtual synthesizer / instrument: MIDI -> audio out */
    hardwareAudioInOut, /**< Physical hardware unit: audio/MIDI -> audio capture */
    unknown
};

[[nodiscard]] inline std::string deviceUnderTestToString(DeviceUnderTest dut)
{
    switch (dut)
    {
        case DeviceUnderTest::audioEffect:        return "audioEffect";
        case DeviceUnderTest::instrument:         return "instrument";
        case DeviceUnderTest::hardwareAudioInOut: return "hardwareAudioInOut";
        case DeviceUnderTest::unknown:           return "unknown";
        default:                                  return "unknown";
    }
}

[[nodiscard]] inline DeviceUnderTest deviceUnderTestFromString(const std::string& str)
{
    if (str == "audioEffect")        return DeviceUnderTest::audioEffect;
    if (str == "instrument")         return DeviceUnderTest::instrument;
    if (str == "hardwareAudioInOut") return DeviceUnderTest::hardwareAudioInOut;
    return DeviceUnderTest::unknown;
}

/**
 * @brief Standardized excitation stimulus typology.
 */
enum class StimulusType
{
    logSineSweep,
    impulse,
    whiteNoise,
    pinkNoise,
    sineTone,
    multiTone,
    midiNote,
    midiNoteSequence,
    amplitudeRamp
};

[[nodiscard]] inline std::string stimulusTypeToString(StimulusType type)
{
    switch (type)
    {
        case StimulusType::logSineSweep:     return "logSineSweep";
        case StimulusType::impulse:          return "impulse";
        case StimulusType::whiteNoise:       return "whiteNoise";
        case StimulusType::pinkNoise:        return "pinkNoise";
        case StimulusType::sineTone:         return "sineTone";
        case StimulusType::multiTone:        return "multiTone";
        case StimulusType::midiNote:         return "midiNote";
        case StimulusType::midiNoteSequence: return "midiNoteSequence";
        case StimulusType::amplitudeRamp:    return "amplitudeRamp";
        default:                             return "unknown";
    }
}

[[nodiscard]] inline StimulusType stimulusTypeFromString(const std::string& str)
{
    if (str == "logSineSweep")     return StimulusType::logSineSweep;
    if (str == "impulse")          return StimulusType::impulse;
    if (str == "whiteNoise")       return StimulusType::whiteNoise;
    if (str == "pinkNoise")        return StimulusType::pinkNoise;
    if (str == "sineTone")         return StimulusType::sineTone;
    if (str == "multiTone")        return StimulusType::multiTone;
    if (str == "midiNote")         return StimulusType::midiNote;
    if (str == "midiNoteSequence") return StimulusType::midiNoteSequence;
    if (str == "amplitudeRamp")    return StimulusType::amplitudeRamp;
    return StimulusType::logSineSweep;
}

/**
 * @brief Execution outcome status of a measurement process.
 * NOTE: 'completed' denotes an observable measurement was obtained, NOT a quality verdict.
 */
enum class MeasurementStatus
{
    completed,   /**< Measurement executed and observable. */
    unreliable,  /**< Effect detected but signal does not satisfy certainty conditions (e.g. short gate). */
    invalid,     /**< Corrupt signal, excessive clipping, or invalid operational range. */
    skipped,     /**< Omitted due to missing or unsupported target control. */
    failed       /**< Audio execution or capture subsystem error. */
};

[[nodiscard]] inline std::string measurementStatusToString(MeasurementStatus status)
{
    switch (status)
    {
        case MeasurementStatus::completed:   return "completed";
        case MeasurementStatus::unreliable:  return "unreliable";
        case MeasurementStatus::invalid:     return "invalid";
        case MeasurementStatus::skipped:     return "skipped";
        case MeasurementStatus::failed:      return "failed";
        default:                             return "failed";
    }
}

[[nodiscard]] inline MeasurementStatus measurementStatusFromString(const std::string& str)
{
    if (str == "completed")   return MeasurementStatus::completed;
    if (str == "unreliable")  return MeasurementStatus::unreliable;
    if (str == "invalid")     return MeasurementStatus::invalid;
    if (str == "skipped")     return MeasurementStatus::skipped;
    if (str == "failed")      return MeasurementStatus::failed;
    return MeasurementStatus::failed;
}

/**
 * @brief Formal signal observability descriptor for distinguishing measurement outcome from signal fidelity.
 */
struct ObservabilityInfo
{
    std::string status { "observed" }; /**< "observed", "unreliable", "invalid", "not_observable" */
    std::optional<std::string> reason { std::nullopt }; /**< Explanatory diagnostic if not observed or unreliable */
};

/**
 * @brief Strongly-typed scalar metric with formal unit and individual observability status.
 */
struct MeasurementMetric
{
    juce::String name;
    double value { 0.0 };
    juce::String unit;
    juce::String status { "observed" }; /**< "observed", "unreliable", "invalid" */
};

/**
 * @brief Strongly-typed 2D measurement curve (e.g. time vs amplitude, frequency vs magnitude).
 */
struct MeasurementCurve
{
    juce::String xName;
    juce::String xUnit;
    juce::String yName;
    juce::String yUnit;
    std::vector<double> x;
    std::vector<double> y;
};

/**
 * @brief DUT physical or digital identity metadata.
 */
struct DutIdentity
{
    std::string name;
    std::string format;
    std::string version;
    std::string type; /**< "instrument", "audioEffect", "hardwareAudioInOut" */
};

/**
 * @brief Operational audio and capture execution metadata.
 */
struct ExecutionMetadata
{
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    int numChannels { 2 };
    int64_t numSamples { 0 };
    int latencySamples { 0 };
};

/**
 * @brief Calculation engine identifier and version.
 */
struct AnalyzerInfo
{
    std::string name;
    std::string version;
};

/**
 * @brief Deterministic excitation stimulus parameters.
 */
struct StimulusSpec
{
    StimulusType type { StimulusType::logSineSweep };
    float startFreqHz { 20.0f };
    float endFreqHz { 20000.0f };
    double durationSec { 2.0 };
    float levelDbfs { -6.0f };
    float phaseRad { 0.0f };
    uint32_t seed { 0x48271983 };

    // MIDI articulation parameters
    int midiChannel { 1 };
    int midiNoteNumber { 60 };
    float midiVelocity { 0.8f };
    size_t noteOnSample { 0 };
    size_t noteOffSample { 0 };

    std::string sha256;
};

/**
 * @brief Analytical parameters and algorithm tuning.
 */
struct AnalysisSpec
{
    std::string analysisType;
    std::vector<std::pair<std::string, std::string>> options;
};

/**
 * @brief Statistical repetition and settling parameters.
 */
struct RepetitionSpec
{
    int numPasses { 1 };
    double stabilizationWaitMs { 50.0 };
};

/**
 * @brief Persisted artifact references (audio WAV, SHA-256).
 */
struct MeasurementArtifacts
{
    std::string audioPath;
    std::string audioSha256;
};

/**
 * @brief Comprehensive specification defining what and how to measure.
 */
struct MeasurementSpec
{
    std::string schemaVersion { "response-measurement-1.0" };
    std::string schemaUri { "urn:abdaudiolab:response-measurement:1.0" };
    std::string measurementId;
    std::string measurementType; /**< "envelope", "filter", "modulation", "dynamics" */

    DeviceUnderTest dutType { DeviceUnderTest::unknown };
    std::string parameterId;
    std::string parameterName;

    ExecutionMetadata execution;
    StimulusSpec stimulus;
    AnalysisSpec analysis;
    RepetitionSpec repetition;
};

/**
 * @brief Complete immutable measurement result record with full execution metadata.
 */
struct MeasurementResult
{
    std::string schemaVersion { "response-measurement-1.0" };
    std::string schemaUri { "urn:abdaudiolab:response-measurement:1.0" };
    std::string measurementId;
    std::string measurementType;

    MeasurementStatus status { MeasurementStatus::failed };
    std::string reason;

    DutIdentity dut;
    ExecutionMetadata execution;
    StimulusSpec stimulus;
    AnalyzerInfo analyzer;
    ObservabilityInfo observability;

    std::vector<MeasurementMetric> metrics;
    MeasurementCurve curve;
    MeasurementArtifacts artifacts;

    bool integrityVerified { false };
};

} // namespace abdaudiolab::measurement
