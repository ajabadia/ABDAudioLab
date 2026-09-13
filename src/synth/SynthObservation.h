#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include <cstdint>
#include <juce_core/juce_core.h>
#include "SynthPresetState.h"

namespace abdaudiolab::synth
{

/**
 * @brief Estructura de entrega de bloques capturados en tiempo real.
 * Cero asignaciones en el hilo de audio: puntero a búfer preasignado y metadatos.
 */
struct CapturedAudioBlock
{
    const float* channelData { nullptr };
    std::size_t numSamples { 0 };
    std::uint64_t firstSampleIndex { 0 };
    std::uint64_t monotonicTimestampNs { 0 };
};

/**
 * @brief Estado de observabilidad metrológica de un rasgo acústico.
 */
enum class MetricStatus
{
    Observed,                   /**< Medición directa observable. */
    EstimatedWithUncertainty,   /**< Estimación cuantitativa con intervalo de incertidumbre formal. */
    NotObservableInGate,        /**< Parámetro no observable porque la compuerta se apagó antes. */
    Unreliable,                 /**< Señal con baja confianza (ruido, periodicidad deficiente). */
    Invalid                     /**< Condición corrupta o inválida. */
};

[[nodiscard]] inline std::string metricStatusToString(MetricStatus s)
{
    switch (s)
    {
        case MetricStatus::Observed:                 return "OBSERVED";
        case MetricStatus::EstimatedWithUncertainty: return "ESTIMATED";
        case MetricStatus::NotObservableInGate:      return "NOT_OBSERVABLE_IN_GATE";
        case MetricStatus::Unreliable:               return "UNRELIABLE";
        case MetricStatus::Invalid:                  return "INVALID";
        default:                                     return "UNKNOWN";
    }
}

/**
 * @brief Intervalo de confianza estadístico explícito.
 */
struct ConfidenceInterval
{
    double lower { 0.0 };
    double upper { 0.0 };
    double confidenceLevel { 0.95 };
    int sampleCount { 3 };
    int degreesOfFreedom { 2 };
    std::string method { "Student-t" };

    [[nodiscard]] double halfWidth() const noexcept { return std::abs(upper - lower) * 0.5; }
};

/**
 * @brief Envoltorio de valor con estado formal de observabilidad y dispersión estadística.
 */
template <typename T>
struct ObservableMetric
{
    T value {};
    MetricStatus status { MetricStatus::NotObservableInGate };
    double stdDev { 0.0 };
    ConfidenceInterval ci;
    std::string unit;
    std::string note;

    [[nodiscard]] bool isReliable() const noexcept
    {
        return status == MetricStatus::Observed || status == MetricStatus::EstimatedWithUncertainty;
    }
};

/**
 * @brief Veredicto formal del comportamiento acústico observado.
 */
enum class BehaviorValidationStatus
{
    Passed,
    Inconclusive,
    Rejected,
    InvalidMeasurement
};

[[nodiscard]] inline std::string behaviorValidationStatusToString(BehaviorValidationStatus status)
{
    switch (status)
    {
        case BehaviorValidationStatus::Passed:             return "PASSED";
        case BehaviorValidationStatus::Inconclusive:       return "INCONCLUSIVE";
        case BehaviorValidationStatus::Rejected:           return "REJECTED";
        case BehaviorValidationStatus::InvalidMeasurement: return "INVALID_MEASUREMENT";
        default:                                           return "UNKNOWN";
    }
}

/**
 * @brief Metrología temporal y separación explícita de offset de transporte.
 */
struct TimingMetrics
{
    double tMidiScheduledMs { 0.0 };
    double tMidiDispatchedMs { 0.0 };
    bool isMidiDispatchedTimeKnown { false };

    double tAudioOnsetMs { 0.0 };
    int64_t onsetSampleAbsolute { 0 };

    // Desglose de calibración de transporte
    double transportOffsetEstimateMs { 0.0 };
    double transportOffsetUncertaintyMs { 0.0 };
    double calibrationPresetIntrinsicOnsetMs { 0.0 };
    double onsetDetectorUncertaintyMs { 0.0 };

    ObservableMetric<double> netAttackMs;
    double timingJitterMs { 0.0 };
};

/**
 * @brief Resultado desacoplado del estimador de tono.
 */
struct PitchEstimate
{
    double frequencyHz { 0.0 };
    double centsError { 0.0 };
    double voicedConfidence { 0.0 };
    std::string estimatorId { "NSDF_Parabolic" };
    ConfidenceInterval uncertainty;
    MetricStatus status { MetricStatus::NotObservableInGate };
};

/**
 * @brief Métricas de afinación y estabilidad espectral.
 */
struct PitchMetrics
{
    double nominalFrequencyHz { 261.6256 }; // C4
    PitchEstimate estimate;
    double analysisWindowStartMs { 0.0 };
    double analysisWindowDurationMs { 0.0 };
};

/**
 * @brief Envolvente ADSR identificada y adaptada a la duración real de compuerta.
 */
struct EnvelopeMetrics
{
    ObservableMetric<double> attackTimeMs;
    ObservableMetric<double> decayTimeMs;
    ObservableMetric<double> sustainLevelDb;
    ObservableMetric<double> releaseTimeMs;
    double peakAmplitudeDbfs { -100.0 };
};

/**
 * @brief Integridad de señal acústica (clipping y relación señal/ruido).
 */
struct SignalQualityMetrics
{
    double snrDb { 0.0 };
    double noiseFloorDbfs { -96.0 };
    bool clippingDetected { false };
    int clippedSamplesCount { 0 };
};

/**
 * @brief Observación de una toma experimental individual.
 */
struct SynthTrialObservation
{
    int passIndex { 1 };
    std::string audioSha256Hash;
    TimingMetrics timing;
    PitchMetrics pitch;
    EnvelopeMetrics envelope;
    SignalQualityMetrics quality;
    bool isValid { true };
    std::string invalidReason;
};

/**
 * @brief Resumen estadístico de las N repeticiones para una condición fija (velocidad, duración).
 */
struct ConditionSummary
{
    int velocityByte { 64 };
    double gateDurationSec { 0.25 };
    int numPasses { 3 };
    int validPassesCount { 0 };

    std::vector<SynthTrialObservation> trials;

    ObservableMetric<double> netAttackMs;
    ObservableMetric<double> decayMs;
    ObservableMetric<double> sustainDb;
    ObservableMetric<double> releaseMs;
    ObservableMetric<double> pitchErrorCents;
    ObservableMetric<double> onsetRepeatabilityMs;
    double averageSnrDb { 0.0 };
    bool hasClipping { false };

    BehaviorValidationStatus conditionStatus { BehaviorValidationStatus::Passed };
    std::string diagnostics;
};

/**
 * @brief Ficha canónica y reporte global del experimento factorial de síntesis.
 */
struct SynthProfileReport
{
    std::string schemaVersion { kSynthSchemaVersion };
    std::string protocolVersion { kSynthProtocolVersion };
    std::string algorithmVersion { kSynthAlgorithmVersion };

    std::string presetId { "anchor_001" };
    std::string stateHash;
    std::string rawSysExHash;
    std::string normalizedParameterHash;
    std::string experimentHash; // SHA-256 de las condiciones ambientales/experimentales

    std::string stateValidation { "PASSED" };
    std::string behaviorValidation { "PASSED" };

    // Sincronización y transporte
    double midiAudioOffsetMs { 0.0 };
    double midiAudioOffsetStdDevMs { 0.0 };
    double onsetRepeatabilityMs { 0.0 };

    // Afinación
    double pitchMeanCents { 0.0 };
    double pitchStdDevCents { 0.0 };

    // Envolvente global representativa
    ObservableMetric<double> attackMs;
    ObservableMetric<double> decayMs;
    ObservableMetric<double> sustainDb;
    ObservableMetric<double> releaseMs;

    // Dominio
    int velocityMin { 40 };
    int velocityMax { 110 };
    double durationMinSec { 0.05 };
    double durationMaxSec { 2.0 };
    std::string velocitySensitivityStatus { "OBSERVED" };
    std::string durationInvarianceStatus { "VERIFIED_ATTACK_INVARIANT" };

    std::vector<ConditionSummary> conditions;
    std::vector<std::string> notesAndWarnings;

    [[nodiscard]] std::string formatCanonicalText() const
    {
        juce::String out;
        out << "Preset: " << presetId << "\n";
        out << "State validation: " << stateValidation << "\n";
        out << "Behavior validation: " << behaviorValidation << "\n";
        out << "MIDI/audio offset: " << juce::String(midiAudioOffsetMs, 2) << " ms \u00b1 "
            << juce::String(midiAudioOffsetStdDevMs, 2) << " ms\n";
        out << "Onset repeatability: " << juce::String(onsetRepeatabilityMs, 2) << " ms\n";
        out << "Pitch: " << (pitchMeanCents >= 0.0 ? "+" : "") << juce::String(pitchMeanCents, 1) << " cents \u00b1 "
            << juce::String(pitchStdDevCents, 1) << " cents\n";
        out << "Envelope:\n";
        out << "  attack: " << juce::String(attackMs.value, 1) << " ms \u00b1 "
            << juce::String(attackMs.stdDev, 1) << " ms\n";

        if (decayMs.status == MetricStatus::NotObservableInGate)
            out << "  decay: not observable\n";
        else
            out << "  decay: " << juce::String(decayMs.value, 0) << " ms \u00b1 "
                << juce::String(decayMs.stdDev, 0) << " ms\n";

        if (sustainDb.status == MetricStatus::NotObservableInGate)
            out << "  sustain: not observable\n";
        else
            out << "  sustain: " << juce::String(sustainDb.value, 1) << " dB \u00b1 "
                << juce::String(sustainDb.stdDev, 1) << " dB\n";

        out << "  release: " << juce::String(releaseMs.value, 0) << " ms \u00b1 "
            << juce::String(releaseMs.stdDev, 0) << " ms\n";

        out << "Domain:\n";
        out << "  velocity: " << velocityMin << ".." << velocityMax << "\n";
        out << "  duration: " << juce::String(durationMinSec * 1000.0, 0) << " ms.."
            << juce::String(durationMaxSec, 0) << " s\n";
        out << "  note range: documented\n";
        out << "  effects: disabled\n";

        return out.toStdString();
    }
};

} // namespace abdaudiolab::synth
