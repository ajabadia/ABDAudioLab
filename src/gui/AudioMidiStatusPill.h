#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "SoundIdTheme.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab::gui
{

/**
 * @brief Header bar status pill with vector Gear icon (audio/midi config button) and 4 LED indicators:
 * Audio In, Audio Out, MIDI In, MIDI Out.
 * Displays green when configured/active, red when disconnected/unassigned.
 * Hovering displays the literal port names.
 */
class AudioMidiStatusPill : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    std::function<void()> onConfigureClicked;

    class GearButton : public juce::Component,
                       public juce::SettableTooltipClient
    {
    public:
        std::function<void()> onClick;

        GearButton()
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            setTooltip("Audio & MIDI Configuration - Select audio interfaces, buffer sizes, sample rate, and MIDI routing");
        }

        void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { isHovered = false; isDown = false; repaint(); }
        void mouseDown(const juce::MouseEvent&) override  { isDown = true; repaint(); }
        void mouseUp(const juce::MouseEvent&) override
        {
            if (isDown && isHovered && onClick)
                onClick();
            isDown = false;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();

            if (isHovered)
            {
                g.setColour(SoundIdTheme::bgCardHover);
                g.fillRoundedRectangle(b.reduced(1.0f), 4.0f);
            }

            auto c = b.getCentre();
            float rOuter = 8.0f;
            float rInner = 5.6f;
            float rHole  = 2.8f;

            juce::Path gear;
            const int numTeeth = 6;
            const float angleStep = juce::MathConstants<float>::twoPi / static_cast<float>(numTeeth);
            const float toothHalf = angleStep * 0.22f;

            for (int i = 0; i < numTeeth; ++i)
            {
                float a = static_cast<float>(i) * angleStep - juce::MathConstants<float>::halfPi;
                float a0 = a - toothHalf;
                float a1 = a + toothHalf;
                float aValley0 = a + angleStep * 0.35f;
                float aValley1 = a + angleStep * 0.65f;

                if (i == 0)
                    gear.startNewSubPath(c.x + rOuter * std::cos(a0), c.y + rOuter * std::sin(a0));
                else
                    gear.lineTo(c.x + rOuter * std::cos(a0), c.y + rOuter * std::sin(a0));

                gear.lineTo(c.x + rOuter * std::cos(a1), c.y + rOuter * std::sin(a1));
                gear.lineTo(c.x + rInner * std::cos(aValley0), c.y + rInner * std::sin(aValley0));
                gear.lineTo(c.x + rInner * std::cos(aValley1), c.y + rInner * std::sin(aValley1));
            }
            gear.closeSubPath();

            juce::Path hole;
            hole.addEllipse(c.x - rHole, c.y - rHole, rHole * 2.0f, rHole * 2.0f);

            juce::Colour gearCol = isHovered ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary;
            g.setColour(gearCol);
            g.fillPath(gear);

            g.setColour(isHovered ? SoundIdTheme::bgCardHover : SoundIdTheme::bgCard);
            g.fillPath(hole);
        }

    private:
        bool isHovered { false };
        bool isDown { false };
    };

    class StatusIndicator : public juce::Component,
                            public juce::SettableTooltipClient
    {
    public:
        StatusIndicator(const juce::String& labelText, std::function<void()>& clickCallback)
            : label(labelText), onClick(clickCallback)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void setStatus(bool configured, const juce::String& literalName)
        {
            isConfigured = configured;
            portLiteral = literalName;
            setTooltip(label + ": " + portLiteral + (isConfigured ? " [Configured / Active]" : " [Not Connected / Inactive]"));
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            float ledDiameter = 6.5f;
            float ledX = b.getX() + 2.0f;
            float ledY = b.getCentreY() - ledDiameter * 0.5f;

            juce::Colour ledCol = isConfigured ? SoundIdTheme::accentGreen : SoundIdTheme::accentRed;

            // Subtle outer glow ring
            g.setColour(ledCol.withAlpha(0.20f));
            g.fillEllipse(ledX - 1.5f, ledY - 1.5f, ledDiameter + 3.0f, ledDiameter + 3.0f);

            // Core LED
            g.setColour(ledCol);
            g.fillEllipse(ledX, ledY, ledDiameter, ledDiameter);

            // Specular highlight on LED
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(ledX + 1.2f, ledY + 1.2f, 1.8f, 1.8f);

            // Label text
            auto textRect = b.withTrimmedLeft(ledX + ledDiameter + 3.0f);
            g.setColour(isConfigured ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.drawText(label, textRect, juce::Justification::centredLeft, true);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

    private:
        juce::String label;
        bool isConfigured { false };
        juce::String portLiteral { "None" };
        std::function<void()>& onClick;
    };

    AudioMidiStatusPill()
        : indAudioIn("A-IN", onConfigureClicked),
          indAudioOut("A-OUT", onConfigureClicked),
          indMidiIn("M-IN", onConfigureClicked),
          indMidiOut("M-OUT", onConfigureClicked)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);

        btnGear.onClick = [this] { if (onConfigureClicked) onConfigureClicked(); };
        addAndMakeVisible(btnGear);

        addAndMakeVisible(indAudioIn);
        addAndMakeVisible(indAudioOut);
        addAndMakeVisible(indMidiIn);
        addAndMakeVisible(indMidiOut);
    }

    ~AudioMidiStatusPill() override = default;

    void updateStatus(audio::LabAudioEngine& engine)
    {
        auto& dm = engine.getDeviceManager();
        auto* dev = dm.getCurrentAudioDevice();

        bool audioInOk = false;
        juce::String audioInName = "None / Disconnected";
        bool audioOutOk = false;
        juce::String audioOutName = "None / Disconnected";

        if (dev != nullptr)
        {
            auto inChans = dev->getActiveInputChannels();
            int numIn = inChans.countNumberOfSetBits();
            audioInOk = (numIn > 0);
            if (audioInOk)
                audioInName = dev->getName() + " (" + juce::String(numIn) + (numIn > 1 ? " in)" : " in)");
            else
                audioInName = dev->getName() + " (No active inputs)";

            auto outChans = dev->getActiveOutputChannels();
            int numOut = outChans.countNumberOfSetBits();
            audioOutOk = (numOut > 0);
            if (audioOutOk)
                audioOutName = dev->getName() + " (" + juce::String(numOut) + (numOut > 1 ? " out)" : " out)");
            else
                audioOutName = dev->getName() + " (No active outputs)";
        }

        // MIDI In - strictly check if enabled in device manager
        bool midiInOk = false;
        juce::String midiInName = "None / Disconnected";
        auto inDevices = juce::MidiInput::getAvailableDevices();
        for (const auto& d : inDevices)
        {
            if (dm.isMidiInputDeviceEnabled(d.identifier))
            {
                midiInOk = true;
                midiInName = d.name;
                break;
            }
        }

        // MIDI Out - strictly check if default midi output is assigned
        bool midiOutOk = false;
        juce::String midiOutName = "None / Disconnected";
        auto* defMidiOut = dm.getDefaultMidiOutput();
        if (defMidiOut != nullptr)
        {
            midiOutOk = true;
            midiOutName = defMidiOut->getName();
        }

        indAudioIn.setStatus(audioInOk, audioInName);
        indAudioOut.setStatus(audioOutOk, audioOutName);
        indMidiIn.setStatus(midiInOk, midiInName);
        indMidiOut.setStatus(midiOutOk, midiOutName);

        // Overall tooltip showing literal ports
        juce::String summary;
        summary << "AUDIO & MIDI HARDWARE STATUS\n"
                << "----------------------------------------\n"
                << "• Audio In:   " << audioInName << (audioInOk ? "  [OK]" : "  [DISCONNECTED]") << "\n"
                << "• Audio Out:  " << audioOutName << (audioOutOk ? "  [OK]" : "  [DISCONNECTED]") << "\n"
                << "• MIDI In:    " << midiInName << (midiInOk ? "  [OK]" : "  [DISCONNECTED]") << "\n"
                << "• MIDI Out:   " << midiOutName << (midiOutOk ? "  [OK]" : "  [DISCONNECTED]") << "\n"
                << "----------------------------------------\n"
                << "Click gear to open Audio & MIDI configuration.";
        setTooltip(summary);

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float radius = bounds.getHeight() * 0.5f;

        // Card background (SurfaceSubtle)
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(bounds, radius);

        // Border
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

        // Vertical divider after gear button
        float divX = btnGear.getRight() + 4.0f;
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawLine(divX, bounds.getY() + 7.0f, divX, bounds.getBottom() - 7.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(2, 2);
        btnGear.setBounds(area.removeFromLeft(28).withSizeKeepingCentre(24, 24));
        area.removeFromLeft(6); // Space past divider

        int itemW = area.getWidth() / 4;
        indAudioIn.setBounds(area.removeFromLeft(itemW));
        indAudioOut.setBounds(area.removeFromLeft(itemW));
        indMidiIn.setBounds(area.removeFromLeft(itemW));
        indMidiOut.setBounds(area);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onConfigureClicked) onConfigureClicked();
    }

private:
    GearButton btnGear;
    StatusIndicator indAudioIn;
    StatusIndicator indAudioOut;
    StatusIndicator indMidiIn;
    StatusIndicator indMidiOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiStatusPill)
};

} // namespace abdaudiolab::gui
