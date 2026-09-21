/**
 * @file MeasurementFrequencyCurveComponent.cpp
 * @brief Implementation of MeasurementFrequencyCurveComponent.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementFrequencyCurveComponent.h"
#include "../AppTheme.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::measurement
{

MeasurementFrequencyCurveComponent::MeasurementFrequencyCurveComponent()
{
    setRepaintsOnMouseActivity(true);
}

void MeasurementFrequencyCurveComponent::updateTheme()
{
    repaint();
}

void MeasurementFrequencyCurveComponent::setCurve(const abdaudiolab::measurement::MeasurementCurve& curve,
                                                  const std::optional<abdaudiolab::measurement::SlopeFitMetadata>& slopeFit,
                                                  double cutoffHz,
                                                  bool isCutoffObservable,
                                                  bool isIntegrityVerified)
{
    curve_ = curve;
    slopeFit_ = slopeFit;
    cutoffHz_ = cutoffHz;
    isCutoffObservable_ = isCutoffObservable;
    isIntegrityVerified_ = isIntegrityVerified;
    repaint();
}

void MeasurementFrequencyCurveComponent::clear()
{
    curve_ = abdaudiolab::measurement::MeasurementCurve();
    slopeFit_ = std::nullopt;
    cutoffHz_ = -1.0;
    isCutoffObservable_ = false;
    isIntegrityVerified_ = true;
    repaint();
}

void MeasurementFrequencyCurveComponent::resized()
{
    auto b = getLocalBounds().toFloat().reduced(12.0f);
    plotBounds_ = b.withTrimmedLeft(45.0f).withTrimmedBottom(25.0f);
}

float MeasurementFrequencyCurveComponent::freqToX(double freqHz) const noexcept
{
    double clampedFreq = std::clamp(freqHz, minFreq_, maxFreq_);
    double logMinF = std::log10(minFreq_);
    double logMaxF = std::log10(maxFreq_);
    double norm = (std::log10(clampedFreq) - logMinF) / (logMaxF - logMinF);
    return plotBounds_.getX() + static_cast<float>(norm) * plotBounds_.getWidth();
}

float MeasurementFrequencyCurveComponent::dbToY(double dbVal) const noexcept
{
    double clampedDb = std::clamp(dbVal, minDb_, maxDb_);
    double norm = (maxDb_ - clampedDb) / (maxDb_ - minDb_);
    return plotBounds_.getY() + static_cast<float>(norm) * plotBounds_.getHeight();
}

double MeasurementFrequencyCurveComponent::xToFreq(float x) const noexcept
{
    float norm = std::clamp((x - plotBounds_.getX()) / plotBounds_.getWidth(), 0.0f, 1.0f);
    double logMinF = std::log10(minFreq_);
    double logMaxF = std::log10(maxFreq_);
    return std::pow(10.0, logMinF + norm * (logMaxF - logMinF));
}

double MeasurementFrequencyCurveComponent::yToDb(float y) const noexcept
{
    float norm = std::clamp((y - plotBounds_.getY()) / plotBounds_.getHeight(), 0.0f, 1.0f);
    return maxDb_ - norm * (maxDb_ - minDb_);
}

void MeasurementFrequencyCurveComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const bool isDark = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);

    // 1. Container background & border
    g.setColour(AppTheme::SurfaceCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(AppTheme::BorderCard);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // 2. Empty state
    if (curve_.x.empty())
    {
        g.setColour(AppTheme::TextMuted);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("No frequency curve data available", bounds, juce::Justification::centred, false);
        return;
    }

    // 3. Horizontal dB grid lines & labels
    const double dbSteps[] = { 12.0, 6.0, 0.0, -6.0, -12.0, -24.0, -36.0, -48.0, -60.0, -72.0 };
    g.setFont(juce::Font(juce::FontOptions(10.0f)));

    for (double db : dbSteps)
    {
        float y = dbToY(db);

        bool isZero = (std::abs(db) < 1e-4);
        g.setColour(isZero ? (isDark ? juce::Colour(0xff475569) : juce::Colour(0xff94a3b8)) : AppTheme::BorderSubtle);
        g.drawLine(plotBounds_.getX(), y, plotBounds_.getRight(), y, isZero ? 1.5f : 1.0f);

        g.setColour(AppTheme::TextSecondary);
        std::string dbStr = std::to_string(static_cast<int>(db)) + " dB";
        g.drawText(dbStr, juce::Rectangle<float>(4.0f, y - 8.0f, 46.0f, 16.0f), juce::Justification::centredRight, false);
    }

    // 4. Vertical Log-Frequency grid lines & labels
    const double fSteps[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    const float freqDashes[2] = { 2.0f, 2.0f };
    for (double f : fSteps)
    {
        float x = freqToX(f);

        g.setColour(AppTheme::BorderSubtle);
        g.drawDashedLine(juce::Line<float>(x, plotBounds_.getY(), x, plotBounds_.getBottom()),
                         freqDashes, 2, 1.0f);

        std::string fStr = (f >= 1000.0) ? (std::to_string(static_cast<int>(f / 1000.0)) + "k") : std::to_string(static_cast<int>(f));
        g.setColour(AppTheme::TextSecondary);
        g.drawText(fStr, juce::Rectangle<float>(x - 20.0f, plotBounds_.getBottom() + 4.0f, 40.0f, 16.0f), juce::Justification::centred, false);
    }

    // 5. Build vector path
    juce::Path curvePath;
    juce::Path areaPath;

    bool first = true;
    float baselineY = dbToY(minDb_);

    for (size_t i = 0; i < curve_.x.size(); ++i)
    {
        double f = curve_.x[i];
        if (f < minFreq_ || f > maxFreq_)
            continue;

        float x = freqToX(f);
        float y = dbToY(curve_.y[i]);

        if (first)
        {
            curvePath.startNewSubPath(x, y);
            areaPath.startNewSubPath(x, baselineY);
            areaPath.lineTo(x, y);
            first = false;
        }
        else
        {
            curvePath.lineTo(x, y);
            areaPath.lineTo(x, y);
        }
    }

    const juce::Colour baseCurveCol = isDark ? juce::Colour(0xff38bdf8) : juce::Colour(0xff0284c7);

    if (!first)
    {
        areaPath.lineTo(plotBounds_.getRight(), baselineY);
        areaPath.closeSubPath();

        // Area Gradient
        juce::ColourGradient grad(baseCurveCol.withAlpha(isDark ? 0.35f : 0.20f), plotBounds_.getX(), plotBounds_.getY(),
                                  baseCurveCol.withAlpha(0.01f), plotBounds_.getX(), plotBounds_.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(areaPath);

        // Curve stroke
        g.setColour(baseCurveCol);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 6. Cutoff frequency marker (ONLY if observable)
    if (isCutoffObservable_ && cutoffHz_ >= minFreq_ && cutoffHz_ <= maxFreq_)
    {
        float cx = freqToX(cutoffHz_);
        g.setColour(AppTheme::AccentWarning);
        const float dashLengths[] = { 4.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(cx, plotBounds_.getY(), cx, plotBounds_.getBottom()),
                         dashLengths, 2, 1.5f);

        // Label
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        std::ostringstream fcSs;
        fcSs << "fc: " << static_cast<int>(std::round(cutoffHz_)) << " Hz";
        g.drawText(fcSs.str(), juce::Rectangle<float>(cx + 4.0f, plotBounds_.getY() + 4.0f, 90.0f, 16.0f), juce::Justification::left, false);
    }

    // 7. Slope fit regression line (if present)
    if (slopeFit_.has_value() && slopeFit_->sampleCount >= 2 && slopeFit_->frequencyStartHz < slopeFit_->frequencyEndHz)
    {
        float sx1 = freqToX(slopeFit_->frequencyStartHz);
        float sx2 = freqToX(slopeFit_->frequencyEndHz);

        const juce::Colour purpleCol = isDark ? juce::Colour(0xffc084fc) : juce::Colour(0xff7e22ce);
        g.setColour(purpleCol);
        g.drawLine(sx1, plotBounds_.getBottom() - 3.0f, sx2, plotBounds_.getBottom() - 3.0f, 3.0f);

        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        std::ostringstream sfSs;
        sfSs << "Fit Region (" << static_cast<int>(slopeFit_->frequencyStartHz) << "-"
             << static_cast<int>(slopeFit_->frequencyEndHz) << " Hz, R²="
             << std::fixed << std::setprecision(3) << slopeFit_->rSquared << ")";
        g.drawText(juce::String::fromUTF8(sfSs.str().c_str()), juce::Rectangle<float>(sx1, plotBounds_.getBottom() - 20.0f, (sx2 - sx1), 16.0f), juce::Justification::centred, false);
    }

    // 8. Interactive crosshair & tooltip
    if (isHovering_ && plotBounds_.contains(mousePos_))
    {
        // Hairlines
        g.setColour(AppTheme::BorderCard);
        g.drawLine(mousePos_.getX(), plotBounds_.getY(), mousePos_.getX(), plotBounds_.getBottom(), 1.0f);
        g.drawLine(plotBounds_.getX(), mousePos_.getY(), plotBounds_.getRight(), mousePos_.getY(), 1.0f);

        // Tooltip badge
        double freqUnderCursor = xToFreq(mousePos_.getX());
        double dbUnderCursor = yToDb(mousePos_.getY());

        std::ostringstream tipSs;
        tipSs << static_cast<int>(std::round(freqUnderCursor)) << " Hz | "
              << std::fixed << std::setprecision(1) << dbUnderCursor << " dB";

        auto tipText = tipSs.str();
        g.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(g.getCurrentFont(), tipText, 0.0f, 0.0f);
        float textW = glyphs.getBoundingBox(0, -1, true).getWidth() + 16.0f;
        float tipX = std::clamp(mousePos_.getX() + 10.0f, plotBounds_.getX(), plotBounds_.getRight() - textW);
        float tipY = std::clamp(mousePos_.getY() - 26.0f, plotBounds_.getY(), plotBounds_.getBottom() - 22.0f);

        juce::Rectangle<float> tipRect(tipX, tipY, textW, 20.0f);
        g.setColour(AppTheme::PillBlackBg);
        g.fillRoundedRectangle(tipRect, 4.0f);
        g.setColour(AppTheme::BorderSubtle);
        g.drawRoundedRectangle(tipRect, 4.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.drawText(tipText, tipRect, juce::Justification::centred, false);
    }

    // 9. Integrity Warning Overlay if corrupted/unverified
    if (!isIntegrityVerified_)
    {
        g.setColour(juce::Colour(0xaa7f1d1d));
        g.fillRoundedRectangle(plotBounds_, 4.0f);

        g.setColour(juce::Colour(0xfff87171));
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.drawText("[X] UNVERIFIED / CORRUPT CONTAINER - CRYPTOGRAPHIC MISMATCH",
                   plotBounds_, juce::Justification::centred, false);
    }
}

void MeasurementFrequencyCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    mousePos_ = e.position;
    isHovering_ = plotBounds_.contains(mousePos_);
    repaint();
}

void MeasurementFrequencyCurveComponent::mouseExit(const juce::MouseEvent&)
{
    isHovering_ = false;
    repaint();
}

} // namespace abdaudiolab::gui::measurement
