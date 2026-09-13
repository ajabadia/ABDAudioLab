#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Sha256.h"

namespace abdaudiolab::synth
{

inline constexpr const char* kSynthSchemaVersion    = "1.0.0";
inline constexpr const char* kSynthProtocolVersion  = "1.0.0";
inline constexpr const char* kSynthAlgorithmVersion = "1.0.0";

/**
 * @brief Estado de validación de la aplicación del preset en el sintetizador.
 */
enum class StateAppliedStatus
{
    Unverified,
    Passed,
    Failed
};

[[nodiscard]] inline std::string stateAppliedStatusToString(StateAppliedStatus status)
{
    switch (status)
    {
        case StateAppliedStatus::Passed:     return "PASSED";
        case StateAppliedStatus::Failed:     return "FAILED";
        case StateAppliedStatus::Unverified: return "UNVERIFIED";
        default:                             return "UNKNOWN";
    }
}

/**
 * @brief Parámetro normalizado [0.0 .. 1.0] con orden canónico determinista por ID.
 */
struct NormalizedParameter
{
    std::string id;
    double value { 0.0 };
    std::string name;

    bool operator<(const NormalizedParameter& other) const noexcept
    {
        return id < other.id;
    }
};

/**
 * @brief Estado inmutable y auditable del preset bajo ensayo.
 */
struct SynthPresetState
{
    std::string schemaVersion { kSynthSchemaVersion };
    std::string protocolVersion { kSynthProtocolVersion };
    std::string algorithmVersion { kSynthAlgorithmVersion };

    std::string presetId { "anchor_001" };
    std::string presetName { "Anchor Calibration Preset" };
    std::string deviceModel { "virtual_monosynth" };

    // SysEx bruto
    std::vector<uint8_t> rawSysEx;
    std::string rawSysExHash; // SHA-256

    // Parámetros normalizados ordenados canónicamente
    std::vector<NormalizedParameter> normalizedParameters;
    std::string normalizedParameterHash; // SHA-256

    // Hash canónico del estado del preset (solo estado, sin condiciones ambientales)
    std::string stateHash; // SHA-256

    // Atributos semánticos del preset
    bool isMonophonic { true };
    bool effectsEnabled { false };
    bool lfoEnabled { false };
    bool filterWideOpen { true };
    bool velocitySensitive { true };

    StateAppliedStatus stateStatus { StateAppliedStatus::Unverified };

    bool finalizeAndComputeHashes()
    {
        for (auto& p : normalizedParameters)
        {
            if (std::isnan(p.value) || std::isinf(p.value))
                return false;
            p.value = std::clamp(p.value, 0.0, 1.0);
        }

        std::sort(normalizedParameters.begin(), normalizedParameters.end());

        if (!rawSysEx.empty())
        {
            rawSysExHash = Sha256::computeHex(rawSysEx.data(), rawSysEx.size());
        }
        else
        {
            rawSysExHash = "0000000000000000000000000000000000000000000000000000000000000000";
        }

        std::string paramBlob;
        paramBlob.reserve(normalizedParameters.size() * 64);
        char numBuf[32];
        for (const auto& p : normalizedParameters)
        {
            std::snprintf(numBuf, sizeof(numBuf), "%.6f", p.value);
            paramBlob += p.id;
            paramBlob += "=";
            paramBlob += numBuf;
            paramBlob += "\n";
        }
        normalizedParameterHash = Sha256::computeHex(paramBlob);

        std::string stateBlob = schemaVersion + "\n"
                              + deviceModel + "\n"
                              + presetId + "\n"
                              + rawSysExHash + "\n"
                              + normalizedParameterHash + "\n"
                              + (isMonophonic ? "mono\n" : "poly\n")
                              + (effectsEnabled ? "fx_on\n" : "fx_off\n")
                              + (lfoEnabled ? "lfo_on\n" : "lfo_off\n");
        stateHash = Sha256::computeHex(stateBlob);
        return true;
    }
};

} // namespace abdaudiolab::synth
