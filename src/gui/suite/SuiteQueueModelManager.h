/**
 * @file SuiteQueueModelManager.h
 * @brief Manages data structures, point execution statuses, multi-selection ranges,
 *        and sorting/filtering for the Test Suite Queue.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SuiteDataModels.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace abdaudiolab::gui::suite
{

class SuiteQueueModelManager
{
public:
    SuiteQueueModelManager() = default;
    ~SuiteQueueModelManager() = default;

    void ensureNoiseBaselineTestPinned();
    bool isTestInQueue(const juce::String& signature) const noexcept;
    bool addTest(const QueueItem& item);
    void updateTest(int index, const QueueItem& item);
    void duplicateTest(int index);
    bool removeTestDirectly(int index);
    void invalidateTest(int index);
    void moveUp(int index);
    void moveDown(int index);
    void toggleSkipped(int index);
    void toggleExpanded(int index);
    void clear();

    [[nodiscard]] const std::vector<QueueItem>& getQueue() const noexcept { return queue; }
    [[nodiscard]] std::vector<QueueItem>& getQueue() noexcept { return queue; }
    [[nodiscard]] int getQueueSize() const noexcept { return static_cast<int>(queue.size()); }
    [[nodiscard]] int getTotalPointCount() const noexcept;

    // Status management
    void updateItemStatus(int index, QueueItemStatus status, int currentPoint = 0);
    void resetAllStatuses();
    void setPointStatus(int queueIndex, int pointIndex, PointStatus status);
    [[nodiscard]] PointStatus getPointStatus(int queueIndex, int pointIndex) const;
    void resetPointStatuses(int queueIndex);

    // 1.7.2: Multi-selection & Error Patching
    void setPointSelected(int queueIndex, int pointIndex, bool isSelected);
    [[nodiscard]] bool isPointSelected(int queueIndex, int pointIndex) const;
    void togglePointSelection(int queueIndex, int pointIndex);
    void selectPointRange(int queueIndex, int startPointIndex, int endPointIndex, bool isSelected = true);
    void selectAllPointsInTest(int queueIndex, bool isSelected = true);
    void selectAllInvalidatedPoints();
    void clearAllSelections();
    [[nodiscard]] std::vector<std::pair<int, int>> getSelectedPoints() const;
    [[nodiscard]] int getSelectedPointCount() const;
    void invalidateSelectedPoints();

    // Selection tracking anchors (for Shift+Click ranges)
    [[nodiscard]] int getLastSelectedQueueIndex() const noexcept { return lastSelectedQueueIdx; }
    [[nodiscard]] int getLastSelectedPointIndex() const noexcept { return lastSelectedPointIdx; }
    void setLastSelectedIndices(int qIdx, int pIdx) noexcept
    {
        lastSelectedQueueIdx = qIdx;
        lastSelectedPointIdx = pIdx;
    }

private:
    std::vector<QueueItem> queue;
    int lastSelectedQueueIdx { -1 };
    int lastSelectedPointIdx { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuiteQueueModelManager)
};

} // namespace abdaudiolab::gui::suite
