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
        setSize(580, 310);
        loadSplashArtImage();

        lblCategory.setText("ABD SYNTHS  |  HARDWARE PROFILING LAB", juce::dontSendNotification);
        lblCategory.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        lblCategory.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        lblCategory.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblCategory);

        lblTitle.setText("ABDAudioLab", juce::dontSendNotification);
        lblTitle.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        lblTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        lblTitle.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblTitle);

        lblSubtitle.setText("Hardware Audio & ACB Profiling System", juce::dontSendNotification);
        lblSubtitle.setFont(juce::FontOptions(12.0f, juce::Font::plain));
        lblSubtitle.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        lblSubtitle.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblSubtitle);

        lblVersion.setText("v" + juce::String(version::kAppVersion) + " (Build " + juce::String(version::kBuildNumber) + ")", juce::dontSendNotification);
        lblVersion.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        lblVersion.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
        lblVersion.setJustificationType(juce::Justification::left);
        addAndMakeVisible(lblVersion);

        lblStatus.setText("Initializing Audio & Hardware Engines...", juce::dontSendNotification);
        lblStatus.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        lblStatus.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
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
        if (!f.existsAsFile())
            f = exeDir.getChildFile("../assets/splash_art.jpg");
        if (!f.existsAsFile())
            f = exeDir.getChildFile("../../assets/splash_art.jpg");
        if (!f.existsAsFile())
            f = exeDir.getChildFile("../../../assets/splash_art.jpg");
        if (!f.existsAsFile())
            f = exeDir.getChildFile("../../../../assets/splash_art.jpg");
        if (!f.existsAsFile())
            f = juce::File::getCurrentWorkingDirectory().getChildFile("assets/splash_art.jpg");
        if (!f.existsAsFile())
            f = juce::File("d:/desarrollos/ABDSynths/ABDAudioLab/assets/splash_art.jpg");

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

        // 1. Outer drop shadow
        g.setColour(juce::Colour(0x30000000));
        g.fillRoundedRectangle(bounds, 16.0f);

        auto card = bounds.reduced(3.0f);
        juce::Colour bgCardCol = SoundIdTheme::bgLight;
        g.setColour(bgCardCol);
        g.fillRoundedRectangle(card, 14.0f);

        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(card, 14.0f, 1.2f);

        // 2. Left Sidebar Artwork (Adobe-Style Split)
        const float artWidth = 230.0f;
        auto artBounds = card.removeFromLeft(artWidth);

        if (splashArt.isValid())
        {
            juce::Path leftClipPath;
            leftClipPath.addRoundedRectangle(artBounds.getX(), artBounds.getY(), artBounds.getWidth(), artBounds.getHeight(),
                                              14.0f, 14.0f, true, false, true, false);
            g.saveState();
            g.reduceClipRegion(leftClipPath);

            // Draw scaled image (cover/crop fill)
            g.drawImage(splashArt, artBounds, juce::RectanglePlacement::fillDestination);

            // Smooth right edge fade gradient into card background
            juce::ColourGradient grad(juce::Colours::transparentBlack, artBounds.getRight() - 70.0f, artBounds.getY(),
                                       bgCardCol, artBounds.getRight(), artBounds.getY(), false);
            g.setGradientFill(grad);
            g.fillRect(artBounds);

            g.restoreState();
        }

        // 3. Top Accent Line (Right Content Area)
        auto rightArea = card;
        auto accentLine = rightArea.removeFromTop(4.0f).withTrimmedLeft(10.0f);
        g.setColour(SoundIdTheme::accentGreen);
        g.fillRoundedRectangle(accentLine, 2.0f);

        // 4. Feature Badges (Center Right - 2 Rows)
        struct BadgeItem { juce::String text; float width; };
        std::vector<std::vector<BadgeItem>> badgeRows = {
            { { "Farina Sweep", 96.0f }, { "Wiener-Hammerstein", 120.0f }, { "Catmull-Rom", 80.0f } },
            { { "NAM / RTNeural", 96.0f }, { "MIDI SysEx ID", 100.0f }, { "2048-pt FFT", 100.0f } }
        };

        float curY = 158.0f;
        for (const auto& row : badgeRows)
        {
            float curX = 252.0f;
            for (const auto& item : row)
            {
                juce::Rectangle<float> tagBg(curX, curY, item.width, 18.0f);
                g.setColour(juce::Colour(0x10000000));
                g.fillRoundedRectangle(tagBg, 4.0f);
                g.setColour(SoundIdTheme::borderCard);
                g.drawRoundedRectangle(tagBg, 4.0f, 1.0f);

                g.setColour(SoundIdTheme::textSecondary);
                g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText(item.text, tagBg, juce::Justification::centred);
                curX += item.width + 4.0f;
            }
            curY += 23.0f;
        }

        // 5. Progress / Loading Bar
        auto barArea = juce::Rectangle<float>(252.0f, static_cast<float>(getHeight() - 32), 304.0f, 4.0f);
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
        auto bounds = getLocalBounds();
        bounds.removeFromLeft(252); // Reserve left side for artwork
        bounds.reduce(0, 18);
        bounds.removeFromRight(20);

        lblCategory.setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(2);
        lblTitle.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(2);
        lblSubtitle.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(4);
        lblVersion.setBounds(bounds.removeFromTop(16));

        lblStatus.setBounds(juce::Rectangle<int>(252, getHeight() - 58, 304, 22));
    }

private:
    juce::Label lblCategory;
    juce::Label lblTitle;
    juce::Label lblSubtitle;
    juce::Label lblVersion;
    juce::Label lblStatus;

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
