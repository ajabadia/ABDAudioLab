#pragma once

#include "PluginHostManager.h"
#include "../HardwareContractRegistry.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::core
{

/**
 * @class PluginHardwareContractAdapter
 * @brief Introspects an instantiated juce::AudioPluginInstance and creates a dynamic HardwareContract
 *        mapping its parameters to HardwareControl items ready for automated lab sweeps and profiling.
 */
class PluginHardwareContractAdapter
{
public:
    /**
     * @brief Creates a HardwareContract from a plugin/processor instance and its description.
     */
    static HardwareContract createContractFromPlugin(juce::AudioProcessor& plugin,
                                                    const juce::PluginDescription& desc);

    /**
     * @brief Sets a plugin parameter value by its contract index [1..N] using normalized value [0.0 .. 1.0].
     */
    static void setParameterNormalized(juce::AudioProcessor& plugin, int controlIndex, float normVal);

    /**
     * @brief Reads a normalized parameter value from the plugin by contract index [1..N].
     */
    static float getParameterNormalized(const juce::AudioProcessor& plugin, int controlIndex);
};

} // namespace abdaudiolab::core
