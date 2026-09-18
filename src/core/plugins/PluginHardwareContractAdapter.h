#pragma once

#include "PluginHostManager.h"
#include "../HardwareContractRegistry.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::core
{

/**
 * @brief Formal metadata descriptor for a VST3 plugin measurement session.
 */
struct Vst3SessionDescriptor
{
    std::string pluginIdentifier;
    std::string pluginVersion;
    std::string parameterListHash;
    std::string statePresetHash;
    double sampleRate { 48000.0 };
    int blockSize { 512 };
    std::string automationMode { "sample_accurate_gesture" };

    [[nodiscard]] nlohmann::ordered_json toJson() const;
};

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

    /**
     * @brief Begins parameter editing gesture conforming to VST3 beginEdit specification.
     */
    static void beginParameterEdit(juce::AudioProcessor& plugin, int controlIndex);

    /**
     * @brief Performs parameter editing conforming to VST3 performEdit specification.
     */
    static void performParameterEdit(juce::AudioProcessor& plugin, int controlIndex, float normVal);

    /**
     * @brief Ends parameter editing gesture conforming to VST3 endEdit specification.
     */
    static void endParameterEdit(juce::AudioProcessor& plugin, int controlIndex);

    /**
     * @brief Computes SHA-256 fingerprint of the plugin's parameter tree and names.
     */
    static std::string computeParameterListHash(const juce::AudioProcessor& plugin);

    /**
     * @brief Computes SHA-256 fingerprint of the plugin's current binary state / preset.
     */
    static std::string computeStateHash(juce::AudioProcessor& plugin);

    /**
     * @brief Builds complete VST3 session descriptor from an instantiated plugin.
     */
    static Vst3SessionDescriptor createSessionDescriptor(juce::AudioProcessor& plugin,
                                                        const juce::PluginDescription& desc,
                                                        double sampleRate,
                                                        int blockSize);
};

} // namespace abdaudiolab::core
