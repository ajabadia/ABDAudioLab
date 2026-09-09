/**
 * @file DrawerDataModels.h
 * @brief Data structures representing hardware items, functions, and controls in the drawer UI.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace abdaudiolab::gui
{

struct ControlItem
{
    juce::String name;
    juce::String type;
};

struct FunctionItem
{
    juce::String id;
    juce::String name;
    juce::String blockType;
    juce::String stimulusOutput;
    juce::String responseInput;
    juce::String notes;
    juce::String captureMode { "FIXED_TIME" };
    float defaultBurstDurationSec { 1.0f };
    std::vector<ControlItem> controls;
};

struct HardwareItem
{
    juce::String id;
    juce::String displayName;
    juce::String description;
    juce::String category;
    juce::String brand;
    juce::String brandLogo;
    juce::String modelImage;
    juce::String protocol; // "AIRA_SYSEX", "MIDI_CC", "MANUAL_ANALOGUE", "MOCK_DSP"
    std::vector<FunctionItem> functions;
};

} // namespace abdaudiolab::gui
