/**
 * @file MeasurementTemporalCurveComponent.cpp
 * @brief Implementation of MeasurementTemporalCurveComponent.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementTemporalCurveComponent.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::measurement
{

MeasurementTemporalCurveComponent::MeasurementTemporalCurveComponent()
{
    setRepaintsOnMouseActivity(true);
}

void MeasurementTemporalCurveComponent::setCurve(const abdaudiolab::measurement::MeasurementCurve& curve,
                                                 bool isIntegrityVerified)
{
    curve_ = curve;
    isIntegrityVerified_ = isIntegrityVerified;
    recalculateBounds();
    repaint();
}

void MeasurementTemporalCurveComponent::clear()
{
    curve_ = abdaudiolab::measurement::MeasurementCurve();
    isIntegrityVerified_ = true;
    recalculateBounds();
    repaint();
}

void MeasurementTemporalCurveComponent::resized()
{
    recalculateBounds();
}

void MeasurementTemporalCurveComponent::recalculateBounds()
{
    auto b = getLocalBounds().toFloat();
    const float padLeft = 55.0f;
    const float padRight = 20.0f;
    const float padTop = 25.0f;
    const float padBottom = 30.0f;

    plotBounds_ = juce::Rectangle<float>(padLeft, padTop,
                                         std::max(10.0f, b.getWidth() - padLeft - padRight),
                                         std::max(10.0f, b.getHeight() - padTop - padBottom));

    if (!curve_.x.empty())
    {
        minTimeMs_ = curve_.x.front();
        maxTimeMs_ = curve_.x.back();
        if (std::abs(maxTimeMs_ - minTimeMs_) < 1e-4)
            maxTimeMs_ = minTimeMs_ + 100.0;
    }
    else
    {
        minTimeMs_ = 0.0;
        maxTimeMs_ = 100.0;
    }
}

void MeasurementTemporalCurveComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Dark container background
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // 2. Empty state
    if (curve_.x.empty())
    {
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::Font(13.0f));
        g.drawText("No temporal envelope curve data available", bounds, juce::Justification::centred, false);
        return;
    }

    // 3. Horizontal dBFS grid lines & labels
    const double dbSteps[] = { 0.0, -20.0, -40.0, -60.0, -80.0, -96.0 };
    g.setFont(juce::Font(10.0f));

    for (double db : dbSteps)
    {
        float normY = static_cast<float>((maxDb_ - db) / (maxDb_ - minDb_));
        float y = plotBounds_.getY() + normY * plotBounds_.getHeight();

        g.setColour(juce::Colour(0xff1e293b));
        g.drawLine(plotBounds_.getX(), y, plotBounds_.getRight(), y, 1.0f);

        g.setColour(juce::Colour(0xff64748b));
        juce::String label = juce::String(static_cast<int>(db)) + " dB";
        g.drawText(label, 0, static_cast<int>(y - 7.0f), static_cast<int>(plotBounds_.getX() - 6.0f), 14,
                   juce::Justification::centredRight, false);
    }

    // 4. Vertical time grid lines & labels
    const int numTimeSteps = 5;
    for (int i = 0; i <= numTimeSteps; ++i)
    {
        float factor = static_cast<float>(i) / static_cast<float>(numTimeSteps);
        float x = plotBounds_.getX() + factor * plotBounds_.getWidth();
        double t = minTimeMs_ + factor * (maxTimeMs_ - minTimeMs_);

        g.setColour(juce::Colour(0xff1e293b));
        g.drawDashedLine(juce::Line<float>(x, plotBounds_.getY(), x, plotBounds_.getBottom()),
                         new float[2]{ 3.0f, 3.0f }, 2, 1.0f);

        g.setColour(juce::Colour(0xff64748b));
        juce::String tLabel = juce::String(static_cast<int>(std::round(t))) + " ms";
        g.drawText(tLabel, static_cast<int>(x - 30.0f), static_cast<int>(plotBounds_.getBottom() + 4.0f),
                   60, 18, juce::Justification::centred, false);
    }

    // 5. Construct curve path & fill area
    juce::Path curvePath;
    juce::Path areaPath;

    for (size_t i = 0; i < curve_.x.size(); ++i)
    {
        double t = std::clamp(curve_.x[i], minTimeMs_, maxTimeMs_);
        double db = std::clamp(curve_.y[i], minDb_, maxDb_);

        float px = plotBounds_.getX() + static_cast<float>((t - minTimeMs_) / (maxTimeMs_ - minTimeMs_)) * plotBounds_.getWidth();
        float py = plotBounds_.getY() + static_cast<float>((maxDb_ - db) / (maxDb_ - minDb_)) * plotBounds_.getHeight();

        if (i == 0)
        {
            curvePath.startNewSubPath(px, py);
            areaPath.startNewSubPath(px, plotBounds_.getBottom());
            areaPath.lineTo(px, py);
        }
        else
        {
            curvePath.lineTo(px, py);
            areaPath.lineTo(px, py);
        }
    }

    areaPath.lineTo(plotBounds_.getRight(), plotBounds_.getBottom());
    areaPath.closeSubPath();

    // Area gradient
    juce::ColourGradient grad(juce::Colour(0x500284c7), plotBounds_.getX(), plotBounds_.getY(),
                              juce::Colour(0x020284c7), plotBounds_.getX(), plotBounds_.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(areaPath);

    // Stroke line
    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 6. Unverified / Corrupt watermark warning
    if (!isIntegrityVerified_)
    {
        juce::Rectangle<int> warnRect(static_cast<int>(plotBounds_.getRight() - 170.0f),
                                     static_cast<int>(plotBounds_.getY() + 8.0f), 160, 24);
        g.setColour(juce::Colour(0xff7f1d1d));
        g.fillRoundedRectangle(warnRect.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xfff87171));
        g.drawRoundedRectangle(warnRect.toFloat(), 4.0f, 1.0f);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("[X] UNVERIFIED / CORRUPT", warnRect, juce::Justification::centred, false);
    }

    // 7. Interactive crosshair tooltip
    if (isHovering_ && plotBounds_.contains(mousePos_))
    {
        g.setColour(juce::Colour(0x6038bdf8));
        g.drawDashedLine(juce::Line<float>(plotBounds_.getX(), mousePos_.getY(), plotBounds_.getRight(), mousePos_.getY()),
                         new float[2]{ 2.0f, 2.0f }, 2, 1.0f);
        g.drawDashedLine(juce::Line<float>(mousePos_.getX(), plotBounds_.getY(), mousePos_.getX(), plotBounds_.getBottom()),
                         new float[2]{ 2.0f, 2.0f }, 2, 1.0f);

        // Compute current hover values
        double hoverT = minTimeMs_ + (mousePos_.getX() - plotBounds_.getX()) / plotBounds_.getWidth() * (maxTimeMs_ - minTimeMs_);
        double hoverDb = maxDb_ - (mousePos_.getY() - plotBounds_.getY()) / plotBounds_.getHeight() * (maxDb_ - minDb_);

        juce::String tip = juce::String(static_cast<int>(std::round(hoverT))) + " ms | " +
                           juce::String(hoverDb, 1) + " dBFS";

        int tipW = 120;
        int tipH = 22;
        int tipX = std::clamp(static_cast<int>(mousePos_.getX() + 10.0f),
                              static_cast<int>(plotBounds_.getX()),
                              static_cast<int>(plotBounds_.getRight() - tipW));
        int tipY = std::clamp(static_cast<int>(mousePos_.getY() - tipH - 6.0f),
                              static_cast<int>(plotBounds_.getY()),
                              static_cast<int>(plotBounds_.getBottom() - tipH));

        juce::Rectangle<int> tipBox(tipX, tipY, tipW, tipH);
        g.setColour(juce::Colour(0xf01e293b));
        g.fillRoundedRectangle(tipBox.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawRoundedRectangle(tipBox.toFloat(), 4.0f, 1.0f);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xfff1f5f9));
        g.drawText(tip, tipBox, juce::Justification::centred, false);
    }
}

void MeasurementTemporalCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    mousePos_ = e.position;
    isHovering_ = plotBounds_.contains(mousePos_);
    repaint();
}

void MeasurementTemporalCurveComponent::mouseExit(const juce::MouseEvent& /*e*/)
{
    isHovering_ = false;
    repaint();
}

} // namespace abdaudiolab::gui::measurement
