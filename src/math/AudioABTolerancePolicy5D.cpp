#include "AudioABTolerancePolicy5D.h"
#include <cmath>

namespace abdaudiolab::math::qa5d
{

AudioABTolerancePolicy5D AudioABTolerancePolicy5D::getDefaultProvisionalPolicy()
{
    AudioABTolerancePolicy5D policy;
    policy.policyId = "audio-ab-5d-provisional-v1";
    policy.version = "1.0.0";
    policy.calibrationStatus = "ProvisionalEvidenceBased";
    policy.sourceBuild = "Build #513";
    policy.effectiveFrom = "2026-09-29";
    policy.reviewRequiredBefore = "HITO-AUDIO-AB-5D.8";

    // 1. CleanReference
    AudioABClassTolerance cleanTol;
    cleanTol.passMinimumCorrelation = 0.9999f;
    cleanTol.warnMinimumCorrelation = 0.9990f;
    cleanTol.passMaximumMeanSpectralDeltaDb = 0.01f;
    cleanTol.warnMaximumMeanSpectralDeltaDb = 0.05f;
    cleanTol.passMaximumRmsDeltaDb = 0.05f;
    cleanTol.warnMaximumRmsDeltaDb = 0.10f;
    cleanTol.passMaximumAbsoluteLagSamples = 0;
    cleanTol.warnMaximumAbsoluteLagSamples = 1;
    policy.classTolerances["CleanReference"] = cleanTol;

    // 2. GentleModulation
    AudioABClassTolerance modTol;
    modTol.passMinimumCorrelation = 0.9950f;
    modTol.warnMinimumCorrelation = 0.9900f;
    modTol.passMaximumMeanSpectralDeltaDb = 0.25f;
    modTol.warnMaximumMeanSpectralDeltaDb = 0.50f;
    modTol.passMaximumRmsDeltaDb = 0.20f;
    modTol.warnMaximumRmsDeltaDb = 0.40f;
    modTol.passMaximumAbsoluteLagSamples = 1;
    modTol.warnMaximumAbsoluteLagSamples = 4;
    policy.classTolerances["GentleModulation"] = modTol;

    // 3. AggressiveNonlinear
    AudioABClassTolerance nonlinTol;
    nonlinTol.passMinimumCorrelation = 0.9850f;
    nonlinTol.warnMinimumCorrelation = 0.9750f;
    nonlinTol.passMaximumMeanSpectralDeltaDb = 0.60f;
    nonlinTol.warnMaximumMeanSpectralDeltaDb = 1.00f;
    nonlinTol.passMaximumRmsDeltaDb = 0.30f;
    nonlinTol.warnMaximumRmsDeltaDb = 0.60f;
    nonlinTol.passMaximumAbsoluteLagSamples = 4;
    nonlinTol.warnMaximumAbsoluteLagSamples = 16;
    policy.classTolerances["AggressiveNonlinear"] = nonlinTol;

    // 4. LowLevelDynamic
    AudioABClassTolerance lowTol;
    lowTol.passMinimumCorrelation = 0.9995f;
    lowTol.warnMinimumCorrelation = 0.9990f;
    lowTol.passMaximumMeanSpectralDeltaDb = 0.01f;
    lowTol.warnMaximumMeanSpectralDeltaDb = 0.05f;
    lowTol.passMaximumRmsDeltaDb = 0.05f;
    lowTol.warnMaximumRmsDeltaDb = 0.10f;
    lowTol.passMaximumAbsoluteLagSamples = 0;
    lowTol.warnMaximumAbsoluteLagSamples = 1;
    policy.classTolerances["LowLevelDynamic"] = lowTol;

    // 5. HighDensitySpectral
    AudioABClassTolerance denseTol;
    denseTol.passMinimumCorrelation = 0.8000f;
    denseTol.warnMinimumCorrelation = 0.7500f;
    denseTol.passMaximumMeanSpectralDeltaDb = 0.75f;
    denseTol.warnMaximumMeanSpectralDeltaDb = 1.25f;
    denseTol.passMaximumRmsDeltaDb = 0.50f;
    denseTol.warnMaximumRmsDeltaDb = 1.00f;
    denseTol.passMaximumAbsoluteLagSamples = 4;
    denseTol.warnMaximumAbsoluteLagSamples = 16;
    policy.classTolerances["HighDensitySpectral"] = denseTol;

    return policy;
}

AudioABToleranceVerdict AudioABTolerancePolicy5D::evaluate(const AudioABEvaluationResult& result) const
{
    AudioABToleranceVerdict verdict;

    // =========================================================================
    // CAPA 1: HARD LIMITS (Invariantes Estructurales - Fallo Absoluto)
    // =========================================================================

    if (hardLimits.failOnNondeterministicRender && !result.intraEngineBitIdentical)
    {
        verdict.triggeredFails.push_back("HARD_FAIL_NONDETERMINISTIC_RENDER");
    }

    if (std::abs(result.temporal.alignmentLagSamples) > hardLimits.maxAbsoluteLagSamples)
    {
        verdict.triggeredFails.push_back("HARD_FAIL_EXCESSIVE_LAG_BEYOND_128");
    }

    if (hardLimits.failOnEventOrderViolation && result.events.eventOrderViolation)
    {
        verdict.triggeredFails.push_back("HARD_FAIL_EVENT_ORDER_VIOLATION");
    }

    if (hardLimits.failOnUnexpectedClipping && result.amplitude.unexpectedClipping)
    {
        verdict.triggeredFails.push_back("HARD_FAIL_UNEXPECTED_CLIPPING");
    }

    if (hardLimits.requireWarmupDigitalSilence &&
        (!result.temporal.warmupAIsDigitalSilence || !result.temporal.warmupBIsDigitalSilence))
    {
        verdict.triggeredFails.push_back("HARD_FAIL_WARMUP_NOT_DIGITAL_SILENCE");
    }

    if (hardLimits.failOnEnvelopeCollapse && result.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE))
    {
        verdict.triggeredFails.push_back("HARD_FAIL_ENVELOPE_COLLAPSE");
    }

    if (hardLimits.failOnPolarityInversion && result.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION))
    {
        verdict.triggeredFails.push_back("HARD_FAIL_POLARITY_INVERSION");
    }

    // Precedencia absoluta: si hay cualquier Hard Limit incumplido, se emite FAIL inmediato
    if (!verdict.triggeredFails.empty())
    {
        verdict.level = VerdictLevel::Fail;
        verdict.disposition = AcceptanceDisposition::Rejected;
        verdict.reasonCode = "HARD_LIMIT_VIOLATION";
        return verdict;
    }

    // =========================================================================
    // CAPA 2: TOLERANCIAS POR CLASE ACÚSTICA
    // =========================================================================

    auto it = classTolerances.find(result.presetClass);
    if (it == classTolerances.end())
    {
        verdict.level = VerdictLevel::Fail;
        verdict.reasonCode = "UNKNOWN_PRESET_CLASS";
        verdict.triggeredFails.push_back("CLASS_TOLERANCE_NOT_DEFINED_FOR_" + result.presetClass);
        return verdict;
    }

    const auto& tol = it->second;

    // 1. Correlación Cruzada Normalizada
    const float corr = result.temporal.normalizedCrossCorrelation;
    if (corr < tol.warnMinimumCorrelation)
    {
        verdict.triggeredFails.push_back("CORRELATION_BELOW_WARN_THRESHOLD");
    }
    else if (corr < tol.passMinimumCorrelation)
    {
        verdict.triggeredWarns.push_back("CORRELATION_IN_WARN_RANGE");
    }

    // 2. Diferencia Espectral Media en dB (si es aplicable)
    if (result.spectral.meanSpectralDeltaDb.has_value())
    {
        const float specDelta = *result.spectral.meanSpectralDeltaDb;
        if (specDelta > tol.warnMaximumMeanSpectralDeltaDb)
        {
            verdict.triggeredFails.push_back("SPECTRAL_DELTA_EXCEEDS_WARN_THRESHOLD");
        }
        else if (specDelta > tol.passMaximumMeanSpectralDeltaDb)
        {
            verdict.triggeredWarns.push_back("SPECTRAL_DELTA_IN_WARN_RANGE");
        }
    }

    // 3. Diferencia RMS de Envolvente en dB
    const float rmsDelta = result.amplitude.rmsDeltaDb;
    if (rmsDelta > tol.warnMaximumRmsDeltaDb)
    {
        verdict.triggeredFails.push_back("RMS_DELTA_EXCEEDS_WARN_THRESHOLD");
    }
    else if (rmsDelta > tol.passMaximumRmsDeltaDb)
    {
        verdict.triggeredWarns.push_back("RMS_DELTA_IN_WARN_RANGE");
    }

    // 4. Lag Temporal en Muestras
    const int absLag = std::abs(result.temporal.alignmentLagSamples);
    if (absLag > tol.warnMaximumAbsoluteLagSamples)
    {
        verdict.triggeredFails.push_back("LAG_EXCEEDS_CLASS_WARN_THRESHOLD");
    }
    else if (absLag > tol.passMaximumAbsoluteLagSamples)
    {
        verdict.triggeredWarns.push_back("LAG_IN_CLASS_WARN_RANGE");
    }

    // 5. THD Delta (si es aplicable y está definido en la clase)
    if (tol.warnMaximumThdDeltaDb.has_value() && result.spectral.thdDeltaDb.has_value())
    {
        const float thdDelta = *result.spectral.thdDeltaDb;
        if (thdDelta > *tol.warnMaximumThdDeltaDb)
        {
            verdict.triggeredFails.push_back("THD_DELTA_EXCEEDS_WARN_THRESHOLD");
        }
        else if (tol.passMaximumThdDeltaDb.has_value() && thdDelta > *tol.passMaximumThdDeltaDb)
        {
            verdict.triggeredWarns.push_back("THD_DELTA_IN_WARN_RANGE");
        }
    }

    // 6. SNR Delta (si es aplicable y está definido en la clase)
    if (tol.warnMaximumSnrDeltaDb.has_value() && result.spectral.snrDeltaDb.has_value())
    {
        const float snrDelta = *result.spectral.snrDeltaDb;
        if (snrDelta > *tol.warnMaximumSnrDeltaDb)
        {
            verdict.triggeredFails.push_back("SNR_DELTA_EXCEEDS_WARN_THRESHOLD");
        }
        else if (tol.passMaximumSnrDeltaDb.has_value() && snrDelta > *tol.passMaximumSnrDeltaDb)
        {
            verdict.triggeredWarns.push_back("SNR_DELTA_IN_WARN_RANGE");
        }
    }

    // =========================================================================
    // CAPA 2.5: ESCALADO POR DIAGNÓSTICOS DE EVALUADOR
    // Si el evaluador emitió diagnósticos WARN (DIAG_WARN_PHASE_DISPERSION,
    // DIAG_WARN_HARMONIC_SPREAD) y no hay class-fail activo, el veredicto
    // sube a WARN para preservar visibilidad QA sin tratar como PASS silencioso.
    // Regla: los diagnósticos WARN del evaluador son informativos; no anulan
    // un Pass estructural, pero sí diferencian PASS de WARN.
    // =========================================================================
    if (verdict.triggeredFails.empty())
    {
        const bool hasEvaluatorWarnPhase   = result.hasDiagnostic(DiagnosticCode::DIAG_WARN_PHASE_DISPERSION);
        const bool hasEvaluatorWarnHarmonic = result.hasDiagnostic(DiagnosticCode::DIAG_WARN_HARMONIC_SPREAD);
        if (hasEvaluatorWarnPhase || hasEvaluatorWarnHarmonic)
        {
            verdict.triggeredWarns.push_back("EVALUATOR_WARN_DISPERSION");
        }
    }

    // =========================================================================
    // RESOLUCIÓN DE VEREDICTO FINAL (con AcceptanceDisposition)
    // =========================================================================

    if (!verdict.triggeredFails.empty())
    {
        verdict.level = VerdictLevel::Fail;
        verdict.disposition = AcceptanceDisposition::Rejected;
        verdict.reasonCode = "CLASS_TOLERANCE_EXCEEDED";
    }
    else if (!verdict.triggeredWarns.empty())
    {
        verdict.level = VerdictLevel::Warn;
        verdict.disposition = AcceptanceDisposition::AcceptableWithExpectedDispersion;
        verdict.reasonCode = "WARN_TOLERANCE_RANGE";
    }
    else
    {
        verdict.level = VerdictLevel::Pass;
        verdict.disposition = AcceptanceDisposition::Accepted;
        verdict.reasonCode = "PASS_ALL_CRITERIA";
    }

    return verdict;
}

nlohmann::json AudioABTolerancePolicy5D::toJson() const
{
    nlohmann::json j;
    j["policyId"] = policyId;
    j["version"] = version;
    j["calibrationStatus"] = calibrationStatus;
    j["sourceBuild"] = sourceBuild;
    j["effectiveFrom"] = effectiveFrom;
    j["reviewRequiredBefore"] = reviewRequiredBefore;

    j["hardLimits"] = {
        { "maxAbsoluteLagSamples", hardLimits.maxAbsoluteLagSamples },
        { "failOnUnexpectedClipping", hardLimits.failOnUnexpectedClipping },
        { "failOnEventOrderViolation", hardLimits.failOnEventOrderViolation },
        { "failOnNondeterministicRender", hardLimits.failOnNondeterministicRender },
        { "failOnEnvelopeCollapse", hardLimits.failOnEnvelopeCollapse },
        { "failOnPolarityInversion", hardLimits.failOnPolarityInversion },
        { "requireWarmupDigitalSilence", hardLimits.requireWarmupDigitalSilence }
    };

    nlohmann::json classesJson = nlohmann::json::object();
    for (const auto& [className, tol] : classTolerances)
    {
        auto optToVal = [](const std::optional<float>& opt) -> nlohmann::json {
            if (!opt.has_value()) return nullptr;
            return *opt;
        };

        classesJson[className] = {
            { "passMinimumCorrelation", tol.passMinimumCorrelation },
            { "warnMinimumCorrelation", tol.warnMinimumCorrelation },
            { "passMaximumMeanSpectralDeltaDb", tol.passMaximumMeanSpectralDeltaDb },
            { "warnMaximumMeanSpectralDeltaDb", tol.warnMaximumMeanSpectralDeltaDb },
            { "passMaximumRmsDeltaDb", tol.passMaximumRmsDeltaDb },
            { "warnMaximumRmsDeltaDb", tol.warnMaximumRmsDeltaDb },
            { "passMaximumAbsoluteLagSamples", tol.passMaximumAbsoluteLagSamples },
            { "warnMaximumAbsoluteLagSamples", tol.warnMaximumAbsoluteLagSamples },
            { "passMaximumThdDeltaDb", optToVal(tol.passMaximumThdDeltaDb) },
            { "warnMaximumThdDeltaDb", optToVal(tol.warnMaximumThdDeltaDb) },
            { "passMaximumSnrDeltaDb", optToVal(tol.passMaximumSnrDeltaDb) },
            { "warnMaximumSnrDeltaDb", optToVal(tol.warnMaximumSnrDeltaDb) }
        };
    }

    j["classTolerances"] = classesJson;
    return j;
}

} // namespace abdaudiolab::math::qa5d
