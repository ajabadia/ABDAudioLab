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
     * @brief Muestra el editor grafico del plugin alojado por PluginHostManager.
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
     * @brief Captures a raster snapshot of the hosted plugin editor and saves it to disk.
     */
    void captureSnapshot();

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

    /**
     * @brief Callback notified when a raster snapshot of the plugin editor has been saved.
     */
    std::function<void(const juce::File& savedFile, const juce::String& dataUrl)> onSnapshotCaptured;

private:
    class PluginWindow : public juce::DocumentWindow
    {
    public:
        PluginWindow(const juce::String& title,
                     std::unique_ptr<juce::AudioProcessorEditor> editor,
                     std::function<void()> onCloseCallback);
        ~PluginWindow() override;

        void closeButtonPressed() override;
        void captureSnapshot(const juce::String& pluginName,
                             std::function<void(const juce::File&, const juce::String&)> onSnapshotCallback);

    private:
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
    };

    std::unique_ptr<PluginWindow> activeWindow;
    juce::String currentPluginName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindowController)
};

} // namespace abdaudiolab::gui
