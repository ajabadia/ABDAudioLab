#pragma once

#include "SoundIdTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace abdaudiolab::gui
{

/**
 * @brief Vector-rendered icon representing hardware control types
 * (Knob, Vertical Slider, Horizontal Slider, Push Button, Rotary Switch, Jack Port).
 */
class ControlIconComponent : public juce::Component
{
public:
    ControlIconComponent() = default;
    explicit ControlIconComponent(const juce::String& type) : controlType(type) {}

    void setControlType(const juce::String& type)
    {
        controlType = type;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        if (bounds.isEmpty()) return;

        juce::String t = controlType.toLowerCase();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        g.setColour(SoundIdTheme::textSecondary);

        if (t.contains("slider") || t.contains("fader"))
        {
            if (t.contains("hslider") || t.contains("horizontal"))
            {
                // Horizontal Slider Icon
                g.fillRect(bounds.getX() + 2.0f, cy - 1.0f, bounds.getWidth() - 4.0f, 2.0f);
                g.setColour(SoundIdTheme::textPrimary);
                g.fillRoundedRectangle(cx - 3.0f, cy - 5.0f, 6.0f, 10.0f, 1.5f);
            }
            else
            {
                // Vertical Slider Icon
                g.fillRect(cx - 1.0f, bounds.getY() + 2.0f, 2.0f, bounds.getHeight() - 4.0f);
                g.setColour(SoundIdTheme::textPrimary);
                g.fillRoundedRectangle(cx - 5.0f, cy - 3.0f, 10.0f, 6.0f, 1.5f);
            }
        }
        else if (t.contains("button") || t.contains("push") || t.contains("toggle"))
        {
            // Push Button / Toggle Icon
            auto btnR = bounds.reduced(2.0f, 2.0f);
            g.drawRoundedRectangle(btnR, 3.0f, 1.5f);
            g.setColour(SoundIdTheme::textPrimary);
            g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
        }
        else if (t.contains("switch") || t.contains("selector"))
        {
            // Rotary Switch / Selector Icon
            float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.40f;
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);
            g.setColour(SoundIdTheme::textPrimary);
            g.drawLine(cx, cy, cx + r * 0.7f, cy - r * 0.7f, 2.0f);
            g.fillEllipse(cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        }
        else if (t.contains("jack") || t.contains("port") || t.contains("in") || t.contains("out"))
        {
            // 3.5mm Jack Socket Icon
            float r1 = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;
            float r2 = r1 * 0.45f;
            g.drawEllipse(cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f, 1.5f);
            g.setColour(SoundIdTheme::textPrimary);
            g.fillEllipse(cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f);
        }
        else // Default: Knob / Rotary
        {
            // Knob Icon
            float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);
            g.setColour(SoundIdTheme::textPrimary);
            g.drawLine(cx, cy, cx + r * 0.7f, cy - r * 0.7f, 2.0f);
        }
    }

private:
    juce::String controlType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlIconComponent)
};

} // namespace abdaudiolab::gui
