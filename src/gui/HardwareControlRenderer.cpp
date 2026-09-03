#include "HardwareControlRenderer.h"
#include <algorithm>

namespace abdaudiolab::gui
{

void HardwareControlRenderer::drawTargetValueBadge(juce::Graphics& g, juce::Rectangle<float> valArea, const juce::String& text, juce::Colour textColour)
{
    g.setColour(juce::Colour(0x15000000));
    g.fillRoundedRectangle(valArea, 10.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(valArea, 10.0f, 1.0f);

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(textColour);
    g.drawText(text, valArea, juce::Justification::centred, true);
}

void HardwareControlRenderer::drawKnob(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    float centerSize = 72.0f;
    auto knobCircle = area.withSizeKeepingCentre(centerSize, centerSize).withY(area.getY() + 35.0f);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(ps.paramName).toUpperCase(), area.removeFromTop(20.0f), juce::Justification::centred, true);

    float cx = knobCircle.getCentreX();
    float cy = knobCircle.getCentreY();
    float radius = centerSize * 0.5f;

    g.setColour(SoundIdTheme::bgCardHover);
    g.fillEllipse(knobCircle);
    g.setColour(SoundIdTheme::borderCard);
    g.drawEllipse(knobCircle, 2.0f);

    float norm = std::clamp(ps.normalizedValue, 0.0f, 1.0f);
    float minNorm = std::clamp(ps.minNormalized, 0.0f, 1.0f);
    float maxNorm = std::clamp(ps.maxNormalized, minNorm, 1.0f);

    float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    float endAngle = +juce::MathConstants<float>::pi * 0.75f;
    float currentAngle = startAngle + norm * (endAngle - startAngle);
    float minAngle = startAngle + minNorm * (endAngle - startAngle);
    float maxAngle = startAngle + maxNorm * (endAngle - startAngle);

    juce::Path bgTrackPath;
    bgTrackPath.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f, 0.0f, startAngle, endAngle, true);
    g.setColour(SoundIdTheme::bgCardHover);
    g.strokePath(bgTrackPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path rangeTrackPath;
    rangeTrackPath.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f, 0.0f, minAngle, maxAngle, true);
    g.setColour(juce::Colour(0x30000000));
    g.strokePath(rangeTrackPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path arcPath;
    arcPath.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f, 0.0f, startAngle, currentAngle, true);
    g.setColour(SoundIdTheme::accentGreen);
    g.strokePath(arcPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto drawTick = [&](float angle, juce::Colour col) {
        float tx1 = cx + (radius - 8.0f) * std::sin(angle);
        float ty1 = cy - (radius - 8.0f) * std::cos(angle);
        float tx2 = cx + (radius + 2.0f) * std::sin(angle);
        float ty2 = cy - (radius + 2.0f) * std::cos(angle);
        g.setColour(col);
        g.drawLine(tx1, ty1, tx2, ty2, 2.0f);
    };
    if (minNorm > 0.01f) drawTick(minAngle, SoundIdTheme::accentAmber);
    if (maxNorm < 0.99f) drawTick(maxAngle, SoundIdTheme::accentAmber);

    float px = cx + (radius - 12.0f) * std::sin(currentAngle);
    float py = cy - (radius - 12.0f) * std::cos(currentAngle);
    g.setColour(juce::Colours::white);
    g.drawLine(cx, cy, px, py, 3.0f);

    g.setColour(SoundIdTheme::textPrimary);
    g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

    int pct = static_cast<int>(std::round(norm * 100.0f));
    juce::String valStr = juce::String(pct) + "%";
    if (minNorm > 0.01f || maxNorm < 0.99f)
    {
        int minP = static_cast<int>(std::round(minNorm * 100.0f));
        int maxP = static_cast<int>(std::round(maxNorm * 100.0f));
        valStr += " [" + juce::String(minP) + "%-" + juce::String(maxP) + "%]";
    }

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(90.0f, 20.0f);
    drawTargetValueBadge(g, valArea, valStr, SoundIdTheme::accentGreen);
}

void HardwareControlRenderer::drawSlider(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(ps.paramName).toUpperCase(), area.removeFromTop(20.0f), juce::Justification::centred, true);

    float trackW = 8.0f;
    float trackH = 75.0f;
    auto trackArea = area.withSizeKeepingCentre(trackW, trackH).withY(area.getY() + 30.0f);

    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(trackArea, 4.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(trackArea, 4.0f, 1.0f);

    float norm = std::clamp(ps.normalizedValue, 0.0f, 1.0f);
    float fillH = trackH * norm;
    auto fillArea = trackArea.removeFromBottom(fillH);
    g.setColour(SoundIdTheme::accentGreen);
    g.fillRoundedRectangle(fillArea, 4.0f);

    float handleY = trackArea.getY() + trackH * (1.0f - norm);
    auto handleArea = juce::Rectangle<float>(trackArea.getCentreX() - 14.0f, handleY - 6.0f, 28.0f, 12.0f);
    g.setColour(SoundIdTheme::textPrimary);
    g.fillRoundedRectangle(handleArea, 3.0f);
    g.setColour(juce::Colours::white);
    g.drawHorizontalLine(static_cast<int>(handleY), handleArea.getX() + 4.0f, handleArea.getRight() - 4.0f);

    int pct = static_cast<int>(std::round(norm * 100.0f));
    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);
    drawTargetValueBadge(g, valArea, juce::String(pct) + "%", SoundIdTheme::accentGreen);
}

void HardwareControlRenderer::drawJackPort(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    float centerSize = 64.0f;
    auto jackCircle = area.withSizeKeepingCentre(centerSize, centerSize).withY(area.getY() + 35.0f);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(ps.paramName).toUpperCase(), area.removeFromTop(20.0f), juce::Justification::centred, true);

    float cx = jackCircle.getCentreX();
    float cy = jackCircle.getCentreY();

    g.setColour(juce::Colour(0xFF33373E));
    g.fillEllipse(jackCircle);
    g.setColour(SoundIdTheme::borderCard);
    g.drawEllipse(jackCircle, 2.0f);

    g.setColour(juce::Colour(0xFF808894));
    g.fillEllipse(cx - 20.0f, cy - 20.0f, 40.0f, 40.0f);

    g.setColour(juce::Colours::black);
    g.fillEllipse(cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);

    g.setColour(ps.normalizedValue > 0.5f ? SoundIdTheme::accentGreen : juce::Colour(0xFFE55039));
    g.fillEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);

    juce::String valStr = (ps.normalizedValue > 0.5f) ? "PATCHED" : "UNPATCHED";
    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(80.0f, 20.0f);
    drawTargetValueBadge(g, valArea, valStr, ps.normalizedValue > 0.5f ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber);
}

void HardwareControlRenderer::drawButton(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(ps.paramName).toUpperCase(), area.removeFromTop(20.0f), juce::Justification::centred, true);

    float size = 52.0f;
    auto btnArea = area.withSizeKeepingCentre(size, size).withY(area.getY() + 35.0f);
    bool isOn = ps.normalizedValue > 0.5f;

    g.setColour(juce::Colour(0xFF23252A));
    g.fillRoundedRectangle(btnArea, 8.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(btnArea, 8.0f, 1.5f);

    auto ledArea = btnArea.reduced(14.0f);
    if (isOn)
    {
        g.setColour(SoundIdTheme::accentGreen);
        g.fillEllipse(ledArea);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillEllipse(ledArea.reduced(5.0f).withY(ledArea.getY() + 1.0f));
    }
    else
    {
        g.setColour(juce::Colour(0xFF40444E));
        g.fillEllipse(ledArea);
    }

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);
    drawTargetValueBadge(g, valArea, isOn ? "ON" : "OFF", isOn ? SoundIdTheme::accentGreen : SoundIdTheme::textMuted);
}

void HardwareControlRenderer::drawSwitch(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(ps.paramName).toUpperCase(), area.removeFromTop(20.0f), juce::Justification::centred, true);

    float centerSize = 64.0f;
    auto switchCircle = area.withSizeKeepingCentre(centerSize, centerSize).withY(area.getY() + 35.0f);
    float cx = switchCircle.getCentreX();
    float cy = switchCircle.getCentreY();
    float radius = centerSize * 0.5f;

    g.setColour(SoundIdTheme::bgCardHover);
    g.fillEllipse(switchCircle);
    g.setColour(SoundIdTheme::borderCard);
    g.drawEllipse(switchCircle, 2.0f);

    int numPositions = 4;
    float norm = std::clamp(ps.normalizedValue, 0.0f, 1.0f);
    float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    float endAngle = +juce::MathConstants<float>::pi * 0.75f;
    float angle = startAngle + norm * (endAngle - startAngle);

    for (int p = 0; p < numPositions; ++p)
    {
        float posNorm = static_cast<float>(p) / static_cast<float>(numPositions - 1);
        float posAngle = startAngle + posNorm * (endAngle - startAngle);
        float tx = cx + (radius + 4.0f) * std::sin(posAngle);
        float ty = cy - (radius + 4.0f) * std::cos(posAngle);
        g.setColour(SoundIdTheme::textSecondary);
        g.fillEllipse(tx - 2.5f, ty - 2.5f, 5.0f, 5.0f);
    }

    float px = cx + (radius - 8.0f) * std::sin(angle);
    float py = cy - (radius - 8.0f) * std::cos(angle);
    g.setColour(SoundIdTheme::accentGreen);
    g.drawLine(cx, cy, px, py, 4.0f);

    int posIdx = static_cast<int>(std::round(norm * (numPositions - 1))) + 1;
    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);
    drawTargetValueBadge(g, valArea, "POS " + juce::String(posIdx), SoundIdTheme::accentGreen);
}

} // namespace abdaudiolab::gui
