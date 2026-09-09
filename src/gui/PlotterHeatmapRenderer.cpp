#include "PlotterHeatmapRenderer.h"
#include "SoundIdTheme.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::gui
{

PlotterHeatmapRenderer::PlotterHeatmapRenderer()
{
    setOpaque(false);
}

void PlotterHeatmapRenderer::setPoints(const std::vector<exporting::MeasuredPoint>& newPoints)
{
    points = newPoints;
    repaint();
}

void PlotterHeatmapRenderer::clear()
{
    points.clear();
    repaint();
}

juce::Colour PlotterHeatmapRenderer::viridisColor(float t) noexcept
{
    t = std::clamp(t, 0.0f, 1.0f);

    struct ColorStop { float pos; uint8_t r, g, b; };
    static constexpr ColorStop stops[] = {
        { 0.00f,  35,  18,  72 }, // Deep violet
        { 0.16f,  45,  55, 120 }, // Cobalt
        { 0.32f,  30, 100, 140 }, // Cyan/Blue
        { 0.50f,  20, 135, 120 }, // Teal
        { 0.68f,  29, 185,  84 }, // Vibrant green (#1DB954)
        { 0.84f, 210, 175,  35 }, // Gold
        { 1.00f, 245, 166,  35 }  // Technical Amber (#F5A623)
    };

    int idx = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (t >= stops[i].pos && t <= stops[i + 1].pos)
        {
            idx = i;
            break;
        }
    }

    float segT = (t - stops[idx].pos) / (stops[idx + 1].pos - stops[idx].pos);
    segT = std::clamp(segT, 0.0f, 1.0f);

    auto lerp = [](uint8_t a, uint8_t b, float f) -> uint8_t {
        return static_cast<uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * f);
    };

    return juce::Colour(lerp(stops[idx].r, stops[idx + 1].r, segT),
                        lerp(stops[idx].g, stops[idx + 1].g, segT),
                        lerp(stops[idx].b, stops[idx + 1].b, segT));
}

void PlotterHeatmapRenderer::paint(juce::Graphics& g)
{
    auto plotArea = getLocalBounds().toFloat().reduced(4.0f, 2.0f);

    if (points.empty())
    {
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No 2D parameter grid data collected yet.", plotArea, juce::Justification::centred, true);
        return;
    }

    // Reserve space for color bar on the right
    auto colorBarArea = plotArea.removeFromRight(24.0f);
    plotArea.removeFromRight(8.0f);

    int gridDim = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(points.size()))));
    if (gridDim < 2) gridDim = 2;

    float cellW = plotArea.getWidth() / static_cast<float>(gridDim);
    float cellH = plotArea.getHeight() / static_cast<float>(gridDim);

    float minVal = 1e9f, maxVal = -1e9f;
    bool hasNonZero = false;
    for (const auto& pt : points)
    {
        if (std::abs(pt.muSigmaValue.mean) > 1e-4f)
            hasNonZero = true;
        if (pt.muSigmaValue.mean < minVal) minVal = pt.muSigmaValue.mean;
        if (pt.muSigmaValue.mean > maxVal) maxVal = pt.muSigmaValue.mean;
    }
    if (!hasNonZero || std::abs(maxVal - minVal) < 1e-4f)
    {
        minVal = 0.0f;
        maxVal = 1.0f;
    }

    // Draw cells with high-contrast perceptual color map
    for (size_t i = 0; i < points.size(); ++i)
    {
        int row = static_cast<int>(i / static_cast<size_t>(gridDim));
        int col = static_cast<int>(i % static_cast<size_t>(gridDim));

        float val = points[i].muSigmaValue.mean;
        juce::Colour cellColor = SoundIdTheme::surfaceSubtle;
        if (hasNonZero)
        {
            float normVal = (val - minVal) / (maxVal - minVal);
            cellColor = viridisColor(normVal);
        }

        auto cellRect = juce::Rectangle<float>(plotArea.getX() + col * cellW,
                                                plotArea.getY() + row * cellH,
                                                cellW - 1.0f,
                                                cellH - 1.0f);
        g.setColour(cellColor);
        g.fillRoundedRectangle(cellRect, 2.0f);

        // Value text overlay with dynamic perceptual contrast
        if (cellW > 32.0f && cellH > 18.0f)
        {
            juce::Colour textCol = (cellColor.getPerceivedBrightness() > 0.62f) ? SoundIdTheme::textPrimary : juce::Colours::white;
            g.setColour(textCol);
            g.setFont(juce::FontOptions(std::min(9.5f, cellH * 0.5f)));
            g.drawText(juce::String(points[i].muSigmaValue.mean, 1), cellRect, juce::Justification::centred, false);
        }
    }

    // Axis labels
    g.setColour(SoundIdTheme::textMuted);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Param 1 ->", plotArea.withHeight(14.0f).translated(0.0f, plotArea.getHeight() + 2.0f), juce::Justification::centred, true);

    // Vertical color bar with gradient
    for (int y = 0; y < static_cast<int>(colorBarArea.getHeight()); ++y)
    {
        float normY = 1.0f - (static_cast<float>(y) / colorBarArea.getHeight());
        g.setColour(viridisColor(normY));
        g.fillRect(colorBarArea.getX(), colorBarArea.getY() + static_cast<float>(y), colorBarArea.getWidth(), 1.0f);
    }
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(colorBarArea, 2.0f, 1.0f);

    // Min/Max labels
    g.setColour(SoundIdTheme::textMuted);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(juce::String(maxVal, 1), colorBarArea.translated(0.0f, -12.0f).withHeight(12.0f), juce::Justification::centred, false);
    g.drawText(juce::String(minVal, 1), colorBarArea.translated(0.0f, colorBarArea.getHeight() + 1.0f).withHeight(12.0f), juce::Justification::centred, false);
}

} // namespace abdaudiolab::gui
