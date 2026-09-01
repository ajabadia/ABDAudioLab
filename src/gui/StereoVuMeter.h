#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief High-precision stereo VU meter component.
 * 
 * Features:
 * - Dual channel L/R bargraph with RMS and Peak hold.
 * - -3 dBfs reference line for safe gain staging.
 * - Red clipping alert LED.
 */
class StereoVuMeter : public juce::Component,
                      public juce::Timer
{
public:
    StereoVuMeter(const juce::String& labelTitle);
    ~StereoVuMeter() override;

    void setLevels(float peakL, float peakR, float rmsL, float rmsR);
    void timerCallback() override;

    void paint(juce::Graphics& g) override;

private:
    juce::String title;
    float currentPeakL { 0.0f };
    float currentPeakR { 0.0f };
    float currentRmsL { 0.0f };
    float currentRmsR { 0.0f };

    float displayPeakL { 0.0f };
    float displayPeakR { 0.0f };
    float displayRmsL { 0.0f };
    float displayRmsR { 0.0f };

    bool clipL { false };
    bool clipR { false };
    int clipHoldCounterL { 0 };
    int clipHoldCounterR { 0 };
};

} // namespace abdaudiolab::gui
