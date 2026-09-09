/**
 * @file SuiteRowRenderer.h
 * @brief Rendering engine for test suite rows, badges, progress bars, and point inspectors.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SuiteDataModels.h"
#include "SuiteRowLayout.h"
#include "SuiteIcons.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

class SuiteRowRenderer
{
public:
    static void renderMainRow(juce::Graphics& g,
                              const QueueItem& item,
                              const SuiteMainRowLayout& layout,
                              size_t index,
                              size_t totalQueueSize,
                              juce::Point<float> hoveredPos);

    static void renderProgressBar(juce::Graphics& g,
                                  const QueueItem& item,
                                  juce::Rectangle<float> bounds);

    static void renderPointRow(juce::Graphics& g,
                               int pointIndex,
                               int totalPoints,
                               PointStatus status,
                               const SuitePointRowLayout& layout,
                               juce::Point<float> hoveredPos,
                               bool isSelected = false);
};

} // namespace abdaudiolab::gui
