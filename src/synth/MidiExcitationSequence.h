#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include "SynthPresetState.h"

namespace abdaudiolab::synth
{

/**
 * @brief Evento MIDI canónico muestra a muestra con payload binario exacto (Fase 20.11 T3.1).
 */
struct MidiExcitationEvent
{
    int32_t sampleOffset { 0 };
    std::vector<uint8_t> bytes;

    bool operator==(const MidiExcitationEvent& other) const noexcept
    {
        return sampleOffset == other.sampleOffset && bytes == other.bytes;
    }
};


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

    // --- Campos de Especificación Canónica (Fase 20.11 T3.1) ---
    double sampleRateHz { 0.0 };
    int32_t midiChannel { 1 };
    int32_t note { 60 };
    int32_t velocity { 100 };
    double noteDurationSec { 0.0 };
    double releaseTailSec { 0.0 };
    std::vector<MidiExcitationEvent> canonicalEvents;
    std::string canonicalSha256;

    /**
     * @brief Computa el hash canónico determinista del estímulo sobre una representación RFC 8785 inmutable.
     * Incluye: sampleRateHz, midiChannel, note, velocity, noteDurationSec, releaseTailSec y eventos con sampleOffset y bytes exactos.
     */
    std::string computeCanonicalSha256()
    {
        canonicalSha256 = Sha256::computeHex(serializeCanonicalJson());
        return canonicalSha256;
    }

    /**
     * @brief Serializa la secuencia de excitación a JSON canónico determinista RFC 8785.
     */
    [[nodiscard]] std::string serializeCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["midiChannel"] = midiChannel;
        j["note"] = note;
        j["noteDurationSec"] = noteDurationSec;
        j["releaseTailSec"] = releaseTailSec;
        j["sampleRateHz"] = sampleRateHz;
        j["velocity"] = velocity;

        nlohmann::ordered_json evs = nlohmann::ordered_json::array();
        for (const auto& ev : canonicalEvents)
        {
            nlohmann::ordered_json ej;
            ej["bytes"] = ev.bytes;
            ej["sampleOffset"] = ev.sampleOffset;
            evs.push_back(ej);
        }
        j["events"] = evs;

        return j.dump(); // Canónico compacto sin espacios
    }

    /**
     * @brief Deserializa la secuencia de excitación desde un JSON canónico.
     */
    static bool deserializeCanonicalJson(const std::string& jsonStr,
                                         MidiExcitationSequence& outSeq,
                                         std::string& outError)
    {
        try
        {
            auto j = nlohmann::json::parse(jsonStr);
            if (!j.is_object())
            {
                outError = "Root is not a JSON object";
                return false;
            }

            outSeq.midiChannel = j.value("midiChannel", 1);
            outSeq.note = j.value("note", 60);
            outSeq.noteDurationSec = j.value("noteDurationSec", 0.0);
            outSeq.releaseTailSec = j.value("releaseTailSec", 0.0);
            outSeq.sampleRateHz = j.value("sampleRateHz", 48000.0);
            outSeq.velocity = j.value("velocity", 100);

            // Sincronizar campos legados
            outSeq.channel = outSeq.midiChannel;
            outSeq.noteNumber = outSeq.note;
            outSeq.midiVelocity = outSeq.velocity;
            outSeq.normalizedVelocity = static_cast<float>(outSeq.velocity) / 127.0f;
            outSeq.gateDurationSec = outSeq.noteDurationSec;
            outSeq.postSilenceSec = outSeq.releaseTailSec;
            outSeq.totalDurationSec = outSeq.noteDurationSec + outSeq.releaseTailSec;

            outSeq.canonicalEvents.clear();
            outSeq.events.clear();

            if (j.contains("events") && j["events"].is_array())
            {
                for (const auto& ej : j["events"])
                {
                    MidiExcitationEvent ev;
                    ev.sampleOffset = ej.value("sampleOffset", int32_t(0));
                    if (ej.contains("bytes") && ej["bytes"].is_array())
                        ev.bytes = ej["bytes"].get<std::vector<uint8_t>>();
                    outSeq.canonicalEvents.push_back(ev);

                    // Mapear a TimedMidiEvent legado si es un NoteOn o NoteOff
                    if (ev.bytes.size() >= 3)
                    {
                        uint8_t statusNibble = ev.bytes[0] & 0xF0;
                        if (statusNibble == 0x90 && ev.bytes[2] > 0)
                        {
                            outSeq.events.push_back(TimedMidiEvent{
                                TimedMidiType::NoteOn,
                                (ev.bytes[0] & 0x0F) + 1,
                                ev.bytes[1],
                                static_cast<float>(ev.bytes[2]) / 127.0f,
                                ev.sampleOffset,
                                outSeq.sampleRateHz > 0.0 ? (double)ev.sampleOffset / outSeq.sampleRateHz * 1000.0 : 0.0
                            });
                        }
                        else if (statusNibble == 0x80 || (statusNibble == 0x90 && ev.bytes[2] == 0))
                        {
                            outSeq.events.push_back(TimedMidiEvent{
                                TimedMidiType::NoteOff,
                                (ev.bytes[0] & 0x0F) + 1,
                                ev.bytes[1],
                                0.0f,
                                ev.sampleOffset,
                                outSeq.sampleRateHz > 0.0 ? (double)ev.sampleOffset / outSeq.sampleRateHz * 1000.0 : 0.0
                            });
                        }
                    }
                }
            }

            outSeq.computeCanonicalSha256();
            outSeq.computeHash();
            return true;
        }
        catch (const std::exception& e)
        {
            outError = "JSON parse error in MidiExcitationSequence: " + std::string(e.what());
            return false;
        }
    }

    /**
     * @brief Construye un ensayo de excitación canónico con offsets de muestra exactos y bytes MIDI de 3 octetos.
     */
    static MidiExcitationSequence createCanonicalNoteTrial(double sampleRate,
                                                          int32_t channel,
                                                          int32_t midiNote,
                                                          int32_t midiVel,
                                                          double noteDurSec,
                                                          double releaseSec,
                                                          int32_t noteOnSampleOffset = 0)
    {
        MidiExcitationSequence seq;
        seq.sampleRateHz = sampleRate;
        seq.midiChannel = channel;
        seq.note = midiNote;
        seq.velocity = midiVel;
        seq.noteDurationSec = noteDurSec;
        seq.releaseTailSec = releaseSec;
        seq.totalDurationSec = noteDurSec + releaseSec;

        // Sincronizar legados
        seq.channel = channel;
        seq.noteNumber = midiNote;
        seq.midiVelocity = midiVel;
        seq.normalizedVelocity = static_cast<float>(midiVel) / 127.0f;
        seq.gateDurationSec = noteDurSec;
        seq.postSilenceSec = releaseSec;

        int32_t noteOnOffset = noteOnSampleOffset;
        int32_t noteOffOffset = noteOnOffset + static_cast<int32_t>(std::lround(noteDurSec * sampleRate));

        uint8_t statusOn = static_cast<uint8_t>(0x90 | ((channel - 1) & 0x0F));
        uint8_t statusOff = static_cast<uint8_t>(0x80 | ((channel - 1) & 0x0F));

        MidiExcitationEvent evOn;
        evOn.sampleOffset = noteOnOffset;
        evOn.bytes = { statusOn, static_cast<uint8_t>(midiNote & 0x7F), static_cast<uint8_t>(midiVel & 0x7F) };
        seq.canonicalEvents.push_back(evOn);

        MidiExcitationEvent evOff;
        evOff.sampleOffset = noteOffOffset;
        evOff.bytes = { statusOff, static_cast<uint8_t>(midiNote & 0x7F), 0 };
        seq.canonicalEvents.push_back(evOff);

        // Mapear eventos a events para ejecución directa en ExternalPluginFixture
        seq.events.push_back(TimedMidiEvent{
            TimedMidiType::NoteOn, channel, midiNote, seq.normalizedVelocity, noteOnOffset, (double)noteOnOffset / sampleRate * 1000.0
        });
        seq.events.push_back(TimedMidiEvent{
            TimedMidiType::NoteOff, channel, midiNote, 0.0f, noteOffOffset, (double)noteOffOffset / sampleRate * 1000.0
        });

        seq.computeCanonicalSha256();
        seq.computeHash();
        return seq;
    }
};

} // namespace abdaudiolab::synth
