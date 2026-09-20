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

    void simulateClick()
    {
        if (onClick)
            onClick();
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

        // 1. Hardware Thumbnail icon on the left (or vector plug icon if software plugin)
        bool isPlugin = hwDisplayName.containsIgnoreCase("Plugin") ||
                        hwDisplayName.containsIgnoreCase("[Instrument") ||
                        hwDisplayName.containsIgnoreCase("[Effect") ||
                        hwDisplayName.containsIgnoreCase("[Instrumento]") ||
                        hwDisplayName.containsIgnoreCase("[Efecto]") ||
                        hwFunctionName.containsIgnoreCase("Virtual");

        if (hwThumbnail.isValid())
        {
            auto thumbArea = content.removeFromLeft(30.0f);
            g.drawImage(hwThumbnail, thumbArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
            content.removeFromLeft(6.0f);
        }
        else if (isPlugin)
        {
            auto thumbArea = content.removeFromLeft(20.0f);
            auto c = thumbArea.getCentre();
            g.setColour(SoundIdTheme::accentBlue);
            g.drawLine(c.x - 3.0f, c.y - 6.5f, c.x - 3.0f, c.y - 3.0f, 1.2f);
            g.drawLine(c.x + 3.0f, c.y - 6.5f, c.x + 3.0f, c.y - 3.0f, 1.2f);
            juce::Path plug;
            plug.startNewSubPath(c.x - 5.5f, c.y - 3.0f);
            plug.lineTo(c.x + 5.5f, c.y - 3.0f);
            plug.lineTo(c.x + 5.5f, c.y + 2.0f);
            plug.lineTo(c.x + 2.5f, c.y + 5.5f);
            plug.lineTo(c.x - 2.5f, c.y + 5.5f);
            plug.lineTo(c.x - 5.5f, c.y + 2.0f);
            plug.closeSubPath();
            g.strokePath(plug, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine(c.x, c.y + 5.5f, c.x, c.y + 8.5f, 1.3f);
            content.removeFromLeft(6.0f);
        }

        // 2. Status LED dot indicator (Traffic light) on the right
        auto ledArea = content.removeFromRight(14.0f);
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
