#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::gui {

/**
 * @struct PluginIdentityPresentation
 * @brief View-layer visual identity metadata transformed from domain plugin descriptions.
 */
struct PluginIdentityPresentation
{
    juce::String pluginName;
    juce::String formatName;
    bool isInstrument { false };
    juce::String category { "PLUGIN_VIRTUAL" };
    juce::String legalTargetId;
    juce::String titleBadge;
    juce::String busDescription;
    juce::String modelAssetPath;
};

/**
 * @class IPluginUiHost
 * @brief Abstract port decoupling PluginUiCoordinator from concrete UI layout and dialogs.
 */
class IPluginUiHost
{
public:
    virtual ~IPluginUiHost() = default;

    /**
     * @brief Updates loading spinner / status indicator during async plugin load operations.
     */
    virtual void updatePluginLoadingState(bool isSuccess, const juce::String& message) = 0;

    /**
     * @brief Updates UI badges, headers, drawer setup info and stepper cards for the active plugin.
     */
    virtual void updatePluginIdentity(const PluginIdentityPresentation& identity,
                                      const juce::PluginDescription& description) = 0;

    /**
     * @brief Displays an error dialog or banner if plugin loading or instantiating fails.
     */
    virtual void showPluginError(const juce::String& title, const juce::String& message) = 0;

    /**
     * @brief Clears active plugin presentation badges, headers and disables standard tests.
     */
    virtual void notifyPluginUnloaded() = 0;
};

} // namespace abdaudiolab::gui
