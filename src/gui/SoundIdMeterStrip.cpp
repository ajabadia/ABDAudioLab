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

    startTimerHz(30);
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

void SoundIdMeterStrip::timerCallback()
{
    displayInPeak = std::max(currentInPeak, displayInPeak * 0.92f);
    displayInRms = std::max(currentInRms, displayInRms * 0.88f);
    displayOutPeak = std::max(currentOutPeak, displayOutPeak * 0.92f);
    displayOutRms = std::max(currentOutRms, displayOutRms * 0.88f);
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
    auto topArea = bounds.removeFromTop(28.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("In", topArea.removeFromLeft(30.0f), juce::Justification::centred, true);
    g.drawText("Out", topArea.removeFromLeft(30.0f), juce::Justification::centred, true);

    float peakDb = displayInPeak > 1e-4f ? 20.0f * std::log10(displayInPeak) : -96.0f;
    juce::String peakStr = juce::String(peakDb, 1);
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(peakDb > -3.0f ? SoundIdTheme::accentAmber : SoundIdTheme::textPrimary);
    g.drawText(peakStr, topArea, juce::Justification::centredRight, true);

    auto botArea = bounds.removeFromBottom(60.0f);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(isProfilingActive ? SoundIdTheme::accentGreen : SoundIdTheme::textSecondary);
    g.drawText(isProfilingActive ? "Profiling\nActive" : "Profiling\nStopped", botArea.removeFromBottom(24.0f), juce::Justification::centred, true);

    // Draw Meter Bars
    auto meterArea = bounds.reduced(6.0f, 6.0f);
    float barWidth = 6.0f;
    float barHeight = meterArea.getHeight();

    auto barInRect = juce::Rectangle<float>(meterArea.getX() + 10.0f, meterArea.getY(), barWidth, barHeight);
    auto barOutRect = juce::Rectangle<float>(meterArea.getX() + 32.0f, meterArea.getY(), barWidth, barHeight);

    drawMeterBar(g, barInRect, displayInPeak, displayInRms);
    drawMeterBar(g, barOutRect, displayOutPeak, displayOutRms);

    // Safe Headroom -3 dBfs tick mark line (0.707 linear)
    float markY = meterArea.getBottom() - (0.707f * barHeight);
    g.setColour(SoundIdTheme::accentRed.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(markY), meterArea.getX() + 6.0f, meterArea.getX() + 42.0f);
}

void SoundIdMeterStrip::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> barRect, float peak, float rms)
{
    // Background track
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(barRect, 3.0f);

    float rmsHeight = std::clamp(rms, 0.0f, 1.0f) * barRect.getHeight();
    float peakY = barRect.getBottom() - std::clamp(peak, 0.0f, 1.0f) * barRect.getHeight();

    // RMS Solid Green Fill
    auto fillRect = barRect.withTop(barRect.getBottom() - rmsHeight);
    g.setColour(SoundIdTheme::accentGreen);
    g.fillRoundedRectangle(fillRect, 3.0f);

    // Peak Indicator Line
    if (peak > 0.01f)
    {
        g.setColour(peak >= 0.99f ? SoundIdTheme::accentRed : juce::Colour(0xff059669));
        g.drawHorizontalLine(static_cast<int>(peakY), barRect.getX(), barRect.getRight());
    }
}

} // namespace abdaudiolab::gui
