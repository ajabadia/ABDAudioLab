#include "HardwareWiringDiagramComponent.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

HardwareWiringDiagramComponent::HardwareWiringDiagramComponent()
{
    routingStimulusText = juce::String::fromUTF8(u8"Salida Audio 1 (DAC) ➔ Entrada de Audio del Hardware");
    routingResponseText = juce::String::fromUTF8(u8"Salida de Audio del Hardware ➔ Entrada Audio 1 (ADC)");
    routingNotesText = juce::String::fromUTF8(u8"Conecta los cables de audio analógicos y el interfaz MIDI antes de continuar.");
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
    g.drawText("ESQUEMA DE CONEXIONADO (CLOSED LOOP)", rInner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    rInner.removeFromTop(8.0f);

    auto drawWire = [&](const juce::String& from, const juce::String& to, const juce::Colour& clr) {
        auto wireRow = rInner.removeFromTop(28.0f);
        float boxW = 150.0f;

        auto b1 = wireRow.removeFromLeft(boxW).reduced(0.0f, 2.0f);
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(b1, 4.0f);
        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(b1.reduced(0.5f), 4.0f, 1.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(from, b1, juce::Justification::centred, true);

        auto arrowArea = wireRow.removeFromLeft(36.0f);
        g.setColour(clr);
        g.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8(u8"➔"), arrowArea, juce::Justification::centred, false);

        auto b2 = wireRow.removeFromLeft(wireRow.getWidth()).reduced(0.0f, 2.0f);
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(b2, 4.0f);
        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(b2.reduced(0.5f), 4.0f, 1.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(to, b2.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);

        rInner.removeFromTop(6.0f);
    };

    if (isMidiAutonomous)
    {
        drawWire("Salida MIDI / USB", routingStimulusText, SoundIdTheme::accentBlue);
    }
    else
    {
        drawWire("Salida Audio 1 (DAC)", routingStimulusText, SoundIdTheme::accentGreen);
    }
    drawWire("Entrada Audio 1 (ADC)", routingResponseText, SoundIdTheme::accentAmber);

    rInner.removeFromTop(4.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("Nota: " + routingNotesText, rInner, juce::Justification::topLeft, true);
}

} // namespace abdaudiolab::gui
