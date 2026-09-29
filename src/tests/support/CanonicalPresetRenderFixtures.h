#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace abdaudiolab::test::qa
{

enum class CanonicalPresetClass
{
    CleanReference,
    GentleModulation,
    AggressiveNonlinear,
    LowLevelDynamic,
    HighDensitySpectral
};

inline const char* getPresetClassName(CanonicalPresetClass cls) noexcept
{
    switch (cls)
    {
        case CanonicalPresetClass::CleanReference:      return "CleanReference";
        case CanonicalPresetClass::GentleModulation:    return "GentleModulation";
        case CanonicalPresetClass::AggressiveNonlinear: return "AggressiveNonlinear";
        case CanonicalPresetClass::LowLevelDynamic:     return "LowLevelDynamic";
        case CanonicalPresetClass::HighDensitySpectral: return "HighDensitySpectral";
    }
    return "Unknown";
}

struct CanonicalRunConfig
{
    std::string runId;
    std::string presetId;
    CanonicalPresetClass presetClass { CanonicalPresetClass::CleanReference };
    double sampleRate { 48000.0 };
    int blockSize { 256 };
    int midiNote { 60 }; // C4
    float velocity { 100.0f / 127.0f }; // 100 velocity
    double warmupDurationSec { 0.1 };   // 100 ms silent warmup
    double gateDurationSec { 1.0 };     // 1.0 s note sustain
    double tailDurationSec { 0.5 };     // 500 ms release tail
    uint64_t deterministicSeed { 0x5D0001ULL };

    [[nodiscard]] double getTotalDurationSec() const noexcept
    {
        return warmupDurationSec + gateDurationSec + tailDurationSec;
    }

    [[nodiscard]] int getTotalSamples() const noexcept
    {
        return static_cast<int>(std::ceil(getTotalDurationSec() * sampleRate));
    }
};

enum class EngineVariant
{
    ReferenceAnalytical, // Motor A: Referencia analítica de máxima precisión
    CandidateDSP         // Motor B: Variante candidata de implementación DSP
};

/**
 * @class CanonicalPresetSynthesizer
 * @brief Sintetizador software determinista in-memory para las 10 corridas de la Fase 5D.
 *
 * Implementa renderizado bloque a bloque sin asignaciones en el bucle de procesamiento,
 * garantizando determinismo estricto e invariante de repetibilidad intra-motor.
 */
class CanonicalPresetSynthesizer
{
public:
    CanonicalPresetSynthesizer() = default;

    void prepare(const CanonicalRunConfig& config, EngineVariant variant)
    {
        config_ = config;
        variant_ = variant;
        sampleRate_ = (config.sampleRate > 1000.0) ? config.sampleRate : 48000.0;
        reset();
    }

    void reset() noexcept
    {
        phase1_ = 0.0;
        phase2_ = 0.0;
        lfoPhase_ = 0.0;
        filterState1_ = 0.0f;
        filterState2_ = 0.0f;
        envStage_ = EnvStage::Idle;
        envLevel_ = 0.0f;
        rngState_ = config_.deterministicSeed;

        // Limpiar buffer circular de delay (para GentleModulation)
        delayBuffer_.assign(static_cast<size_t>(sampleRate_ * 0.05), 0.0f); // 50 ms max delay
        delayWritePos_ = 0;

        // Inicializar 6 voces para HighDensitySpectral
        for (int v = 0; v < 6; ++v)
        {
            voicePhases_[v] = 0.0;
            voiceSubPhases_[v] = 0.0;
            // Desafinación determinista por voz
            double detuneCents = (static_cast<double>(v) - 2.5) * 1.5;
            voiceDetuneRatios_[v] = std::pow(2.0, detuneCents / 1200.0);
        }
    }

    /**
     * @brief Renderiza un buffer de audio completo de forma determinista por bloques.
     */
    void renderComplete(juce::AudioBuffer<float>& outputBuffer)
    {
        const int totalSamples = config_.getTotalSamples();
        const int blockSize = config_.blockSize;
        outputBuffer.setSize(1, totalSamples);
        outputBuffer.clear();

        const int warmupSamples = static_cast<int>(config_.warmupDurationSec * sampleRate_);
        const int noteOnSample = warmupSamples;
        const int noteOffSample = noteOnSample + static_cast<int>(config_.gateDurationSec * sampleRate_);

        int samplesRemaining = totalSamples;
        int currentSampleOffset = 0;

        juce::AudioBuffer<float> blockBuffer(1, blockSize);

        while (samplesRemaining > 0)
        {
            const int currentBlockSize = std::min(samplesRemaining, blockSize);
            blockBuffer.setSize(1, currentBlockSize, false, false, true);
            blockBuffer.clear();

            float* writePtr = blockBuffer.getWritePointer(0);

            for (int i = 0; i < currentBlockSize; ++i)
            {
                const int globalSample = currentSampleOffset + i;

                // Eventos de control temporales
                if (globalSample == noteOnSample)
                {
                    triggerNoteOn();
                }
                else if (globalSample == noteOffSample)
                {
                    triggerNoteOff();
                }

                writePtr[i] = renderSample();
            }

            // Copiar el bloque al buffer global
            outputBuffer.copyFrom(0, currentSampleOffset, blockBuffer, 0, 0, currentBlockSize);

            currentSampleOffset += currentBlockSize;
            samplesRemaining -= currentBlockSize;
        }
    }

private:
    enum class EnvStage { Idle, Attack, Decay, Sustain, Release };

    CanonicalRunConfig config_;
    EngineVariant variant_ { EngineVariant::ReferenceAnalytical };
    double sampleRate_ { 48000.0 };

    double phase1_ { 0.0 };
    double phase2_ { 0.0 };
    double lfoPhase_ { 0.0 };
    float filterState1_ { 0.0f };
    float filterState2_ { 0.0f };

    EnvStage envStage_ { EnvStage::Idle };
    float envLevel_ { 0.0f };
    uint64_t rngState_ { 0x5D0001ULL };

    std::vector<float> delayBuffer_;
    size_t delayWritePos_ { 0 };

    double voicePhases_[6] { 0.0 };
    double voiceSubPhases_[6] { 0.0 };
    double voiceDetuneRatios_[6] { 1.0 };

    void triggerNoteOn() noexcept
    {
        envStage_ = EnvStage::Attack;
    }

    void triggerNoteOff() noexcept
    {
        envStage_ = EnvStage::Release;
    }

    [[nodiscard]] double nextRandomUniform() noexcept
    {
        // Generador LCG determinista rápido
        rngState_ = rngState_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(rngState_ >> 11) * (1.0 / 9007199254740992.0);
    }

    [[nodiscard]] float renderSample() noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        updateEnvelope();

        if (envLevel_ <= 1e-7f && envStage_ == EnvStage::Idle)
            return 0.0f;

        const double baseFreq = 440.0 * std::pow(2.0, (static_cast<double>(config_.midiNote) - 69.0) / 12.0);
        float sample = 0.0f;

        switch (config_.presetClass)
        {
            case CanonicalPresetClass::CleanReference:
            {
                // DCO Sierra pura con ganancia calibrada
                sample = renderSaw(phase1_, baseFreq);
                sample *= config_.velocity * 0.70794578f; // -3 dBFS
                break;
            }

            case CanonicalPresetClass::GentleModulation:
            {
                // Sierra + Sub (-6 dB) + BBD Chorus
                float saw = renderSaw(phase1_, baseFreq);
                float sub = renderSquare(phase2_, baseFreq * 0.5) * 0.5f;
                float dry = (saw + sub) * 0.6f * config_.velocity;

                // Filtro 4 kHz
                float filtered = applyLowPass(dry, 4000.0f, 0.25f);

                // Chorus BBD (retardo modulado por LFO 0.5 Hz)
                const double lfoFreq = 0.5;
                const double lfo = std::sin(lfoPhase_);
                lfoPhase_ += (2.0 * std::numbers::pi * lfoFreq) / sampleRate_;
                if (lfoPhase_ >= 2.0 * std::numbers::pi) lfoPhase_ -= 2.0 * std::numbers::pi;

                // Motor B añade una modesta dispersión de LFO de 0.05 Hz para caracterizar WARN de fase
                double delayMs = 4.0 + lfo * 1.5;
                if (variant_ == EngineVariant::CandidateDSP)
                {
                    delayMs = 4.0 + std::sin(lfoPhase_ * 1.02) * 1.5;
                }

                float wet = readDelayInterpolated(delayMs);
                writeDelay(filtered);

                sample = (filtered * 0.6f + wet * 0.4f);
                break;
            }

            case CanonicalPresetClass::AggressiveNonlinear:
            {
                // Dual Osc detuned + VCF resonante con saturación tanh
                double detuneRatio = (variant_ == EngineVariant::CandidateDSP) ? 1.0020 : 1.0018; // 3 cents detune
                float osc1 = renderSaw(phase1_, baseFreq);
                float osc2 = renderPulse(phase2_, baseFreq * detuneRatio, 0.35f);
                float mix = (osc1 + osc2) * 0.5f * config_.velocity;

                // Envolvente de filtro barriendo de 800 Hz a 4000 Hz
                float cutoff = 800.0f + envLevel_ * 3200.0f;
                float filtered = applyLowPassResonantNonlinear(mix, cutoff, 0.75f);

                // Saturación suave
                float drive = (variant_ == EngineVariant::CandidateDSP) ? 1.6f : 1.5f;
                sample = std::tanh(filtered * drive) / drive;
                break;
            }

            case CanonicalPresetClass::LowLevelDynamic:
            {
                // Seno a muy baja amplitud (-48 dBFS peak = 0.00398)
                double sine = std::sin(phase1_);
                phase1_ += (2.0 * std::numbers::pi * baseFreq) / sampleRate_;
                if (phase1_ >= 2.0 * std::numbers::pi) phase1_ -= 2.0 * std::numbers::pi;

                constexpr float kBaseAmp = 0.00398107f; // -48 dBFS
                sample = static_cast<float>(sine) * kBaseAmp * config_.velocity;

                // CandidateDSP introduce una micro-variación numérica de 0.005 dB para comprobar umbral SNR
                if (variant_ == EngineVariant::CandidateDSP)
                {
                    sample *= 0.9995f;
                }
                break;
            }

            case CanonicalPresetClass::HighDensitySpectral:
            {
                // Acorde de 6 voces con dispersión estocástica determinista
                const int chordIntervals[6] = { 0, 4, 7, 11, 14, 19 }; // C4, E4, G4, B4, D5, G5
                float chordSum = 0.0f;

                for (int v = 0; v < 6; ++v)
                {
                    double vFreq = baseFreq * std::pow(2.0, chordIntervals[v] / 12.0) * voiceDetuneRatios_[v];
                    if (variant_ == EngineVariant::CandidateDSP)
                    {
                        // Ligera variación en el tracking del candidato
                        vFreq *= (1.0 + (static_cast<double>(v) - 2.5) * 0.0002);
                    }

                    float vSaw = renderSaw(voicePhases_[v], vFreq);
                    float vSub = renderSquare(voiceSubPhases_[v], vFreq * 0.5) * 0.4f;
                    chordSum += (vSaw + vSub);
                }

                chordSum *= (1.0f / 3.8f) * config_.velocity; // Normalizado con margen seguro contra clipping
                sample = applyLowPass(chordSum, 3500.0f, 0.4f);
                break;
            }
        }

        return sample * envLevel_;
    }

    void updateEnvelope() noexcept
    {
        // Envolventes según clase
        float attackTime = 0.005f;
        float decayTime = 0.050f;
        float sustainLevel = 1.0f;
        float releaseTime = 0.050f;

        if (config_.presetClass == CanonicalPresetClass::GentleModulation)
        {
            attackTime = 0.010f;
            decayTime = 0.100f;
            sustainLevel = 0.85f;
            releaseTime = 0.150f;
        }
        else if (config_.presetClass == CanonicalPresetClass::AggressiveNonlinear)
        {
            attackTime = 0.002f;
            decayTime = 0.200f;
            sustainLevel = 0.60f;
            releaseTime = 0.100f;
        }
        else if (config_.presetClass == CanonicalPresetClass::LowLevelDynamic)
        {
            attackTime = 0.050f;
            decayTime = 0.500f;
            sustainLevel = 0.10f; // -20 dB sustain adicional
            releaseTime = 0.800f;
        }
        else if (config_.presetClass == CanonicalPresetClass::HighDensitySpectral)
        {
            attackTime = 0.001f;
            decayTime = 0.150f;
            sustainLevel = 0.70f;
            releaseTime = 0.200f;
        }

        const float dt = 1.0f / static_cast<float>(sampleRate_);

        switch (envStage_)
        {
            case EnvStage::Idle:
                envLevel_ = 0.0f;
                break;
            case EnvStage::Attack:
                envLevel_ += dt / attackTime;
                if (envLevel_ >= 1.0f)
                {
                    envLevel_ = 1.0f;
                    envStage_ = EnvStage::Decay;
                }
                break;
            case EnvStage::Decay:
                envLevel_ -= dt / decayTime * (1.0f - sustainLevel);
                if (envLevel_ <= sustainLevel)
                {
                    envLevel_ = sustainLevel;
                    envStage_ = EnvStage::Sustain;
                }
                break;
            case EnvStage::Sustain:
                envLevel_ = sustainLevel;
                break;
            case EnvStage::Release:
                envLevel_ -= dt / releaseTime * sustainLevel;
                if (envLevel_ <= 1e-7f)
                {
                    envLevel_ = 0.0f;
                    envStage_ = EnvStage::Idle;
                }
                break;
        }
    }

    [[nodiscard]] float renderSaw(double& phase, double freq) noexcept
    {
        float val = static_cast<float>(2.0 * (phase / (2.0 * std::numbers::pi)) - 1.0);
        phase += (2.0 * std::numbers::pi * freq) / sampleRate_;
        if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
        return val;
    }

    [[nodiscard]] float renderSquare(double& phase, double freq) noexcept
    {
        float val = (phase < std::numbers::pi) ? 1.0f : -1.0f;
        phase += (2.0 * std::numbers::pi * freq) / sampleRate_;
        if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
        return val;
    }

    [[nodiscard]] float renderPulse(double& phase, double freq, float width) noexcept
    {
        float val = (phase < 2.0 * std::numbers::pi * width) ? 1.0f : -1.0f;
        phase += (2.0 * std::numbers::pi * freq) / sampleRate_;
        if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
        return val;
    }

    [[nodiscard]] float applyLowPass(float input, float cutoffHz, float resonance) noexcept
    {
        float normCutoff = std::clamp(cutoffHz / static_cast<float>(sampleRate_ * 0.5), 0.01f, 0.99f);
        filterState1_ += normCutoff * (input - filterState1_ + resonance * (filterState1_ - filterState2_));
        filterState2_ += normCutoff * (filterState1_ - filterState2_);
        return filterState2_;
    }

    [[nodiscard]] float applyLowPassResonantNonlinear(float input, float cutoffHz, float resonance) noexcept
    {
        float normCutoff = std::clamp(cutoffHz / static_cast<float>(sampleRate_ * 0.5), 0.01f, 0.99f);
        float feedback = resonance * filterState2_;
        float driven = std::tanh(input - feedback);
        filterState1_ += normCutoff * (driven - filterState1_);
        filterState2_ += normCutoff * (filterState1_ - filterState2_);
        return filterState2_;
    }

    void writeDelay(float sample) noexcept
    {
        if (delayBuffer_.empty()) return;
        delayBuffer_[delayWritePos_] = sample;
        delayWritePos_ = (delayWritePos_ + 1) % delayBuffer_.size();
    }

    [[nodiscard]] float readDelayInterpolated(double delayMs) const noexcept
    {
        if (delayBuffer_.empty()) return 0.0f;
        double delaySamples = (delayMs * 0.001) * sampleRate_;
        double readPos = static_cast<double>(delayWritePos_) - delaySamples;
        while (readPos < 0.0) readPos += delayBuffer_.size();
        while (readPos >= delayBuffer_.size()) readPos -= delayBuffer_.size();

        size_t idx0 = static_cast<size_t>(readPos);
        size_t idx1 = (idx0 + 1) % delayBuffer_.size();
        float frac = static_cast<float>(readPos - static_cast<double>(idx0));

        return delayBuffer_[idx0] * (1.0f - frac) + delayBuffer_[idx1] * frac;
    }
};

} // namespace abdaudiolab::test::qa
