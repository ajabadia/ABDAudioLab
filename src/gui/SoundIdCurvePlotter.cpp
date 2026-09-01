#include "SoundIdCurvePlotter.h"
#include <cmath>

namespace abdaudiolab::gui
{

SoundIdCurvePlotter::SoundIdCurvePlotter()
{
    btnCurve.setButtonText(juce::String::fromUTF8(u8"Curve (μ ± σ)"));
    btnHeatmap.setButtonText(juce::String::fromUTF8(u8"2D Heatmap"));

    btnCurve.setTooltip("Display statistical mean response curve (μ) with shaded confidence band (±σ)");
    btnHeatmap.setTooltip("Display 2D parameter excitation grid heatmap");

    addAndMakeVisible(btnCurve);
    addAndMakeVisible(btnHeatmap);

    btnCurve.onClick = [this] { setViewMode(ViewMode::FrequencyCurve); };
    btnHeatmap.onClick = [this] { setViewMode(ViewMode::Heatmap2D); };
    setViewMode(ViewMode::FrequencyCurve);
}

void SoundIdCurvePlotter::clear()
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points.clear();
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

void SoundIdCurvePlotter::setViewMode(ViewMode mode)
{
    currentView = mode;
    btnCurve.setColour(juce::TextButton::buttonColourId, mode == ViewMode::FrequencyCurve ? SoundIdTheme::pillBlackBg : SoundIdTheme::pillWhiteBg);
    btnCurve.setColour(juce::TextButton::textColourOffId, mode == ViewMode::FrequencyCurve ? juce::Colours::white : SoundIdTheme::textPrimary);

    btnHeatmap.setColour(juce::TextButton::buttonColourId, mode == ViewMode::Heatmap2D ? SoundIdTheme::pillBlackBg : SoundIdTheme::pillWhiteBg);
    btnHeatmap.setColour(juce::TextButton::textColourOffId, mode == ViewMode::Heatmap2D ? juce::Colours::white : SoundIdTheme::textPrimary);
    repaint();
}

void SoundIdCurvePlotter::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(32);
    btnCurve.setBounds(topBar.removeFromRight(130).reduced(2));
    btnHeatmap.setBounds(topBar.removeFromRight(110).reduced(2));
}

void SoundIdCurvePlotter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // White Card Background with rounded corners & border
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    auto headerArea = bounds.removeFromTop(32.0f).reduced(12.0f, 0.0f);
    drawLegend(g, headerArea);

    auto plotArea = bounds.reduced(16.0f, 12.0f);

    if (currentView == ViewMode::FrequencyCurve)
    {
        drawFrequencyPlot(g, plotArea);
    }
    else
    {
        drawHeatmap2D(g, plotArea);
    }
}

void SoundIdCurvePlotter::drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea)
{
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Parameter & Frequency Response Curves", legendArea.removeFromLeft(280.0f), juce::Justification::centredLeft, true);

    auto drawDot = [&](const juce::String& text, juce::Colour col, float width)
    {
        auto itemArea = legendArea.removeFromLeft(width);
        float cy = itemArea.getCentreY();
        g.setColour(col);
        g.fillEllipse(itemArea.getX(), cy - 4.0f, 8.0f, 8.0f);

        g.setColour(SoundIdTheme::textSecondary);
        g.setFont(juce::FontOptions(11.5f));
        g.drawText(text, itemArea.withTrimmedLeft(12.0f), juce::Justification::centredLeft, true);
    };

    drawDot(juce::String::fromUTF8(u8"Mean (μ)"), SoundIdTheme::accentGreen, 90.0f);
    drawDot(juce::String::fromUTF8(u8"±σ Band"), SoundIdTheme::accentPurple, 90.0f);
    drawDot("THD %", SoundIdTheme::accentAmber, 80.0f);
}

void SoundIdCurvePlotter::drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    // Draw horizontal dB grid lines
    float dbValues[] = { 12.0f, 8.0f, 4.0f, 0.0f, -4.0f, -8.0f, -12.0f };
    float topDb = 12.0f;
    float botDb = -12.0f;

    auto gridBounds = plotArea.withTrimmedRight(45.0f).withTrimmedBottom(20.0f);

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

        g.setColour(juce::Colours::white);
        g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 2.0f);
    }
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
        juce::Colour cellColor = juce::Colour::fromHSV(0.35f + (normVal * 0.45f), 0.7f, 0.9f, 1.0f);

        auto cellRect = juce::Rectangle<float>(plotArea.getX() + col * cellW,
                                                plotArea.getY() + row * cellH,
                                                cellW - 1.0f,
                                                cellH - 1.0f);
        g.setColour(cellColor);
        g.fillRoundedRectangle(cellRect, 3.0f);
    }
}

} // namespace abdaudiolab::gui
