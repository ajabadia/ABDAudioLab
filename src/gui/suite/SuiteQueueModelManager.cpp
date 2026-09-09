/**
 * @file SuiteQueueModelManager.cpp
 * @brief Implementation of SuiteQueueModelManager data mutations, status tracking,
 *        and multi-point selection algorithms.
 * @author ABDSynths
 * @date 2026
 */

#include "SuiteQueueModelManager.h"
#include <algorithm>

namespace abdaudiolab::gui::suite
{

void SuiteQueueModelManager::ensureNoiseBaselineTestPinned()
{
    if (queue.empty() || !queue[0].isPinned)
    {
        QueueItem noiseItem;
        noiseItem.id = "system:noise_floor_baseline";
        noiseItem.hwId = "system";
        noiseItem.funcId = "noise_baseline";
        noiseItem.badgeText = "NOI";
        noiseItem.badgeColor = juce::Colour(0xff94a3b8);
        noiseItem.title = "0. Noise Floor Baseline & SNR Check";
        noiseItem.description = "Measure open thermal noise to set SNR threshold";
        noiseItem.stimulusType = audio::StimulusType::Silence;
        noiseItem.burstDurationSec = 1.0f;
        noiseItem.totalPoints = 1;
        noiseItem.status = QueueItemStatus::Queued;
        noiseItem.isPinned = true;
        noiseItem.isSkipped = false;

        queue.insert(queue.begin(), noiseItem);
    }
}

bool SuiteQueueModelManager::isTestInQueue(const juce::String& signature) const noexcept
{
    for (const auto& item : queue)
    {
        if (item.id == signature)
            return true;
    }
    return false;
}

bool SuiteQueueModelManager::addTest(const QueueItem& item)
{
    if (isTestInQueue(item.id))
        return false;

    queue.push_back(item);
    return true;
}

void SuiteQueueModelManager::updateTest(int index, const QueueItem& item)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)] = item;
        if (queue[static_cast<size_t>(index)].status == QueueItemStatus::Completed)
        {
            queue[static_cast<size_t>(index)].status = QueueItemStatus::Invalidated;
        }
    }
}

void SuiteQueueModelManager::duplicateTest(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        QueueItem cloned = queue[static_cast<size_t>(index)];
        cloned.id = cloned.id + "_copy_" + juce::String(juce::Random::getSystemRandom().nextInt(10000));
        cloned.title = cloned.title + " (Copy)";
        cloned.status = QueueItemStatus::Queued;
        cloned.currentRunningPoint = 0;
        cloned.isPinned = false;
        queue.push_back(cloned);
    }
}

bool SuiteQueueModelManager::removeTestDirectly(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        if (queue[static_cast<size_t>(index)].isPinned)
            return false;

        queue.erase(queue.begin() + index);
        return true;
    }
    return false;
}

void SuiteQueueModelManager::invalidateTest(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].status = QueueItemStatus::Invalidated;
    }
}

void SuiteQueueModelManager::moveUp(int index)
{
    if (index > 1 && index < static_cast<int>(queue.size()))
    {
        std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index - 1)]);
    }
}

void SuiteQueueModelManager::moveDown(int index)
{
    if (index >= 1 && index < static_cast<int>(queue.size()) - 1)
    {
        std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index + 1)]);
    }
}

void SuiteQueueModelManager::toggleSkipped(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].isSkipped = !queue[static_cast<size_t>(index)].isSkipped;
    }
}

void SuiteQueueModelManager::toggleExpanded(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].isExpanded = !queue[static_cast<size_t>(index)].isExpanded;
    }
}

void SuiteQueueModelManager::clear()
{
    queue.clear();
    ensureNoiseBaselineTestPinned();
    lastSelectedQueueIdx = -1;
    lastSelectedPointIdx = -1;
}

int SuiteQueueModelManager::getTotalPointCount() const noexcept
{
    int total = 0;
    for (const auto& item : queue)
        total += item.totalPoints;
    return std::max(1, total);
}

void SuiteQueueModelManager::updateItemStatus(int index, QueueItemStatus status, int currentPoint)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(index)];
        item.status = status;
        item.currentRunningPoint = currentPoint;

        if (item.pointStatuses.size() != static_cast<size_t>(item.totalPoints))
            item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);

        if (status == QueueItemStatus::Completed)
        {
            for (auto& pSt : item.pointStatuses)
            {
                if (pSt != PointStatus::Annulled)
                    pSt = PointStatus::Completed;
            }
        }
    }
}

void SuiteQueueModelManager::resetAllStatuses()
{
    for (auto& item : queue)
    {
        item.status = QueueItemStatus::Queued;
        item.currentRunningPoint = 0;
        item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);
    }
}

void SuiteQueueModelManager::setPointStatus(int queueIndex, int pointIndex, PointStatus status)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        if (item.pointStatuses.size() != static_cast<size_t>(item.totalPoints))
            item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);

        if (pointIndex >= 0 && pointIndex < item.totalPoints)
        {
            item.pointStatuses[static_cast<size_t>(pointIndex)] = status;
        }
    }
}

PointStatus SuiteQueueModelManager::getPointStatus(int queueIndex, int pointIndex) const
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        const auto& item = queue[static_cast<size_t>(queueIndex)];
        if (pointIndex >= 0 && pointIndex < static_cast<int>(item.pointStatuses.size()))
            return item.pointStatuses[static_cast<size_t>(pointIndex)];
    }
    return PointStatus::Queued;
}

void SuiteQueueModelManager::resetPointStatuses(int queueIndex)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);
        item.currentRunningPoint = 0;
        item.status = QueueItemStatus::Queued;
    }
}

void SuiteQueueModelManager::setPointSelected(int queueIndex, int pointIndex, bool isSelected)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        if (item.pointSelections.size() != static_cast<size_t>(item.totalPoints))
            item.pointSelections.assign(static_cast<size_t>(item.totalPoints), false);

        if (pointIndex >= 0 && pointIndex < item.totalPoints)
        {
            item.pointSelections[static_cast<size_t>(pointIndex)] = isSelected;
            lastSelectedQueueIdx = queueIndex;
            lastSelectedPointIdx = pointIndex;
        }
    }
}

bool SuiteQueueModelManager::isPointSelected(int queueIndex, int pointIndex) const
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        const auto& item = queue[static_cast<size_t>(queueIndex)];
        if (pointIndex >= 0 && pointIndex < static_cast<int>(item.pointSelections.size()))
            return item.pointSelections[static_cast<size_t>(pointIndex)];
    }
    return false;
}

void SuiteQueueModelManager::togglePointSelection(int queueIndex, int pointIndex)
{
    bool current = isPointSelected(queueIndex, pointIndex);
    setPointSelected(queueIndex, pointIndex, !current);
}

void SuiteQueueModelManager::selectPointRange(int queueIndex, int startPointIndex, int endPointIndex, bool isSelected)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        if (item.pointSelections.size() != static_cast<size_t>(item.totalPoints))
            item.pointSelections.assign(static_cast<size_t>(item.totalPoints), false);

        int pMin = std::max(0, std::min(startPointIndex, endPointIndex));
        int pMax = std::min(item.totalPoints - 1, std::max(startPointIndex, endPointIndex));

        for (int p = pMin; p <= pMax; ++p)
        {
            item.pointSelections[static_cast<size_t>(p)] = isSelected;
        }
        lastSelectedQueueIdx = queueIndex;
        lastSelectedPointIdx = endPointIndex;
    }
}

void SuiteQueueModelManager::selectAllPointsInTest(int queueIndex, bool isSelected)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        item.pointSelections.assign(static_cast<size_t>(item.totalPoints), isSelected);
        lastSelectedQueueIdx = queueIndex;
        lastSelectedPointIdx = item.totalPoints - 1;
    }
}

void SuiteQueueModelManager::selectAllInvalidatedPoints()
{
    for (auto& item : queue)
    {
        if (item.pointSelections.size() != static_cast<size_t>(item.totalPoints))
            item.pointSelections.assign(static_cast<size_t>(item.totalPoints), false);

        for (int p = 0; p < item.totalPoints; ++p)
        {
            PointStatus st = PointStatus::Queued;
            if (static_cast<size_t>(p) < item.pointStatuses.size())
                st = item.pointStatuses[static_cast<size_t>(p)];

            if (st == PointStatus::Invalidated || st == PointStatus::Annulled)
                item.pointSelections[static_cast<size_t>(p)] = true;
        }
    }
}

void SuiteQueueModelManager::clearAllSelections()
{
    for (auto& item : queue)
    {
        item.pointSelections.assign(static_cast<size_t>(item.totalPoints), false);
    }
    lastSelectedQueueIdx = -1;
    lastSelectedPointIdx = -1;
}

std::vector<std::pair<int, int>> SuiteQueueModelManager::getSelectedPoints() const
{
    std::vector<std::pair<int, int>> result;
    for (size_t q = 0; q < queue.size(); ++q)
    {
        const auto& item = queue[q];
        for (size_t p = 0; p < item.pointSelections.size(); ++p)
        {
            if (item.pointSelections[p])
                result.emplace_back(static_cast<int>(q), static_cast<int>(p));
        }
    }
    return result;
}

int SuiteQueueModelManager::getSelectedPointCount() const
{
    int count = 0;
    for (const auto& item : queue)
    {
        for (bool sel : item.pointSelections)
        {
            if (sel) ++count;
        }
    }
    return count;
}

void SuiteQueueModelManager::invalidateSelectedPoints()
{
    for (size_t q = 0; q < queue.size(); ++q)
    {
        auto& item = queue[q];
        for (size_t p = 0; p < item.pointSelections.size(); ++p)
        {
            if (item.pointSelections[p])
            {
                if (item.pointStatuses.size() != static_cast<size_t>(item.totalPoints))
                    item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);
                item.pointStatuses[p] = PointStatus::Invalidated;
            }
        }
    }
}

} // namespace abdaudiolab::gui::suite
