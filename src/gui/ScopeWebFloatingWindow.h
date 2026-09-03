#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <JUCE/JuceWebScopeComponent.h>
#include "../audio/LabAudioEngine.h"
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @class ScopeWebFloatingWindow
 * @brief Native floating tool window embedding the WebUI ABDScope Suite (Multi-Lane & Waterfall).
 */
class ScopeWebFloatingWindow : public juce::DocumentWindow
{
public:
    ScopeWebFloatingWindow(audio::LabAudioEngine& audioEngineRef,
                           std::function<void()> onClose = nullptr)
        : DocumentWindow("ABDScope - Studio Web Telemetry",
                         juce::Colour(0xff06120a),
                         DocumentWindow::allButtons),
          audioEngine(audioEngineRef),
          onCloseCallback(std::move(onClose))
    {
        setUsingNativeTitleBar(true);

        auto webScope = std::make_unique<abd::scope::JuceWebScopeComponent>(
            audioEngineRef.getScopeCollector(),
            audioEngineRef.getCurrentSampleRate(),
            30
        );
        setContentOwned(webScope.release(), true);

        setResizable(true, true);
        setResizeLimits(640, 400, 2560, 1440);
        centreWithSize(960, 580);
    }

    ~ScopeWebFloatingWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);

        if (onCloseCallback)
            onCloseCallback();
    }

    void onWindowShown()
    {
        for (size_t i = 0; i < audioEngine.getScopeCollector().getTapCount(); ++i)
        {
            if (auto* tap = const_cast<abd::scope::ScopeTap*>(audioEngine.getScopeCollector().getTap(i)))
                tap->setActive(true);
        }
        if (auto* ws = dynamic_cast<abd::scope::JuceWebScopeComponent*>(getContentComponent()))
            ws->setSampleRate(audioEngine.getCurrentSampleRate());
    }

private:
    audio::LabAudioEngine& audioEngine;
    std::function<void()> onCloseCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeWebFloatingWindow)
};

} // namespace abdaudiolab::gui