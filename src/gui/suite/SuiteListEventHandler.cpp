/**
 * @file SuiteListEventHandler.cpp
 * @brief Implementation of SuiteListEventHandler click dispatching, context menus,
 *        and row button actions.
 * @author ABDSynths
 * @date 2026
 */

#include "SuiteListEventHandler.h"

namespace abdaudiolab::gui::suite
{

void SuiteListEventHandler::showPointContextMenu(int queueIndex, int pointIndex)
{
    juce::PopupMenu menu;
    bool isSel = model.isPointSelected(queueIndex, pointIndex);

    menu.addItem(1, isSel ? "Deselect Point #" + juce::String(pointIndex + 1) : "Select Point #" + juce::String(pointIndex + 1));
    if (model.getLastSelectedQueueIndex() == queueIndex && model.getLastSelectedPointIndex() >= 0 && model.getLastSelectedPointIndex() != pointIndex)
    {
        menu.addItem(2, "Select Range (from Point #" + juce::String(model.getLastSelectedPointIndex() + 1) + " to #" + juce::String(pointIndex + 1) + ")");
    }
    menu.addItem(3, "Select All Points in This Test");
    menu.addItem(4, "Select All Invalidated / Error Points");
    if (model.getSelectedPointCount() > 0)
    {
        menu.addItem(5, "Clear All Selections");
    }
    menu.addSeparator();
    menu.addItem(6, "Mark as Re-Run (Invalidate)");
    menu.addItem(7, "Mark as Annulled");
    if (model.getSelectedPointCount() > 0)
    {
        menu.addSeparator();
        menu.addItem(8, "Re-Measure Selected (" + juce::String(model.getSelectedPointCount()) + ") Points Now");
    }
    // Live single-point re-run (only available when a session is active)
    if (cb.onRerunPointRequested)
    {
        menu.addSeparator();
        menu.addItem(9, "Re-run Point #" + juce::String(pointIndex + 1) + " (Live Session)");
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, queueIndex, pointIndex](int result) {
        if (result == 1) model.togglePointSelection(queueIndex, pointIndex);
        else if (result == 2) model.selectPointRange(queueIndex, model.getLastSelectedPointIndex(), pointIndex, true);
        else if (result == 3) model.selectAllPointsInTest(queueIndex, true);
        else if (result == 4) model.selectAllInvalidatedPoints();
        else if (result == 5) model.clearAllSelections();
        else if (result == 6)
        {
            model.setPointStatus(queueIndex, pointIndex, PointStatus::Invalidated);
            model.setPointSelected(queueIndex, pointIndex, true);
        }
        else if (result == 7)
        {
            model.setPointStatus(queueIndex, pointIndex, PointStatus::Annulled);
        }
        else if (result == 8)
        {
            auto sel = model.getSelectedPoints();
            if (!sel.empty() && cb.onRerunSelectedClicked)
                cb.onRerunSelectedClicked(sel);
        }
        else if (result == 9)
        {
            if (cb.onRerunPointRequested)
                cb.onRerunPointRequested(queueIndex, pointIndex);
        }

        if (cb.onQueueChanged)
            cb.onQueueChanged();
    });
}

bool SuiteListEventHandler::handleMouseDown(const juce::MouseEvent& e,
                                            float width,
                                            bool isCompactView)
{
    float currentY = 0.0f;
    const auto& queue = model.getQueue();

    for (size_t i = 0; i < queue.size(); ++i)
    {
        const auto& item = queue[i];
        auto layout = SuiteMainRowLayout::calculate(currentY, width, item, i, queue.size());
        currentY += 34.0f;

        if (layout.rowRect.contains(e.position))
        {
            if (!item.isPinned)
            {
                if (layout.reorderUpRect.contains(e.position))
                {
                    model.moveUp(static_cast<int>(i));
                    if (cb.onQueueChanged) cb.onQueueChanged();
                    return true;
                }
                if (layout.reorderDownRect.contains(e.position))
                {
                    model.moveDown(static_cast<int>(i));
                    if (cb.onQueueChanged) cb.onQueueChanged();
                    return true;
                }
                if (layout.delBtnRect.contains(e.position))
                {
                    if (cb.onRequestDeleteTest)
                        cb.onRequestDeleteTest(static_cast<int>(i), item);
                    else
                        model.removeTestDirectly(static_cast<int>(i));

                    if (cb.onQueueChanged) cb.onQueueChanged();
                    return true;
                }
                if (layout.copyBtnRect.contains(e.position))
                {
                    model.duplicateTest(static_cast<int>(i));
                    if (cb.onQueueChanged) cb.onQueueChanged();
                    return true;
                }
                if (layout.editBtnRect.contains(e.position))
                {
                    if (cb.onEditTestClicked)
                        cb.onEditTestClicked(static_cast<int>(i), item);
                    return true;
                }
            }

            if (item.status == QueueItemStatus::Incomplete)
            {
                if (layout.contBtnRect.contains(e.position))
                {
                    if (cb.onContinueTestClicked) cb.onContinueTestClicked(static_cast<int>(i));
                    return true;
                }
                if (layout.resetBtnRect.contains(e.position))
                {
                    if (cb.onRestartTestClicked) cb.onRestartTestClicked(static_cast<int>(i));
                    return true;
                }
            }
            else if (item.status == QueueItemStatus::Invalidated)
            {
                if (layout.rerunBtnRect.contains(e.position))
                {
                    if (cb.onRestartTestClicked) cb.onRestartTestClicked(static_cast<int>(i));
                    return true;
                }
            }

            if (layout.bypassPillRect.contains(e.position))
            {
                model.toggleSkipped(static_cast<int>(i));
                if (cb.onQueueChanged) cb.onQueueChanged();
                return true;
            }

            // Expand arrow or title click toggles expansion
            model.toggleExpanded(static_cast<int>(i));
            if (cb.onQueueChanged) cb.onQueueChanged();
            return true;
        }

        // Sub-rows hit test
        if (item.isExpanded)
        {
            currentY += 28.0f; // Skip progress bar

            if (!isCompactView)
            {
                for (int pIdx = 0; pIdx < item.totalPoints; ++pIdx)
                {
                    PointStatus ptStatus = PointStatus::Queued;
                    if (static_cast<size_t>(pIdx) < item.pointStatuses.size())
                        ptStatus = item.pointStatuses[static_cast<size_t>(pIdx)];

                    auto ptLayout = SuitePointRowLayout::calculate(currentY, width, pIdx, item.totalPoints, ptStatus);
                    currentY += 24.0f;

                    if (ptLayout.rowRect.contains(e.position))
                    {
                        if (e.mods.isPopupMenu())
                        {
                            showPointContextMenu(static_cast<int>(i), pIdx);
                            return true;
                        }

                        if (ptLayout.delBtnRect.contains(e.position))
                        {
                            if (cb.onDeletePointClicked) cb.onDeletePointClicked(static_cast<int>(i), pIdx);
                            return true;
                        }
                        if (ptLayout.clearBtnRect.contains(e.position))
                        {
                            model.setPointStatus(static_cast<int>(i), pIdx, PointStatus::Invalidated);
                            model.setPointSelected(static_cast<int>(i), pIdx, true);
                            if (cb.onClearPointClicked) cb.onClearPointClicked(static_cast<int>(i), pIdx);
                            if (cb.onQueueChanged) cb.onQueueChanged();
                            return true;
                        }
                        if (ptLayout.viewBtnRect.contains(e.position))
                        {
                            if (cb.onSelectPointClicked) cb.onSelectPointClicked(static_cast<int>(i), pIdx);
                            return true;
                        }
                        if (ptLayout.statusPillRect.contains(e.position))
                        {
                            if (ptStatus == PointStatus::Invalidated)
                            {
                                auto sel = model.getSelectedPoints();
                                if (sel.empty()) sel.emplace_back(static_cast<int>(i), pIdx);
                                if (cb.onRerunSelectedClicked) cb.onRerunSelectedClicked(sel);
                            }
                            else
                            {
                                if (cb.onSelectPointClicked) cb.onSelectPointClicked(static_cast<int>(i), pIdx);
                            }
                            return true;
                        }

                        // Checkbox selection toggle or range selection via Shift+Click
                        if (e.mods.isShiftDown())
                        {
                            model.selectPointRange(static_cast<int>(i), model.getLastSelectedPointIndex(), pIdx, true);
                        }
                        else
                        {
                            model.togglePointSelection(static_cast<int>(i), pIdx);
                        }

                        if (cb.onQueueChanged) cb.onQueueChanged();
                        return true;
                    }
                }
                currentY += 6.0f;
            }
        }
    }

    return false;
}

} // namespace abdaudiolab::gui::suite
