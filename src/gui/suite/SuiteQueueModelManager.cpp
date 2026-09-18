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
    bool hasPinned = false;
    for (const auto& item : queue)
    {
        if (item.isPinned)
        {
            hasPinned = true;
            break;
        }
    }

    if (!hasPinned)
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
    if (item.id.isEmpty() || isTestInQueue(item.id))
        return false;

    queue.push_back(item);
    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::updateTest(int index, const QueueItem& item)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    if (queue[static_cast<size_t>(index)].isPinned)
        return false;

    bool wasSelected = (selectedItemId == queue[static_cast<size_t>(index)].id);
    queue[static_cast<size_t>(index)] = item;
    if (wasSelected)
        selectedItemId = item.id;

    if (queue[static_cast<size_t>(index)].status == QueueItemStatus::Completed)
    {
        queue[static_cast<size_t>(index)].status = QueueItemStatus::Invalidated;
    }

    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::duplicateTest(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    QueueItem cloned = queue[static_cast<size_t>(index)];
    juce::String baseId = cloned.id;
    int copyNum = 1;
    juce::String newId = baseId + "_copy_" + juce::String(copyNum);
    while (isTestInQueue(newId))
    {
        ++copyNum;
        newId = baseId + "_copy_" + juce::String(copyNum);
    }
    cloned.id = newId;
    cloned.title = cloned.title + " (Copy)";
    cloned.status = QueueItemStatus::Queued;
    cloned.currentRunningPoint = 0;
    cloned.isPinned = false;
    cloned.pointStatuses.assign(static_cast<size_t>(cloned.totalPoints), PointStatus::Queued);
    cloned.pointSelections.assign(static_cast<size_t>(cloned.totalPoints), false);

    queue.push_back(cloned);
    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::removeTestDirectly(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    if (queue[static_cast<size_t>(index)].isPinned)
        return false;

    auto removedId = queue[static_cast<size_t>(index)].id;
    queue.erase(queue.begin() + index);

    if (selectedItemId == removedId)
        selectedItemId.clear();

    if (lastSelectedQueueIdx == index)
    {
        lastSelectedQueueIdx = -1;
        lastSelectedPointIdx = -1;
    }
    else if (lastSelectedQueueIdx > index)
    {
        lastSelectedQueueIdx--;
    }

    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::invalidateTest(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    if (queue[static_cast<size_t>(index)].isPinned)
        return false;

    auto& item = queue[static_cast<size_t>(index)];
    item.status = QueueItemStatus::Invalidated;
    for (auto& pSt : item.pointStatuses)
    {
        if (pSt != PointStatus::Annulled)
            pSt = PointStatus::Invalidated;
    }

    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::moveUp(int index)
{
    if (index <= 0 || index >= static_cast<int>(queue.size()))
        return false;

    if (queue[static_cast<size_t>(index)].isPinned)
        return false;

    if (queue[static_cast<size_t>(index - 1)].isPinned)
        return false;

    std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index - 1)]);

    if (lastSelectedQueueIdx == index)
        lastSelectedQueueIdx = index - 1;
    else if (lastSelectedQueueIdx == index - 1)
        lastSelectedQueueIdx = index;

    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::moveDown(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()) - 1)
        return false;

    if (queue[static_cast<size_t>(index)].isPinned)
        return false;

    if (queue[static_cast<size_t>(index + 1)].isPinned)
        return false;

    std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index + 1)]);

    if (lastSelectedQueueIdx == index)
        lastSelectedQueueIdx = index + 1;
    else if (lastSelectedQueueIdx == index + 1)
        lastSelectedQueueIdx = index;

    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::toggleSkipped(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    queue[static_cast<size_t>(index)].isSkipped = !queue[static_cast<size_t>(index)].isSkipped;
    notifyQueueChanged();
    return true;
}

bool SuiteQueueModelManager::toggleExpanded(int index)
{
    if (index < 0 || index >= static_cast<int>(queue.size()))
        return false;

    queue[static_cast<size_t>(index)].isExpanded = !queue[static_cast<size_t>(index)].isExpanded;
    notifyQueueChanged();
    return true;
}

void SuiteQueueModelManager::clear()
{
    queue.clear();
    ensureNoiseBaselineTestPinned();
    selectedItemId.clear();
    lastSelectedQueueIdx = -1;
    lastSelectedPointIdx = -1;
    notifyQueueChanged();
}

void SuiteQueueModelManager::selectItem(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        selectedItemId = queue[static_cast<size_t>(index)].id;
    }
    else
    {
        selectedItemId.clear();
    }
    notifyQueueChanged();
}

void SuiteQueueModelManager::selectItemById(const juce::String& id)
{
    if (id.isNotEmpty() && isTestInQueue(id))
    {
        selectedItemId = id;
    }
    else
    {
        selectedItemId.clear();
    }
    notifyQueueChanged();
}

void SuiteQueueModelManager::clearItemSelection() noexcept
{
    if (selectedItemId.isNotEmpty())
    {
        selectedItemId.clear();
        notifyQueueChanged();
    }
}

int SuiteQueueModelManager::getSelectedItemIndex() const noexcept
{
    if (selectedItemId.isEmpty())
        return -1;

    for (size_t i = 0; i < queue.size(); ++i)
    {
        if (queue[i].id == selectedItemId)
            return static_cast<int>(i);
    }
    return -1;
}

const juce::String& SuiteQueueModelManager::getSelectedItemId() const noexcept
{
    return selectedItemId;
}

const QueueItem* SuiteQueueModelManager::getSelectedItem() const noexcept
{
    int idx = getSelectedItemIndex();
    if (idx >= 0 && idx < static_cast<int>(queue.size()))
        return &queue[static_cast<size_t>(idx)];
    return nullptr;
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
