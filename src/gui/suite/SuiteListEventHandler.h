/**
 * @file SuiteListEventHandler.h
 * @brief Handles user interactions, right-click context menus, and row hit-tests
 *        for the Test Suite Queue.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SuiteQueueModelManager.h"
#include "SuiteRowLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <utility>

namespace abdaudiolab::gui::suite
{

class SuiteListEventHandler
{
public:
    struct Callbacks
    {
        std::function<void(int index, const QueueItem& item)> onEditTestClicked;
        std::function<void(int index, const QueueItem& item)> onRequestDeleteTest;
        std::function<void(int index)> onContinueTestClicked;
        std::function<void(int index)> onRestartTestClicked;
        std::function<void(int queueIndex, int pointIndex)> onSelectPointClicked;
        std::function<void(int queueIndex, int pointIndex)> onClearPointClicked;
        std::function<void(int queueIndex, int pointIndex)> onDeletePointClicked;
        std::function<void(const std::vector<std::pair<int, int>>& points)> onRerunSelectedClicked;
        /** Fired when the user selects "Re-run this Point" from the context menu while session is active. */
        std::function<void(int queueIndex, int pointIndex)> onRerunPointRequested;
        std::function<void()> onQueueChanged;
    };

    explicit SuiteListEventHandler(SuiteQueueModelManager& modelManagerRef, Callbacks callbacksRef)
        : model(modelManagerRef), cb(std::move(callbacksRef)) {}

    void setCallbacks(Callbacks newCallbacks) { cb = std::move(newCallbacks); }

    void showPointContextMenu(int queueIndex, int pointIndex);

    bool handleMouseDown(const juce::MouseEvent& e,
                         float width,
                         bool isCompactView);

private:
    SuiteQueueModelManager& model;
    Callbacks cb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuiteListEventHandler)
};

} // namespace abdaudiolab::gui::suite
