#pragma once

#include "SoundIdTheme.h"
#include "../BuildVersion.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Modern Floating Splash Screen for ABDAudioLab ("Técnica Refinada").
 * Clean 45% / 55% split, geometric image clip without blurry vignettes,
 * and minimal 2.5px progress bar with Inter typography.
 */
class SoundIdSplashScreen : public juce::Component,
                            public juce::Timer
{
public:
    explicit SoundIdSplashScreen(bool allowClose = false, std::function<void()> onClose = nullptr)
        : isCloseable(allowClose),
          onCloseCallback(std::move(onClose))
    {
        setSize(560, 320);
        loadSplashArtImage();

        if (isCloseable)
        {
            setWantsKeyboardFocus(true);
            btnClose.setButtonText(juce::String::fromUTF8(u8"\u2715"));
            btnClose.setTooltip("Close");
            btnClose.setColour(juce::TextButton::buttonColourId, juce::Colour(0x66000000));
            btnClose.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0x99000000));
            btnClose.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btnClose.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btnClose.onClick = [this] {
                if (onCloseCallback)
                    onCloseCallback();
            };
            addAndMakeVisible(btnClose);

            btnCheckUpdates.setButtonText("Check for Updates...");
            btnCheckUpdates.setTooltip("Check GitHub repository for newer releases of ABDAudioLab");
            btnCheckUpdates.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
            btnCheckUpdates.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
            btnCheckUpdates.onClick = [this] {
                if (onCheckUpdatesCallback)
                    onCheckUpdatesCallback();
            };
            addAndMakeVisible(btnCheckUpdates);

            lblCredits.setText(juce::String::fromUTF8(u8"\u00A9 2026 ABD Synths \u2022 Audio Lab Research Group"), juce::dontSendNotification);
            lblCredits.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
            lblCredits.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
            lblCredits.setJustificationType(juce::Justification::left);
            addAndMakeVisible(lblCredits);
        }

        lblCategory.setText(juce::String::fromUTF8(u8"ABD SYNTHS  \u2022  HARDWARE PROFILING LAB"), juce::dontSendNotification);
        lblCategory.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::bold));
        lblCategory.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        lblCategory.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblCategory);

        lblTitle.setText("ABDAudioLab", juce::dontSendNotification);
        lblTitle.setFont(juce::FontOptions("Inter", 24.0f, juce::Font::bold));
        lblTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        lblTitle.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblTitle);

        lblSubtitle.setText("Hardware Audio & ACB Profiling System", juce::dontSendNotification);
        lblSubtitle.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
        lblSubtitle.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        lblSubtitle.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblSubtitle);

        lblVersion.setText("v" + juce::String(version::kAppVersion) + "  " + juce::String::fromUTF8(u8"\u2022") + "  Build " + juce::String(version::kBuildNumber) + " (" + juce::String(version::kBuildDate) + ")", juce::dontSendNotification);
        lblVersion.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        lblVersion.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
        lblVersion.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblVersion);

        lblDspEngine.setText(juce::String::fromUTF8(u8"DSP Engine: Farina \u2022 Wiener-Hammerstein \u2022 NAM / RTNeural \u2022 SysEx"), juce::dontSendNotification);
        lblDspEngine.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        lblDspEngine.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        lblDspEngine.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblDspEngine);

        lblStatus.setText("Scanning Audio Interfaces & ASIO Drivers...", juce::dontSendNotification);
        lblStatus.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        lblStatus.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        lblStatus.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblStatus);

        startTimerHz(60);
    }

    ~SoundIdSplashScreen() override
    {
        stopTimer();
    }

    void loadSplashArtImage()
    {
        juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        juce::File f = exeDir.getChildFile("assets/splash_art.jpg");
        if (!f.existsAsFile()) f = exeDir.getChildFile("../assets/splash_art.jpg");
        if (!f.existsAsFile()) f = exeDir.getChildFile("../../assets/splash_art.jpg");
        if (!f.existsAsFile()) f = exeDir.getChildFile("../../../assets/splash_art.jpg");
        if (!f.existsAsFile()) f = juce::File::getCurrentWorkingDirectory().getChildFile("assets/splash_art.jpg");
        if (!f.existsAsFile()) f = juce::File("d:/desarrollos/ABDSynths/ABDAudioLab/assets/splash_art.jpg");

        if (f.existsAsFile())
        {
            splashArt = juce::ImageFileFormat::loadFrom(f);
        }
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
        spinnerPhase += 0.03f;
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

        // 1. Clean outer shadow
        g.setColour(juce::Colour(0x22000000));
        g.fillRoundedRectangle(bounds, 14.0f);

        auto card = bounds.reduced(2.0f);
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(card, 12.0f);

        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(card, 12.0f, 1.0f);

        // 2. Left Panel Artwork: 45% exact split with geometric clip
        float leftWidth = card.getWidth() * 0.44f;
        auto leftImageBounds = card.removeFromLeft(leftWidth);

        if (splashArt.isValid())
        {
            juce::Path clipPath;
            clipPath.addRoundedRectangle(leftImageBounds.getX(), leftImageBounds.getY(),
                                         leftImageBounds.getWidth(), leftImageBounds.getHeight(),
                                         12.0f, 12.0f, true, false, true, false);
            g.saveState();
            g.reduceClipRegion(clipPath);

            // Clean scaled image without blurry vignette
            g.drawImage(splashArt, leftImageBounds, juce::RectanglePlacement::fillDestination);

            // Light cooling/desaturation tint to align with Nordic light aesthetic
            g.setColour(juce::Colour(0x1800a86b));
            g.fillRect(leftImageBounds);

            g.restoreState();

            // 1px clean vertical separator between image and right content
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawLine(leftImageBounds.getRight(), card.getY(), leftImageBounds.getRight(), card.getBottom(), 1.0f);
        }

        // 3. Version badge background pill (reduced vertical padding to 2px, height 17px)
        auto rightArea = card.reduced(24.0f, 20.0f);
        auto badgeArea = juce::Rectangle<float>(rightArea.getX(), rightArea.getY() + 92.0f, 62.0f, 17.0f);
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(badgeArea, 3.5f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(badgeArea, 3.5f, 1.0f);

        if (!isCloseable)
        {
            // 4. Minimalist Progress Bar (2.5px height, moved up 8px from bottom)
            float barY = card.getBottom() - 40.0f;
            float barX = card.getX() + 24.0f;
            float barW = card.getWidth() - 48.0f;
            auto barArea = juce::Rectangle<float>(barX, barY, barW, 2.5f);

            g.setColour(SoundIdTheme::borderSubtle);
            g.fillRoundedRectangle(barArea, 1.25f);

            if (progress >= 0.0f)
            {
                auto fillArea = barArea.withWidth(barArea.getWidth() * std::clamp(progress, 0.0f, 1.0f));
                g.setColour(SoundIdTheme::accentGreen);
                g.fillRoundedRectangle(fillArea, 1.25f);
            }
            else
            {
                // Indeterminate subtle shimmer
                float glowX = barArea.getX() + barArea.getWidth() * spinnerPhase;
                auto glowArea = juce::Rectangle<float>(glowX - 40.0f, barArea.getY(), 80.0f, barArea.getHeight()).getIntersection(barArea);
                g.setColour(SoundIdTheme::accentGreen);
                g.fillRoundedRectangle(glowArea, 1.25f);
            }
        }
        else
        {
            float barY = card.getBottom() - 62.0f;
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawHorizontalLine(static_cast<int>(barY), card.getX() + leftWidth + 24.0f, card.getRight() - 20.0f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int leftW = static_cast<int>(bounds.getWidth() * 0.44f);
        bounds.removeFromLeft(leftW + 24); // Reserve left side for artwork + margin
        bounds.reduce(0, 20);
        bounds.removeFromRight(20);

        lblCategory.setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(4);
        lblTitle.setBounds(bounds.removeFromTop(32));
        bounds.removeFromTop(2);
        lblSubtitle.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(12);

        // Version badge line
        lblVersion.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(14);

        // DSP Engine description line
        lblDspEngine.setBounds(bounds.removeFromTop(18));

        if (isCloseable)
        {
            btnClose.setBounds(10, 10, 24, 24);
            bounds.removeFromTop(4);
            lblCredits.setBounds(bounds.removeFromTop(18));

            btnCheckUpdates.setBounds(leftW + 24, getHeight() - 50, 144, 26);
            lblStatus.setBounds(juce::Rectangle<int>(leftW + 24 + 152, getHeight() - 48, getWidth() - leftW - 24 - 152 - 20, 22));
        }
        else
        {
            // Status message above progress bar (moved up 8px)
            lblStatus.setBounds(juce::Rectangle<int>(leftW + 24, getHeight() - 64, getWidth() - leftW - 48, 20));
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (isCloseable && key.isKeyCode(juce::KeyPress::escapeKey))
        {
            if (onCloseCallback)
                onCloseCallback();
            return true;
        }
        return false;
    }

    std::function<void()> onCheckUpdatesCallback;

private:
    juce::Label lblCategory;
    juce::Label lblTitle;
    juce::Label lblSubtitle;
    juce::Label lblVersion;
    juce::Label lblDspEngine;
    juce::Label lblCredits;
    juce::Label lblStatus;
    juce::TextButton btnClose;
    juce::TextButton btnCheckUpdates;

    bool isCloseable { false };
    std::function<void()> onCloseCallback;

    juce::Image splashArt;
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
    explicit SoundIdSplashWindow(bool allowClose = false)
    {
        setOpaque(false);
        setAlwaysOnTop(true);

        splashComp = std::make_unique<SoundIdSplashScreen>(allowClose, [this] {
            dismiss([this] {
                if (onCloseRequest)
                    onCloseRequest();
            });
        });
        splashComp->onCheckUpdatesCallback = [this] {
            if (onCheckUpdates)
                onCheckUpdates();
        };
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

    std::function<void()> onCloseRequest;
    std::function<void()> onCheckUpdates;

private:
    std::unique_ptr<SoundIdSplashScreen> splashComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSplashWindow)
};

} // namespace abdaudiolab::gui
