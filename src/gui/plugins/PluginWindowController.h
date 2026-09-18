#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::core
{
class PluginHostManager;
}

namespace abdaudiolab::gui
{

/**
 * @class PluginWindowController
 * @brief Manages floating window presentation for hosted audio plugin custom UI editors.
 *        Guarantees idempotent closing, safe editor destruction, and callback disconnection.
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
     * @brief Displays the GUI editor for the plugin actively hosted by PluginHostManager.
     */
    void showPluginWindow(core::PluginHostManager& hostManager, const juce::String& windowTitle = {});

    /**
     * @brief Closes and releases the active plugin window.
     */
    void closePluginWindow();

    /**
     * @brief Returns true if a plugin window is currently visible.
     */
    [[nodiscard]] bool isWindowOpen() const noexcept;

    /**
     * @brief Updates active window styling when the application theme changes.
     */
    void updateTheme();

    /**
     * @brief Callback notified when the plugin window is shown (true) or closed (false).
     */
    std::function<void(bool isOpen)> onWindowStateChanged;

    /**
     * @brief Callback notified when the user clicks 'Teclado MIDI' inside the plugin window.
     */
    std::function<void()> onOpenKeyboardRequested;

private:
    class PluginWindow : public juce::DocumentWindow
    {
    public:
        PluginWindow(const juce::String& title,
                     std::unique_ptr<juce::AudioProcessorEditor> editor,
                     std::function<void()> onCloseCallback,
                     std::function<void()> onOpenKeyboardCallback);
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
