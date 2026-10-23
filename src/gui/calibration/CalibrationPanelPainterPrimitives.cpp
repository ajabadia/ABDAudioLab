/**
 * @file CalibrationPanelPainterPrimitives.cpp
 * @brief Shared drawing primitives for the Step 2 calibration card.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationPanelPainter.h"

#include <cmath>

#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::calibrationpanel::painter
{

void drawBadge(juce::Graphics& g,
               const juce::Rectangle<float>& target,
               const juce::String& label,
               const juce::Colour& background,
               const juce::Colour& foreground,
               float textHeight,
               juce::Justification justification,
               float cornerRadius)
{
    if (background.getAlpha() > 0.0f)
    {
        g.setColour(background);
        g.fillRoundedRectangle(target, cornerRadius);
    }

    g.setFont(font(textHeight, juce::Font::bold));
    g.setColour(foreground);
    g.drawText(label, target, justification, true);
}

void drawCardShell(juce::Graphics& g,
                   const juce::Rectangle<float>& card,
                   juce::Colour background,
                   juce::Colour border,
                   float cornerRadius,
                   float borderThickness)
{
    g.setColour(background);
    g.fillRoundedRectangle(card, cornerRadius);
    g.setColour(border);
    g.drawRoundedRectangle(card.reduced(0.5f), cornerRadius, borderThickness);
}

void drawInstructionStep(juce::Graphics& g,
                         juce::Rectangle<float>& column,
                         int number,
                         const juce::String& title,
                         const juce::String& description,
                         juce::Colour accent,
                         float rowHeight,
                         float descriptionFontHeight,
                         float gapAfter)
{
    auto stepRow = column.removeFromTop(rowHeight);
    auto circleBounds = stepRow.removeFromLeft(30.0f).withSizeKeepingCentre(24.0f, 24.0f);

    g.setColour(accent.withAlpha(0.15f));
    g.fillEllipse(circleBounds);
    g.setColour(accent);
    g.drawEllipse(circleBounds, 1.5f);

    g.setFont(font(12.0f, juce::Font::bold));
    g.drawText(juce::String(number), circleBounds, juce::Justification::centred, false);

    stepRow.removeFromLeft(10.0f);
    g.setFont(font(14.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(title, stepRow.removeFromTop(20.0f), juce::Justification::centredLeft, true);

    g.setFont(font(descriptionFontHeight));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(description, stepRow, juce::Justification::topLeft, true);

    column.removeFromTop(gapAfter);
}

void drawTipBox(juce::Graphics& g,
                juce::Rectangle<float>& column,
                const juce::String& text,
                juce::Colour accent,
                juce::Font::FontStyleFlags style,
                float borderAlpha)
{
    column.removeFromTop(4.0f);
    auto tipBox = column.removeFromTop(40.0f);

    g.setColour(accent.withAlpha(0.08f));
    g.fillRoundedRectangle(tipBox, 6.0f);
    g.setColour(accent.withAlpha(borderAlpha));
    g.drawRoundedRectangle(tipBox.reduced(0.5f), 6.0f, 1.0f);

    g.setFont(font(12.0f, style));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(text, tipBox.reduced(10.0f, 2.0f), juce::Justification::centredLeft, true);
}

juce::String formatTrimDb(float linearGain, int decimals)
{
    const auto trimDb = 20.0f * std::log10(std::max(linearGain, 1e-4f));
    return (trimDb >= 0.0f ? "+" : "") + juce::String(trimDb, decimals) + " dB";
}

juce::String formatDbfs(float dbfs, int decimals)
{
    if (dbfs < -120.0f)
        return "-inf dBFS";

    return juce::String(dbfs, decimals) + " dBFS";
}

bool isLoopbackFailureState(LoopbackState state)
{
    return state == LoopbackState::Failed
        || state == LoopbackState::SignalTooLow
        || state == LoopbackState::Clipped;
}

} // namespace abdaudiolab::gui::calibrationpanel::painter
