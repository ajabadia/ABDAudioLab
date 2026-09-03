/**
 * @file LabApplication.h
 * @brief JUCE application bootstrap: main window and application lifecycle.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../BuildVersion.h"
#include "gui/SoundIdSplashScreen.h"
#include "gui/MainContentComponent.h"

namespace abdaudiolab
{
class LabMainWindow : public juce::DocumentWindow
{
public:
    explicit LabMainWindow(juce::String name)
        : DocumentWindow(name,
                         gui::SoundIdTheme::bgLight,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainContentComponent(), true);

        #if JUCE_IOS || JUCE_ANDROID
         setFullScreen(true);
        #else
         setResizable(true, true);
         setResizeLimits(850, 580, 1920, 1080);
         centreWithSize(getWidth(), getHeight());
        #endif

        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabMainWindow)
};

class LabApplication : public juce::JUCEApplication
{
public:
    LabApplication() = default;

    const juce::String getApplicationName() override       { return "ABDAudioLab"; }
    const juce::String getApplicationVersion() override    { return version::kAppVersion; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // 1. Show Instant Floating Splash Window (< 50ms)
        splashWindow = std::make_unique<gui::SoundIdSplashWindow>();
        splashWindow->setStatus("Scanning Audio Interfaces & ASIO Drivers...", 0.25f);

        // 2. Initialize Main Engine and Window
        juce::MessageManager::callAsync([this]() {
            if (splashWindow)
                splashWindow->setStatus("Loading Hardware Contracts & DSP Profiles...", 0.65f);

            mainWindow = std::make_unique<LabMainWindow>(getApplicationName());

            if (splashWindow)
                splashWindow->setStatus("Ready.", 1.0f);

            // 3. Smooth fade-out transition
            juce::Component::SafePointer<gui::SoundIdSplashWindow> safeSplash(splashWindow.get());
            juce::Timer::callAfterDelay(450, [safeSplash, this]() {
                if (safeSplash != nullptr)
                {
                    safeSplash->dismiss([this]() {
                        splashWindow.reset();
                    });
                }
            });
        });
    }

    void shutdown() override
    {
        splashWindow.reset();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
    }

private:
    std::unique_ptr<gui::SoundIdSplashWindow> splashWindow;
    std::unique_ptr<LabMainWindow> mainWindow;
};
} // namespace abdaudiolab

