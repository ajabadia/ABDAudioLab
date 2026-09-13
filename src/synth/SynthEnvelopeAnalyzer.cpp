#include "SynthEnvelopeAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace abdaudiolab::synth
{

EnvelopeMetrics SynthEnvelopeAnalyzer::analyzeEnvelope(const std::vector<float>& audioBuffer,
                                                       double sampleRate,
                                                       size_t noteOnSample,
                                                       size_t noteOffSample,
                                                       size_t detectedOnsetSample)
{
    EnvelopeMetrics result;

    if (audioBuffer.empty() || sampleRate <= 0.0 || noteOffSample <= noteOnSample)
    {
        result.attackTimeMs.status = MetricStatus::Invalid;
        result.decayTimeMs.status = MetricStatus::Invalid;
        result.sustainLevelDb.status = MetricStatus::Invalid;
        result.releaseTimeMs.status = MetricStatus::Invalid;
        return result;
    }

    // 1. Seguidor de envolvente por picos sin retardo de fase (ataque instantáneo, release 40ms para suavizar rizado inter-ciclo)
    std::vector<float> env(audioBuffer.size(), 0.0f);
    float relCoeff = static_cast<float>(std::exp(-1.0 / (sampleRate * 0.040)));
    float currEnv = 0.0f;
    for (size_t i = 0; i < audioBuffer.size(); ++i)
    {
        float absVal = std::abs(audioBuffer[i]);
        if (absVal > currEnv)
            currEnv = absVal;
        else
            currEnv = currEnv * relCoeff;
        env[i] = currEnv;
    }

    // 2. Localizar pico de amplitud durante la compuerta de nota
    size_t searchStart = std::min(detectedOnsetSample, audioBuffer.size() - 1);
    size_t searchEnd = std::min(noteOffSample + static_cast<size_t>(sampleRate * 0.02), audioBuffer.size());

    float maxVal = 0.0f;
    for (size_t i = searchStart; i < searchEnd; ++i)
    {
        if (env[i] > maxVal)
            maxVal = env[i];
    }

    if (maxVal < 1e-5f)
    {
        result.attackTimeMs.status = MetricStatus::Unreliable;
        result.decayTimeMs.status = MetricStatus::Unreliable;
        result.sustainLevelDb.status = MetricStatus::Unreliable;
        result.releaseTimeMs.status = MetricStatus::Unreliable;
        result.peakAmplitudeDbfs = -96.0;
        return result;
    }

    result.peakAmplitudeDbfs = 20.0 * std::log10(std::max(1e-5f, maxVal));

    // Localizar el primer pico del ataque (donde la envolvente alcanza >= 97% del maximo)
    float attackThreshold = maxVal * 0.97f;
    size_t peakIdx = searchStart;
    for (size_t i = searchStart; i < searchEnd; ++i)
    {
        if (env[i] >= attackThreshold)
        {
            peakIdx = i;
            while (peakIdx + 1 < searchEnd && env[peakIdx + 1] >= env[peakIdx])
            {
                peakIdx++;
            }
            break;
        }
    }

    // 3. Tiempo de ataque
    double attackMs = (peakIdx >= searchStart) ? ((static_cast<double>(peakIdx - searchStart) / sampleRate) * 1000.0) : 0.0;
    result.attackTimeMs.value = attackMs;
    result.attackTimeMs.unit = "ms";
    result.attackTimeMs.status = MetricStatus::Observed;

    // 4. Observabilidad de Decay y Sustain
    double gateDurationSec = static_cast<double>(noteOffSample - noteOnSample) / sampleRate;
    double postPeakMarginSec = (noteOffSample > peakIdx) ? (static_cast<double>(noteOffSample - peakIdx) / sampleRate) : 0.0;

    float sustainLinear = maxVal;
    if (gateDurationSec >= 0.120 && postPeakMarginSec >= 0.080)
    {
        size_t sustainWindowSamples = static_cast<size_t>(std::min(sampleRate * 0.10, static_cast<double>(noteOffSample - peakIdx) * 0.4));
        size_t sustainStart = (noteOffSample > sustainWindowSamples) ? (noteOffSample - sustainWindowSamples) : peakIdx;

        float sustainSum = 0.0f;
        int count = 0;
        for (size_t i = sustainStart; i < noteOffSample && i < env.size(); ++i)
        {
            sustainSum += env[i];
            count++;
        }

        sustainLinear = (count > 0) ? (sustainSum / static_cast<float>(count)) : maxVal;
        double sustainRatio = std::clamp(static_cast<double>(sustainLinear / maxVal), 1e-4, 1.0);
        double sustainDb = 20.0 * std::log10(sustainRatio);

        result.sustainLevelDb.value = sustainDb;
        result.sustainLevelDb.unit = "dB";
        result.sustainLevelDb.status = MetricStatus::EstimatedWithUncertainty;
    }
    else
    {
        result.sustainLevelDb.value = 0.0;
        result.sustainLevelDb.unit = "dB";
        result.sustainLevelDb.status = MetricStatus::NotObservableInGate;
        result.sustainLevelDb.note = "Compuerta de nota demasiado corta para estabilizar el nivel de sustain";
    }

    // Decay es observable si hay al menos 15 ms posteriores al pico antes del Note-Off
    if (postPeakMarginSec >= 0.015)
    {
        // El decaimiento termina cuando la envolvente desciende al nivel de sustain (+3% del salto)
        float decayTarget = sustainLinear + 0.03f * (maxVal - sustainLinear);
        size_t decayEndIdx = peakIdx;

        for (size_t i = peakIdx; i < noteOffSample && i < env.size(); ++i)
        {
            if (env[i] <= decayTarget)
            {
                decayEndIdx = i;
                break;
            }
        }

        double decayMs = (static_cast<double>(decayEndIdx - peakIdx) / sampleRate) * 1000.0;
        result.decayTimeMs.value = decayMs;
        result.decayTimeMs.unit = "ms";
        result.decayTimeMs.status = MetricStatus::EstimatedWithUncertainty;
    }
    else
    {
        result.decayTimeMs.value = 0.0;
        result.decayTimeMs.unit = "ms";
        result.decayTimeMs.status = MetricStatus::NotObservableInGate;
        result.decayTimeMs.note = "Compuerta de nota sin margen posterior al pico para observar decaimiento";
    }

    // 5. Release: comienza en el Note-Off real y concluye al descender al 4% del nivel de sostenido
    float releaseCutoff = std::max(1e-5f, sustainLinear * 0.04f);
    size_t releaseStart = std::min(noteOffSample, audioBuffer.size() - 1);
    size_t releaseEnd = audioBuffer.size();

    for (size_t i = releaseStart; i < audioBuffer.size(); ++i)
    {
        if (env[i] <= releaseCutoff)
        {
            releaseEnd = i;
            break;
        }
    }

    double releaseMs = (static_cast<double>(releaseEnd - releaseStart) / sampleRate) * 1000.0;
    result.releaseTimeMs.value = releaseMs;
    result.releaseTimeMs.unit = "ms";
    result.releaseTimeMs.status = MetricStatus::EstimatedWithUncertainty;

    return result;
}

} // namespace abdaudiolab::synth
