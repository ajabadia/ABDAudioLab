#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "SoundIdTheme.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab::gui
{

/**
 * @brief Header bar status pill with Gear icon (audio/midi config button) and 4 LED indicators:
 * Audio In, Audio Out, MIDI In, MIDI Out.
 * Displays green when configured/active, red when disconnected/unassigned.
 * Hovering displays the literal port names.
 */
class AudioMidiStatusPill : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    std::function<void()> onConfigureClicked;

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

            juce::Colour ledCol = isConfigured ? juce::Colour(0xff10b981) : juce::Colour(0xffef4444);

            // Subtle outer glow ring
            g.setColour(ledCol.withAlpha(0.25f));
            g.fillEllipse(ledX - 1.5f, ledY - 1.5f, ledDiameter + 3.0f, ledDiameter + 3.0f);

            // Core LED
            g.setColour(ledCol);
            g.fillEllipse(ledX, ledY, ledDiameter, ledDiameter);

            // Specular highlight on LED
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(ledX + 1.2f, ledY + 1.2f, 2.0f, 2.0f);

            // Label text
            auto textRect = b.withTrimmedLeft(ledX + ledDiameter + 3.0f);
            g.setColour(isConfigured ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted);
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
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

        btnGear.setButtonText(juce::String::fromUTF8(u8"\u2699"));
        btnGear.setTooltip("Audio & MIDI Configuration - Select audio interfaces, buffer sizes, sample rate, and MIDI routing");
        btnGear.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btnGear.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
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
                audioInName = dev->getName() + " (No inputs active)";

            auto outChans = dev->getActiveOutputChannels();
            int numOut = outChans.countNumberOfSetBits();
            audioOutOk = (numOut > 0);
            if (audioOutOk)
                audioOutName = dev->getName() + " (" + juce::String(numOut) + (numOut > 1 ? " out)" : " out)");
            else
                audioOutName = dev->getName() + " (No outputs active)";
        }

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
        if (!midiInOk && !inDevices.isEmpty())
        {
            midiInOk = true;
            midiInName = inDevices[0].name;
        }

        bool midiOutOk = false;
        juce::String midiOutName = "None / Disconnected";
        auto* defMidiOut = dm.getDefaultMidiOutput();
        if (defMidiOut != nullptr)
        {
            midiOutOk = true;
            midiOutName = defMidiOut->getName();
        }
        else
        {
            auto outDevices = juce::MidiOutput::getAvailableDevices();
            if (!outDevices.isEmpty())
            {
                midiOutOk = true;
                midiOutName = outDevices[0].name;
            }
        }

        indAudioIn.setStatus(audioInOk, audioInName);
        indAudioOut.setStatus(audioOutOk, audioOutName);
        indMidiIn.setStatus(midiInOk, midiInName);
        indMidiOut.setStatus(midiOutOk, midiOutName);

        // Overall tooltip showing literal ports
        juce::String summary;
        summary << "AUDIO & MIDI HARDWARE STATUS\n"
                << "----------------------------------------\n"
                << "• Audio In:   " << audioInName << (audioInOk ? "  [OK]" : "  [NO]") << "\n"
                << "• Audio Out:  " << audioOutName << (audioOutOk ? "  [OK]" : "  [NO]") << "\n"
                << "• MIDI In:    " << midiInName << (midiInOk ? "  [OK]" : "  [NO]") << "\n"
                << "• MIDI Out:   " << midiOutName << (midiOutOk ? "  [OK]" : "  [NO]") << "\n"
                << "----------------------------------------\n"
                << "Click to configure Audio & MIDI settings.";
        setTooltip(summary);

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float radius = 8.0f;

        // Card background
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(bounds, radius);

        // Border
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

        // Vertical divider after gear button
        float divX = btnGear.getRight() + 3.0f;
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
    juce::TextButton btnGear;
    StatusIndicator indAudioIn;
    StatusIndicator indAudioOut;
    StatusIndicator indMidiIn;
    StatusIndicator indMidiOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiStatusPill)
};

} // namespace abdaudiolab::gui
