#include "PluginWindowController.h"
#include "../SoundIdTheme.h"
#include "../../core/plugins/PluginHostManager.h"
#include "../../core/LabDataDirectories.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace abdaudiolab::gui
{

PluginWindowController::PluginWindow::PluginWindow(const juce::String& title,
                                                   std::unique_ptr<juce::AudioProcessorEditor> editor,
                                                   std::function<void()> onCloseCallback)
    : juce::DocumentWindow(title, SoundIdTheme::bgCard, juce::DocumentWindow::closeButton),
      onClose(std::move(onCloseCallback))
{
    juce::Logger::writeToLog("[PluginWindow] Constructing DocumentWindow for: " + title);
    setUsingNativeTitleBar(true);

    if (editor != nullptr)
    {
        const int w = editor->getWidth();
        const int h = editor->getHeight();
        if (w <= 0 || h <= 0)
            editor->setSize(600, 400);

        juce::Logger::writeToLog("[PluginWindow] Setting owned content component with size: "
            + juce::String(editor->getWidth()) + "x" + juce::String(editor->getHeight()));

        const bool canResize = editor->isResizable();
        setContentOwned(editor.release(), true);
        setResizable(canResize, false);
    }
    else
    {
        setSize(600, 400);
    }

    centreWithSize(getWidth(), getHeight());
    setVisible(true);
    toFront(true);
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

void PluginWindowController::PluginWindow::captureSnapshot(const juce::String& pluginName,
                                                           std::function<void(const juce::File&, const juce::String&)> onSnapshotCallback)
{
    auto* editor = getContentComponent();
    if (editor == nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] captureSnapshot: content editor is nullptr.");
        return;
    }

    auto bounds = editor->getLocalBounds();
    if (bounds.isEmpty())
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] captureSnapshot: editor bounds are empty.");
        return;
    }

    juce::Image snap;

#if JUCE_WINDOWS
    toFront(true);
    auto screenBounds = editor->getScreenBounds();
    const int w = screenBounds.getWidth();
    const int h = screenBounds.getHeight();
    const int x = screenBounds.getX();
    const int y = screenBounds.getY();

    if (w > 0 && h > 0)
    {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
        HGDIOBJ hOld = SelectObject(hdcMem, hbm);

        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        snap = juce::Image(juce::Image::RGB, w, h, false);
        juce::Image::BitmapData destData(snap, juce::Image::BitmapData::writeOnly);

        std::vector<uint32_t> rawPixels(static_cast<size_t>(w * h));
        GetDIBits(hdcMem, hbm, 0, h, rawPixels.data(), &bmi, DIB_RGB_COLORS);

        for (int row = 0; row < h; ++row)
        {
            const auto* src = rawPixels.data() + row * w;
            for (int col = 0; col < w; ++col)
            {
                uint32_t c = src[col];
                uint8_t b = static_cast<uint8_t>(c & 0xFF);
                uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFF);
                uint8_t r = static_cast<uint8_t>((c >> 16) & 0xFF);
                destData.setPixelColour(col, row, juce::Colour(r, g, b));
            }
        }

        SelectObject(hdcMem, hOld);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
    }
#endif

    if (!snap.isValid())
    {
        snap = editor->createComponentSnapshot(bounds);
    }

    if (!snap.isValid())
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] captureSnapshot: failed to create component snapshot.");
        return;
    }

    auto dirs = abdaudiolab::core::resolveLabDataDirectories();
    juce::File snapshotsDir = dirs.dataRoot.getChildFile("snapshots");
    if (!snapshotsDir.exists())
        snapshotsDir.createDirectory();

    juce::File globalSnapshotsDir = abdaudiolab::core::PluginHostManager::getDefaultCacheFile()
                                        .getParentDirectory()
                                        .getChildFile("snapshots");
    if (!globalSnapshotsDir.exists())
        globalSnapshotsDir.createDirectory();

    juce::String safeName = pluginName.isNotEmpty()
        ? juce::File::createLegalFileName(pluginName)
        : "vst_snapshot";

    juce::File targetFile = snapshotsDir.getChildFile(safeName + ".png");
    juce::File globalTargetFile = globalSnapshotsDir.getChildFile(safeName + ".png");

    {
        juce::FileOutputStream stream(targetFile);
        if (stream.openedOk())
        {
            stream.setPosition(0);
            stream.truncate();
            juce::PNGImageFormat png;
            png.writeImageToStream(snap, stream);
            juce::Logger::writeToLog("[PluginWindow] Snapshot PNG saved to: " + targetFile.getFullPathName());
        }
        else
        {
            juce::Logger::writeToLog("[PluginWindow ERROR] Failed to open stream for snapshot file: " + targetFile.getFullPathName());
        }
    }

    if (targetFile.existsAsFile())
    {
        targetFile.copyFileTo(globalTargetFile);
        juce::Logger::writeToLog("[PluginWindow] Snapshot PNG synced to global catalog: " + globalTargetFile.getFullPathName());
    }

    juce::MemoryOutputStream memStream;
    juce::PNGImageFormat png;
    png.writeImageToStream(snap, memStream);
    juce::String base64Data = "data:image/png;base64," + juce::Base64::toBase64(memStream.getData(), memStream.getDataSize());

    if (onSnapshotCallback != nullptr)
        onSnapshotCallback(targetFile, base64Data);
}

PluginWindowController::PluginWindowController() = default;

PluginWindowController::~PluginWindowController()
{
    juce::Logger::writeToLog("[PluginWindowController] Destructor called.");
    closePluginWindow();
}

void PluginWindowController::captureSnapshot()
{
    if (activeWindow != nullptr && isWindowOpen())
        activeWindow->captureSnapshot(currentPluginName, onSnapshotCaptured);
}

void PluginWindowController::showPluginWindow(juce::AudioPluginInstance* plugin, const juce::String& windowTitle)
{
    juce::Logger::writeToLog("[PluginWindow] showPluginWindow called. WindowTitle: " + windowTitle);

    if (plugin == nullptr)
    {
        closePluginWindow();
        juce::Logger::writeToLog("[PluginWindow WARNING] Cannot show window: plugin is nullptr.");
        return;
    }

    // Idempotent guard: if window is already open, bring it to front
    if (activeWindow != nullptr && isWindowOpen())
    {
        activeWindow->toFront(true);
        return;
    }

    closePluginWindow();

    juce::Logger::writeToLog("[PluginWindow] Plugin name: '" + plugin->getName()
        + "', hasEditor: " + juce::String(plugin->hasEditor() ? "YES" : "NO"));

    juce::Logger::writeToLog("[PluginWindow] Calling plugin->createEditorIfNeeded()...");
    juce::AudioProcessorEditor* editor = nullptr;
    try
    {
        editor = plugin->createEditorIfNeeded();
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("[PluginWindow ERROR] Exception in createEditorIfNeeded(): " + juce::String(e.what()));
        return;
    }
    catch (...)
    {
        juce::Logger::writeToLog("[PluginWindow ERROR] Unknown native exception in createEditorIfNeeded().");
        return;
    }

    if (editor == nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] plugin->createEditorIfNeeded() returned nullptr for: " + plugin->getName());
        return;
    }

    juce::Logger::writeToLog("[PluginWindow] createEditorIfNeeded returned valid editor pointer: "
        + juce::String::toHexString((juce::uint64)editor)
        + " [Bounds: " + editor->getBounds().toString() + "]");

    juce::String title = windowTitle.isNotEmpty() ? windowTitle : plugin->getName();
    currentPluginName = title;
    activeWindow = std::make_unique<PluginWindow>(title,
                                                  std::unique_ptr<juce::AudioProcessorEditor>(editor),
                                                  [this] { closePluginWindow(); });

    if (onWindowStateChanged != nullptr)
        onWindowStateChanged(true);
}

void PluginWindowController::showPluginWindow(core::PluginHostManager& hostManager, const juce::String& windowTitle)
{
    if (!hostManager.hasActivePlugin())
    {
        juce::Logger::writeToLog("[PluginWindow WARNING] Cannot show window: PluginHostManager has no active plugin.");
        closePluginWindow();
        return;
    }

    juce::String title = windowTitle.isNotEmpty()
        ? windowTitle
        : juce::String(hostManager.getActivePluginIdentity().name);

    showPluginWindow(hostManager.getActivePluginInstance(), title);
}

void PluginWindowController::closePluginWindow()
{
    if (activeWindow != nullptr)
    {
        juce::Logger::writeToLog("[PluginWindow] Closing active plugin window.");
        activeWindow->setVisible(false);
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
