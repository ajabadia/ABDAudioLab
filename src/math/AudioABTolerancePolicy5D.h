#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>
#include "AudioABMetrics5D.h"

namespace abdaudiolab::math::qa5d
{

enum class VerdictLevel
{
    Pass,
    Warn,
    Fail
};

/// @brief Disposición de aceptación del veredicto.
/// Complementa VerdictLevel con semántica QA explícita.
enum class AcceptanceDisposition
{
    /// Sin fallos ni warnings relevantes — aceptado formalmente.
    Accepted,
    /// Dispersión esperada para la clase acústica; visible en QA, no es regresión.
    AcceptableWithExpectedDispersion,
    /// Hard fail o class fail — comparación no aceptable.
    Rejected
};

inline const char* toString(VerdictLevel level) noexcept
{
    switch (level)
    {
        case VerdictLevel::Pass: return "PASS";
        case VerdictLevel::Warn: return "WARN";
        case VerdictLevel::Fail: return "FAIL";
    }
    return "UNKNOWN";
}

inline const char* toString(AcceptanceDisposition disposition) noexcept
{
    switch (disposition)
    {
        case AcceptanceDisposition::Accepted:                      return "Accepted";
        case AcceptanceDisposition::AcceptableWithExpectedDispersion: return "AcceptableWithExpectedDispersion";
        case AcceptanceDisposition::Rejected:                      return "Rejected";
    }
    return "Unknown";
}

struct AudioABHardLimits
{
    int maxAbsoluteLagSamples { 128 };
    bool failOnUnexpectedClipping { true };
    bool failOnEventOrderViolation { true };
    bool failOnNondeterministicRender { true };
    bool failOnEnvelopeCollapse { true };
    bool failOnPolarityInversion { true };
    bool requireWarmupDigitalSilence { true };
};

struct AudioABClassTolerance
{
    float passMinimumCorrelation { 0.9999f };
    float warnMinimumCorrelation { 0.9990f };

    float passMaximumMeanSpectralDeltaDb { 0.01f };
    float warnMaximumMeanSpectralDeltaDb { 0.05f };

    float passMaximumRmsDeltaDb { 0.05f };
    float warnMaximumRmsDeltaDb { 0.10f };

    int passMaximumAbsoluteLagSamples { 0 };
    int warnMaximumAbsoluteLagSamples { 1 };

    std::optional<float> passMaximumThdDeltaDb { std::nullopt };
    std::optional<float> warnMaximumThdDeltaDb { std::nullopt };

    std::optional<float> passMaximumSnrDeltaDb { std::nullopt };
    std::optional<float> warnMaximumSnrDeltaDb { std::nullopt };
};

struct AudioABToleranceVerdict
{
    VerdictLevel level { VerdictLevel::Fail };
    AcceptanceDisposition disposition { AcceptanceDisposition::Rejected };
    std::string reasonCode;
    std::vector<std::string> triggeredFails;
    std::vector<std::string> triggeredWarns;

    [[nodiscard]] bool isPass() const noexcept { return level == VerdictLevel::Pass; }
    [[nodiscard]] bool isWarn() const noexcept { return level == VerdictLevel::Warn; }
    [[nodiscard]] bool isFail() const noexcept { return level == VerdictLevel::Fail; }

    [[nodiscard]] bool isAccepted() const noexcept { return disposition == AcceptanceDisposition::Accepted; }
    [[nodiscard]] bool isAcceptableWithDispersion() const noexcept
    {
        return disposition == AcceptanceDisposition::AcceptableWithExpectedDispersion;
    }
    [[nodiscard]] bool isRejected() const noexcept { return disposition == AcceptanceDisposition::Rejected; }

    [[nodiscard]] nlohmann::json toJson() const
    {
        return {
            { "verdictLevel", toString(level) },
            { "acceptanceDisposition", toString(disposition) },
            { "reasonCode", reasonCode },
            { "triggeredFails", triggeredFails },
            { "triggeredWarns", triggeredWarns }
        };
    }
};

class AudioABTolerancePolicy5D
{
public:
    std::string policyId { "audio-ab-5d-provisional-v1" };
    std::string version { "1.0.0" };
    std::string calibrationStatus { "ProvisionalEvidenceBased" };
    std::string sourceBuild { "Build #513" };
    std::string effectiveFrom { "2026-09-29" };
    std::string reviewRequiredBefore { "HITO-AUDIO-AB-5D.8" };

    AudioABHardLimits hardLimits;
    std::map<std::string, AudioABClassTolerance> classTolerances;

    /**
     * @brief Genera la política calibrada provisional basada en RUN_5D_01 a RUN_5D_10.
     */
    static AudioABTolerancePolicy5D getDefaultProvisionalPolicy();

    /**
     * @brief Evalúa un resultado según la jerarquía estricta:
     * Hard FAIL > Class FAIL > WARN > PASS.
     */
    [[nodiscard]] AudioABToleranceVerdict evaluate(const AudioABEvaluationResult& result) const;

    [[nodiscard]] nlohmann::json toJson() const;
};

} // namespace abdaudiolab::math::qa5d
