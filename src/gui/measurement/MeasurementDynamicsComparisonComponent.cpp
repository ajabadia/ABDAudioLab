/**
 * @file MeasurementDynamicsComparisonComponent.cpp
 * @brief Implementation of MeasurementDynamicsComparisonComponent.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementDynamicsComparisonComponent.h"
#include <cmath>

namespace abdaudiolab::gui::measurement
{

MeasurementDynamicsComparisonComponent::MeasurementDynamicsComparisonComponent(MeasurementComparisonSession& session)
    : session_(session)
{
    session_.addListener(this);

    addAndMakeVisible(btnModeLevel_);
    addAndMakeVisible(btnModeCentroid_);
    addAndMakeVisible(btnModeRolloff_);

    btnModeLevel_.onClick = [this]() { setComparisonMode(DynamicsComparisonMode::LevelDbfs); };
    btnModeCentroid_.onClick = [this]() { setComparisonMode(DynamicsComparisonMode::TimbreCentroidHz); };
    btnModeRolloff_.onClick = [this]() { setComparisonMode(DynamicsComparisonMode::TimbreRolloffHz); };

    btnModeLevel_.setToggleState(true, juce::dontSendNotification);
    rebuildCurves();
}

MeasurementDynamicsComparisonComponent::~MeasurementDynamicsComparisonComponent()
{
    session_.removeListener(this);
}

void MeasurementDynamicsComparisonComponent::setComparisonMode(DynamicsComparisonMode mode)
{
    if (mode_ == mode)
        return;

    mode_ = mode;
    btnModeLevel_.setToggleState(mode_ == DynamicsComparisonMode::LevelDbfs, juce::dontSendNotification);
    btnModeCentroid_.setToggleState(mode_ == DynamicsComparisonMode::TimbreCentroidHz, juce::dontSendNotification);
    btnModeRolloff_.setToggleState(mode_ == DynamicsComparisonMode::TimbreRolloffHz, juce::dontSendNotification);

    rebuildCurves();
    repaint();
}

void MeasurementDynamicsComparisonComponent::containerStateChanged(int, ContainerLoadState)
{
    rebuildCurves();
    repaint();
}

void MeasurementDynamicsComparisonComponent::containerListChanged()
{
    rebuildCurves();
    repaint();
}

void MeasurementDynamicsComparisonComponent::domainFilterChanged()
{
    rebuildCurves();
    repaint();
}

void MeasurementDynamicsComparisonComponent::rebuildCurves()
{
    seriesList_.clear();
    const auto eligible = session_.getEligibleComparisonContainers();
    if (eligible.empty())
        return;

    const auto* refVm = eligible.front().viewModel.get();

    for (const auto& entry : eligible)
    {
        if (entry.viewModel == nullptr)
            continue;

        SeriesRenderData data;
        data.entry = entry;

        juce::String incompReason;
        if (entry.id != eligible.front().id && refVm != nullptr)
        {
            data.isCompatible = MeasurementComparisonSession::areMeasurementBasesCompatible(*refVm, *entry.viewModel, incompReason);
            data.incompatibilityReason = incompReason;
        }

        if (data.isCompatible)
        {
            const auto& vm = *entry.viewModel;
            if (mode_ == DynamicsComparisonMode::LevelDbfs)
            {
                if (vm.dynamicsResult.has_value() && !vm.dynamicsResult->amplitudeCurve.x.empty())
                {
                    const auto& crv = vm.dynamicsResult->amplitudeCurve;
                    for (size_t i = 0; i < crv.x.size() && i < crv.y.size(); ++i)
                        data.points.push_back({ crv.x[i], crv.y[i] });
                }
                else
                {
                    for (size_t i = 0; i < vm.curve.x.size() && i < vm.curve.y.size(); ++i)
                        data.points.push_back({ vm.curve.x[i], vm.curve.y[i] });
                }
            }
            else if (mode_ == DynamicsComparisonMode::TimbreCentroidHz)
            {
                if (vm.dynamicsResult.has_value() && !vm.dynamicsResult->brightnessCurve.x.empty())
                {
                    const auto& crv = vm.dynamicsResult->brightnessCurve;
                    for (size_t i = 0; i < crv.x.size() && i < crv.y.size(); ++i)
                        data.points.push_back({ crv.x[i], crv.y[i] });
                }
            }
            else if (mode_ == DynamicsComparisonMode::TimbreRolloffHz)
            {
                if (vm.dynamicsResult.has_value())
                {
                    for (const auto& dp : vm.dynamicsResult->points)
                    {
                        if (dp.spectralRolloffHz > 0.0)
                            data.points.push_back(std::make_pair(static_cast<double>(dp.velocity), dp.spectralRolloffHz));
                    }
                }
            }
        }

        seriesList_.push_back(data);
    }
}

void MeasurementDynamicsComparisonComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121217));

    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    bounds.removeFromTop(32.0f); // Top toolbar space

    auto legendArea = bounds.removeFromRight(220.0f);
    auto plotArea = bounds.reduced(35.0f, 15.0f);

    // Determine Y range
    double minY = (mode_ == DynamicsComparisonMode::LevelDbfs) ? -96.0 : 0.0;
    double maxY = (mode_ == DynamicsComparisonMode::LevelDbfs) ? 0.0 : 16000.0;

    renderGridAndAxes(g, plotArea, minY, maxY);

    for (const auto& s : seriesList_)
    {
        if (s.isCompatible && !s.points.empty())
            renderSeries(g, s, plotArea, minY, maxY);
    }

    renderLegend(g, legendArea);

    if (isHovering_ && hoverTooltipText_.isNotEmpty())
        renderHoverTooltip(g);
}

void MeasurementDynamicsComparisonComponent::renderGridAndAxes(juce::Graphics& g, juce::Rectangle<float> plotArea, double minY, double maxY)
{
    g.setColour(juce::Colour(0xff1f1f2a));
    g.fillRect(plotArea);
    g.setColour(juce::Colour(0xff2d2d3d));
    g.drawRect(plotArea, 1.0f);

    // X Grid: Velocities [0, 32, 64, 96, 127]
    const int velTicks[] = { 0, 32, 64, 96, 127 };
    g.setFont(juce::FontOptions(10.0f));

    for (int v : velTicks)
    {
        const float x = plotArea.getX() + (static_cast<float>(v) / 127.0f) * plotArea.getWidth();
        g.setColour(juce::Colour(0xff252535));
        g.drawVerticalLine(static_cast<int>(x), plotArea.getY(), plotArea.getBottom());

        g.setColour(juce::Colour(0xff8e8ea0));
        g.drawText(juce::String(v), static_cast<int>(x - 15.0f), static_cast<int>(plotArea.getBottom() + 2.0f), 30, 14, juce::Justification::centred);
    }

    // Y Grid
    const int numYDivs = 4;
    for (int i = 0; i <= numYDivs; ++i)
    {
        const float frac = static_cast<float>(i) / static_cast<float>(numYDivs);
        const float y = plotArea.getBottom() - frac * plotArea.getHeight();
        const double val = minY + frac * (maxY - minY);

        g.setColour(juce::Colour(0xff252535));
        g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());

        g.setColour(juce::Colour(0xff8e8ea0));
        juce::String yStr = (mode_ == DynamicsComparisonMode::LevelDbfs)
                            ? juce::String(val, 0) + " dB"
                            : juce::String(val / 1000.0, 1) + " kHz";
        g.drawText(yStr, static_cast<int>(plotArea.getX() - 48.0f), static_cast<int>(y - 7.0f), 44, 14, juce::Justification::centredRight);
    }
}

void MeasurementDynamicsComparisonComponent::renderSeries(juce::Graphics& g, const SeriesRenderData& series, juce::Rectangle<float> plotArea, double minY, double maxY)
{
    if (series.points.empty())
        return;

    juce::Path p;
    std::vector<juce::Point<float>> screenPoints;

    for (size_t i = 0; i < series.points.size(); ++i)
    {
        const double vel = series.points[i].first;
        const double val = series.points[i].second;

        const float x = plotArea.getX() + (static_cast<float>(vel) / 127.0f) * plotArea.getWidth();
        const float normY = static_cast<float>((val - minY) / (maxY - minY));
        const float y = plotArea.getBottom() - juce::jlimit(0.0f, 1.0f, normY) * plotArea.getHeight();

        juce::Point<float> pt(x, y);
        screenPoints.push_back(pt);

        if (i == 0)
            p.startNewSubPath(pt);
        else
            p.lineTo(pt);
    }

    // Accessible line pattern
    g.setColour(series.entry.traceColour);
    if (series.entry.dashPatternIndex == 1) // Dashed
    {
        float dashes[] = { 6.0f, 3.0f };
        juce::Path dashedP;
        juce::PathStrokeType(2.5f).createDashedStroke(dashedP, p, dashes, 2);
        g.fillPath(dashedP);
    }
    else if (series.entry.dashPatternIndex == 2) // Dot-Dash
    {
        float dashes[] = { 8.0f, 3.0f, 2.0f, 3.0f };
        juce::Path dashedP;
        juce::PathStrokeType(2.5f).createDashedStroke(dashedP, p, dashes, 4);
        g.fillPath(dashedP);
    }
    else if (series.entry.dashPatternIndex == 3) // Dotted
    {
        float dashes[] = { 2.0f, 3.0f };
        juce::Path dashedP;
        juce::PathStrokeType(2.5f).createDashedStroke(dashedP, p, dashes, 2);
        g.fillPath(dashedP);
    }
    else
    {
        g.strokePath(p, juce::PathStrokeType(2.5f));
    }

    // Accessible geometric markers at points
    for (const auto& pt : screenPoints)
    {
        // Dark outline for contrast >= 4.5:1
        g.setColour(juce::Colour(0xff0c0c10));
        g.fillEllipse(pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);

        g.setColour(series.entry.traceColour);
        if (series.entry.markerShapeIndex == 1) // Square
            g.fillRect(pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
        else if (series.entry.markerShapeIndex == 2) // Triangle
        {
            juce::Path tri;
            tri.addTriangle(pt.x, pt.y - 4.5f, pt.x - 4.0f, pt.y + 4.0f, pt.x + 4.0f, pt.y + 4.0f);
            g.fillPath(tri);
        }
        else if (series.entry.markerShapeIndex == 3) // Diamond
        {
            juce::Path dia;
            dia.startNewSubPath(pt.x, pt.y - 4.5f);
            dia.lineTo(pt.x + 4.0f, pt.y);
            dia.lineTo(pt.x, pt.y + 4.5f);
            dia.lineTo(pt.x - 4.0f, pt.y);
            dia.closeSubPath();
            g.fillPath(dia);
        }
        else // Circle
            g.fillEllipse(pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
    }
}

void MeasurementDynamicsComparisonComponent::renderLegend(juce::Graphics& g, juce::Rectangle<float> legendArea)
{
    g.setColour(juce::Colour(0xff181820));
    g.fillRoundedRectangle(legendArea, 4.0f);
    g.setColour(juce::Colour(0xff2d2d3d));
    g.drawRoundedRectangle(legendArea, 4.0f, 1.0f);

    auto area = legendArea.reduced(8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("Series Comparadas", area.removeFromTop(18.0f), juce::Justification::centredLeft);

    for (const auto& s : seriesList_)
    {
        auto itemArea = area.removeFromTop(24.0f);
        if (itemArea.getY() > legendArea.getBottom() - 10.0f)
            break;

        // Swatch
        const float cy = itemArea.getCentreY();
        g.setColour(s.entry.traceColour);
        g.drawLine(itemArea.getX(), cy, itemArea.getX() + 20.0f, cy, 2.5f);

        // Text
        juce::String name = (s.entry.viewModel != nullptr && s.entry.viewModel->dutName.isNotEmpty())
                            ? s.entry.viewModel->dutName
                            : s.entry.containerDir.getFileName();

        if (!s.isCompatible)
        {
            g.setColour(juce::Colour(0xffff5252));
            name += " (Excluido)";
        }
        else
        {
            g.setColour(juce::Colour(0xffe0e0e0));
        }

        g.setFont(juce::FontOptions(11.0f));
        g.drawText(name, static_cast<int>(itemArea.getX() + 26.0f), static_cast<int>(itemArea.getY()), static_cast<int>(itemArea.getWidth() - 26.0f), static_cast<int>(itemArea.getHeight()), juce::Justification::centredLeft, true);
    }
}

void MeasurementDynamicsComparisonComponent::renderHoverTooltip(juce::Graphics& g)
{
    g.setFont(juce::FontOptions(11.0f));
    const int w = juce::Font(juce::FontOptions(11.0f)).getStringWidth(hoverTooltipText_) + 16;
    const int h = 22;

    float tx = hoverPos_.x + 12.0f;
    float ty = hoverPos_.y - 25.0f;

    if (tx + w > getWidth() - 10.0f)
        tx = hoverPos_.x - w - 12.0f;
    if (ty < 5.0f)
        ty = hoverPos_.y + 15.0f;

    g.setColour(juce::Colour(0xee141418));
    g.fillRoundedRectangle(tx, ty, static_cast<float>(w), static_cast<float>(h), 3.0f);
    g.setColour(juce::Colour(0xff00d4ff));
    g.drawRoundedRectangle(tx, ty, static_cast<float>(w), static_cast<float>(h), 3.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.drawText(hoverTooltipText_, static_cast<int>(tx), static_cast<int>(ty), w, h, juce::Justification::centred);
}

void MeasurementDynamicsComparisonComponent::mouseMove(const juce::MouseEvent& e)
{
    hoverPos_ = e.position;
    isHovering_ = false;
    hoverTooltipText_.clear();

    auto plotArea = getLocalBounds().toFloat().reduced(6.0f);
    plotArea.removeFromTop(32.0f);
    plotArea.removeFromRight(220.0f);
    plotArea = plotArea.reduced(35.0f, 15.0f);

    if (plotArea.contains(hoverPos_))
    {
        double minY = (mode_ == DynamicsComparisonMode::LevelDbfs) ? -96.0 : 0.0;
        double maxY = (mode_ == DynamicsComparisonMode::LevelDbfs) ? 0.0 : 16000.0;

        for (const auto& s : seriesList_)
        {
            if (!s.isCompatible)
                continue;

            for (const auto& pt : s.points)
            {
                const float x = plotArea.getX() + (static_cast<float>(pt.first) / 127.0f) * plotArea.getWidth();
                const float normY = static_cast<float>((pt.second - minY) / (maxY - minY));
                const float y = plotArea.getBottom() - juce::jlimit(0.0f, 1.0f, normY) * plotArea.getHeight();

                if (hoverPos_.getDistanceFrom({ x, y }) < 8.0f)
                {
                    isHovering_ = true;
                    juce::String name = (s.entry.viewModel != nullptr && s.entry.viewModel->dutName.isNotEmpty())
                                        ? s.entry.viewModel->dutName : "DUT";
                    juce::String unit = (mode_ == DynamicsComparisonMode::LevelDbfs) ? " dBFS" : " Hz";
                    hoverTooltipText_ = name + " | Vel " + juce::String(static_cast<int>(pt.first)) + " -> " + juce::String(pt.second, 2) + unit;
                    repaint();
                    return;
                }
            }
        }
    }

    repaint();
}

void MeasurementDynamicsComparisonComponent::resized()
{
    auto area = getLocalBounds().removeFromTop(28).reduced(6, 2);
    const int btnW = 120;
    btnModeLevel_.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(4);
    btnModeCentroid_.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(4);
    btnModeRolloff_.setBounds(area.removeFromLeft(btnW));
}

} // namespace abdaudiolab::gui::measurement
