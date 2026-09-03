/**
 * @file HardwareControlRenderer.h
 * @brief Reusable vector drawing utility for hardware controls (Knob, Slider, JackPort, Button, Switch).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SoundIdTheme.h"
#include "../core/ProfilingSession.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @class HardwareControlRenderer
 * @brief Vector graphic rendering engine for hardware controls and operator step prompts.
 */
class HardwareControlRenderer
{
public:
    HardwareControlRenderer() = delete;

    static void drawKnob(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps);
    static void drawSlider(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps);
    static void drawJackPort(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps);
    static void drawButton(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps);
    static void drawSwitch(juce::Graphics& g, juce::Rectangle<float> area, const core::ParameterStep& ps);
    static void drawTargetValueBadge(juce::Graphics& g, juce::Rectangle<float> valArea, const juce::String& text, juce::Colour textColour);
};

} // namespace abdaudiolab::gui
