/**
 * @file SuiteIcons.h
 * @brief Unified, vector-rendered Lucide/Feather icons for the test suite and action bars.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui::suite_icons
{

inline void drawTrash(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    // Top lid separated from body by 1.5 px
    float lidY = c.y - 4.5f;
    g.drawLine(c.x - 5.5f, lidY, c.x + 5.5f, lidY, 1.2f);
    g.drawLine(c.x - 2.0f, lidY - 1.5f, c.x + 2.0f, lidY - 1.5f, 1.1f);

    // Body with 1.5px gap below lid
    float bodyTopY = lidY + 1.5f;
    juce::Path bin;
    bin.startNewSubPath(c.x - 4.2f, bodyTopY);
    bin.lineTo(c.x - 3.2f, c.y + 5.0f);
    bin.lineTo(c.x + 3.2f, c.y + 5.0f);
    bin.lineTo(c.x + 4.2f, bodyTopY);
    bin.closeSubPath();
    g.strokePath(bin, juce::PathStrokeType(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Two interior vertical lines
    g.drawLine(c.x - 1.4f, bodyTopY + 2.0f, c.x - 1.2f, c.y + 3.5f, 0.9f);
    g.drawLine(c.x + 1.4f, bodyTopY + 2.0f, c.x + 1.2f, c.y + 3.5f, 0.9f);
}

inline void drawCopy(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col.withAlpha(0.5f));
    g.drawRoundedRectangle(c.x - 5.0f, c.y - 5.0f, 7.0f, 8.5f, 1.0f, 1.0f);
    g.setColour(col);
    g.drawRoundedRectangle(c.x - 2.5f, c.y - 2.5f, 7.0f, 8.5f, 1.0f, 1.1f);
}

inline void drawEdit(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);
    juce::Path p;
    p.startNewSubPath(c.x - 4.5f, c.y + 4.5f);
    p.lineTo(c.x - 4.5f, c.y + 2.0f);
    p.lineTo(c.x + 2.5f, c.y - 5.0f);
    p.lineTo(c.x + 5.0f, c.y - 2.5f);
    p.lineTo(c.x - 2.0f, c.y + 4.5f);
    p.closeSubPath();
    g.strokePath(p, juce::PathStrokeType(1.1f));
    g.drawLine(c.x - 4.5f, c.y + 4.5f, c.x - 3.5f, c.y + 2.0f, 0.9f);
}

inline void drawEye(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    juce::Path eyeTop, eyeBot;
    eyeTop.addCentredArc(c.x, c.y + 3.2f, 6.2f, 6.2f, 0.0f, -0.92f, 0.92f, true);
    eyeBot.addCentredArc(c.x, c.y - 3.2f, 6.2f, 6.2f, 0.0f, 3.14159f - 0.92f, 3.14159f + 0.92f, true);

    g.strokePath(eyeTop, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(eyeBot, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.fillEllipse(c.x - 1.0f, c.y - 1.0f, 2.0f, 2.0f);
}

inline void drawReset(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    juce::Path arc;
    float r = 4.2f;
    arc.addCentredArc(c.x, c.y, r, r, 0.0f, 0.0f, 4.71239f, true);
    g.strokePath(arc, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float endX = c.x - r;
    float endY = c.y;
    juce::Path arrow;
    arrow.startNewSubPath(endX - 2.5f, endY - 2.0f);
    arrow.lineTo(endX + 0.5f, endY + 1.5f);
    arrow.lineTo(endX + 3.0f, endY - 2.0f);
    arrow.closeSubPath();
    g.fillPath(arrow);
}

inline void drawReorderChevron(juce::Graphics& g, juce::Rectangle<float> bounds, bool pointingUp, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);
    juce::Path p;
    float halfW = 3.5f;
    float h = pointingUp ? -3.0f : 3.0f;
    p.startNewSubPath(c.x - halfW, c.y - h * 0.5f);
    p.lineTo(c.x, c.y + h * 0.5f);
    p.lineTo(c.x + halfW, c.y - h * 0.5f);
    g.strokePath(p, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void drawExpandArrow(juce::Graphics& g, juce::Rectangle<float> bounds, bool expanded, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);
    juce::Path p;
    if (expanded)
    {
        // Down arrow ▼
        p.startNewSubPath(c.x - 3.5f, c.y - 2.0f);
        p.lineTo(c.x, c.y + 2.5f);
        p.lineTo(c.x + 3.5f, c.y - 2.0f);
    }
    else
    {
        // Right arrow ▶
        p.startNewSubPath(c.x - 2.0f, c.y - 3.5f);
        p.lineTo(c.x + 2.5f, c.y);
        p.lineTo(c.x - 2.0f, c.y + 3.5f);
    }
    p.closeSubPath();
    g.fillPath(p);
}

/** @brief Lucide bell icon — matches ABDSharedAssets/icons/bell.svg */
inline void drawBell(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    static const juce::String bellSvgXml =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
        "stroke=\"#FFFFFF\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
        "<path d=\"M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9\"/>"
        "<path d=\"M10.3 21a1.94 1.94 0 0 0 3.4 0\"/>"
        "</svg>";

    auto xml = juce::parseXML(bellSvgXml);
    if (xml != nullptr)
    {
        auto drawable = juce::Drawable::createFromSVG(*xml);
        if (drawable != nullptr)
        {
            drawable->replaceColour(juce::Colours::white, col);
            drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
}

} // namespace abdaudiolab::gui::suite_icons
