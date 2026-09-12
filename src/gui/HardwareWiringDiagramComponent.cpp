#include "HardwareWiringDiagramComponent.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

HardwareWiringDiagramComponent::HardwareWiringDiagramComponent()
{
    // Start in cleared/empty state until a device or plugin is configured
    isCleared = true;
    isPluginVirtual = false;
    isMidiAutonomous = false;
}

void HardwareWiringDiagramComponent::setRouting(const juce::String& stimulus,
                                               const juce::String& response,
                                               const juce::String& notes,
                                               bool midiAutonomous)
{
    routingStimulusText = stimulus;
    routingResponseText = response;
    routingNotesText = notes;
    isMidiAutonomous = midiAutonomous;
    isPluginVirtual = false;
    isCleared = false;
    repaint();
}

void HardwareWiringDiagramComponent::setPluginRouting(const juce::String& stimulusFrom,
                                                      const juce::String& stimulusTo,
                                                      const juce::String& responseFrom,
                                                      const juce::String& responseTo,
                                                      const juce::String& notes)
{
    plugStimFrom = stimulusFrom;
    plugStimTo   = stimulusTo;
    plugRespFrom = responseFrom;
    plugRespTo   = responseTo;
    routingNotesText = notes;
    isPluginVirtual = true;
    isCleared = false;
    isMidiAutonomous = false;
    repaint();
}

void HardwareWiringDiagramComponent::clear()
{
    routingStimulusText = {};
    routingResponseText = {};
    routingNotesText = {};
    plugStimFrom = {};
    plugStimTo   = {};
    plugRespFrom = {};
    plugRespTo   = {};
    isMidiAutonomous = false;
    isPluginVirtual = false;
    isCleared = true;
    repaint();
}

void HardwareWiringDiagramComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(area.reduced(0.5f), 8.0f, 1.0f);

    auto rInner = area.reduced(14.0f, 10.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("WIRING SCHEMATIC (CLOSED LOOP)", rInner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    rInner.removeFromTop(8.0f);

    // --- Empty state: no device selected yet ---
    if (isCleared)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::italic));
        g.setColour(SoundIdTheme::textMuted.withAlpha(0.5f));
        g.drawText("No device selected. Choose hardware or plugin to view wiring schematic.",
                   rInner, juce::Justification::centred, true);
        return;
    }

    // --- Plugin virtual mode: show a clean digital-bus diagram ---
    if (isPluginVirtual)
    {
        auto drawDigitalBus = [&](const juce::String& from, const juce::String& to, const juce::Colour& clr)
        {
            auto wireRow = rInner.removeFromTop(28.0f);
            float availW = wireRow.getWidth() - 32.0f;
            float box1W = std::clamp(availW * 0.44f, 110.0f, 180.0f);

            auto b1 = wireRow.removeFromLeft(box1W).reduced(0.0f, 2.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b1, 4.0f);
            g.setColour(clr.withAlpha(0.6f));
            g.drawRoundedRectangle(b1.reduced(0.5f), 4.0f, 1.5f);
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
            g.setColour(clr);
            g.drawText(from, b1, juce::Justification::centred, true);

            auto arrowArea = wireRow.removeFromLeft(32.0f);
            g.setColour(clr);
            g.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8(u8"➔"), arrowArea, juce::Justification::centred, false);

            auto b2 = wireRow.reduced(0.0f, 2.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b2, 4.0f);
            g.setColour(clr.withAlpha(0.6f));
            g.drawRoundedRectangle(b2.reduced(0.5f), 4.0f, 1.5f);
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(to, b2.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);

            rInner.removeFromTop(6.0f);
        };

        drawDigitalBus(plugStimFrom, plugStimTo, SoundIdTheme::accentBlue);
        drawDigitalBus(plugRespFrom, plugRespTo, SoundIdTheme::accentGreen);

        rInner.removeFromTop(4.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
        g.setColour(SoundIdTheme::accentBlue.withAlpha(0.7f));
        g.drawText(juce::String::fromUTF8(u8"Nota: ") + routingNotesText, rInner, juce::Justification::topLeft, true);
        return;
    }

    // --- Physical hardware mode ---
    auto drawWire = [&](const juce::String& from, const juce::String& to, const juce::Colour& clr) {
        if (rInner.getWidth() < 240.0f)
        {
            auto b1 = rInner.removeFromTop(24.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b1, 4.0f);
            g.setColour(SoundIdTheme::borderCard);
            g.drawRoundedRectangle(b1.reduced(0.5f), 4.0f, 1.0f);
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(from, b1, juce::Justification::centred, true);

            auto arrowArea = rInner.removeFromTop(18.0f);
            g.setColour(clr);
            g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8(u8"\u2193"), arrowArea, juce::Justification::centred, false);

            auto b2 = rInner.removeFromTop(24.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b2, 4.0f);
            g.setColour(SoundIdTheme::borderCard);
            g.drawRoundedRectangle(b2.reduced(0.5f), 4.0f, 1.0f);
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(to, b2.reduced(6.0f, 0.0f), juce::Justification::centred, true);

            rInner.removeFromTop(8.0f);
        }
        else
        {
            auto wireRow = rInner.removeFromTop(28.0f);
            float availW = wireRow.getWidth() - 32.0f;
            float box1W = std::clamp(availW * 0.44f, 110.0f, 180.0f);

            auto b1 = wireRow.removeFromLeft(box1W).reduced(0.0f, 2.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b1, 4.0f);
            g.setColour(SoundIdTheme::borderCard);
            g.drawRoundedRectangle(b1.reduced(0.5f), 4.0f, 1.0f);
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(from, b1, juce::Justification::centred, true);

            auto arrowArea = wireRow.removeFromLeft(32.0f);
            g.setColour(clr);
            g.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8(u8"\u27a4"), arrowArea, juce::Justification::centred, false);

            auto b2 = wireRow.reduced(0.0f, 2.0f);
            g.setColour(SoundIdTheme::bgCard);
            g.fillRoundedRectangle(b2, 4.0f);
            g.setColour(SoundIdTheme::borderCard);
            g.drawRoundedRectangle(b2.reduced(0.5f), 4.0f, 1.0f);
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(to, b2.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);

            rInner.removeFromTop(6.0f);
        }
    };

    if (isMidiAutonomous)
        drawWire("MIDI / USB Output", routingStimulusText, SoundIdTheme::accentBlue);
    else
        drawWire("Audio Out 1 (DAC)", routingStimulusText, SoundIdTheme::accentGreen);

    drawWire("Audio In 1 (ADC)", routingResponseText, SoundIdTheme::accentAmber);

    rInner.removeFromTop(4.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("Note: " + routingNotesText, rInner, juce::Justification::topLeft, true);
}

} // namespace abdaudiolab::gui
