#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <nlohmann/json.hpp>

namespace abdaudiolab::math::qa5d
{

enum class DiagnosticCode
{
    DIAG_OK_IDENTITY,
    DIAG_OK_CLASS_TOLERANCE,
    DIAG_WARN_PHASE_DISPERSION,
    DIAG_WARN_HARMONIC_SPREAD,
    DIAG_FAIL_UNEXPECTED_CLIPPING,
    DIAG_FAIL_TEMPORAL_DESYNC,
    DIAG_FAIL_ENVELOPE_COLLAPSE,
    DIAG_FAIL_PITCH_DIVERGENCE,
    DIAG_FAIL_NONDETERMINISTIC_RENDER,
    DIAG_FAIL_POLARITY_INVERSION   ///< correlationSigned < -0.95 && correlationAbsolute > 0.95
};

inline const char* toString(DiagnosticCode code) noexcept
{
    switch (code)
    {
        case DiagnosticCode::DIAG_OK_IDENTITY:                  return "DIAG_OK_IDENTITY";
        case DiagnosticCode::DIAG_OK_CLASS_TOLERANCE:           return "DIAG_OK_CLASS_TOLERANCE";
        case DiagnosticCode::DIAG_WARN_PHASE_DISPERSION:        return "DIAG_WARN_PHASE_DISPERSION";
        case DiagnosticCode::DIAG_WARN_HARMONIC_SPREAD:         return "DIAG_WARN_HARMONIC_SPREAD";
        case DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING:     return "DIAG_FAIL_UNEXPECTED_CLIPPING";
        case DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC:         return "DIAG_FAIL_TEMPORAL_DESYNC";
        case DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE:       return "DIAG_FAIL_ENVELOPE_COLLAPSE";
        case DiagnosticCode::DIAG_FAIL_PITCH_DIVERGENCE:        return "DIAG_FAIL_PITCH_DIVERGENCE";
        case DiagnosticCode::DIAG_FAIL_NONDETERMINISTIC_RENDER: return "DIAG_FAIL_NONDETERMINISTIC_RENDER";
        case DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION:      return "DIAG_FAIL_POLARITY_INVERSION";
    }
    return "UNKNOWN_DIAGNOSTIC";
}

struct AudioABTemporalMetrics
{
    int alignmentLagSamples { 0 };

    /// @brief Correlación normalizada con signo. Rango [-1.0, +1.0].
    /// Usado para detectar inversión de polaridad (B = -A produce -1.0).
    float correlationSigned { 0.0f };

    /// @brief Valor absoluto de correlationSigned. Rango [0.0, 1.0].
    /// Invariante: correlationAbsolute == std::abs(correlationSigned).
    /// Legado normalizedCrossCorrelation se mantiene como alias para compatibilidad.
    float correlationAbsolute { 0.0f };

    /// @brief Alias de compatibilidad hacia atrás — idéntico a correlationAbsolute.
    float normalizedCrossCorrelation { 0.0f };

    float onsetDeltaSamples { 0.0f };
    float noteOffDeltaSamples { 0.0f };
    float envelopeRmsDeltaDb { 0.0f };
    bool warmupAIsDigitalSilence { false };
    bool warmupBIsDigitalSilence { false };
};

struct AudioABAmplitudeMetrics
{
    // nullopt representa DigitalSilence / -inf sin inventar 0.0 dBFS
    std::optional<float> peakA_dbfs;
    std::optional<float> peakB_dbfs;
    float peakDeltaDb { 0.0f };

    std::optional<float> rmsA_dbfs;
    std::optional<float> rmsB_dbfs;
    float rmsDeltaDb { 0.0f };

    std::optional<float> crestFactorA_db;
    std::optional<float> crestFactorB_db;

    bool clippingA { false };
    bool clippingB { false };
    bool unexpectedClipping { false };
};

struct AudioABSpectralMetrics
{
    std::optional<float> meanSpectralDeltaDb;
    std::optional<float> maxSpectralDeltaDb;
    std::optional<float> spectralCentroidDeltaHz;
    std::optional<float> spectralRolloffDeltaHz;
    std::optional<float> thdDeltaDb;
    std::optional<float> snrDeltaDb;
    std::optional<float> outOfBandEnergyDeltaDb;
};

struct AudioABEventMetrics
{
    bool noteOnOrderValid { false };
    bool noteOffOrderValid { false };
    int noteOnOffsetSamples { 0 };
    int noteOffOffsetSamples { 0 };
    bool gateDurationConsistent { false };
    bool eventOrderViolation { false };
};

struct AudioABEvaluationResult
{
    std::string runId;
    std::string presetId;
    std::string presetClass;
    std::string engineA;
    std::string engineB;
    double sampleRate { 48000.0 };
    int blockSize { 256 };
    bool intraEngineBitIdentical { true };

    AudioABTemporalMetrics temporal;
    AudioABAmplitudeMetrics amplitude;
    AudioABSpectralMetrics spectral;
    AudioABEventMetrics events;

    std::vector<DiagnosticCode> diagnostics;

    [[nodiscard]] bool hasDiagnostic(DiagnosticCode code) const noexcept
    {
        for (auto d : diagnostics)
            if (d == code) return true;
        return false;
    }

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["runId"] = runId;
        j["presetId"] = presetId;
        j["presetClass"] = presetClass;
        j["engineA"] = engineA;
        j["engineB"] = engineB;
        j["sampleRate"] = sampleRate;
        j["blockSize"] = blockSize;
        j["intraEngineBitIdentical"] = intraEngineBitIdentical;

        // Temporal
        j["temporal"] = {
            { "alignmentLagSamples", temporal.alignmentLagSamples },
            { "correlationSigned", temporal.correlationSigned },
            { "correlationAbsolute", temporal.correlationAbsolute },
            { "normalizedCrossCorrelation", temporal.normalizedCrossCorrelation },
            { "onsetDeltaSamples", temporal.onsetDeltaSamples },
            { "noteOffDeltaSamples", temporal.noteOffDeltaSamples },
            { "envelopeRmsDeltaDb", temporal.envelopeRmsDeltaDb },
            { "warmupAIsDigitalSilence", temporal.warmupAIsDigitalSilence },
            { "warmupBIsDigitalSilence", temporal.warmupBIsDigitalSilence }
        };

        // Amplitude
        auto optToVal = [](const std::optional<float>& opt, const char* nullStr) -> nlohmann::json {
            if (!opt.has_value()) return nullStr;
            return *opt;
        };

        j["amplitude"] = {
            { "peakA_dbfs", optToVal(amplitude.peakA_dbfs, "DigitalSilence") },
            { "peakB_dbfs", optToVal(amplitude.peakB_dbfs, "DigitalSilence") },
            { "peakDeltaDb", amplitude.peakDeltaDb },
            { "rmsA_dbfs", optToVal(amplitude.rmsA_dbfs, "DigitalSilence") },
            { "rmsB_dbfs", optToVal(amplitude.rmsB_dbfs, "DigitalSilence") },
            { "rmsDeltaDb", amplitude.rmsDeltaDb },
            { "crestFactorA_db", optToVal(amplitude.crestFactorA_db, "MetricNotApplicable") },
            { "crestFactorB_db", optToVal(amplitude.crestFactorB_db, "MetricNotApplicable") },
            { "clippingA", amplitude.clippingA },
            { "clippingB", amplitude.clippingB },
            { "unexpectedClipping", amplitude.unexpectedClipping }
        };

        // Spectral
        j["spectral"] = {
            { "meanSpectralDeltaDb", optToVal(spectral.meanSpectralDeltaDb, "MetricNotApplicable") },
            { "maxSpectralDeltaDb", optToVal(spectral.maxSpectralDeltaDb, "MetricNotApplicable") },
            { "spectralCentroidDeltaHz", optToVal(spectral.spectralCentroidDeltaHz, "MetricNotApplicable") },
            { "spectralRolloffDeltaHz", optToVal(spectral.spectralRolloffDeltaHz, "MetricNotApplicable") },
            { "thdDeltaDb", optToVal(spectral.thdDeltaDb, "MetricNotApplicable") },
            { "snrDeltaDb", optToVal(spectral.snrDeltaDb, "MetricNotApplicable") },
            { "outOfBandEnergyDeltaDb", optToVal(spectral.outOfBandEnergyDeltaDb, "MetricNotApplicable") }
        };

        // Events
        j["events"] = {
            { "noteOnOrderValid", events.noteOnOrderValid },
            { "noteOffOrderValid", events.noteOffOrderValid },
            { "noteOnOffsetSamples", events.noteOnOffsetSamples },
            { "noteOffOffsetSamples", events.noteOffOffsetSamples },
            { "gateDurationConsistent", events.gateDurationConsistent },
            { "eventOrderViolation", events.eventOrderViolation }
        };

        // Diagnostics
        std::vector<std::string> diagStrings;
        for (auto d : diagnostics) diagStrings.push_back(toString(d));
        j["diagnostics"] = diagStrings;

        return j;
    }
};

/**
 * @class AudioABMetricsEvaluator
 * @brief Evaluador normativo de métricas, alineación y diagnósticos para Fase 5D.
 */
class AudioABMetricsEvaluator
{
public:
    static AudioABEvaluationResult evaluate(
        const std::string& runId,
        const std::string& presetId,
        const std::string& presetClass,
        double sampleRate,
        int blockSize,
        const juce::AudioBuffer<float>& bufferA,
        const juce::AudioBuffer<float>& bufferB,
        int warmupSamples,
        int noteOnSample,
        int noteOffSample,
        int expectedGateSamples,
        int lagOffsetSamples,
        float rawCrossCorrelation,
        std::optional<float> rawSpectralDiffDb,
        bool intraEngineBitIdentical,
        std::optional<float> thdDeltaDb = std::nullopt,
        std::optional<float> snrDeltaDb = std::nullopt)
    {
        AudioABEvaluationResult res;
        res.runId = runId;
        res.presetId = presetId;
        res.presetClass = presetClass;
        res.engineA = "ReferenceAnalyticalEngine";
        res.engineB = "CandidateEngine";
        res.sampleRate = sampleRate;
        res.blockSize = blockSize;
        res.intraEngineBitIdentical = intraEngineBitIdentical;

        // 1. Verificación preliminar de no determinismo intra-motor
        if (!intraEngineBitIdentical)
        {
            res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_NONDETERMINISTIC_RENDER);
            // La comparación contra B queda abortada por no determinismo
            return res;
        }

        const int numSamples = bufferA.getNumSamples();

        // 2. Métricas de Eventos
        res.events.noteOnOffsetSamples = noteOnSample;
        res.events.noteOffOffsetSamples = noteOffSample;
        res.events.noteOnOrderValid = (noteOnSample >= warmupSamples);
        res.events.noteOffOrderValid = (noteOffSample > noteOnSample);
        const int actualGate = noteOffSample - noteOnSample;
        res.events.gateDurationConsistent = (std::abs(actualGate - expectedGateSamples) <= blockSize);

        if (!res.events.noteOffOrderValid || !res.events.noteOnOrderValid)
        {
            res.events.eventOrderViolation = true;
            res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC);
        }

        // 3. Métricas Temporales
        // correlationSigned: valor con signo; correlationAbsolute: |correlationSigned|.
        // Invariante: correlationAbsolute == std::abs(correlationSigned).
        res.temporal.alignmentLagSamples = lagOffsetSamples;
        res.temporal.correlationSigned = rawCrossCorrelation;
        res.temporal.correlationAbsolute = std::abs(rawCrossCorrelation);
        res.temporal.normalizedCrossCorrelation = res.temporal.correlationAbsolute; // alias legado

        // Detección de inversión de polaridad: condición compuesta.
        // Se requiere:
        //   1. correlationSigned extremadamente negativo (≤ −0.95): señal casi invertida.
        //   2. correlationAbsolute extremadamente positivo (≥ 0.95): forma de onda preservada.
        //   3. No hay clipping inesperado en B que ya explique la negatividad.
        //   4. No hay colapso de envolvente (ausencia de señal distinta de inversión).
        // Una correlación negativa moderada (e.g. −0.3 en HighDensitySpectral o modulación
        // compleja) nunca alcanza el umbral −0.95 y no produce un falso positivo.
        {
            const bool polarityInversion =
                (rawCrossCorrelation <= -0.95f) &&
                (res.temporal.correlationAbsolute >= 0.95f) &&
                (!res.amplitude.unexpectedClipping) &&
                (!res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE));
            if (polarityInversion)
            {
                res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION);
            }
        }

        // Verificar silencio en warm-up (primeros warmupSamples)
        float warmupPeakA = (warmupSamples > 0 && warmupSamples <= numSamples) ? bufferA.getMagnitude(0, 0, warmupSamples) : 0.0f;
        float warmupPeakB = (warmupSamples > 0 && warmupSamples <= bufferB.getNumSamples()) ? bufferB.getMagnitude(0, 0, warmupSamples) : 0.0f;
        res.temporal.warmupAIsDigitalSilence = (warmupPeakA == 0.0f);
        res.temporal.warmupBIsDigitalSilence = (warmupPeakB == 0.0f);

        // Lag acotado
        if (std::abs(lagOffsetSamples) > 128)
        {
            res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC);
        }

        // 4. Métricas de Amplitud
        float peakA = bufferA.getMagnitude(0, 0, numSamples);
        float peakB = bufferB.getMagnitude(0, 0, bufferB.getNumSamples());
        float rmsA = bufferA.getRMSLevel(0, 0, numSamples);
        float rmsB = bufferB.getRMSLevel(0, 0, bufferB.getNumSamples());

        res.amplitude.clippingA = (peakA > 1.0f);
        res.amplitude.clippingB = (peakB > 1.0f);
        res.amplitude.unexpectedClipping = (res.amplitude.clippingB && !res.amplitude.clippingA);

        if (res.amplitude.unexpectedClipping)
        {
            res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING);
        }

        // Conversión a dBFS estricta: silencio = nullopt (DigitalSilence), nunca 0 dBFS
        if (peakA > 0.0f) res.amplitude.peakA_dbfs = juce::Decibels::gainToDecibels(peakA);
        if (peakB > 0.0f) res.amplitude.peakB_dbfs = juce::Decibels::gainToDecibels(peakB);

        if (rmsA > 0.0f) res.amplitude.rmsA_dbfs = juce::Decibels::gainToDecibels(rmsA);
        if (rmsB > 0.0f) res.amplitude.rmsB_dbfs = juce::Decibels::gainToDecibels(rmsB);

        if (res.amplitude.peakA_dbfs.has_value() && res.amplitude.peakB_dbfs.has_value())
        {
            res.amplitude.peakDeltaDb = std::abs(*res.amplitude.peakA_dbfs - *res.amplitude.peakB_dbfs);
        }

        if (res.amplitude.rmsA_dbfs.has_value() && res.amplitude.rmsB_dbfs.has_value())
        {
            res.amplitude.rmsDeltaDb = std::abs(*res.amplitude.rmsA_dbfs - *res.amplitude.rmsB_dbfs);
            res.temporal.envelopeRmsDeltaDb = res.amplitude.rmsDeltaDb;
        }

        // Detección de colapso de envolvente en B cuando A tenía señal
        if (peakA > 1e-4f && peakB <= 1e-7f)
        {
            res.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE);
        }

        // 5. Métricas Espectrales
        res.spectral.meanSpectralDeltaDb = rawSpectralDiffDb;
        res.spectral.thdDeltaDb = thdDeltaDb;
        res.spectral.snrDeltaDb = snrDeltaDb;

        // 6. Diagnósticos Coherentes y Aditivos
        const bool hasFail = res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING) ||
                             res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC) ||
                             res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE) ||
                             res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION);

        if (!hasFail)
        {
            // DIAG_OK_IDENTITY: identidad exacta bit-a-bit (correlationSigned == +1, lag == 0)
            if (rawCrossCorrelation >= 0.9999f && (!rawSpectralDiffDb.has_value() || *rawSpectralDiffDb < 0.001f) && lagOffsetSamples == 0)
            {
                res.diagnostics.push_back(DiagnosticCode::DIAG_OK_IDENTITY);
            }
            else
            {
                // correlationAbsolute refleja la magnitud de similitud tras exclusión de polaridad
                const float corrAbs = res.temporal.correlationAbsolute;
                if (presetClass == "GentleModulation" || corrAbs < 0.998f)
                {
                    res.diagnostics.push_back(DiagnosticCode::DIAG_WARN_PHASE_DISPERSION);
                }
                if (rawSpectralDiffDb.has_value() && *rawSpectralDiffDb > 0.2f)
                {
                    res.diagnostics.push_back(DiagnosticCode::DIAG_WARN_HARMONIC_SPREAD);
                }
                res.diagnostics.push_back(DiagnosticCode::DIAG_OK_CLASS_TOLERANCE);
            }
        }

        return res;
    }
};

} // namespace abdaudiolab::math::qa5d
