#pragma once

#include "SoundIdTheme.h"
#include "TestConfigModal.h"
#include "../audio/LabStimulusGenerator.h"
#include "suite/SuiteDataModels.h"
#include "suite/SuiteQueueModelManager.h"
#include "suite/SuiteListEventHandler.h"
#include "suite/SuiteIcons.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>

namespace abdaudiolab::gui
{

/**
 * @brief Monochromatic vector button for targeted point re-measurement (no emojis).
 */
class ReMeasureButton : public juce::TextButton
{
public:
    ReMeasureButton() : juce::TextButton("Re-Measure (0)") {}

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        auto bgCol = findColour(juce::TextButton::buttonColourId);
        auto textCol = isEnabled() ? findColour(juce::TextButton::textColourOffId)
                                   : SoundIdTheme::textMuted.withAlpha(0.6f);

        if (shouldDrawButtonAsHighlighted && isEnabled())
            bgCol = bgCol.brighter(0.08f);
        if (shouldDrawButtonAsDown && isEnabled())
            bgCol = bgCol.darker(0.05f);

        g.setColour(bgCol);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        // Vector monochromatic reload / refresh icon on the left
        auto content = bounds.reduced(10.0f, 0.0f);
        auto iconBounds = content.removeFromLeft(13.0f).withSizeKeepingCentre(13.0f, 13.0f);
        suite_icons::drawReset(g, iconBounds, textCol);

        content.removeFromLeft(6.0f);
        g.setColour(textCol);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.drawText(getButtonText(), content, juce::Justification::centredLeft, true);
    }
};

/**
 * @brief Interactive Batch Test Plan Queue with single-line rows, vertical scrolling Viewport,
 * per-test bypass (RUN / SKIP), and run triggers.
 * Refactored to delegate data modeling to SuiteQueueModelManager and interaction/menus to SuiteListEventHandler.
 */
class SoundIdSuiteList : public juce::Component
{
public:
    SoundIdSuiteList();
    ~SoundIdSuiteList() override = default;

    void ensureNoiseBaselineTestPinned();
    void addTestToQueue(const QueueItem& item);
    void updateTestInQueue(int index, const QueueItem& item);
    void duplicateTestInQueue(int index);
    void removeTestFromQueue(int index);
    void removeTestDirectly(int index);
    void invalidateTest(int index);
    void moveTestUp(int index);
    void moveTestDown(int index);
    void setPointStatus(int queueIndex, int pointIndex, PointStatus status);
    [[nodiscard]] PointStatus getPointStatus(int queueIndex, int pointIndex) const;
    void resetPointStatuses(int queueIndex);

    // 1.7.2: Point Range Re-Measurement & Error Patching
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
    void updateSelectionButton();
    void showPointContextMenu(int queueIndex, int pointIndex);

    void toggleTestSkipped(int index);
    void toggleTestExpanded(int index);
    void clearQueue();

    [[nodiscard]] const std::vector<QueueItem>& getQueue() const noexcept { return modelManager.getQueue(); }
    [[nodiscard]] int getQueueSize() const noexcept { return modelManager.getQueueSize(); }
    [[nodiscard]] int getTotalPointCount() const noexcept { return modelManager.getTotalPointCount(); }
    [[nodiscard]] bool isTestInQueue(const juce::String& signature) const noexcept;

    void updateItemStatus(int index, QueueItemStatus status, int currentPoint = 0);
    void resetAllStatuses();
    void setSessionRunning(bool isRunning);

    std::function<void(int index, const QueueItem& item)> onEditTestClicked;
    std::function<void(int index, const QueueItem& item)> onRequestDeleteTest;
    std::function<void(int index)> onContinueTestClicked;
    std::function<void(int index)> onRestartTestClicked;
    std::function<void(int queueIndex, int pointIndex)> onSelectPointClicked;
    std::function<void(int queueIndex, int pointIndex)> onClearPointClicked;
    std::function<void(int queueIndex, int pointIndex)> onDeletePointClicked;
    std::function<void(const std::vector<std::pair<int, int>>& points)> onRerunSelectedClicked;
    /** Fired when the user right-clicks a point and selects "Re-run this Point" during an active session. */
    std::function<void(int queueIndex, int pointIndex)> onRerunPointRequested;
    std::function<void()> onAddStandardClicked;
    std::function<void()> onAddCustomClicked;
    std::function<void(bool start)> onToggleSessionRunClicked;
    std::function<void(const juce::String& message)> onDuplicateWarning;
    std::function<void()> onToggleCollapse;

    void setCollapsed(bool collapsed);
    void setChevronGlyph(const juce::String& glyph);
    [[nodiscard]] bool getIsCollapsed() const noexcept { return isCollapsed; }
    void updateTheme();
    void setStandardTestAvailable(bool available);
    void updateCompactViewButtonState();

    void paint(juce::Graphics& g) override;
    void resized() override;

    suite::SuiteQueueModelManager& getModelManager() noexcept { return modelManager; }
    const suite::SuiteQueueModelManager& getModelManager() const noexcept { return modelManager; }

private:
    class RowsContentComponent : public juce::Component,
                                 public juce::TooltipClient
    {
    public:
        explicit RowsContentComponent(SoundIdSuiteList& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        juce::String getTooltip() override;
    private:
        SoundIdSuiteList& owner;
        juce::Point<float> hoveredPos { -1.0f, -1.0f };
    };

    void layoutRows();
    void syncEventHandlerCallbacks();

    suite::SuiteQueueModelManager modelManager;
    std::unique_ptr<suite::SuiteListEventHandler> eventHandler;

    bool isSessionRunning { false };
    bool isCollapsed { false };
    bool isCompactView { false };

    juce::TextButton btnRunSession { "RUN SESSION TESTS" };
    ReMeasureButton btnRerunSelected;
    juce::TextButton btnAddStandard { "+ Add Standard Test" };
    juce::TextButton btnAddCustom { "+ Add Custom Test" };
    juce::TextButton btnViewMode { "Compact View" };
    juce::TextButton btnClear { "Clear All" };
    juce::TextButton btnToggleCollapse;

    juce::Viewport viewport;
    RowsContentComponent rowsContent { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSuiteList)
};

} // namespace abdaudiolab::gui
