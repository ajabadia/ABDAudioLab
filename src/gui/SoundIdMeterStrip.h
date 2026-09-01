#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @brief Right-hand vertical meter and master profiling control strip.
 * 
 * Features:
 * - Dual vertical LED meters (In / Out) with smooth RMS and Peak hold.
 * - Numerical peak dB readout.
 * - Vertical Gain Trim fader.
 * - Circular master start / pause toggle button.
 */
class SoundIdMeterStrip : public juce::Component,
                          public juce::Timer
{
public:
    SoundIdMeterStrip();
    ~SoundIdMeterStrip() override;

    void setLevels(float inPeakL, float inPeakR, float inRms,
                   float outPeakL, float outPeakR, float outRms);
    void setProfilingActive(bool active);

    std::function<void()> onMasterToggleClicked;
    std::function<void(bool start)> onProfilingToggled;
    std::function<void()> onAutoTrimClicked;
    std::function<void(float trimGain)> onTrimChanged;

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    float currentInPeak { 0.0f }, currentInRms { 0.0f };
    float currentOutPeak { 0.0f }, currentOutRms { 0.0f };

    float displayInPeak { 0.0f }, displayInRms { 0.0f };
    float displayOutPeak { 0.0f }, displayOutRms { 0.0f };

    bool isProfilingActive { false };
    float trimGainLinear { 1.0f };

    juce::Slider trimSlider;
    juce::ShapeButton masterButton { "Start/Stop", SoundIdTheme::accentGreen, SoundIdTheme::accentGreen, SoundIdTheme::accentGreen };

    void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> barRect, float peak, float rms);
};

} // namespace abdaudiolab::gui
