#pragma once

#include "SoundIdTheme.h"
#include "../BuildVersion.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Modern Floating Splash Screen for ABDAudioLab with status telemetry and smooth fade-out.
 */
class SoundIdSplashScreen : public juce::Component,
                            public juce::Timer
{
public:
    SoundIdSplashScreen()
    {
        setSize(460, 260);

        lblTitle.setText("ABDAudioLab", juce::dontSendNotification);
        lblTitle.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        lblTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        lblTitle.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblTitle);

        lblSubtitle.setText("Hardware Audio & ACB Profiling Laboratory", juce::dontSendNotification);
        lblSubtitle.setFont(juce::FontOptions(12.5f, juce::Font::plain));
        lblSubtitle.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        lblSubtitle.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblSubtitle);

        lblVersion.setText("v" + juce::String(version::kAppVersion) + " (Build " + juce::String(version::kBuildNumber) + ")", juce::dontSendNotification);
        lblVersion.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        lblVersion.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
        lblVersion.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblVersion);

        lblStatus.setText("Initializing Audio & Hardware Engines...", juce::dontSendNotification);
        lblStatus.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        lblStatus.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        lblStatus.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblStatus);

        startTimerHz(60);
    }

    ~SoundIdSplashScreen() override
    {
        stopTimer();
    }

    void setStatusMessage(const juce::String& msg, float progress0to1 = -1.0f)
    {
        lblStatus.setText(msg, juce::dontSendNotification);
        if (progress0to1 >= 0.0f)
            progress = progress0to1;
        repaint();
    }

    void startDismissAnimation(std::function<void()> onDismissCompleted)
    {
        dismissCallback = std::move(onDismissCompleted);
        isDismissing = true;
    }

    void timerCallback() override
    {
        spinnerPhase += 0.04f;
        if (spinnerPhase > 1.0f) spinnerPhase -= 1.0f;

        if (isDismissing)
        {
            dismissAlpha -= 0.08f;
            if (dismissAlpha <= 0.0f)
            {
                stopTimer();
                if (dismissCallback)
                    dismissCallback();
                return;
            }
            setAlpha(dismissAlpha);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Soft drop shadow card
        g.setColour(juce::Colour(0x22000000));
        g.fillRoundedRectangle(bounds, 16.0f);

        auto card = bounds.reduced(3.0f);
        g.setColour(SoundIdTheme::bgLight);
        g.fillRoundedRectangle(card, 14.0f);

        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(card, 14.0f, 1.2f);

        // 2. Decorative Top Accent Line
        auto accentLine = card.removeFromTop(4.0f);
        g.setColour(SoundIdTheme::accentGreen);
        g.fillRoundedRectangle(accentLine, 2.0f);

        // 3. Progress / Loading Bar
        auto barArea = juce::Rectangle<float>(40.0f, static_cast<float>(getHeight() - 48), static_cast<float>(getWidth() - 80), 4.0f);
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(barArea, 2.0f);

        if (progress >= 0.0f)
        {
            auto fillArea = barArea.withWidth(barArea.getWidth() * std::clamp(progress, 0.0f, 1.0f));
            g.setColour(SoundIdTheme::accentGreen);
            g.fillRoundedRectangle(fillArea, 2.0f);
        }
        else
        {
            // Indeterminate pulsing shimmer
            float glowX = barArea.getX() + barArea.getWidth() * spinnerPhase;
            auto glowArea = juce::Rectangle<float>(glowX - 35.0f, barArea.getY(), 70.0f, barArea.getHeight()).getIntersection(barArea);
            g.setColour(SoundIdTheme::accentGreen);
            g.fillRoundedRectangle(glowArea, 2.0f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        bounds.removeFromTop(16);

        lblTitle.setBounds(bounds.removeFromTop(34));
        bounds.removeFromTop(2);
        lblSubtitle.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(4);
        lblVersion.setBounds(bounds.removeFromTop(18));

        lblStatus.setBounds(bounds.removeFromBottom(38));
    }

private:
    juce::Label lblTitle;
    juce::Label lblSubtitle;
    juce::Label lblVersion;
    juce::Label lblStatus;

    float progress { -1.0f };
    float spinnerPhase { 0.0f };
    bool isDismissing { false };
    float dismissAlpha { 1.0f };
    std::function<void()> dismissCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSplashScreen)
};

/**
 * @brief Floating borderless window holding SoundIdSplashScreen
 */
class SoundIdSplashWindow : public juce::Component
{
public:
    SoundIdSplashWindow()
    {
        setOpaque(false);
        setAlwaysOnTop(true);

        splashComp = std::make_unique<SoundIdSplashScreen>();
        addAndMakeVisible(splashComp.get());
        setSize(splashComp->getWidth(), splashComp->getHeight());

        addToDesktop(juce::ComponentPeer::windowHasDropShadow | juce::ComponentPeer::windowIsTemporary);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    ~SoundIdSplashWindow() override
    {
        removeFromDesktop();
    }

    void setStatus(const juce::String& msg, float progress = -1.0f)
    {
        if (splashComp)
            splashComp->setStatusMessage(msg, progress);
    }

    void dismiss(std::function<void()> onDone)
    {
        if (splashComp)
        {
            splashComp->startDismissAnimation([this, onDone = std::move(onDone)]() {
                setVisible(false);
                if (onDone) onDone();
            });
        }
        else if (onDone)
        {
            onDone();
        }
    }

private:
    std::unique_ptr<SoundIdSplashScreen> splashComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSplashWindow)
};

} // namespace abdaudiolab::gui
