#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <juce_core/juce_core.h>
#include "SynthPresetState.h"

namespace abdaudiolab::synth
{

enum class TimedMidiType
{
    NoteOn,
    NoteOff,
    AllNotesOff,
    ResetControllers
};

struct TimedMidiEvent
{
    TimedMidiType type { TimedMidiType::NoteOn };
    int channel { 1 };
    int noteNumber { 60 };
    float velocity { 0.8f };       // [0.0 .. 1.0]
    int sampleOffset { 0 };         // Desplazamiento en muestras dentro del bloque de ensayo
    double scheduledTimeMs { 0.0 }; // Tiempo programado en milisegundos relativos al inicio del ensayo
};

enum class ParameterEventStatus
{
    Requested,           /**< Declarado por la receta científica. */
    AcceptedByHost,      /**< Validado por el host contra el contrato del target. */
    DispatchedToTarget,  /**< Enviado hacia la capa de transporte del target. */
    AppliedByTarget,     /**< Confirmado o aplicado por el motor del target. */
    ObservedInAudio,     /**< Verificado por análisis acústico con variación estadísticamente significativa. */
    Rejected             /**< Rechazado por estar fuera de rango, no existir o target bloqueado. */
};

enum class AppliedConfirmation
{
    Unconfirmed,         /**< Sin confirmación explícita (solo invocación ciega de transporte). */
    ConfirmedByAPI,      /**< Retorno exitoso o código de estado positivo de la API de control. */
    ConfirmedByReadback, /**< Lectura posterior de verificación del valor interno del target. */
    InferredFromAudio    /**< Confirmado indirectamente por la respuesta acústica observada. */
};

enum class ObservationOutcome
{
    Observed,                       /**< Efecto acústico verificable por encima del umbral de incertidumbre. */
    NotObservedInCurrentCondition, /**< Parámetro aplicado pero inaudible en el preset/condición actual. */
    Inconclusive,                   /**< Incertidumbre o ruido en la toma impiden una conclusión firme. */
    Rejected                        /**< Datos inválidos, clipping o alteración no atribuible al parámetro. */
};

enum class TransportAccuracy
{
    SampleAccurate,   /**< Modulación muestra a muestra exacta dentro del bloque. */
    BlockAccurate,    /**< Modulación cuantizada al borde del bloque de audio. */
    Timestamped,      /**< Timestamp MIDI/SysEx con jitter de transmisión. */
    BestEffort,       /**< Envío asíncrono sin sincronismo garantizado. */
    Unknown
};

/**
 * @brief Evento temporizado de automatización de parámetro.
 */
struct TimedParameterEvent
{
    std::string normalizedParameterId;   /**< Identificador canónico (ej. "filter_cutoff"). */
    std::string nativeParameterId;       /**< Identificador nativo del target (ej. "Param_37" o "VCF Cutoff"). */
    double normalizedValue { 0.0 };      /**< Valor objetivo en escala [0.0 .. 1.0]. */
    int64_t absoluteSample { 0 };        /**< Índice absoluto de muestra en el plan global de ensayo. */
    int sampleOffset { 0 };              /**< Desplazamiento en muestras dentro del bloque de audio. */
    double scheduledTimeMs { 0.0 };      /**< Tiempo programado en milisegundos. */

    TransportAccuracy transportAccuracy { TransportAccuracy::SampleAccurate };
    ParameterEventStatus status { ParameterEventStatus::Requested };
    AppliedConfirmation appliedConfirmation { AppliedConfirmation::Unconfirmed };
    ObservationOutcome observationOutcome { ObservationOutcome::Inconclusive };
    std::string statusDetails;
};

/**
 * @brief Secuencia reproducible de excitación MIDI con trazabilidad SHA-256 canónica.
 */
struct MidiExcitationSequence
{
    std::string schemaVersion { kSynthSchemaVersion };
    std::string sequenceId;
    int channel { 1 };
    int noteNumber { 60 };
    int midiVelocity { 64 };
    float normalizedVelocity { 0.5f };
    double gateDurationSec { 0.25 };
    double preSilenceSec { 0.05 };
    double postSilenceSec { 0.50 };
    double totalDurationSec { 0.80 };

    std::vector<TimedMidiEvent> events;
    std::vector<TimedParameterEvent> parameterEvents;
    std::string sequenceHash; // SHA-256 canónico de la secuencia de eventos

    static MidiExcitationSequence createNoteTrial(int channel,
                                                  int noteNumber,
                                                  int velocityByte,
                                                  double gateSec,
                                                  double sampleRate,
                                                  double preSilenceSec = 0.05,
                                                  double postSilenceSec = 0.50)
    {
        MidiExcitationSequence seq;
        seq.channel = channel;
        seq.noteNumber = noteNumber;
        seq.midiVelocity = juce::jlimit(1, 127, velocityByte);
        seq.normalizedVelocity = static_cast<float>(seq.midiVelocity) / 127.0f;
        seq.gateDurationSec = gateSec;
        seq.preSilenceSec = preSilenceSec;
        seq.postSilenceSec = postSilenceSec;
        seq.totalDurationSec = preSilenceSec + gateSec + postSilenceSec;

        seq.sequenceId = "SEQ_N" + std::to_string(noteNumber)
                       + "_V" + std::to_string(seq.midiVelocity)
                       + "_G" + std::to_string(static_cast<int>(std::lround(gateSec * 1000.0))) + "MS";

        int noteOnSample = static_cast<int>(std::lround(preSilenceSec * sampleRate));
        int noteOffSample = noteOnSample + static_cast<int>(std::lround(gateSec * sampleRate));

        // Evento 1: Reset previo
        seq.events.push_back({
            TimedMidiType::AllNotesOff,
            channel,
            noteNumber,
            0.0f,
            0,
            0.0
        });

        // Evento 2: Note On
        seq.events.push_back({
            TimedMidiType::NoteOn,
            channel,
            noteNumber,
            seq.normalizedVelocity,
            noteOnSample,
            preSilenceSec * 1000.0
        });

        // Evento 3: Note Off
        seq.events.push_back({
            TimedMidiType::NoteOff,
            channel,
            noteNumber,
            0.0f,
            noteOffSample,
            (preSilenceSec + gateSec) * 1000.0
        });

        // Evento 4: All-Notes-Off al final
        int endSample = static_cast<int>(std::lround(seq.totalDurationSec * sampleRate));
        seq.events.push_back({
            TimedMidiType::AllNotesOff,
            channel,
            noteNumber,
            0.0f,
            endSample,
            seq.totalDurationSec * 1000.0
        });

        seq.computeHash();
        return seq;
    }

    void computeHash()
    {
        std::string blob = schemaVersion + "\n"
                         + sequenceId + "\n"
                         + "CH=" + std::to_string(channel) + "\n"
                         + "NOTE=" + std::to_string(noteNumber) + "\n"
                         + "VEL=" + std::to_string(midiVelocity) + "\n"
                         + "GATE=" + std::to_string(gateDurationSec) + "\n";
        for (const auto& ev : events)
        {
            blob += std::to_string(static_cast<int>(ev.type)) + "@" + std::to_string(ev.sampleOffset) + "\n";
        }
        for (const auto& pev : parameterEvents)
        {
            blob += "P:" + pev.normalizedParameterId + "=" + std::to_string(pev.normalizedValue) + "@" + std::to_string(pev.sampleOffset) + "\n";
        }
        sequenceHash = Sha256::computeHex(blob);
    }
};

} // namespace abdaudiolab::synth
