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
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    auto titleArea = area.removeFromTop(20.0f);
    g.drawText(juce::String(ps.paramName).toUpperCase(), titleArea, juce::Justification::centred, true);

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(90.0f, 20.0f);

    auto knobArea = area.reduced(4.0f, 2.0f);
    float squareSize = std::clamp(juce::jmin(knobArea.getWidth(), knobArea.getHeight()), 32.0f, 72.0f);
    auto knobCircle = knobArea.withSizeKeepingCentre(squareSize, squareSize);

    float cx = knobCircle.getCentreX();
    float cy = knobCircle.getCentreY();
    float radius = squareSize * 0.5f;
    float scale = squareSize / 72.0f;

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

    float trackInset = std::max(2.5f, 4.0f * scale);
    float trackRadius = radius - trackInset;
    float strokeW = std::max(3.0f, 6.0f * scale);

    juce::Path bgTrackPath;
    bgTrackPath.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(SoundIdTheme::bgCardHover);
    g.strokePath(bgTrackPath, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path rangeTrackPath;
    rangeTrackPath.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f, minAngle, maxAngle, true);
    g.setColour(juce::Colour(0x30000000));
    g.strokePath(rangeTrackPath, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path arcPath;
    arcPath.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f, startAngle, currentAngle, true);
    g.setColour(SoundIdTheme::accentGreen);
    g.strokePath(arcPath, juce::PathStrokeType(std::max(2.0f, 4.0f * scale), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto drawTick = [&](float angle, juce::Colour col) {
        float tx1 = cx + (radius - 8.0f * scale) * std::sin(angle);
        float ty1 = cy - (radius - 8.0f * scale) * std::cos(angle);
        float tx2 = cx + (radius + 2.0f * scale) * std::sin(angle);
        float ty2 = cy - (radius + 2.0f * scale) * std::cos(angle);
        g.setColour(col);
        g.drawLine(tx1, ty1, tx2, ty2, std::max(1.0f, 2.0f * scale));
    };
    if (minNorm > 0.01f) drawTick(minAngle, SoundIdTheme::accentAmber);
    if (maxNorm < 0.99f) drawTick(maxAngle, SoundIdTheme::accentAmber);

    float px = cx + (radius - 12.0f * scale) * std::sin(currentAngle);
    float py = cy - (radius - 12.0f * scale) * std::cos(currentAngle);
    g.setColour(juce::Colours::white);
    g.drawLine(cx, cy, px, py, std::max(1.5f, 3.0f * scale));

    float capR = std::max(3.0f, 5.0f * scale);
    g.setColour(SoundIdTheme::textPrimary);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    int pct = static_cast<int>(std::round(norm * 100.0f));
    juce::String valStr = juce::String(pct) + "%";
    if (minNorm > 0.01f || maxNorm < 0.99f)
    {
        int minP = static_cast<int>(std::round(minNorm * 100.0f));
        int maxP = static_cast<int>(std::round(maxNorm * 100.0f));
        valStr += " [" + juce::String(minP) + "%-" + juce::String(maxP) + "%]";
    }

    drawTargetValueBadge(g, valArea, valStr, SoundIdTheme::accentGreen);
}

void HardwareControlRenderer::drawSlider(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    auto titleArea = area.removeFromTop(20.0f);
    g.drawText(juce::String(ps.paramName).toUpperCase(), titleArea, juce::Justification::centred, true);

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);

    auto sliderArea = area.reduced(4.0f, 2.0f);
    float trackW = 8.0f;
    float trackH = std::clamp(sliderArea.getHeight() - 8.0f, 24.0f, 75.0f);
    auto trackArea = sliderArea.withSizeKeepingCentre(trackW, trackH);

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
    drawTargetValueBadge(g, valArea, juce::String(pct) + "%", SoundIdTheme::accentGreen);
}

void HardwareControlRenderer::drawJackPort(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    auto titleArea = area.removeFromTop(20.0f);
    g.drawText(juce::String(ps.paramName).toUpperCase(), titleArea, juce::Justification::centred, true);

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(80.0f, 20.0f);

    auto portArea = area.reduced(4.0f, 2.0f);
    float centerSize = std::clamp(juce::jmin(portArea.getWidth(), portArea.getHeight()), 28.0f, 64.0f);
    auto jackCircle = portArea.withSizeKeepingCentre(centerSize, centerSize);
    float scale = centerSize / 64.0f;

    float cx = jackCircle.getCentreX();
    float cy = jackCircle.getCentreY();

    g.setColour(juce::Colour(0xFF33373E));
    g.fillEllipse(jackCircle);
    g.setColour(SoundIdTheme::borderCard);
    g.drawEllipse(jackCircle, 2.0f);

    float rOuter = 20.0f * scale;
    g.setColour(juce::Colour(0xFF808894));
    g.fillEllipse(cx - rOuter, cy - rOuter, rOuter * 2.0f, rOuter * 2.0f);

    float rInner = 10.0f * scale;
    g.setColour(juce::Colours::black);
    g.fillEllipse(cx - rInner, cy - rInner, rInner * 2.0f, rInner * 2.0f);

    float rCenter = 7.0f * scale;
    g.setColour(ps.normalizedValue > 0.5f ? SoundIdTheme::accentGreen : juce::Colour(0xFFE55039));
    g.fillEllipse(cx - rCenter, cy - rCenter, rCenter * 2.0f, rCenter * 2.0f);

    juce::String valStr = (ps.normalizedValue > 0.5f) ? "PATCHED" : "UNPATCHED";
    drawTargetValueBadge(g, valArea, valStr, ps.normalizedValue > 0.5f ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber);
}

void HardwareControlRenderer::drawButton(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    auto titleArea = area.removeFromTop(20.0f);
    g.drawText(juce::String(ps.paramName).toUpperCase(), titleArea, juce::Justification::centred, true);

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);

    auto bArea = area.reduced(4.0f, 2.0f);
    float size = std::clamp(juce::jmin(bArea.getWidth(), bArea.getHeight()), 26.0f, 52.0f);
    auto btnArea = bArea.withSizeKeepingCentre(size, size);
    bool isOn = ps.normalizedValue > 0.5f;

    g.setColour(juce::Colour(0xFF23252A));
    g.fillRoundedRectangle(btnArea, 8.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(btnArea, 8.0f, 1.5f);

    auto ledArea = btnArea.reduced(size * 0.26f);
    if (isOn)
    {
        g.setColour(SoundIdTheme::accentGreen);
        g.fillEllipse(ledArea);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillEllipse(ledArea.reduced(ledArea.getWidth() * 0.25f).withY(ledArea.getY() + 1.0f));
    }
    else
    {
        g.setColour(juce::Colour(0xFF40444E));
        g.fillEllipse(ledArea);
    }

    drawTargetValueBadge(g, valArea, isOn ? "ON" : "OFF", isOn ? SoundIdTheme::accentGreen : SoundIdTheme::textMuted);
}

void HardwareControlRenderer::drawSwitch(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    auto titleArea = area.removeFromTop(20.0f);
    g.drawText(juce::String(ps.paramName).toUpperCase(), titleArea, juce::Justification::centred, true);

    auto valArea = area.removeFromBottom(22.0f).withSizeKeepingCentre(64.0f, 20.0f);

    auto sArea = area.reduced(4.0f, 2.0f);
    float centerSize = std::clamp(juce::jmin(sArea.getWidth(), sArea.getHeight()), 28.0f, 64.0f);
    auto switchCircle = sArea.withSizeKeepingCentre(centerSize, centerSize);
    float cx = switchCircle.getCentreX();
    float cy = switchCircle.getCentreY();
    float radius = centerSize * 0.5f;
    float scale = centerSize / 64.0f;

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
        float tx = cx + (radius + 4.0f * scale) * std::sin(posAngle);
        float ty = cy - (radius + 4.0f * scale) * std::cos(posAngle);
        g.setColour(SoundIdTheme::textSecondary);
        float dotR = 2.5f * scale;
        g.fillEllipse(tx - dotR, ty - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    float px = cx + (radius - 8.0f * scale) * std::sin(angle);
    float py = cy - (radius - 8.0f * scale) * std::cos(angle);
    g.setColour(SoundIdTheme::accentGreen);
    g.drawLine(cx, cy, px, py, std::max(2.0f, 4.0f * scale));

    int posIdx = static_cast<int>(std::round(norm * (numPositions - 1))) + 1;
    drawTargetValueBadge(g, valArea, "POS " + juce::String(posIdx), SoundIdTheme::accentGreen);
}

} // namespace abdaudiolab::gui
