#include "PluginWindowController.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

PluginWindowController::PluginWindow::PluginWindow(const juce::String& title,
                                                  std::unique_ptr<juce::AudioProcessorEditor> editor,
                                                  std::function<void()> onCloseCallback)
    : juce::DocumentWindow(title, SoundIdTheme::bgCard, juce::DocumentWindow::closeButton),
      pluginEditor(std::move(editor)),
      onClose(std::move(onCloseCallback))
{
    juce::Logger::writeToLog("[PluginWindow] Constructing DocumentWindow for: " + title);
    setUsingNativeTitleBar(true);

    int w = 600;
    int h = 400;

    if (pluginEditor != nullptr)
    {
        w = pluginEditor->getWidth();
        h = pluginEditor->getHeight();
        if (w <= 0 || h <= 0)
        {
            w = 600;
            h = 400;
            pluginEditor->setSize(w, h);
        }
        juce::Logger::writeToLog("[PluginWindow] Setting owned content component with size: "
            + juce::String(w) + "x" + juce::String(h));

        setContentOwned(pluginEditor.release(), true);
        setResizable(true, true);
    }

    centreWithSize(w, h);
    setVisible(true);
    juce::Logger::writeToLog("[PluginWindow] Window set visible successfully.");
}

PluginWindowController::PluginWindow::~PluginWindow()
{
    juce::Logger::writeToLog("[PluginWindow] PluginWindow destructor: clearing content component.");
    clearContentComponent();
}

void PluginWindowController::PluginWindow::closeButtonPressed()
{
    juce::Logger::writeToLog("[PluginWindow] Close button pressed by user.");
    if (onClose != nullptr)
        onClose();
}

PluginWindowController::PluginWindowController() = default;

PluginWindowController::~PluginWindowController()
{
    juce::Logger::writeToLog("[PluginWindowController] Destructor called.");
    closePluginWindow();
}

void PluginWindowController::showPluginWindow(juce::AudioPluginInstance* plugin, const juce::String& windowTitle)
{
    juce::Logger::writeToLog("[PluginWindow] showPluginWindow called. WindowTitle: " + windowTitle);
    closePluginWindow();

    if (plugin == nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] Cannot show window: plugin is nullptr.");
        return;
    }

    juce::Logger::writeToLog("[PluginWindow] Plugin name: '" + plugin->getName()
        + "', hasEditor: " + juce::String(plugin->hasEditor() ? "YES" : "NO"));

    juce::Logger::writeToLog("[PluginWindow] Calling plugin->createEditorIfNeeded()...");
    auto* editor = plugin->createEditorIfNeeded();

    if (editor == nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] plugin->createEditorIfNeeded() returned nullptr for: " + plugin->getName());
        return;
    }

    juce::Logger::writeToLog("[PluginWindow] createEditorIfNeeded returned valid editor pointer: "
        + juce::String::toHexString((juce::uint64)editor)
        + " [Bounds: " + editor->getBounds().toString() + "]");

    juce::String title = windowTitle.isNotEmpty() ? windowTitle : plugin->getName();
    activeWindow = std::make_unique<PluginWindow>(title,
                                                  std::unique_ptr<juce::AudioProcessorEditor>(editor),
                                                  [this] { closePluginWindow(); });
}

void PluginWindowController::closePluginWindow()
{
    if (activeWindow != nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow] Closing active plugin window.");
        activeWindow.reset();
        juce::Logger::writeToLog("[PluginWindow] Active plugin window reset complete.");
    }
}

bool PluginWindowController::isWindowOpen() const noexcept
{
    return activeWindow != nullptr && activeWindow->isVisible();
}

} // namespace abdaudiolab::gui
