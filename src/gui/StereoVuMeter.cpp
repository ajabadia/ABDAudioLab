#include "StereoVuMeter.h"
#include <cmath>

namespace abdaudiolab::gui
{

StereoVuMeter::StereoVuMeter(const juce::String& labelTitle)
    : title(labelTitle)
{
    startTimerHz(30); // 30 FPS meter refresh
}

StereoVuMeter::~StereoVuMeter()
{
    stopTimer();
}

void StereoVuMeter::setLevels(float peakL, float peakR, float rmsL, float rmsR)
{
    currentPeakL = peakL;
    currentPeakR = peakR;
    currentRmsL = rmsL;
    currentRmsR = rmsR;

    if (peakL >= 0.999f)
    {
        clipL = true;
        clipHoldCounterL = 30; // hold clip LED for 1 second
    }
    if (peakR >= 0.999f)
    {
        clipR = true;
        clipHoldCounterR = 30;
    }
}

void StereoVuMeter::timerCallback()
{
    // Smooth decay
    displayPeakL = std::max(currentPeakL, displayPeakL * 0.92f);
    displayPeakR = std::max(currentPeakR, displayPeakR * 0.92f);
    displayRmsL = std::max(currentRmsL, displayRmsL * 0.88f);
    displayRmsR = std::max(currentRmsR, displayRmsR * 0.88f);

    if (clipHoldCounterL > 0)
    {
        if (--clipHoldCounterL == 0)
            clipL = false;
    }
    if (clipHoldCounterR > 0)
    {
        if (--clipHoldCounterR == 0)
            clipR = false;
    }

    repaint();
}

void StereoVuMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background panel
    g.setColour(juce::Colour(0xff16171a));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a2d34));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    auto content = bounds.reduced(4.0f);

    // Title label
    g.setColour(juce::Colour(0xffa0a0a0));
    g.setFont(10.0f);
    auto titleArea = content.removeFromTop(12.0f);
    g.drawText(title, titleArea, juce::Justification::centredLeft, true);

    float channelHeight = (content.getHeight() - 4.0f) / 2.0f;
    auto barL = content.removeFromTop(channelHeight);
    content.removeFromTop(4.0f);
    auto barR = content.removeFromTop(channelHeight);

    auto drawChannelBar = [&](juce::Rectangle<float> rect, float peak, float rms, bool clipped)
    {
        // Dark channel track
        g.setColour(juce::Colour(0xff0d0e10));
        g.fillRect(rect);

        // Meter gradient
        float barWidth = rect.getWidth() - 14.0f; // Leave space for clip LED
        float rmsW = std::clamp(rms, 0.0f, 1.0f) * barWidth;
        float peakW = std::clamp(peak, 0.0f, 1.0f) * barWidth;

        // Draw RMS fill
        juce::ColourGradient grad(juce::Colour(0xff00e676), rect.getX(), rect.getY(),
                                  juce::Colour(0xffff9100), rect.getX() + barWidth * 0.707f, rect.getY(), false);
        grad.addColour(0.85, juce::Colour(0xffff3d00));

        g.setGradientFill(grad);
        g.fillRect(rect.withWidth(rmsW));

        // Draw Peak indicator line
        if (peakW > 1.0f)
        {
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(static_cast<int>(rect.getX() + peakW), rect.getY(), rect.getBottom());
        }

        // Draw -3 dBfs mark (0.707 linear)
        float markX = rect.getX() + (0.707f * barWidth);
        g.setColour(juce::Colour(0xff505050));
        g.drawVerticalLine(static_cast<int>(markX), rect.getY(), rect.getBottom());

        // Clip LED
        auto ledRect = rect.withLeft(rect.getRight() - 10.0f).reduced(1.0f);
        g.setColour(clipped ? juce::Colour(0xffff1744) : juce::Colour(0xff3a1015));
        g.fillRoundedRectangle(ledRect, 2.0f);
    };

    drawChannelBar(barL, displayPeakL, displayRmsL, clipL);
    drawChannelBar(barR, displayPeakR, displayRmsR, clipR);
}

} // namespace abdaudiolab::gui
