#pragma once

#include "SoundIdTheme.h"
#include "TestConfigModal.h"
#include "../audio/LabStimulusGenerator.h"
#include <juce_gui_basics/juce_gui_basics.h>
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
};

/**
 * @brief Interactive Batch Test Plan Queue with single-line rows, vertical scrolling Viewport,
 * per-test bypass (RUN / SKIP), and run triggers.
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
    void toggleTestSkipped(int index);
    void toggleTestExpanded(int index);
    void clearQueue();

    [[nodiscard]] const std::vector<QueueItem>& getQueue() const noexcept { return queue; }
    [[nodiscard]] int getQueueSize() const noexcept { return static_cast<int>(queue.size()); }
    [[nodiscard]] int getTotalPointCount() const noexcept
    {
        int total = 0;
        for (const auto& item : queue)
            total += item.totalPoints;
        return std::max(1, total);
    }
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
    std::function<void()> onAddStandardClicked;
    std::function<void()> onAddCustomClicked;
    std::function<void(bool start)> onToggleSessionRunClicked;
    std::function<void(const juce::String& message)> onDuplicateWarning;
    std::function<void()> onToggleCollapse;

    void setCollapsed(bool collapsed);
    void setChevronGlyph(const juce::String& glyph);
    [[nodiscard]] bool getIsCollapsed() const noexcept { return isCollapsed; }
    void updateTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class RowsContentComponent : public juce::Component,
                                 public juce::TooltipClient
    {
    public:
        RowsContentComponent(SoundIdSuiteList& ownerRef) : owner(ownerRef) {}
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

    std::vector<QueueItem> queue;
    bool isSessionRunning { false };
    bool isCollapsed { false };
    bool isCompactView { false };

    juce::TextButton btnRunSession { "RUN SESSION TESTS" };
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
