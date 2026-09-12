#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "HardwareContractRegistry.h"
#include "../hardware/HardwareController.h"

namespace abdaudiolab::core
{

/**
 * @class ProfilingHardwareDispatcher
 * @brief Handles low-level physical injection of MIDI CC, 14-bit NRPN, SysEx,
 *        velocity notes, and measurement preset recipes to hardware targets.
 */
class ProfilingHardwareDispatcher
{
public:
    explicit ProfilingHardwareDispatcher(hardware::IHardwareController* hardwareInterface = nullptr);
    ~ProfilingHardwareDispatcher() = default;

    void setHardwareController(hardware::IHardwareController* newHardware) noexcept
    {
        hardware = newHardware;
    }

    [[nodiscard]] hardware::IHardwareController* getHardwareController() const noexcept
    {
        return hardware;
    }

    void setTargetPluginInstance(juce::AudioPluginInstance* plugin) noexcept
    {
        targetPlugin = plugin;
    }

    [[nodiscard]] juce::AudioPluginInstance* getTargetPluginInstance() const noexcept
    {
        return targetPlugin;
    }

    void setParameter(int paramIndex, float normalizedValue);
    void executeLifecycleActions(const std::vector<HardwareSetupAction>& actions);
    void executeMeasurementRecipe(const MeasurementPresetRecipe& recipe);

    void sendNoteOn(int channel, int noteNumber, float normalizedVelocity);
    void sendNoteOff(int channel, int noteNumber, float velocity = 0.0f);
    void sendAllNotesOff(int channel);

    enum class ModExcitationType { Velocity, CC, Aftertouch, SysExAmount };

    void injectHardwareModulationValue(ModExcitationType excitationType,
                                       int channel,
                                       int controlCCNumber,
                                       const juce::String& sysexTemplate,
                                       int rawValue);

private:
    hardware::IHardwareController* hardware { nullptr };
    juce::AudioPluginInstance* targetPlugin { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfilingHardwareDispatcher)
};

} // namespace abdaudiolab::core
