#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <JUCE/JuceScopeComponent.h>
#include <Core/ScopeTap.h>
#include <functional>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @class ScopeContentComponent
 * @brief Embeddable control panel and canvas for JuceScopeComponent.
 */
class ScopeContentComponent : public juce::Component,
                              public juce::Button::Listener
{
public:
    ScopeContentComponent(abd::scope::ScopeTap* tapToMonitor, float sampleRate)
        : scopeComponent(tapToMonitor, sampleRate)
    {
        // 1. Configure Visualizer Aesthetic (AudioLab Emerald & Dark Lab Canvas)
        scopeComponent.setTraceColour(juce::Colour(0xff00e676), juce::Colour(0xff00c3ff));
        scopeComponent.setBackgroundColour(juce::Colour(0xff06120a));
        scopeComponent.setMode(abd::scope::NativeScopeMode::Oscilloscope);
        addAndMakeVisible(scopeComponent);

        // 2. Setup Mode Switch Buttons
        setupButton(btnOsc, "Oscilloscope", true);
        setupButton(btnSpec, "Spectrum FFT", false);
        setupButton(btnLiss, "Lissajous", false);
        setupButton(btnPhase, "Phase Meter", false);
        setupButton(btnFreeze, "Freeze", false);

        btnOsc.addListener(this);
        btnSpec.addListener(this);
        btnLiss.addListener(this);
        btnPhase.addListener(this);
        btnFreeze.addListener(this);

        btnFreeze.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14221a));
        btnFreeze.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e676));

        // Header Title Label
        titleLabel.setText("ABDSCOPE - HARDWARE MONITOR", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88a090));
        addAndMakeVisible(titleLabel);
    }

    ~ScopeContentComponent() override
    {
        btnOsc.removeListener(this);
        btnSpec.removeListener(this);
        btnLiss.removeListener(this);
        btnPhase.removeListener(this);
        btnFreeze.removeListener(this);
    }

    void setTap(abd::scope::ScopeTap* tap) noexcept
    {
        scopeComponent.setTap(tap);
    }

    void setSampleRate(float sampleRate) noexcept
    {
        scopeComponent.setSampleRate(sampleRate);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark lab header background
        g.fillAll(juce::Colour(0xff0a1610));

        // Top bar separator line
        g.setColour(juce::Colour(0xff182e22));
        g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto topBar = bounds.removeFromTop(44).reduced(8, 6);

        titleLabel.setBounds(topBar.removeFromLeft(200));
        topBar.removeFromLeft(10);

        const int btnW = 96;
        btnOsc.setBounds(topBar.removeFromLeft(btnW).reduced(2, 0));
        btnSpec.setBounds(topBar.removeFromLeft(btnW).reduced(2, 0));
        btnLiss.setBounds(topBar.removeFromLeft(btnW).reduced(2, 0));
        btnPhase.setBounds(topBar.removeFromLeft(btnW).reduced(2, 0));

        btnFreeze.setBounds(topBar.removeFromRight(80).reduced(2, 0));

        // Scope canvas fills the rest of the window
        scopeComponent.setBounds(bounds);
    }

    void buttonClicked(juce::Button* btn) override
    {
        if (btn == &btnOsc)
        {
            scopeComponent.setMode(abd::scope::NativeScopeMode::Oscilloscope);
            updateActiveButton(&btnOsc);
        }
        else if (btn == &btnSpec)
        {
            scopeComponent.setMode(abd::scope::NativeScopeMode::Spectrum);
            updateActiveButton(&btnSpec);
        }
        else if (btn == &btnLiss)
        {
            scopeComponent.setMode(abd::scope::NativeScopeMode::Lissajous);
            updateActiveButton(&btnLiss);
        }
        else if (btn == &btnPhase)
        {
            scopeComponent.setMode(abd::scope::NativeScopeMode::PhaseMeter);
            updateActiveButton(&btnPhase);
        }
        else if (btn == &btnFreeze)
        {
            isFrozen = !isFrozen;
            btnFreeze.setButtonText(isFrozen ? "Resume" : "Freeze");
            btnFreeze.setColour(juce::TextButton::buttonColourId, isFrozen ? juce::Colour(0xffef4444) : juce::Colour(0xff14221a));
            btnFreeze.setColour(juce::TextButton::textColourOffId, isFrozen ? juce::Colours::white : juce::Colour(0xff00e676));

            // When frozen, we disconnect the tap so the visualizer holds the current buffer
            if (isFrozen)
            {
                scopeComponent.setTap(nullptr);
            }
            else
            {
                scopeComponent.setTap(boundTap);
            }
        }
    }

    void setBoundTap(abd::scope::ScopeTap* tap) noexcept
    {
        boundTap = tap;
        if (!isFrozen)
            scopeComponent.setTap(tap);
    }

private:
    void setupButton(juce::TextButton& btn, const juce::String& text, bool isActive)
    {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId, isActive ? juce::Colour(0xff00e676) : juce::Colour(0xff14221a));
        btn.setColour(juce::TextButton::textColourOffId, isActive ? juce::Colour(0xff06120a) : juce::Colour(0xffb0c8b8));
        btn.setColour(juce::TextButton::textColourOnId, isActive ? juce::Colour(0xff06120a) : juce::Colour(0xffb0c8b8));
        addAndMakeVisible(btn);
    }

    void updateActiveButton(juce::TextButton* activeBtn)
    {
        std::array<juce::TextButton*, 4> modeBtns { &btnOsc, &btnSpec, &btnLiss, &btnPhase };
        for (auto* b : modeBtns)
        {
            bool isAct = (b == activeBtn);
            b->setColour(juce::TextButton::buttonColourId, isAct ? juce::Colour(0xff00e676) : juce::Colour(0xff14221a));
            b->setColour(juce::TextButton::textColourOffId, isAct ? juce::Colour(0xff06120a) : juce::Colour(0xffb0c8b8));
            b->repaint();
        }
    }

    abd::scope::JuceScopeComponent scopeComponent;
    abd::scope::ScopeTap* boundTap { nullptr };

    juce::Label titleLabel;
    juce::TextButton btnOsc;
    juce::TextButton btnSpec;
    juce::TextButton btnLiss;
    juce::TextButton btnPhase;
    juce::TextButton btnFreeze;
    bool isFrozen { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeContentComponent)
};

/**
 * @class ScopeFloatingWindow
 * @brief Floating, resizable standalone tool window for live ABDScope telemetry.
 */
class ScopeFloatingWindow : public juce::DocumentWindow
{
public:
    ScopeFloatingWindow(abd::scope::ScopeTap* tapToMonitor,
                        float sampleRate,
                        std::function<void()> onClose = nullptr)
        : DocumentWindow("ABDScope - Live Audio Visualizer",
                         juce::Colour(0xff06120a),
                         DocumentWindow::allButtons),
          onCloseCallback(std::move(onClose)),
          tap(tapToMonitor)
    {
        setUsingNativeTitleBar(true);
        auto* content = new ScopeContentComponent(tapToMonitor, sampleRate);
        content->setBoundTap(tapToMonitor);
        setContentOwned(content, true);

        setResizable(true, true);
        setResizeLimits(540, 360, 1920, 1200);
        centreWithSize(740, 480);
    }

    ~ScopeFloatingWindow() override
    {
        if (tap != nullptr)
            tap->setActive(false);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        if (tap != nullptr)
            tap->setActive(false);

        if (onCloseCallback)
            onCloseCallback();
    }

    void updateSampleRate(float sampleRate)
    {
        if (auto* content = dynamic_cast<ScopeContentComponent*>(getContentComponent()))
            content->setSampleRate(sampleRate);
    }

    void setTap(abd::scope::ScopeTap* newTap)
    {
        tap = newTap;
        if (auto* content = dynamic_cast<ScopeContentComponent*>(getContentComponent()))
            content->setBoundTap(newTap);
    }

private:
    std::function<void()> onCloseCallback;
    abd::scope::ScopeTap* tap { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeFloatingWindow)
};

} // namespace abdaudiolab::gui
