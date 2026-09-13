#pragma once

#include "TargetContract.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace abdaudiolab::synth
{

/**
 * @brief Motor agnóstico de introspección para sintetizar el TargetContract de cualquier plugin VST3.
 *
 * Cumple estrictamente la directriz: "Descubrir no es comprender".
 * - No asume categorías físicas como certezas definitivas (etiqueta semanticEvidence = InferredFromName).
 * - Identifica parámetros de infraestructura técnica (ParameterRole::TestInfrastructure).
 * - Genera una serialización canónica con parameterContractHash reproducible.
 */
class TargetContractDiscovery
{
public:
    TargetContractDiscovery() = default;

    /**
     * @brief Descubre el contrato canónico a partir de una instancia de juce::AudioProcessor / juce::AudioPluginInstance.
     */
    [[nodiscard]] TargetContract discoverContract(juce::AudioProcessor& processor) const;

    /**
     * @brief Exporta el contrato a una cadena JSON auditable.
     */
    [[nodiscard]] std::string exportToJson(const TargetContract& contract) const;

private:
    [[nodiscard]] ParameterRole inferParameterRole(const std::string& nativeId, const std::string& name) const noexcept;
    [[nodiscard]] ParameterCategory inferParameterCategory(const std::string& name, ParameterRole role) const noexcept;
};

} // namespace abdaudiolab::synth
