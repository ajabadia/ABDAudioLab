/**
 * @file SuiteRowLayout.h
 * @brief Unified geometry layout calculator for queue rows and point sub-rows.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SuiteDataModels.h"

namespace abdaudiolab::gui
{

struct SuiteMainRowLayout
{
    juce::Rectangle<float> rowRect;
    juce::Rectangle<float> reorderArea;
    juce::Rectangle<float> reorderUpRect;
    juce::Rectangle<float> reorderDownRect;
    juce::Rectangle<float> badgeRect;
    juce::Rectangle<float> expandBtnRect;
    juce::Rectangle<float> titleRect;

    // Right Action Buttons
    juce::Rectangle<float> delBtnRect;
    juce::Rectangle<float> copyBtnRect;
    juce::Rectangle<float> editBtnRect;

    // State actions
    juce::Rectangle<float> contBtnRect;
    juce::Rectangle<float> resetBtnRect;
    juce::Rectangle<float> rerunBtnRect;

    // Status display & bypass pill
    juce::Rectangle<float> statusTextRect;
    juce::Rectangle<float> bypassPillRect;

    static SuiteMainRowLayout calculate(float y, float width, const QueueItem& item, size_t index, size_t totalQueueSize);
};

struct SuitePointRowLayout
{
    juce::Rectangle<float> rowRect;
    juce::Rectangle<float> selectBoxRect;
    juce::Rectangle<float> labelRect;
    juce::Rectangle<float> statusPillRect;
    juce::Rectangle<float> viewBtnRect;
    juce::Rectangle<float> clearBtnRect;
    juce::Rectangle<float> delBtnRect;

    static SuitePointRowLayout calculate(float y, float width, int pointIndex, int totalPoints, PointStatus status);
};

} // namespace abdaudiolab::gui
