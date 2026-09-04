#include "SoundIdMeterStrip.h"
#include <cmath>

namespace abdaudiolab::gui
{

SoundIdMeterStrip::SoundIdMeterStrip()
{
    trimSlider.setSliderStyle(juce::Slider::LinearVertical);
    trimSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    trimSlider.setRange(-24.0, 12.0, 0.1);
    trimSlider.setValue(0.0);
    trimSlider.setTooltip("Input gain trim fader (dB) - Auto-trim stages signal to -3.0 dBfs reference headroom");
    trimSlider.onValueChange = [this] {
        float db = static_cast<float>(trimSlider.getValue());
        trimGainLinear = std::pow(10.0f, db / 20.0f);
        if (onTrimChanged) onTrimChanged(trimGainLinear);
    };
    addAndMakeVisible(trimSlider);

    juce::Path powerIcon;
    powerIcon.addEllipse(2.0f, 2.0f, 20.0f, 20.0f);
    masterButton.setShape(powerIcon, true, true, false);
    masterButton.onClick = [this] {
        if (onProfilingToggled) onProfilingToggled(!isProfilingActive);
        if (onMasterToggleClicked) onMasterToggleClicked();
    };
    addAndMakeVisible(masterButton);

    startTimerHz(60);
}

SoundIdMeterStrip::~SoundIdMeterStrip()
{
    stopTimer();
}

void SoundIdMeterStrip::setLevels(float inPeakL, float inPeakR, float inRms,
                                  float outPeakL, float outPeakR, float outRms)
{
    currentInPeak = std::max(inPeakL, inPeakR);
    currentInRms = inRms;
    currentOutPeak = std::max(outPeakL, outPeakR);
    currentOutRms = outRms;
}

void SoundIdMeterStrip::setProfilingActive(bool active)
{
    isProfilingActive = active;
    repaint();
}

float SoundIdMeterStrip::amplitudeToNorm(float linearAmp) noexcept
{
    if (linearAmp <= 1e-4f)
        return 0.0f;
    float db = 20.0f * std::log10(linearAmp);
    // Map -60.0 dBfs -> 0.0, 0.0 dBfs -> 1.0 (clamped)
    return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

void SoundIdMeterStrip::timerCallback()
{
    // 60 Hz ballistic update: instantaneous attack, smooth exponential decay (~20 dB/s)
    displayInRms = std::max(currentInRms, displayInRms * 0.92f);
    displayInPeak = std::max(currentInPeak, displayInPeak * 0.96f);

    displayOutRms = std::max(currentOutRms, displayOutRms * 0.92f);
    displayOutPeak = std::max(currentOutPeak, displayOutPeak * 0.96f);

    // Peak-Hold Ballistics (In): 1.0s hold = 60 frames at 60Hz
    if (currentInPeak >= peakHoldIn)
    {
        peakHoldIn = currentInPeak;
        peakHoldInTimer = 60;
    }
    else
    {
        if (peakHoldInTimer > 0)
            --peakHoldInTimer;
        else
            peakHoldIn = peakHoldIn * 0.97f; // ~20 dB/s smooth decay
    }

    // Peak-Hold Ballistics (Out): 1.0s hold = 60 frames at 60Hz
    if (currentOutPeak >= peakHoldOut)
    {
        peakHoldOut = currentOutPeak;
        peakHoldOutTimer = 60;
    }
    else
    {
        if (peakHoldOutTimer > 0)
            --peakHoldOutTimer;
        else
            peakHoldOut = peakHoldOut * 0.97f;
    }

    // Reset current transient values for next measurement window
    currentInPeak = 0.0f;
    currentOutPeak = 0.0f;

    repaint();
}

void SoundIdMeterStrip::resized()
{
    auto bounds = getLocalBounds();
    auto botArea = bounds.removeFromBottom(60);
    masterButton.setBounds(botArea.removeFromTop(32).withSizeKeepingCentre(32, 32));

    auto meterArea = bounds.withTrimmedTop(32).reduced(4, 0);
    trimSlider.setBounds(meterArea.removeFromRight(20));
}

void SoundIdMeterStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // White Card Background with rounded corners & border
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Top Header: "In"  "Out"  "-3.0"
    auto topArea = bounds.removeFromTop(28.0f).reduced(6.0f, 0.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("In", topArea.removeFromLeft(24.0f), juce::Justification::centred, true);
    topArea.removeFromLeft(4.0f);
    g.drawText("Out", topArea.removeFromLeft(24.0f), juce::Justification::centred, true);

    float peakDb = displayInPeak > 1e-4f ? 20.0f * std::log10(displayInPeak) : -96.0f;
    juce::String peakStr = juce::String(peakDb, 1);
    g.setFont(juce::FontOptions("Consolas", 10.5f, juce::Font::plain));
    g.setColour(peakDb >= -0.5f ? SoundIdTheme::accentRed : (peakDb > -3.0f ? SoundIdTheme::accentAmber : SoundIdTheme::textPrimary));
    g.drawText(peakStr, topArea, juce::Justification::centredRight, true);

    auto botArea = bounds.removeFromBottom(60.0f);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(isProfilingActive ? SoundIdTheme::accentGreen : SoundIdTheme::textSecondary);
    g.drawText(isProfilingActive ? "Profiling\nActive" : "Profiling\nStopped", botArea.removeFromBottom(24.0f), juce::Justification::centred, true);

    // Draw Meter Bars
    auto meterArea = bounds.reduced(6.0f, 6.0f);
    float barWidth = 7.0f;
    float barHeight = meterArea.getHeight();

    auto barInRect = juce::Rectangle<float>(meterArea.getX() + 8.0f, meterArea.getY(), barWidth, barHeight);
    auto barOutRect = juce::Rectangle<float>(meterArea.getX() + 28.0f, meterArea.getY(), barWidth, barHeight);

    float normInPeak = amplitudeToNorm(displayInPeak);
    float normInRms = amplitudeToNorm(displayInRms);
    float normInHold = amplitudeToNorm(peakHoldIn);

    float normOutPeak = amplitudeToNorm(displayOutPeak);
    float normOutRms = amplitudeToNorm(displayOutRms);
    float normOutHold = amplitudeToNorm(peakHoldOut);

    drawMeterBar(g, barInRect, normInPeak, normInRms, normInHold);
    drawMeterBar(g, barOutRect, normOutPeak, normOutRms, normOutHold);

    // Safe Headroom -3 dBfs tick mark line ((-3 + 60) / 60 = 0.95 norm)
    float markY = meterArea.getBottom() - (0.95f * barHeight);
    g.setColour(SoundIdTheme::accentAmber.withAlpha(0.7f));
    g.drawHorizontalLine(static_cast<int>(markY), meterArea.getX() + 4.0f, meterArea.getX() + 38.0f);
}

void SoundIdMeterStrip::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> barRect, float /*peakNorm*/, float rmsNorm, float peakHoldNorm)
{
    // Background groove
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(barRect, 3.0f);

    float rmsHeight = std::clamp(rmsNorm, 0.0f, 1.0f) * barRect.getHeight();

    // Multi-colour gradient fill: Green <= -12dBFS (0.80), Amber <= -3dBFS (0.95), Red <= 0dBFS (1.00)
    if (rmsHeight > 1.0f)
    {
        auto fillRect = barRect.withTop(barRect.getBottom() - rmsHeight);
        juce::ColourGradient grad(SoundIdTheme::accentGreen, barRect.getX(), barRect.getBottom(),
                                  SoundIdTheme::accentRed, barRect.getX(), barRect.getY(), false);
        grad.addColour(0.78f, SoundIdTheme::accentGreen);
        grad.addColour(0.92f, SoundIdTheme::accentAmber);
        grad.addColour(0.98f, SoundIdTheme::accentRed);

        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillRect, 3.0f);
    }

    // Floating Peak-Hold line (2px thickness)
    if (peakHoldNorm > 0.03f)
    {
        float peakY = barRect.getBottom() - std::clamp(peakHoldNorm, 0.0f, 1.0f) * barRect.getHeight();
        juce::Colour peakCol = peakHoldNorm >= 0.95f ? SoundIdTheme::accentRed
                             : (peakHoldNorm >= 0.80f ? SoundIdTheme::accentAmber : juce::Colours::white);

        // 2px floating peak bar with subtle shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(barRect.getX() - 1.0f, peakY, barRect.getWidth() + 2.0f, 2.0f);

        g.setColour(peakCol);
        g.fillRect(barRect.getX(), peakY - 1.0f, barRect.getWidth(), 2.0f);
    }
}

} // namespace abdaudiolab::gui
