#include "PluginWindowController.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

namespace
{
class PluginContainerComponent : public juce::Component
{
public:
    PluginContainerComponent(std::unique_ptr<juce::AudioProcessorEditor> editor,
                             std::function<void()> onOpenKeyboard)
        : pluginEditor(std::move(editor)),
          openKeyboardCallback(std::move(onOpenKeyboard))
    {
        setOpaque(true);
        btnKeyboard.setButtonText("Teclado MIDI");
        btnKeyboard.setTooltip("Abrir Teclado Virtual MIDI para tocar este plugin");
        btnKeyboard.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
        btnKeyboard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
        btnKeyboard.onClick = [this] {
            if (openKeyboardCallback) openKeyboardCallback();
        };
        addAndMakeVisible(btnKeyboard);

        if (pluginEditor != nullptr)
            addAndMakeVisible(*pluginEditor);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(SoundIdTheme::bgLight);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawHorizontalLine(27, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto topBar = r.removeFromTop(28).reduced(4, 2);
        btnKeyboard.setBounds(topBar.removeFromRight(100));

        if (pluginEditor != nullptr)
            pluginEditor->setBounds(r);
    }

private:
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
    juce::TextButton btnKeyboard;
    std::function<void()> openKeyboardCallback;
};
} // namespace

PluginWindowController::PluginWindow::PluginWindow(const juce::String& title,
                                                   std::unique_ptr<juce::AudioProcessorEditor> editor,
                                                   std::function<void()> onCloseCallback,
                                                   std::function<void()> onOpenKeyboardCallback)
    : juce::DocumentWindow(title, AppTheme::BackgroundApp, juce::DocumentWindow::closeButton),
      onClose(std::move(onCloseCallback))
{
    juce::Logger::writeToLog("[PluginWindow] Constructing DocumentWindow for: " + title);
    setUsingNativeTitleBar(false);

    int w = 600;
    int h = 400;

    if (editor != nullptr)
    {
        w = editor->getWidth();
        h = editor->getHeight();
        if (w <= 0 || h <= 0)
        {
            w = 600;
            h = 400;
            editor->setSize(w, h);
        }
    }

    auto container = std::make_unique<PluginContainerComponent>(std::move(editor), std::move(onOpenKeyboardCallback));
    setContentOwned(container.release(), true);
    setResizable(true, true);
    centreWithSize(w, h + 28);
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
                                                  [this] { closePluginWindow(); },
                                                  [this] { if (onOpenKeyboardRequested) onOpenKeyboardRequested(); });

    if (onWindowStateChanged != nullptr)
        onWindowStateChanged(true);
}

void PluginWindowController::closePluginWindow()
{
    if (activeWindow != nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow] Closing active plugin window.");
        activeWindow.reset();
        juce::Logger::writeToLog("[PluginWindow] Active plugin window reset complete.");

        if (onWindowStateChanged != nullptr)
            onWindowStateChanged(false);
    }
}

bool PluginWindowController::isWindowOpen() const noexcept
{
    return activeWindow != nullptr && activeWindow->isVisible();
}

void PluginWindowController::updateTheme()
{
    if (activeWindow != nullptr)
    {
        activeWindow->setBackgroundColour(AppTheme::BackgroundApp);
        activeWindow->repaint();
    }
}

} // namespace abdaudiolab::gui
