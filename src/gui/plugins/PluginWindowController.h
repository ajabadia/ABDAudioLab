#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::gui
{

/**
 * @class PluginWindowController
 * @brief Manages floating window presentation for hosted audio plugin custom UI editors.
 */
class PluginWindowController
{
public:
    PluginWindowController();
    ~PluginWindowController();

    /**
     * @brief Displays the GUI editor for the provided plugin instance.
     */
    void showPluginWindow(juce::AudioPluginInstance* plugin, const juce::String& windowTitle = {});

    /**
     * @brief Closes and releases the active plugin window.
     */
    void closePluginWindow();

    /**
     * @brief Returns true if a plugin window is currently visible.
     */
    [[nodiscard]] bool isWindowOpen() const noexcept;

private:
    class PluginWindow : public juce::DocumentWindow
    {
    public:
        PluginWindow(const juce::String& title,
                     std::unique_ptr<juce::AudioProcessorEditor> editor,
                     std::function<void()> onCloseCallback);
        ~PluginWindow() override;

        void closeButtonPressed() override;

    private:
        std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
    };

    std::unique_ptr<PluginWindow> activeWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindowController)
};

} // namespace abdaudiolab::gui
