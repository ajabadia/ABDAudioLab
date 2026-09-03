#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

enum class HardwareConnectionStatus
{
    NotApplicable, // Gray (Manual / Mock DSP)
    Connected,     // Green (SysEx/MIDI detected & ready)
    Disconnected   // Red (Automated SysEx/MIDI expected but not detected)
};

/**
 * @brief Header Hardware Selector Pill with thumbnail icon, title, submodule, and status LED.
 */
class HardwareSelectorPill : public juce::Button
{
public:
    HardwareSelectorPill() : juce::Button("HardwareSelectorPill")
    {
        setTooltip("Select target hardware device, active function, and view wiring guide");
    }

    void setHardwareInfo(const juce::String& displayName,
                         const juce::String& functionName,
                         const juce::Image& image,
                         HardwareConnectionStatus status)
    {
        hwDisplayName = displayName;
        hwFunctionName = functionName;
        hwThumbnail = image;
        connStatus = status;
        repaint();
    }

    void setConnectionStatus(HardwareConnectionStatus status)
    {
        connStatus = status;
        repaint();
    }

    void clearHardware()
    {
        hwDisplayName = "Select Target Hardware...";
        hwFunctionName.clear();
        hwThumbnail = juce::Image();
        connStatus = HardwareConnectionStatus::NotApplicable;
        repaint();
    }

    [[nodiscard]] bool hasHardwareSelected() const noexcept
    {
        return hwDisplayName != "Select Target Hardware..." && hwDisplayName != "Select Hardware";
    }

    [[nodiscard]] HardwareConnectionStatus getConnectionStatus() const noexcept { return connStatus; }
    [[nodiscard]] const juce::String& getDisplayName() const noexcept { return hwDisplayName; }
    [[nodiscard]] const juce::String& getFunctionName() const noexcept { return hwFunctionName; }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);

        // Background card
        if (shouldDrawButtonAsDown)
            g.setColour(SoundIdTheme::bgCardHover);
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(SoundIdTheme::bgCardHover);
        else
            g.setColour(SoundIdTheme::bgCard);

        g.fillRoundedRectangle(bounds, 8.0f);

        // Border
        g.setColour(shouldDrawButtonAsHighlighted ? SoundIdTheme::borderCard : SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        auto content = bounds.reduced(8.0f, 3.0f);

        // 1. Hardware Thumbnail icon on the left
        if (hwThumbnail.isValid())
        {
            auto thumbArea = content.removeFromLeft(30.0f);
            g.drawImage(hwThumbnail, thumbArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
            content.removeFromLeft(6.0f);
        }

        // 2. Chevron on the far right
        auto chevArea = content.removeFromRight(10.0f);
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("v", chevArea, juce::Justification::centred, false);
        content.removeFromRight(6.0f);

        // 3. Status LED dot indicator (Traffic light)
        auto ledArea = content.removeFromRight(12.0f);
        float ledSize = 8.0f;
        auto ledRect = juce::Rectangle<float>(ledArea.getCentreX() - ledSize * 0.5f,
                                              ledArea.getCentreY() - ledSize * 0.5f,
                                              ledSize, ledSize);

        juce::Colour ledColour = juce::Colour(0xff9ca3af); // Gray N/A / Manual
        if (connStatus == HardwareConnectionStatus::Connected)
            ledColour = SoundIdTheme::accentGreen; // Green #10b981
        else if (connStatus == HardwareConnectionStatus::Disconnected)
            ledColour = SoundIdTheme::accentRed; // Red #ef4444

        g.setColour(ledColour);
        g.fillEllipse(ledRect);

        // Outer glow halo if connected / error
        if (connStatus == HardwareConnectionStatus::Connected)
        {
            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.28f));
            g.drawEllipse(ledRect.expanded(2.0f), 1.0f);
        }
        else if (connStatus == HardwareConnectionStatus::Disconnected)
        {
            g.setColour(SoundIdTheme::accentRed.withAlpha(0.28f));
            g.drawEllipse(ledRect.expanded(2.0f), 1.0f);
        }

        content.removeFromRight(6.0f);

        // 4. Text Display (Hardware • Submodule)
        juce::String fullText = hwDisplayName;
        if (hwFunctionName.isNotEmpty())
        {
            fullText += juce::String::fromUTF8("  \xe2\x80\xa2  ") + hwFunctionName;
        }

        g.setColour(SoundIdTheme::textPrimary);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(fullText, content, juce::Justification::centredLeft, true);
    }

private:
    juce::String hwDisplayName { "Select Hardware" };
    juce::String hwFunctionName;
    juce::Image hwThumbnail;
    HardwareConnectionStatus connStatus { HardwareConnectionStatus::NotApplicable };
};

} // namespace abdaudiolab::gui
