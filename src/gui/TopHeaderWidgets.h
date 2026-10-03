/**
 * @file TopHeaderWidgets.h
 * @brief Header navigation buttons and queue badge utilities.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"
#include "SoundIdSuiteList.h"
#include "suite/SuiteIcons.h"
#include "../audio/LabStimulusGenerator.h"

namespace abdaudiolab::gui
{

/**
 * @brief Assigns stimulus badge colors and label to a queue item.
 */
inline void applyBadgeForStimulus(gui::QueueItem& item, audio::StimulusType type)
{
    if (type == audio::StimulusType::SyncPulses3)
    {
        item.badgeText = "ENV";
        item.badgeColor = juce::Colour(0xff8b5cf6);
    }
    else if (type == audio::StimulusType::AmplitudeRamp)
    {
        item.badgeText = "SAT";
        item.badgeColor = juce::Colour(0xfff59e0b);
    }
    else if (type == audio::StimulusType::SineWave1kHz)
    {
        item.badgeText = "MOD";
        item.badgeColor = juce::Colour(0xff0284c7);
    }
    else
    {
        item.badgeText = "FLT";
        item.badgeColor = juce::Colour(0xff10b981);
    }
}

/**
 * @class MonochromeInfoButton
 * @brief Minimalist circular info button with vector "i".
 */
class MonochromeInfoButton : public juce::Button
{
public:
    MonochromeInfoButton() : juce::Button("InfoButton")
    {
        setTooltip("System telemetry & active routing information");
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        if (shouldDrawButtonAsDown)
        {
            g.setColour(gui::SoundIdTheme::bgCardHover);
            g.fillEllipse(bounds);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(gui::SoundIdTheme::bgCard);
            g.fillEllipse(bounds);
        }

        // Circular outline
        g.setColour(gui::SoundIdTheme::borderCard);
        g.drawEllipse(bounds.reduced(2.0f), 1.2f);

        // "i" text
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.setColour(gui::SoundIdTheme::textPrimary);
        g.drawText("i", bounds, juce::Justification::centred, false);
    }
};

/**
 * @class ThemeToggleButton
 * @brief Vector sun/moon toggle button switching dark and light interface themes.
 */
class ThemeToggleButton : public juce::Button
{
public:
    ThemeToggleButton() : juce::Button("ThemeToggle")
    {
        setTooltip("Switch Interface Theme (Light / Dark)");
    }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(isDown ? gui::SoundIdTheme::bgCardHover.darker(0.08f)
                           : (isHighlighted ? gui::SoundIdTheme::bgCardHover : gui::SoundIdTheme::bgCard));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(gui::SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        auto c = bounds.getCentre();
        g.setColour(gui::SoundIdTheme::textPrimary);

        if (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark)
        {
            // Monochrome vector crescent moon
            juce::Path crescent;
            float r = 6.2f;
            crescent.startNewSubPath(c.x + r * 0.25f, c.y - r);
            crescent.cubicTo(c.x + r * 1.15f, c.y - r * 0.35f, c.x + r * 1.15f, c.y + r * 0.35f, c.x + r * 0.25f, c.y + r);
            crescent.cubicTo(c.x + r * 0.7f, c.y + r * 0.38f, c.x + r * 0.7f, c.y - r * 0.38f, c.x + r * 0.25f, c.y - r);
            crescent.closeSubPath();
            g.fillPath(crescent);
        }
        else
        {
            // Monochrome vector sun (circle + 8 rays)
            float r = 3.6f;
            g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
            for (int i = 0; i < 8; ++i)
            {
                float angle = static_cast<float>(i) * juce::MathConstants<float>::pi * 0.25f;
                float x1 = c.x + 5.2f * std::cos(angle);
                float y1 = c.y + 5.2f * std::sin(angle);
                float x2 = c.x + 7.6f * std::cos(angle);
                float y2 = c.y + 7.6f * std::sin(angle);
                g.drawLine(x1, y1, x2, y2, 1.2f);
            }
        }
    }
};

/**
 * @class NotificationBellButton
 * @brief Vector bell icon with numeric badge overlay and heartbeat pulse.
 *
 * Uses suite_icons::drawBell (ABDSharedAssets/icons/bell.svg equivalent).
 * When noticeCount > 0, a red badge with the count is drawn and the icon
 * pulses at 2 Hz to attract attention without blocking the workspace.
 */
class NotificationBellButton : public juce::Button,
                                private juce::Timer
{
public:
    NotificationBellButton() : juce::Button("NotificationBell")
    {
        setTooltip("Notifications");

        static const juce::String bellSvgXml =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
            "stroke=\"#FFFFFF\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
            "<path d=\"M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9\"/>"
            "<path d=\"M10.3 21a1.94 1.94 0 0 0 3.4 0\"/>"
            "</svg>";

        auto xml = juce::parseXML(bellSvgXml);
        if (xml != nullptr)
        {
            bellDrawable = juce::Drawable::createFromSVG(*xml);
            currentBellColour = juce::Colours::white;
        }
    }

    void setNoticeCount(int count)
    {
        if (noticeCount == count) return;
        noticeCount = count;

        if (noticeCount > 0 && !isTimerRunning())
            startTimer(500); // 2 Hz heartbeat
        else if (noticeCount == 0 && isTimerRunning())
        {
            stopTimer();
            pulsePhase = false;
        }
        repaint();
    }

    [[nodiscard]] int getNoticeCount() const noexcept { return noticeCount; }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);

        // Background card
        g.setColour(isDown ? gui::SoundIdTheme::bgCardHover.darker(0.08f)
                           : (isHighlighted ? gui::SoundIdTheme::bgCardHover : gui::SoundIdTheme::bgCard));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(gui::SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        // Bell icon
        auto iconArea = bounds.reduced(7.0f, 5.0f);
        juce::Colour bellCol = (noticeCount > 0)
            ? (pulsePhase ? gui::SoundIdTheme::accentAmber : gui::SoundIdTheme::accentRed)
            : (isHighlighted ? gui::SoundIdTheme::textPrimary : gui::SoundIdTheme::textSecondary);

        if (bellDrawable != nullptr)
        {
            if (currentBellColour != bellCol)
            {
                bellDrawable->replaceColour(currentBellColour, bellCol);
                currentBellColour = bellCol;
            }
            bellDrawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            suite_icons::drawBell(g, iconArea, bellCol);
        }

        // Badge with count
        if (noticeCount > 0)
        {
            float badgeSize = 13.0f;
            float pulseScale = pulsePhase ? 1.15f : 1.0f;
            float sz = badgeSize * pulseScale;
            auto badgeRect = juce::Rectangle<float>(bounds.getRight() - sz - 1.0f, bounds.getY() + 1.0f, sz, sz);

            g.setColour(gui::SoundIdTheme::accentRed);
            g.fillEllipse(badgeRect);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
            g.drawText(juce::String(noticeCount), badgeRect, juce::Justification::centred, false);
        }
    }

private:
    std::unique_ptr<juce::Drawable> bellDrawable;
    juce::Colour currentBellColour { juce::Colours::white };
    int noticeCount { 0 };
    bool pulsePhase { false };

    void timerCallback() override
    {
        pulsePhase = !pulsePhase;
        repaint();
    }
};

} // namespace abdaudiolab::gui
