#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "ExternalPluginTypes.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Categoría funcional del parámetro dentro del sintetizador.
 */
enum class ParameterCategory
{
    Oscillator,
    Filter,
    Envelope,
    Modulation,
    Gain,
    Custom
};

/**
 * @brief Descriptor formal de un parámetro expuesto por el contrato del target.
 */
struct TargetParameterDescriptor
{
    std::string normalizedId;  /**< Identificador canónico asignado. */
    std::string nativeId;      /**< Identificador único nativo expuesto por el plugin/SDK. */
    std::string nativeName;    /**< Nombre legible nativo. */
    std::string groupPath;     /**< Jerarquía de grupos (ej. "Filter/Main"). */

    double minValue { 0.0 };
    double maxValue { 1.0 };
    double defaultValue { 0.5 };
    std::string unit;
    int stepCount { 0 };

    bool isDiscrete { false };
    bool isAutomatable { true };

    ParameterRole role { ParameterRole::AudioControl };
    ParameterCategory category { ParameterCategory::Custom };
    SemanticStatus semanticStatus { SemanticStatus::Unverified };
    SemanticEvidence semanticEvidence { SemanticEvidence::Unknown };

    SmoothingEvidence smoothing;

    TargetParameterDescriptor() = default;

    TargetParameterDescriptor(std::string normId,
                              std::string natId,
                              double minV,
                              double maxV,
                              double defV,
                              std::string u,
                              ParameterCategory cat,
                              bool discrete = false,
                              bool smoothed = false)
        : normalizedId(std::move(normId)),
          nativeId(std::move(natId)),
          nativeName(normalizedId),
          minValue(minV),
          maxValue(maxV),
          defaultValue(defV),
          unit(std::move(u)),
          isDiscrete(discrete),
          category(cat)
    {
        smoothing.declaredSmoothing = smoothed;
        smoothing.smoothingKnown = smoothed;
    }
};

/**
 * @brief Contrato formal que declara la semántica, capacidades y parámetros descubiertos del target.
 */
struct TargetContract
{
    std::string schemaVersion { "1.0.0" };
    std::string name;
    std::string manufacturer;
    std::string targetVersion;
    std::string pluginUid;
    std::string format { "VST3" };

    bool declaredDeterministic { true };
    bool supportsReset { true };
    bool acceptsMidi { true };
    int numAudioInputs { 0 };
    int numAudioOutputs { 2 };
    double declaredLatencySamples { 0.0 };

    std::vector<TargetParameterDescriptor> parameters;
    std::string parameterContractHash;

    [[nodiscard]] const TargetParameterDescriptor* findParameter(const std::string& paramId) const noexcept
    {
        for (const auto& p : parameters)
        {
            if (p.normalizedId == paramId || p.nativeId == paramId)
                return &p;
        }
        return nullptr;
    }

    void computeHash()
    {
        std::string blob = schemaVersion + "\n"
                         + name + "\n"
                         + manufacturer + "\n"
                         + targetVersion + "\n"
                         + pluginUid + "\n"
                         + format + "\n"
                         + std::to_string(declaredLatencySamples) + "\n"
                         + std::to_string(parameters.size()) + "\n";

        // Orden canónico por nativeId para garantizar reproducibilidad exacta
        auto sortedParams = parameters;
        std::sort(sortedParams.begin(), sortedParams.end(), [](const auto& a, const auto& b) {
            return a.nativeId < b.nativeId;
        });

        for (const auto& p : sortedParams)
        {
            blob += p.nativeId + "|"
                  + p.nativeName + "|"
                  + p.groupPath + "|"
                  + std::to_string(p.minValue) + "|"
                  + std::to_string(p.maxValue) + "|"
                  + std::to_string(p.defaultValue) + "|"
                  + p.unit + "|"
                  + std::to_string(p.stepCount) + "|"
                  + (p.isDiscrete ? "D1" : "D0") + "|"
                  + (p.isAutomatable ? "A1" : "A0") + "|"
                  + std::to_string(static_cast<int>(p.role)) + "|"
                  + std::to_string(static_cast<int>(p.category)) + "|"
                  + std::to_string(static_cast<int>(p.semanticStatus)) + "|"
                  + std::to_string(static_cast<int>(p.semanticEvidence)) + "\n";
        }

        parameterContractHash = Sha256::computeHex(blob);
    }
};

} // namespace abdaudiolab::synth
