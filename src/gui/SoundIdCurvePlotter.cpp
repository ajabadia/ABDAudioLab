#include "SoundIdCurvePlotter.h"
#include <cmath>

namespace abdaudiolab::gui
{

SoundIdCurvePlotter::SoundIdCurvePlotter()
{
    btnCurve.setButtonText(juce::String::fromUTF8(u8"Curve (\u03bc \u00b1 \u03c3)"));
    btnHeatmap.setButtonText("2D Heatmap");
    btnSpectrum.setButtonText("Spectrum FFT");

    btnCurve.setTooltip(juce::String::fromUTF8(u8"Display statistical mean response curve (\u03bc) with shaded confidence band (\u00b1\u03c3)"));
    btnHeatmap.setTooltip("Display 2D parameter excitation grid heatmap with Viridis color scale");
    btnSpectrum.setTooltip("Live FFT spectrum analyzer (20 Hz - 20 kHz, logarithmic)");

    addAndMakeVisible(btnCurve);
    addAndMakeVisible(btnHeatmap);
    addAndMakeVisible(btnSpectrum);
    addChildComponent(spectrumAnalyzer);

    btnCurve.onClick = [this] { setViewMode(ViewMode::FrequencyCurve); };
    btnHeatmap.onClick = [this] { setViewMode(ViewMode::Heatmap2D); };
    btnSpectrum.onClick = [this] { setViewMode(ViewMode::SpectrumFFT); };
    setViewMode(ViewMode::FrequencyCurve);
}

void SoundIdCurvePlotter::clear()
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points.clear();
    highlightedPointIndex = -1;
    spectrumAnalyzer.clearFrozenSpectrum();
    repaint();
}

void SoundIdCurvePlotter::addMeasuredPoint(const exporting::MeasuredPoint& point)
{
    {
        std::lock_guard<std::mutex> lock(pointsMutex);
        points.push_back(point);
    }
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setPoints(const std::vector<exporting::MeasuredPoint>& newPoints)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points = newPoints;
    highlightedPointIndex = -1;
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::removePoint(int index)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    if (index >= 0 && index < static_cast<int>(points.size()))
    {
        points.erase(points.begin() + index);
        highlightedPointIndex = -1;
    }
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setHighlightedPointIndex(int index)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    highlightedPointIndex = index;
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setViewMode(ViewMode mode)
{
    currentView = mode;

    auto setTab = [](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? SoundIdTheme::pillBlackBg : SoundIdTheme::pillWhiteBg);
        btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : SoundIdTheme::textPrimary);
    };

    setTab(btnCurve, mode == ViewMode::FrequencyCurve);
    setTab(btnHeatmap, mode == ViewMode::Heatmap2D);
    setTab(btnSpectrum, mode == ViewMode::SpectrumFFT);

    spectrumAnalyzer.setVisible(mode == ViewMode::SpectrumFFT);
    repaint();
    resized();
}

void SoundIdCurvePlotter::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(32).reduced(4, 2);

    // Tab buttons aligned to the right with compact spacing
    btnSpectrum.setBounds(topBar.removeFromRight(102));
    topBar.removeFromRight(4);
    btnHeatmap.setBounds(topBar.removeFromRight(92));
    topBar.removeFromRight(4);
    btnCurve.setBounds(topBar.removeFromRight(102));

    // Spectrum analyzer fills the plot area when visible
    if (currentView == ViewMode::SpectrumFFT)
    {
        spectrumAnalyzer.setBounds(area.reduced(4, 0));
    }
}

void SoundIdCurvePlotter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // White Card Background with rounded corners & border
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Reserve right side for buttons (310px) so legend never overlaps tab buttons
    auto headerArea = bounds.removeFromTop(32.0f).reduced(12.0f, 0.0f).withTrimmedRight(310.0f);
    drawLegend(g, headerArea);

    // Skip painting plot content when spectrum is active (it's a child component)
    if (currentView == ViewMode::SpectrumFFT)
        return;

    auto plotArea = bounds.reduced(8.0f, 6.0f);

    if (currentView == ViewMode::FrequencyCurve)
    {
        drawFrequencyPlot(g, plotArea);
    }
    else
    {
        drawHeatmap2D(g, plotArea);
    }
}

void SoundIdCurvePlotter::mouseMove(const juce::MouseEvent& e)
{
    hoverMousePos = e.position;
    isHoveringPlot = lastGridBounds.contains(hoverMousePos);

    if (isHoveringPlot && !points.empty())
    {
        float closestDist = 1e9f;
        int closestIdx = -1;
        for (size_t i = 0; i < points.size(); ++i)
        {
            float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;
            float px = lastGridBounds.getX() + normX * lastGridBounds.getWidth();
            float dist = std::abs(hoverMousePos.x - px);
            if (dist < closestDist)
            {
                closestDist = dist;
                closestIdx = static_cast<int>(i);
            }
        }
        hoverPointIndex = closestIdx;
    }
    else
    {
        hoverPointIndex = -1;
    }
    repaint();
}

void SoundIdCurvePlotter::mouseExit(const juce::MouseEvent&)
{
    isHoveringPlot = false;
    hoverPointIndex = -1;
    repaint();
}

void SoundIdCurvePlotter::drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea)
{
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);

    juce::String title;
    if (currentView == ViewMode::FrequencyCurve)
        title = "Parameter & Response Curves";
    else if (currentView == ViewMode::Heatmap2D)
        title = "2D Parameter Excitation Heatmap";
    else
        title = "Live FFT Spectrum Analyzer";

    // Compact title width
    g.drawText(title, legendArea.removeFromLeft(215.0f), juce::Justification::centredLeft, true);

    if (currentView == ViewMode::FrequencyCurve)
    {
        auto drawDot = [&](const juce::String& text, juce::Colour col, float width)
        {
            if (legendArea.getWidth() < width) return;
            auto itemArea = legendArea.removeFromLeft(width);
            float cy = itemArea.getCentreY();
            g.setColour(col);
            g.fillEllipse(itemArea.getX(), cy - 4.0f, 8.0f, 8.0f);

            g.setColour(SoundIdTheme::textSecondary);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(text, itemArea.withTrimmedLeft(11.0f), juce::Justification::centredLeft, true);
        };

        drawDot(juce::String::fromUTF8(u8"Mean (\u03bc)"), SoundIdTheme::accentGreen, 72.0f);
        drawDot(juce::String::fromUTF8(u8"\u00b1\u03c3 Band"), SoundIdTheme::accentPurple, 72.0f);
        drawDot("THD %", SoundIdTheme::accentAmber, 55.0f);
    }
}

void SoundIdCurvePlotter::drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    // Draw horizontal dB grid lines
    float dbValues[] = { 12.0f, 8.0f, 4.0f, 0.0f, -4.0f, -8.0f, -12.0f };
    float topDb = 12.0f;
    float botDb = -12.0f;

    auto gridBounds = plotArea.withTrimmedRight(38.0f).withTrimmedBottom(18.0f);
    lastGridBounds = gridBounds;

    g.setFont(juce::FontOptions(10.5f));
    for (float db : dbValues)
    {
        float normY = (topDb - db) / (topDb - botDb);
        float y = gridBounds.getY() + normY * gridBounds.getHeight();

        // Line
        if (std::abs(db) < 0.1f)
        {
            g.setColour(SoundIdTheme::textPrimary.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(y), gridBounds.getX(), gridBounds.getRight());
        }
        else
        {
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawHorizontalLine(static_cast<int>(y), gridBounds.getX(), gridBounds.getRight());
        }

        // dB Text on the right
        g.setColour(SoundIdTheme::textMuted);
        juce::String txt = (db > 0 ? "+" : "") + juce::String(static_cast<int>(db)) + "dB";
        g.drawText(txt, static_cast<int>(gridBounds.getRight() + 6.0f), static_cast<int>(y - 6.0f), 35, 12, juce::Justification::centredLeft, false);
    }

    // Draw vertical frequency grid lines (100 Hz, 1 kHz, 10 kHz)
    auto drawFreqLine = [&](float freqHz, const juce::String& label)
    {
        float minF = 20.0f, maxF = 20000.0f;
        float normX = (std::log10(freqHz) - std::log10(minF)) / (std::log10(maxF) - std::log10(minF));
        float x = gridBounds.getX() + normX * gridBounds.getWidth();

        g.setColour(SoundIdTheme::borderSubtle);
        g.drawVerticalLine(static_cast<int>(x), gridBounds.getY(), gridBounds.getBottom());

        g.setColour(SoundIdTheme::textMuted);
        g.drawText(label, static_cast<int>(x - 20.0f), static_cast<int>(gridBounds.getBottom() + 4.0f), 40, 14, juce::Justification::centred, false);
    };

    drawFreqLine(100.0f, "100 Hz");
    drawFreqLine(1000.0f, "1 kHz");
    drawFreqLine(10000.0f, "10 kHz");

    // Plot curves
    std::lock_guard<std::mutex> lock(pointsMutex);
    if (points.empty())
    {
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Ready for measurement. Start profiling session to visualize hardware curves.", gridBounds, juce::Justification::centred, true);
        return;
    }

    // Build curve paths
    juce::Path meanPath;
    juce::Path sigmaBand;
    std::vector<juce::Point<float>> topPts, botPts;

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& pt = points[i];
        float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;

        float valDb = pt.secondaryValue.mean;
        if (std::abs(valDb) < 1e-4f) valDb = (pt.param1Normalized - 0.5f) * 12.0f;

        float normY = std::clamp((topDb - valDb) / (topDb - botDb), 0.0f, 1.0f);
        float sigmaNorm = std::clamp(pt.muSigmaValue.stdDev / 5.0f, 0.02f, 0.2f);

        float px = gridBounds.getX() + normX * gridBounds.getWidth();
        float py = gridBounds.getY() + normY * gridBounds.getHeight();
        float pyTop = gridBounds.getY() + std::clamp(normY - sigmaNorm, 0.0f, 1.0f) * gridBounds.getHeight();
        float pyBot = gridBounds.getY() + std::clamp(normY + sigmaNorm, 0.0f, 1.0f) * gridBounds.getHeight();

        if (i == 0) meanPath.startNewSubPath(px, py);
        else meanPath.lineTo(px, py);

        topPts.push_back({ px, pyTop });
        botPts.push_back({ px, pyBot });
    }

    // 1. Shaded ±σ Band (Soft Lilac / Lavender)
    if (!topPts.empty())
    {
        sigmaBand.startNewSubPath(topPts[0]);
        for (size_t i = 1; i < topPts.size(); ++i) sigmaBand.lineTo(topPts[i]);
        for (int i = static_cast<int>(botPts.size()) - 1; i >= 0; --i) sigmaBand.lineTo(botPts[static_cast<size_t>(i)]);
        sigmaBand.closeSubPath();

        g.setColour(SoundIdTheme::accentPurpleFill);
        g.fillPath(sigmaBand);

        g.setColour(SoundIdTheme::accentPurple.withAlpha(0.6f));
        juce::Path topOutline, botOutline;
        topOutline.startNewSubPath(topPts[0]);
        for (size_t i = 1; i < topPts.size(); ++i) topOutline.lineTo(topPts[i]);
        botOutline.startNewSubPath(botPts[0]);
        for (size_t i = 1; i < botPts.size(); ++i) botOutline.lineTo(botPts[i]);
        g.strokePath(topOutline, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath(botOutline, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 2. Mean Response Curve (Solid Emerald Green)
    g.setColour(SoundIdTheme::accentGreen);
    g.strokePath(meanPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Measured Data Point Nodes
    for (size_t i = 0; i < points.size(); ++i)
    {
        float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;
        float valDb = points[i].secondaryValue.mean;
        if (std::abs(valDb) < 1e-4f) valDb = (points[i].param1Normalized - 0.5f) * 12.0f;
        float normY = std::clamp((topDb - valDb) / (topDb - botDb), 0.0f, 1.0f);

        float px = gridBounds.getX() + normX * gridBounds.getWidth();
        float py = gridBounds.getY() + normY * gridBounds.getHeight();

        if (static_cast<int>(i) == highlightedPointIndex)
        {
            g.setColour(SoundIdTheme::accentAmber.withAlpha(0.4f));
            g.fillEllipse(px - 10.0f, py - 10.0f, 20.0f, 20.0f);
            g.setColour(SoundIdTheme::accentAmber);
            g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.5f);
        }
        else
        {
            g.setColour(juce::Colours::white);
            g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour(SoundIdTheme::accentGreen);
            g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 2.0f);
        }
    }

    // 4. Interactive Crosshair & Tooltip Overlay
    drawCrosshairAndTooltip(g, gridBounds);
}

void SoundIdCurvePlotter::drawCrosshairAndTooltip(juce::Graphics& g, juce::Rectangle<float> gridBounds)
{
    if (!isHoveringPlot || hoverPointIndex < 0 || hoverPointIndex >= static_cast<int>(points.size()))
        return;

    const auto& pt = points[static_cast<size_t>(hoverPointIndex)];
    float normX = (points.size() > 1) ? (static_cast<float>(hoverPointIndex) / static_cast<float>(points.size() - 1)) : 0.5f;
    float valDb = pt.secondaryValue.mean;
    if (std::abs(valDb) < 1e-4f) valDb = (pt.param1Normalized - 0.5f) * 12.0f;

    float topDb = 12.0f;
    float botDb = -12.0f;
    float normY = std::clamp((topDb - valDb) / (topDb - botDb), 0.0f, 1.0f);

    float px = gridBounds.getX() + normX * gridBounds.getWidth();
    float py = gridBounds.getY() + normY * gridBounds.getHeight();

    // Subtle crosshair lines
    g.setColour(SoundIdTheme::textSecondary.withAlpha(0.28f));
    float dashes[] = { 3.0f, 3.0f };
    g.drawDashedLine(juce::Line<float>(px, gridBounds.getY(), px, gridBounds.getBottom()), dashes, 2, 1.0f);
    g.drawDashedLine(juce::Line<float>(gridBounds.getX(), py, gridBounds.getRight(), py), dashes, 2, 1.0f);

    // Target highlight ring
    g.setColour(SoundIdTheme::accentAmber.withAlpha(0.35f));
    g.fillEllipse(px - 9.0f, py - 9.0f, 18.0f, 18.0f);
    g.setColour(SoundIdTheme::accentAmber);
    g.drawEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f, 1.5f);

    // Floating Tooltip Card
    float cardW = 154.0f;
    float cardH = 58.0f;
    float cardX = px + 12.0f;
    if (cardX + cardW > gridBounds.getRight())
        cardX = px - cardW - 12.0f;

    float cardY = py - cardH - 8.0f;
    if (cardY < gridBounds.getY())
        cardY = py + 12.0f;

    auto tooltipRect = juce::Rectangle<float>(cardX, cardY, cardW, cardH);
    g.setColour(SoundIdTheme::pillBlackBg.withAlpha(0.94f));
    g.fillRoundedRectangle(tooltipRect, 6.0f);
    g.setColour(SoundIdTheme::borderCard.withAlpha(0.35f));
    g.drawRoundedRectangle(tooltipRect, 6.0f, 1.0f);

    // Tooltip content
    auto textRect = tooltipRect.reduced(8.0f, 4.0f);
    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    float stepPct = pt.param1Normalized * 100.0f;
    g.drawText("Point #" + juce::String(hoverPointIndex + 1) + " (" + juce::String(stepPct, 0) + "%)",
               textRect.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions("Consolas", 10.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffe5e7eb));
    juce::String gainStr = "Gain: " + juce::String(valDb > 0 ? "+" : "") + juce::String(valDb, 2) + " dB";
    g.drawText(gainStr, textRect.removeFromTop(14.0f), juce::Justification::centredLeft, true);

    g.setColour(SoundIdTheme::accentAmber);
    juce::String metricsStr = "THD: " + juce::String(pt.thdPercent, 2) + "% | SNR: " + juce::String(pt.snrDb, 1) + " dB";
    g.drawText(metricsStr, textRect.removeFromTop(14.0f), juce::Justification::centredLeft, true);
}

void SoundIdCurvePlotter::drawHeatmap2D(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
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
    for (const auto& pt : points)
    {
        if (pt.muSigmaValue.mean < minVal) minVal = pt.muSigmaValue.mean;
        if (pt.muSigmaValue.mean > maxVal) maxVal = pt.muSigmaValue.mean;
    }
    if (std::abs(maxVal - minVal) < 1e-4f) maxVal += 1.0f;

    // Draw cells with high-contrast perceptual color map
    for (size_t i = 0; i < points.size(); ++i)
    {
        int row = static_cast<int>(i / static_cast<size_t>(gridDim));
        int col = static_cast<int>(i % static_cast<size_t>(gridDim));

        float normVal = (points[i].muSigmaValue.mean - minVal) / (maxVal - minVal);
        juce::Colour cellColor = viridisColor(normVal);

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

// Viridis/Plasma warm perceptual color map (with high-contrast amber top)
juce::Colour SoundIdCurvePlotter::viridisColor(float t) noexcept
{
    t = std::clamp(t, 0.0f, 1.0f);

    // Perceptually balanced stops: deep violet -> cobalt -> teal -> emerald -> warm gold/amber
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

} // namespace abdaudiolab::gui
