#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <cmath>
#include "MidiExcitationSequence.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Fiabilidad de la resta diferencial por pares y cancelación de fondo.
 */
enum class PairwiseSubtractionReliability
{
    PairedDeterministic,
    PairedAfterReset,
    StatisticalAverage,
    NotReliable
};

/**
 * @brief Clasificación de respuesta del target ante una rampa de parámetro.
 */
enum class RampProcessingResponse
{
    TransportedSampleAccurate,
    ProcessedBlockSmoothed,
    QuantizedSteps,
    Unresponsive
};

/**
 * @brief Tipo de evento en el flujo unificado del plan.
 */
enum class TargetEventType
{
    Midi,
    Parameter
};

/**
 * @brief Evento unificado despachable por el planificador hacia el target.
 */
struct TargetEvent
{
    TargetEventType eventType { TargetEventType::Midi };
    TimedMidiEvent midi;
    TimedParameterEvent parameter;
    int64_t absoluteSample { 0 };
    double scheduledTimeMs { 0.0 };
};

/**
 * @brief Ventana delimitada de observación acústica para evaluar el efecto de un control.
 */
struct ObservationWindow
{
    std::string windowId;
    int64_t startSample { 0 };
    int64_t endSample { 0 };
    double startTimeMs { 0.0 };
    double durationMs { 0.0 };
    std::string targetParameterId; /**< Parámetro cuyo efecto se monitoriza en esta ventana. */
    std::string domain;            /**< "Filter", "Oscillator", "Envelope", "Modulation". */
};

/**
 * @brief Política de tiempos de asentamiento y reposo entre ensayos.
 */
struct SettlingPolicy
{
    double preSilenceSec { 0.05 };
    double postSilenceSec { 0.50 };
    double interStepSettlingSec { 0.10 };
    bool enforceResetBeforeTrial { false };
};

/**
 * @brief Política de aleatoriedad, semillas y repeticiones.
 */
struct RandomizationPolicy
{
    bool isDeterministic { true };
    uint32_t randomSeed { 42 };
    int repetitionsPerCondition { 1 };
    bool useStatisticalAveraging { false };
    bool pairedTrialsRequired { true };
};

/**
 * @brief Plan canónico formal generado por una receta científica para ser ejecutado.
 */
struct ExperimentPlan
{
    std::string schemaVersion { "1.0.0" };
    std::string recipeId;
    std::string planId;
    std::string recipeType; // "NoteExcitation", "ParameterStep", "ParameterRamp", etc.

    double totalDurationSec { 0.0 };
    double sampleRate { 96000.0 };

    std::vector<TargetEvent> events;
    std::vector<ObservationWindow> windows;
    SettlingPolicy settling;
    RandomizationPolicy randomization;

    std::string planHash; // SHA-256 canónico del plan

    void computeHash()
    {
        std::string blob = schemaVersion + "\n"
                         + recipeId + "\n"
                         + planId + "\n"
                         + recipeType + "\n"
                         + std::to_string(totalDurationSec) + "\n"
                         + std::to_string(sampleRate) + "\n"
                         + std::to_string(events.size()) + "\n";
        for (const auto& ev : events)
        {
            if (ev.eventType == TargetEventType::Midi)
            {
                blob += "M:" + std::to_string(static_cast<int>(ev.midi.type)) + "@" + std::to_string(ev.absoluteSample) + "\n";
            }
            else
            {
                blob += "P:" + ev.parameter.normalizedParameterId + "=" + std::to_string(ev.parameter.normalizedValue) + "@" + std::to_string(ev.absoluteSample) + "\n";
            }
        }
        planHash = Sha256::computeHex(blob);
    }
};

/**
 * @brief Traza temporal de ejecución capturada por el despachador tras interactuar con el target.
 */
struct TargetExecutionTrace
{
    std::string planId;
    int repetitionIndex { 0 };
    bool resetExecutedBeforeTrial { false };
    double actualDurationSec { 0.0 };
    std::vector<TimedParameterEvent> executedParameterEvents;
    std::vector<TimedMidiEvent> executedMidiEvents;
    std::vector<float> capturedAudio;
    bool clippingDetected { false };
    double peakLevelDb { -180.0 };
    double rmsLevelDb { -180.0 };
    std::string traceHash;
};

/**
 * @brief Componentes de incertidumbre para rasgos acústicos individuales.
 */
struct FeatureUncertainty
{
    double spectralCentroidHz { 15.0 };
    double rmsDb { 0.5 };
    double pitchCents { 5.0 };
    double attackMs { 1.0 };
};

/**
 * @brief Presupuesto metrológico de incertidumbre durante la excitación.
 */
struct ExcitationUncertaintyBudget
{
    FeatureUncertainty auditUncertainty;
    struct {
        double jitterMs { 0.0 };
        double parameterResolution { 1e-4 };
        double settlingErrorPercent { 0.05 };
        double measurementNoiseFloorDb { -90.0 };
    } excitationUncertainty;
    FeatureUncertainty combinedUncertainty;

    void computeCombined()
    {
        combinedUncertainty.spectralCentroidHz = std::sqrt(auditUncertainty.spectralCentroidHz * auditUncertainty.spectralCentroidHz + 5.0 * 5.0);
        combinedUncertainty.rmsDb = std::sqrt(auditUncertainty.rmsDb * auditUncertainty.rmsDb + 0.2 * 0.2);
        combinedUncertainty.pitchCents = std::sqrt(auditUncertainty.pitchCents * auditUncertainty.pitchCents + 1.0 * 1.0);
        combinedUncertainty.attackMs = std::sqrt(auditUncertainty.attackMs * auditUncertainty.attackMs + 0.5 * 0.5);
    }
};

} // namespace abdaudiolab::synth
