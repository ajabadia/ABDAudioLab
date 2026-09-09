/**
 * @file SuiteDataModels.h
 * @brief Data structures for Test Suite Queue items and point execution states.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "../../audio/LabStimulusGenerator.h"
#include "../TestConfigModal.h"
#include <vector>

namespace abdaudiolab::gui
{

enum class QueueItemStatus
{
    Queued,
    Running,
    Incomplete,    // Cancelled midway (e.g. 14/64 points)
    Completed,     // 100% measured and verified
    Invalidated    // Marked invalid or modified after completion
};

enum class PointStatus
{
    Queued,      // Pending measurement (Slate/Gray)
    Running,     // Measuring right now (Blue)
    Completed,   // Executed & verified (Green)
    Invalidated, // Marked to replace / re-measure (Amber)
    Annulled     // Annulled / cancelled (Red)
};

struct QueueItem
{
    juce::String id;          // Unique signature: hwId + ":" + funcId + ":" + testName
    juce::String hwId;
    juce::String funcId;
    juce::String badgeText;   // "NOI", "FLT", "ENV", "MOD", "SAT", "VCA", "CST"
    juce::Colour badgeColor;
    juce::String title;
    juce::String description;
    audio::StimulusType stimulusType { audio::StimulusType::LogFarinaSweep };
    float burstDurationSec { 1.0f };
    juce::String captureMode { "FIXED_TIME" };
    std::vector<ControlStepConfig> controls;
    int totalPoints { 32 };
    QueueItemStatus status { QueueItemStatus::Queued };
    int currentRunningPoint { 0 };
    bool isPinned { false };      // Pinned Test 0 (Noise Floor) stays at index 0
    bool isSkipped { false };     // Skip / Bypass flag
    bool isExpanded { false };    // Show / hide detailed step rows
    std::vector<PointStatus> pointStatuses;
    std::vector<bool> pointSelections; // Individual point selection state (for re-measuring / patching)
};

} // namespace abdaudiolab::gui
