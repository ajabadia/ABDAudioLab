#include "LiveCurvePlotter.h"

namespace abdaudiolab::gui
{

LiveCurvePlotter::LiveCurvePlotter()
{
    addAndMakeVisible(btnCurve);
    addAndMakeVisible(btnHeatmap);

    btnCurve.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2d34));
    btnHeatmap.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2d34));

    btnCurve.onClick = [this] { setDisplayMode(DisplayMode::TransferCurve); };
    btnHeatmap.onClick = [this] { setDisplayMode(DisplayMode::Heatmap2D); };
}

void LiveCurvePlotter::clear()
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points.clear();
    repaint();
}

void LiveCurvePlotter::addMeasuredPoint(const exporting::MeasuredPoint& point)
{
    {
        std::lock_guard<std::mutex> lock(pointsMutex);
        points.push_back(point);
    }
    juce::MessageManager::callAsync([this] { repaint(); });
}

void LiveCurvePlotter::setDisplayMode(DisplayMode mode)
{
    currentMode = mode;
    btnCurve.setColour(juce::TextButton::buttonColourId, mode == DisplayMode::TransferCurve ? juce::Colour(0xff007acc) : juce::Colour(0xff2a2d34));
    btnHeatmap.setColour(juce::TextButton::buttonColourId, mode == DisplayMode::Heatmap2D ? juce::Colour(0xff007acc) : juce::Colour(0xff2a2d34));
    repaint();
}

void LiveCurvePlotter::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(26);
    btnCurve.setBounds(topBar.removeFromLeft(170).reduced(2));
    btnHeatmap.setBounds(topBar.removeFromLeft(110).reduced(2));
}

void LiveCurvePlotter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xff16171a));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Border
    g.setColour(juce::Colour(0xff2a2d34));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto plotArea = bounds.withTrimmedTop(30.0f).reduced(12.0f);

    // Inner plot background
    g.setColour(juce::Colour(0xff0f1012));
    g.fillRoundedRectangle(plotArea, 4.0f);

    // Grid lines
    g.setColour(juce::Colour(0xff20232a));
    for (int i = 1; i < 6; ++i)
    {
        float x = plotArea.getX() + (plotArea.getWidth() * i / 6.0f);
        float y = plotArea.getY() + (plotArea.getHeight() * i / 6.0f);
        g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());
        g.drawVerticalLine(static_cast<int>(x), plotArea.getY(), plotArea.getBottom());
    }

    if (currentMode == DisplayMode::TransferCurve)
    {
        drawTransferCurve(g, plotArea);
    }
    else
    {
        drawHeatmap2D(g, plotArea);
    }
}

void LiveCurvePlotter::drawTransferCurve(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    std::lock_guard<std::mutex> lock(pointsMutex);

    if (points.empty())
    {
        g.setColour(juce::Colours::grey.withAlpha(0.6f));
        g.setFont(14.0f);
        g.drawText("Waiting for measurement signals...", plotArea, juce::Justification::centred, true);
        return;
    }

    // Determine min/max values
    float minVal = 1e9f, maxVal = -1e9f;
    for (const auto& pt : points)
    {
        float v = pt.muSigmaValue.mean;
        float s = pt.muSigmaValue.stdDev;
        if (v - s < minVal) minVal = v - s;
        if (v + s > maxVal) maxVal = v + s;
    }

    if (std::abs(maxVal - minVal) < 1e-4f)
    {
        maxVal += 1.0f;
        minVal -= 1.0f;
    }

    // Build curve path
    juce::Path meanPath;
    juce::Path sigmaBandPath;

    std::vector<juce::Point<float>> topPoints;
    std::vector<juce::Point<float>> bottomPoints;

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& pt = points[i];
        float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;
        float normY = (pt.muSigmaValue.mean - minVal) / (maxVal - minVal);
        float normSigma = pt.muSigmaValue.stdDev / (maxVal - minVal);

        float px = plotArea.getX() + normX * plotArea.getWidth();
        float py = plotArea.getBottom() - normY * plotArea.getHeight();
        float pyTop = plotArea.getBottom() - std::clamp(normY + normSigma, 0.0f, 1.0f) * plotArea.getHeight();
        float pyBottom = plotArea.getBottom() - std::clamp(normY - normSigma, 0.0f, 1.0f) * plotArea.getHeight();

        if (i == 0)
            meanPath.startNewSubPath(px, py);
        else
            meanPath.lineTo(px, py);

        topPoints.push_back({ px, pyTop });
        bottomPoints.push_back({ px, pyBottom });
    }

    // Draw shaded sigma band
    if (!topPoints.empty())
    {
        sigmaBandPath.startNewSubPath(topPoints[0]);
        for (size_t i = 1; i < topPoints.size(); ++i)
            sigmaBandPath.lineTo(topPoints[i]);
        for (int i = static_cast<int>(bottomPoints.size()) - 1; i >= 0; --i)
            sigmaBandPath.lineTo(bottomPoints[static_cast<size_t>(i)]);
        sigmaBandPath.closeSubPath();

        g.setColour(juce::Colour(0xff00bcd4).withAlpha(0.2f));
        g.fillPath(sigmaBandPath);
    }

    // Draw Mean Line
    g.setColour(juce::Colour(0xff00e5ff));
    g.strokePath(meanPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Draw measured data points
    for (size_t i = 0; i < points.size(); ++i)
    {
        float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;
        float normY = (points[i].muSigmaValue.mean - minVal) / (maxVal - minVal);
        float px = plotArea.getX() + normX * plotArea.getWidth();
        float py = plotArea.getBottom() - normY * plotArea.getHeight();

        g.setColour(juce::Colour(0xffffffff));
        g.fillEllipse(px - 3.5f, py - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colour(0xff00bcd4));
        g.drawEllipse(px - 3.5f, py - 3.5f, 7.0f, 7.0f, 1.5f);
    }

    // Legend & latest readouts
    const auto& last = points.back();
    juce::String readout = "Latest Point: mu = " + juce::String(last.muSigmaValue.mean, 2)
                         + " | sigma = " + juce::String(last.muSigmaValue.stdDev, 3)
                         + " | Range: [" + juce::String(minVal, 1) + " .. " + juce::String(maxVal, 1) + "]";
    g.setColour(juce::Colour(0xffe0e0e0));
    g.setFont(12.0f);
    g.drawText(readout, plotArea.reduced(6.0f), juce::Justification::topRight, true);
}

void LiveCurvePlotter::drawHeatmap2D(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    std::lock_guard<std::mutex> lock(pointsMutex);

    if (points.empty())
    {
        g.setColour(juce::Colours::grey.withAlpha(0.6f));
        g.setFont(14.0f);
        g.drawText("No 2D parameter grid data collected yet.", plotArea, juce::Justification::centred, true);
        return;
    }

    int gridDim = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(points.size()))));
    if (gridDim < 2) gridDim = 2;

    float cellW = plotArea.getWidth() / static_cast<float>(gridDim);
    float cellH = plotArea.getHeight() / static_cast<float>(gridDim);

    float minVal = 1e9f, maxVal = -1e9f;
    for (const auto& pt : points)
    {
        if (pt.muSigmaValue.mean < minVal) minVal = pt.muSigmaValue.mean;
        if (pt.muSigmaValue.mean > maxVal) maxVal = pt.muSigmaValue.mean;
    }
    if (std::abs(maxVal - minVal) < 1e-4f) maxVal += 1.0f;

    for (size_t i = 0; i < points.size(); ++i)
    {
        int row = static_cast<int>(i / static_cast<size_t>(gridDim));
        int col = static_cast<int>(i % static_cast<size_t>(gridDim));

        float normVal = (points[i].muSigmaValue.mean - minVal) / (maxVal - minVal);
        juce::Colour cellColor = juce::Colour::fromHSV(0.6f - (normVal * 0.6f), 0.85f, 0.9f, 1.0f);

        auto cellRect = juce::Rectangle<float>(plotArea.getX() + col * cellW,
                                                plotArea.getY() + row * cellH,
                                                cellW - 1.0f,
                                                cellH - 1.0f);
        g.setColour(cellColor);
        g.fillRect(cellRect);
    }
}

} // namespace abdaudiolab::gui
