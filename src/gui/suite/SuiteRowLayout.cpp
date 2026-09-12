/**
 * @file SuiteRowLayout.cpp
 * @brief Implementation of SuiteRowLayout geometry calculations.
 * @author ABDSynths
 * @date 2026
 */

#include "SuiteRowLayout.h"

namespace abdaudiolab::gui
{

SuiteMainRowLayout SuiteMainRowLayout::calculate(float y, float width, const QueueItem& item, size_t index, size_t totalQueueSize)
{
    SuiteMainRowLayout l;
    l.rowRect = juce::Rectangle<float>(0.0f, y, width, 34.0f).reduced(0.0f, 1.0f);

    auto area = l.rowRect.reduced(6.0f, 2.0f);

    // 1. Order controls [^] [v] or [PIN]
    l.reorderArea = area.removeFromLeft(36.0f);
    if (!item.isPinned)
    {
        auto tempReorder = l.reorderArea;
        l.reorderUpRect = tempReorder.removeFromLeft(16.0f);
        l.reorderDownRect = tempReorder.removeFromLeft(16.0f);
    }
    area.removeFromLeft(4.0f);

    // 2. Badge (FLT, ENV, etc.)
    l.badgeRect = area.removeFromLeft(38.0f).withSizeKeepingCentre(36.0f, 20.0f);
    area.removeFromLeft(8.0f);

    // 3. Right actions area (290px)
    auto rightActions = area.removeFromRight(290.0f);

    if (!item.isPinned)
    {
        l.delBtnRect = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
        l.copyBtnRect = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
        l.editBtnRect = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
    }
    else
    {
        rightActions.removeFromRight(112.0f);
    }

    rightActions.removeFromRight(6.0f);

    if (item.status == QueueItemStatus::Incomplete)
    {
        l.contBtnRect = rightActions.removeFromRight(58.0f).withSizeKeepingCentre(54.0f, 20.0f);
        l.resetBtnRect = rightActions.removeFromRight(50.0f).withSizeKeepingCentre(46.0f, 20.0f);
    }
    else if (item.status == QueueItemStatus::Invalidated)
    {
        l.rerunBtnRect = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
    }

    // Status text width depending on status
    float statusW = 60.0f;
    if (item.isSkipped || item.status == QueueItemStatus::Completed)
        statusW = 70.0f;
    else if (item.status == QueueItemStatus::Running)
        statusW = 90.0f;
    else if (item.status == QueueItemStatus::Invalidated)
        statusW = 65.0f;

    l.statusTextRect = rightActions.removeFromRight(statusW);
    rightActions.removeFromRight(6.0f);

    l.bypassPillRect = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);

    // 4. Expand button
    l.expandBtnRect = area.removeFromLeft(16.0f);
    area.removeFromLeft(4.0f);

    // 5. Title & description
    l.titleRect = area;

    return l;
}

SuitePointRowLayout SuitePointRowLayout::calculate(float y, float width, int pointIndex, int totalPoints, PointStatus status)
{
    juce::ignoreUnused(pointIndex, totalPoints, status);
    SuitePointRowLayout l;
    l.rowRect = juce::Rectangle<float>(32.0f, y + 1.0f, width - 36.0f, 22.0f);

    auto subArea = l.rowRect.reduced(6.0f, 2.0f);
    l.selectBoxRect = subArea.removeFromLeft(16.0f).withSizeKeepingCentre(13.0f, 13.0f);
    subArea.removeFromLeft(6.0f);

    // Right action buttons first (fixed width)
    l.delBtnRect = subArea.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
    subArea.removeFromRight(6.0f);
    l.clearBtnRect = subArea.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
    subArea.removeFromRight(6.0f);
    l.viewBtnRect = subArea.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
    subArea.removeFromRight(6.0f);

    // Status pill (fixed width)
    l.statusPillRect = subArea.removeFromRight(86.0f).withSizeKeepingCentre(80.0f, 16.0f);
    subArea.removeFromRight(4.0f);

    // Label gets all remaining space
    l.labelRect = subArea;

    return l;
}

} // namespace abdaudiolab::gui
