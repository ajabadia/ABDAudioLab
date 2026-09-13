#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <numbers>
#include <random>
#include <algorithm>
#include "MidiExcitationSequence.h"

namespace abdaudiolab::synth
{

/**
 * @brief Valores verdaderos (Ground Truth) sintetizados por el simulador de referencia.
 */
struct FixtureGroundTruth
{
    double transportLatencyMs { 3.42 };
    double intrinsicAttackMs { 15.0 };
    double decayMs { 247.0 };
    double sustainLevelDb { -6.10 };
    double releaseMs { 612.0 };
    double nominalPitchHz { 261.6256 }; // C4
    double centsDrift { 0.70 };
    double velocityGain { 1.0 };
    double noiseLevelDb { -180.0 };
    double transportJitterMs { 0.0 };
    bool isVelocitySensitive { true };
};

/**
 * @brief Traza temporal exacta de los eventos internos del simulador.
 */
struct FixtureEventTrace
{
    int scheduledNoteOnSample { 0 };
    int physicalNoteOnSample { 0 };
    int noteOffSample { 0 };
    int physicalOnsetSample { 0 };
    int peakSample { 0 };
    bool noteDropped { false };
    bool clippingTriggered { false };
};

/**
 * @brief Salida del renderizado del fixture determinista.
 */
struct FixtureRenderResult
{
    std::vector<float> audio;
    FixtureGroundTruth truth;
    FixtureEventTrace eventTrace;
};

/**
 * @brief Modos de inyección anómala y casos negativos para el fixture.
 */
enum class FixtureFaultMode
{
    None,                       /**< Modo nominal calibrado estándar. */
    NonGaussianJitter,          /**< Jitter bimodal o de salto grande dependiente del bloque. */
    SevereClipping,             /**< Saturación severa a > 0 dBFS. */
    DroppedNote,                /**< Nota perdida / dropout. */
    VelocityInsensitive,        /**< Ganancia fija (sin respuesta a velocity). */
    DurationAltersAttackAnomaly,/**< Violación de falsabilidad: el ataque varía con la duración. */
    ThermalPitchDrift,          /**< Desvío tonal inestable entre repeticiones. */
    HighNoiseFloor,             /**< Ruido analógico elevado (SNR deficiente). */
    FreeRunningPhase,           /**< Fase libre continua entre renders, reseteable con resetPhase(). */
    UnresettablePhase,          /**< Fase libre continua que ignora resetPhase(). */
    InterNoteResidualTail,      /**< Cola de señal residual tras el silencio de compuerta. */
    UnseededStochasticNoise,    /**< Ruido estocástico no reproducible. */
    SilentEffectPlugin,         /**< Sin salida ante notas (emulación de plugin de efecto). */
    AudioWithoutNoteResponse,   /**< Zumbido continuo no reactivo a notas MIDI. */
    StateCorruptionOnRestore    /**< Corrupción sutil de parámetros tras restore. */
};

/**
 * @brief Generador determinista de señales acústicas sintetizadas de referencia (Ground Truth).
 */
class SyntheticSynthFixture
{
public:
    SyntheticSynthFixture(double sampleRate = 96000.0, uint32_t seed = 42)
        : sampleRate_(sampleRate), seed_(seed), rng_(seed)
    {
    }

    void setSampleRate(double newRate) noexcept { sampleRate_ = newRate; }
    void setFaultMode(FixtureFaultMode mode) noexcept
    {
        faultMode_ = mode;
        stateBehavioralCorruptionActive_ = false;
    }
    void setGroundTruth(const FixtureGroundTruth& truth) noexcept { truth_ = truth; }
    [[nodiscard]] const FixtureGroundTruth& getGroundTruth() const noexcept { return truth_; }

    void resetPhase() noexcept
    {
        if (faultMode_ != FixtureFaultMode::UnresettablePhase)
        {
            accumulatedPhase_ = 0.0;
        }
    }

    void resetRng() noexcept
    {
        rng_.seed(seed_);
    }

    [[nodiscard]] std::vector<uint8_t> serializeState() const
    {
        // Serializar los parámetros clave de ground truth en un bloque binario determinista
        std::vector<uint8_t> data(sizeof(FixtureGroundTruth));
        std::memcpy(data.data(), &truth_, sizeof(FixtureGroundTruth));
        return data;
    }

    bool deserializeState(const std::vector<uint8_t>& data)
    {
        if (data.size() != sizeof(FixtureGroundTruth))
            return false;

        std::memcpy(&truth_, data.data(), sizeof(FixtureGroundTruth));

        if (faultMode_ == FixtureFaultMode::StateCorruptionOnRestore)
        {
            // La representación binaria es idéntica pero el comportamiento de renderizado discrepa
            stateBehavioralCorruptionActive_ = true;
        }
        return true;
    }

    /**
     * @brief Renderiza una toma experimental completa para una secuencia MIDI dada.
     */
    FixtureRenderResult renderSequence(const MidiExcitationSequence& sequence, int repetitionIndex = 0)
    {
        FixtureRenderResult res;
        res.truth = truth_;

        int totalSamples = static_cast<int>(std::lround(sequence.totalDurationSec * sampleRate_));
        res.audio.assign(static_cast<size_t>(totalSamples), 0.0f);

        if (faultMode_ == FixtureFaultMode::DroppedNote)
        {
            res.eventTrace.noteDropped = true;
            return res;
        }

        if (faultMode_ == FixtureFaultMode::SilentEffectPlugin)
        {
            return res; // Silencio absoluto
        }

        if (faultMode_ == FixtureFaultMode::AudioWithoutNoteResponse)
        {
            // Zumbido continuo a 120 Hz sin importar NoteOn/NoteOff
            double phase120 = 0.0;
            double inc120 = (2.0 * std::numbers::pi * 120.0) / sampleRate_;
            for (int i = 0; i < totalSamples; ++i)
            {
                phase120 += inc120;
                if (phase120 >= 2.0 * std::numbers::pi) phase120 -= 2.0 * std::numbers::pi;
                res.audio[static_cast<size_t>(i)] = static_cast<float>(0.25 * std::sin(phase120));
            }
            return res;
        }

        struct NoteSpan
        {
            int noteNumber { 60 };
            float velocity { 1.0f };
            int noteOnSample { 0 };
            int noteOffSample { 0 };
        };

        std::vector<NoteSpan> noteSpans;
        for (const auto& ev : sequence.events)
        {
            if (ev.type == TimedMidiType::NoteOn && ev.velocity > 0.0f)
            {
                NoteSpan span;
                span.noteNumber = ev.noteNumber;
                span.velocity = ev.velocity;
                span.noteOnSample = ev.sampleOffset;
                span.noteOffSample = totalSamples;
                noteSpans.push_back(span);
            }
            else if (ev.type == TimedMidiType::NoteOff || (ev.type == TimedMidiType::NoteOn && ev.velocity == 0.0f))
            {
                for (auto it = noteSpans.rbegin(); it != noteSpans.rend(); ++it)
                {
                    if (it->noteNumber == ev.noteNumber && it->noteOffSample == totalSamples)
                    {
                        it->noteOffSample = ev.sampleOffset;
                        break;
                    }
                }
            }
        }

        if (noteSpans.empty())
        {
            return res;
        }

        int noteOnSample = noteSpans[0].noteOnSample;
        int noteOffSample = noteSpans[0].noteOffSample;
        res.eventTrace.scheduledNoteOnSample = noteOnSample;
        res.eventTrace.noteOffSample = noteOffSample;

        // 1. Calcular retardo de transporte y jitter
        double latencyMs = truth_.transportLatencyMs;
        if (faultMode_ == FixtureFaultMode::NonGaussianJitter)
        {
            // Jitter bimodal: salta entre +0.5ms y +6.0ms
            latencyMs += (repetitionIndex % 2 == 0) ? 0.5 : 6.0;
        }
        else if (truth_.transportJitterMs > 0.0)
        {
            std::normal_distribution<double> jitDist(0.0, truth_.transportJitterMs);
            latencyMs += jitDist(rng_);
        }

        int latencySamples = std::max(0, static_cast<int>(std::lround((latencyMs / 1000.0) * sampleRate_)));
        int physicalNoteOn = noteOnSample + latencySamples;
        res.eventTrace.physicalNoteOnSample = physicalNoteOn;
        res.eventTrace.physicalOnsetSample = physicalNoteOn;

        // 2. Parámetros de envolvente
        double attackMs = truth_.intrinsicAttackMs;
        if (faultMode_ == FixtureFaultMode::DurationAltersAttackAnomaly)
        {
            // El ataque se estira anómalamente si la compuerta es larga
            attackMs *= (sequence.gateDurationSec > 0.5) ? 4.0 : 1.0;
        }

        int attackSamples = std::max(1, static_cast<int>(std::lround((attackMs / 1000.0) * sampleRate_)));
        int decaySamples = std::max(1, static_cast<int>(std::lround((truth_.decayMs / 1000.0) * sampleRate_)));
        double sustainLinear = std::pow(10.0, truth_.sustainLevelDb / 20.0);
        int releaseSamples = std::max(1, static_cast<int>(std::lround((truth_.releaseMs / 1000.0) * sampleRate_)));

        res.eventTrace.peakSample = physicalNoteOn + attackSamples;

        // 3. Preparar voces de notas activas
        bool isFreeRunning = (faultMode_ == FixtureFaultMode::FreeRunningPhase ||
                              faultMode_ == FixtureFaultMode::UnresettablePhase);

        struct PreparedNote
        {
            int noteOn { 0 };
            int noteOff { 0 };
            double gain { 1.0 };
            double phaseInc { 0.0 };
            double phase { 0.0 };
        };

        std::vector<PreparedNote> prepNotes;
        prepNotes.reserve(noteSpans.size());

        for (const auto& span : noteSpans)
        {
            PreparedNote pn;
            pn.noteOn = span.noteOnSample + latencySamples;
            pn.noteOff = span.noteOffSample + latencySamples;

            double targetGain = 1.0;
            if (truth_.isVelocitySensitive && faultMode_ != FixtureFaultMode::VelocityInsensitive)
            {
                targetGain = 0.2 + 0.8 * static_cast<double>(span.velocity);
            }
            pn.gain = targetGain;

            double currentPitchCents = truth_.centsDrift;
            if (faultMode_ == FixtureFaultMode::ThermalPitchDrift)
            {
                currentPitchCents += static_cast<double>(repetitionIndex) * 3.5;
            }
            if (stateBehavioralCorruptionActive_)
            {
                currentPitchCents += 35.0; // Desafinación deliberada tras restore corrupto
            }

            double noteFreqHz = truth_.nominalPitchHz * std::pow(2.0, (static_cast<double>(span.noteNumber) - 60.0) / 12.0);
            double effectiveFreqHz = noteFreqHz * std::pow(2.0, currentPitchCents / 1200.0);

            pn.phaseInc = (2.0 * std::numbers::pi * effectiveFreqHz) / sampleRate_;
            pn.phase = isFreeRunning ? accumulatedPhase_ : 0.0;

            prepNotes.push_back(pn);
        }

        if (!prepNotes.empty())
        {
            res.truth.velocityGain = prepNotes[0].gain;
        }

        // 4. Generadores de ruido
        double noiseLinear = 0.0;
        if (faultMode_ == FixtureFaultMode::HighNoiseFloor)
        {
            noiseLinear = std::pow(10.0, -30.0 / 20.0);
        }
        else if (truth_.noiseLevelDb > -120.0 && faultMode_ != FixtureFaultMode::None)
        {
            noiseLinear = std::pow(10.0, truth_.noiseLevelDb / 20.0);
        }
        std::normal_distribution<float> noiseDist(0.0f, static_cast<float>(noiseLinear));

        std::random_device rd;
        std::mt19937 unseededRng(rd());
        std::normal_distribution<float> unseededNoise(0.0f, 0.15f);

        // 5. Síntesis muestra a muestra con soporte de automatización de parámetros
        double currentCutoff = 1.0;
        size_t nextParamIdx = 0;

        for (int i = 0; i < totalSamples; ++i)
        {
            while (nextParamIdx < sequence.parameterEvents.size() &&
                   sequence.parameterEvents[nextParamIdx].sampleOffset <= i)
            {
                const auto& pev = sequence.parameterEvents[nextParamIdx];
                if (pev.normalizedParameterId == "filter_cutoff" || pev.nativeParameterId == "filter_cutoff")
                {
                    currentCutoff = pev.normalizedValue;
                }
                nextParamIdx++;
            }

            float totalSample = 0.0f;
            double maxEnv = 0.0;

            for (auto& pn : prepNotes)
            {
                if (!isFreeRunning && i == pn.noteOn)
                {
                    pn.phase = 0.0;
                }

                double env = 0.0;
                if (i >= pn.noteOn && i < pn.noteOn + attackSamples)
                {
                    env = static_cast<double>(i - pn.noteOn) / static_cast<double>(attackSamples);
                }
                else if (i >= pn.noteOn + attackSamples && i < pn.noteOff)
                {
                    int decayOffset = i - (pn.noteOn + attackSamples);
                    if (decayOffset < decaySamples)
                    {
                        double t = static_cast<double>(decayOffset) / static_cast<double>(decaySamples);
                        env = 1.0 - t * (1.0 - sustainLinear);
                    }
                    else
                    {
                        env = sustainLinear;
                    }
                }
                else if (i >= pn.noteOff && i < pn.noteOff + releaseSamples)
                {
                    double releaseOffset = static_cast<double>(i - pn.noteOff) / static_cast<double>(releaseSamples);
                    env = sustainLinear * std::max(0.0, 1.0 - releaseOffset);
                }
                else if (faultMode_ == FixtureFaultMode::InterNoteResidualTail && i >= pn.noteOff)
                {
                    // Cola residual de reverb/efecto que persiste 1.4 s
                    if (i < pn.noteOff + static_cast<int>(std::lround(1.40 * sampleRate_)))
                    {
                        env = 0.15;
                    }
                }

                if (env > maxEnv) maxEnv = env;

                pn.phase += pn.phaseInc;
                if (pn.phase >= 2.0 * std::numbers::pi) pn.phase -= 2.0 * std::numbers::pi;

                if (env > 1e-6)
                {
                    double h2 = 0.3 * currentCutoff;
                    double h3 = 0.15 * currentCutoff * currentCutoff;
                    double rawOsc = std::sin(pn.phase) + h2 * std::sin(pn.phase * 2.0) + h3 * std::sin(pn.phase * 3.0);
                    double osc = (rawOsc / 1.40) * 0.75;
                    totalSample += static_cast<float>(osc * env * pn.gain);
                }
            }

            if (noiseLinear > 0.0)
            {
                totalSample += noiseDist(rng_);
            }

            if (faultMode_ == FixtureFaultMode::UnseededStochasticNoise && maxEnv > 0.001)
            {
                totalSample += unseededNoise(unseededRng);
            }

            if (faultMode_ == FixtureFaultMode::SevereClipping && maxEnv > 0.5)
            {
                totalSample = (totalSample >= 0.0f) ? 1.0f : -1.0f;
                res.eventTrace.clippingTriggered = true;
            }

            res.audio[static_cast<size_t>(i)] = totalSample;
        }

        if (isFreeRunning && !prepNotes.empty())
        {
            accumulatedPhase_ = prepNotes.back().phase;
        }

        return res;
    }

private:
    double sampleRate_ { 96000.0 };
    uint32_t seed_ { 42 };
    std::mt19937 rng_;
    FixtureGroundTruth truth_;
    FixtureFaultMode faultMode_ { FixtureFaultMode::None };
    bool stateBehavioralCorruptionActive_ { false };
    double accumulatedPhase_ { 0.0 };
};

} // namespace abdaudiolab::synth
