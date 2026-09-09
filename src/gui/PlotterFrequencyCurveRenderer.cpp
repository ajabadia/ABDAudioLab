#include "PlotterFrequencyCurveRenderer.h"
#include "SoundIdTheme.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::gui
{

PlotterFrequencyCurveRenderer::PlotterFrequencyCurveRenderer()
{
    setOpaque(false);
}

void PlotterFrequencyCurveRenderer::setPoints(const std::vector<exporting::MeasuredPoint>& newPoints)
{
    points = newPoints;
    repaint();
}

void PlotterFrequencyCurveRenderer::clear()
{
    points.clear();
    highlightedPointIndex = -1;
    hoverPointIndex = -1;
    repaint();
}

void PlotterFrequencyCurveRenderer::setHighlightedPointIndex(int index)
{
    highlightedPointIndex = index;
    repaint();
}

void PlotterFrequencyCurveRenderer::setShowMeanCurve(bool show) noexcept
{
    showMeanCurve = show;
    repaint();
}

void PlotterFrequencyCurveRenderer::setShowSigmaBand(bool show) noexcept
{
    showSigmaBand = show;
    repaint();
}

void PlotterFrequencyCurveRenderer::setShowThdPoints(bool show) noexcept
{
    showThdPoints = show;
    repaint();
}

void PlotterFrequencyCurveRenderer::setMeasuringProgress(bool measuring, float progress) noexcept
{
    isMeasuring = measuring;
    measuringProgress = progress;
    repaint();
}

void PlotterFrequencyCurveRenderer::setPreScanData(const std::vector<math::PreScanPoint>& trajectory,
                                                  const std::vector<int>& roadmapSteps,
                                                  bool showGhost) noexcept
{
    preScanTrajectory = trajectory;
    preScanRoadmapSteps = roadmapSteps;
    hasPreScanData = !preScanTrajectory.empty();
    showPreScanGhost = showGhost;
    repaint();
}

void PlotterFrequencyCurveRenderer::clearPreScanData() noexcept
{
    preScanTrajectory.clear();
    preScanRoadmapSteps.clear();
    hasPreScanData = false;
    repaint();
}

void PlotterFrequencyCurveRenderer::setHoverPosition(juce::Point<float> pos)
{
    hoverMousePos = pos;
    isHoveringPlot = lastGridBounds.contains(hoverMousePos);

    if (isHoveringPlot && !points.empty())
    {
        float closestDist = 1e9f;
        int bestIdx = -1;

        for (size_t i = 0; i < points.size(); ++i)
        {
            float normX = (points.size() > 1) ? (static_cast<float>(i) / static_cast<float>(points.size() - 1)) : 0.5f;
            float valDb = points[i].secondaryValue.mean;
            if (std::abs(valDb) < 1e-4f) valDb = (points[i].param1Normalized - 0.5f) * 12.0f;
            float normY = std::clamp((12.0f - valDb) / 24.0f, 0.0f, 1.0f);

            float px = lastGridBounds.getX() + normX * lastGridBounds.getWidth();
            float py = lastGridBounds.getY() + normY * lastGridBounds.getHeight();

            float dist = hoverMousePos.getDistanceFrom({ px, py });
            if (dist < closestDist)
            {
                closestDist = dist;
                bestIdx = static_cast<int>(i);
            }
        }

        hoverPointIndex = (closestDist < 35.0f) ? bestIdx : -1;
    }
    else
    {
        hoverPointIndex = -1;
    }
    repaint();
}

void PlotterFrequencyCurveRenderer::clearHover()
{
    isHoveringPlot = false;
    hoverPointIndex = -1;
    repaint();
}

void PlotterFrequencyCurveRenderer::mouseMove(const juce::MouseEvent& e)
{
    setHoverPosition(e.position);
}

void PlotterFrequencyCurveRenderer::mouseExit(const juce::MouseEvent&)
{
    clearHover();
}

void PlotterFrequencyCurveRenderer::paint(juce::Graphics& g)
{
    auto plotArea = getLocalBounds().toFloat().reduced(4.0f, 2.0f);

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

        g.setColour(SoundIdTheme::textMuted);
        juce::String txt = (db > 0 ? "+" : "") + juce::String(static_cast<int>(db)) + "dB";
        g.drawText(txt, static_cast<int>(gridBounds.getRight() + 6.0f), static_cast<int>(y - 6.0f), 35, 12, juce::Justification::centredLeft, false);
    }

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

    // Pre-Scan Adaptive Roadmap Markers and Ghost Curve
    if (hasPreScanData && !preScanTrajectory.empty())
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.22f));
        for (int step : preScanRoadmapSteps)
        {
            float normX = static_cast<float>(step) / 127.0f;
            float xPos = gridBounds.getX() + normX * gridBounds.getWidth();
            g.drawVerticalLine(static_cast<int>(xPos), gridBounds.getY(), gridBounds.getBottom());
        }

        if (showPreScanGhost)
        {
            float minMetric = preScanTrajectory.front().primaryMetric;
            float maxMetric = minMetric;
            for (const auto& pt : preScanTrajectory)
            {
                if (pt.primaryMetric < minMetric) minMetric = pt.primaryMetric;
                if (pt.primaryMetric > maxMetric) maxMetric = pt.primaryMetric;
            }
            float metricRange = (maxMetric - minMetric > 0.0f) ? (maxMetric - minMetric) : 1.0f;

            juce::Path ghostPath;
            bool firstPoint = true;

            for (const auto& pt : preScanTrajectory)
            {
                float normX = std::clamp(pt.controlValue / 127.0f, 0.0f, 1.0f);
                float x = gridBounds.getX() + normX * gridBounds.getWidth();
                float normY = std::clamp((pt.primaryMetric - minMetric) / metricRange, 0.0f, 1.0f);
                float y = gridBounds.getBottom() - (normY * 0.85f + 0.075f) * gridBounds.getHeight();

                if (firstPoint)
                {
                    ghostPath.startNewSubPath(x, y);
                    firstPoint = false;
                }
                else
                {
                    ghostPath.lineTo(x, y);
                }
            }

            g.setColour(juce::Colours::cyan.withAlpha(0.45f));
            juce::Path dashedGhost;
            float dashes[] = { 4.0f, 4.0f };
            juce::PathStrokeType(1.5f).createDashedStroke(dashedGhost, ghostPath, dashes, 2);
            g.strokePath(dashedGhost, juce::PathStrokeType(1.5f));
        }
    }

    if (points.empty())
    {
        if (!hasPreScanData)
        {
            g.setColour(SoundIdTheme::textMuted);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText("Ready for measurement. Start profiling session to visualize hardware curves.", gridBounds, juce::Justification::centred, true);
        }
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

    {
        juce::Graphics::ScopedSaveState clipSave(g);
        g.reduceClipRegion(gridBounds.toNearestInt());

        if (showSigmaBand && !topPts.empty())
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

        if (showMeanCurve)
        {
            g.setColour(SoundIdTheme::accentGreen);
            g.strokePath(meanPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (showThdPoints)
        {
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
                    g.setColour(SoundIdTheme::bgCard);
                    g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
                    g.setColour(SoundIdTheme::accentGreen);
                    g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 2.0f);
                }
            }
        }

        if (isMeasuring && measuringProgress >= 0.0f)
        {
            float beamX = gridBounds.getX() + measuringProgress * gridBounds.getWidth();

            float trailW = std::min(36.0f, beamX - gridBounds.getX());
            if (trailW > 2.0f)
            {
                g.setGradientFill(juce::ColourGradient(
                    juce::Colours::transparentBlack, beamX - trailW, gridBounds.getY(),
                    SoundIdTheme::accentGreen.withAlpha(0.22f), beamX, gridBounds.getY(),
                    false));
                g.fillRect(beamX - trailW, gridBounds.getY(), trailW, gridBounds.getHeight());
            }

            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.85f));
            g.drawVerticalLine(static_cast<int>(beamX), gridBounds.getY(), gridBounds.getBottom());

            g.setColour(juce::Colours::white);
            g.fillEllipse(beamX - 3.0f, gridBounds.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
    }

    drawCrosshairAndTooltip(g, gridBounds);
}

void PlotterFrequencyCurveRenderer::drawCrosshairAndTooltip(juce::Graphics& g, juce::Rectangle<float> gridBounds)
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

    g.setColour(SoundIdTheme::textSecondary.withAlpha(0.28f));
    float dashes[] = { 3.0f, 3.0f };
    g.drawDashedLine(juce::Line<float>(px, gridBounds.getY(), px, gridBounds.getBottom()), dashes, 2, 1.0f);
    g.drawDashedLine(juce::Line<float>(gridBounds.getX(), py, gridBounds.getRight(), py), dashes, 2, 1.0f);

    g.setColour(SoundIdTheme::accentAmber.withAlpha(0.35f));
    g.fillEllipse(px - 9.0f, py - 9.0f, 18.0f, 18.0f);
    g.setColour(SoundIdTheme::accentAmber);
    g.drawEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f, 1.5f);

    float cardW = 180.0f;
    float cardH = 68.0f;
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

    auto textRect = tooltipRect.reduced(8.0f, 4.0f);
    float freqHz = 20.0f * std::pow(1000.0f, normX);
    juce::String freqStr = freqHz >= 1000.0f ? juce::String(freqHz / 1000.0f, 1) + " kHz" : juce::String(freqHz, 0) + " Hz";

    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText("Point #" + juce::String(hoverPointIndex + 1) + " (" + freqStr + ")",
               textRect.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions("Consolas", 9.5f, juce::Font::plain));
    g.setColour(juce::Colour(0xffe5e7eb));
    juce::String gainStr = "Gain: " + juce::String(valDb > 0 ? "+" : "") + juce::String(valDb, 2) + " dB";
    juce::String sigmaStr = juce::String::fromUTF8(u8" | \u03c3: \u00b1") + juce::String(pt.muSigmaValue.stdDev, 2) + " dB";
    g.drawText(gainStr + sigmaStr, textRect.removeFromTop(14.0f), juce::Justification::centredLeft, true);

    g.setColour(SoundIdTheme::accentAmber);
    juce::String metricsStr = "THD: " + juce::String(pt.thdPercent, 2) + "% | SNR: " + juce::String(pt.snrDb, 1) + " dB";
    g.drawText(metricsStr, textRect.removeFromTop(14.0f), juce::Justification::centredLeft, true);

    g.setColour(SoundIdTheme::textMuted);
    float stepPct = pt.param1Normalized * 100.0f;
    g.drawText("Step: " + juce::String(stepPct, 1) + "% (" + juce::String(pt.param1Normalized, 3) + " norm)",
               textRect.removeFromTop(12.0f), juce::Justification::centredLeft, true);
}

} // namespace abdaudiolab::gui
