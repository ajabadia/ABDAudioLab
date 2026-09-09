/**
 * @file SoundIdSuiteList.cpp
 * @brief Main UI component for the Test Suite Queue. Orchestrates buttons, Viewport,
 *        and delegates data to SuiteQueueModelManager and interaction to SuiteListEventHandler.
 * @author ABDSynths
 * @date 2026
 */

#include "SoundIdSuiteList.h"
#include "suite/SuiteRowLayout.h"
#include "suite/SuiteRowRenderer.h"

namespace abdaudiolab::gui
{

SoundIdSuiteList::SoundIdSuiteList()
{
    suite::SuiteListEventHandler::Callbacks cb;
    eventHandler = std::make_unique<suite::SuiteListEventHandler>(modelManager, cb);
    syncEventHandlerCallbacks();

    btnAddStandard.setTooltip("Add Standard Test - Automatically configure optimal sweep matrix for selected hardware module");
    btnAddStandard.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAddStandard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAddStandard.onClick = [this] { if (onAddStandardClicked) onAddStandardClicked(); };
    addAndMakeVisible(btnAddStandard);

    btnAddCustom.setTooltip("Add Custom Test - Define custom sweep steps, stimulus duration, capture mode and parameter ranges");
    btnAddCustom.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAddCustom.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAddCustom.onClick = [this] { if (onAddCustomClicked) onAddCustomClicked(); };
    addAndMakeVisible(btnAddCustom);

    btnViewMode.setButtonText(isCompactView ? "Detailed Points" : "Compact View");
    btnViewMode.setTooltip("Toggle View Mode - Switch between single-bar compact summary and full detailed point inspection");
    btnViewMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnViewMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnViewMode.onClick = [this] {
        isCompactView = !isCompactView;
        btnViewMode.setButtonText(isCompactView ? "Detailed Points" : "Compact View");
        layoutRows();
        rowsContent.repaint();
    };
    addAndMakeVisible(btnViewMode);

    btnRunSession.setButtonText(juce::String::fromUTF8(u8"▶  RUN SESSION TESTS"));
    btnRunSession.setTooltip("Run Session Tests - Execute all queued measurement tests sequentially (or stop running session)");
    btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnRunSession.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnRunSession.onClick = [this] {
        if (onToggleSessionRunClicked)
            onToggleSessionRunClicked(!isSessionRunning);
    };
    addAndMakeVisible(btnRunSession);

    btnRerunSelected.setButtonText(juce::String::fromUTF8(u8"⚡ Re-Measure (0)"));
    btnRerunSelected.setTooltip("Re-Measure Selected Points - Launch profiling only for selected or invalidated points and patch session");
    btnRerunSelected.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnRerunSelected.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    btnRerunSelected.setEnabled(false);
    btnRerunSelected.onClick = [this] {
        auto sel = getSelectedPoints();
        if (!sel.empty() && onRerunSelectedClicked)
            onRerunSelectedClicked(sel);
    };
    addAndMakeVisible(btnRerunSelected);

    btnClear.setTooltip("Clear Queue - Remove all completed or queued tests (preserves pinned baseline)");
    btnClear.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClear.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    btnClear.onClick = [this] { clearQueue(); };
    addAndMakeVisible(btnClear);

    btnToggleCollapse.setButtonText(juce::String::fromUTF8(u8"▲"));
    btnToggleCollapse.setTooltip("Maximize / Restore Test Queue - Expand test list up to fill area or restore balanced split");
    btnToggleCollapse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleCollapse.onClick = [this] { if (onToggleCollapse) onToggleCollapse(); };
    addAndMakeVisible(btnToggleCollapse);

    viewport.setViewedComponent(&rowsContent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    ensureNoiseBaselineTestPinned();

    // Default VCF Sweep Test
    QueueItem defaultItem;
    defaultItem.id = "mock_va_synth:filter:vcf_sweep";
    defaultItem.hwId = "mock_va_synth";
    defaultItem.funcId = "filter";
    defaultItem.badgeText = "FLT";
    defaultItem.badgeColor = SoundIdTheme::accentGreen;
    defaultItem.title = "Resonant Low-Pass Filter (VCF)";
    defaultItem.description = "Farina Log Sweep, 8 x 4 = 32 points (~43s)";
    defaultItem.stimulusType = audio::StimulusType::LogFarinaSweep;
    defaultItem.burstDurationSec = 1.0f;
    defaultItem.totalPoints = 32;
    defaultItem.status = QueueItemStatus::Queued;
    modelManager.addTest(defaultItem);

    layoutRows();
}

void SoundIdSuiteList::syncEventHandlerCallbacks()
{
    if (!eventHandler) return;

    suite::SuiteListEventHandler::Callbacks cb;
    cb.onEditTestClicked = [this](int idx, const QueueItem& item) {
        if (onEditTestClicked) onEditTestClicked(idx, item);
    };
    cb.onRequestDeleteTest = [this](int idx, const QueueItem& item) {
        if (onRequestDeleteTest) onRequestDeleteTest(idx, item);
        else removeTestDirectly(idx);
    };
    cb.onContinueTestClicked = [this](int idx) {
        if (onContinueTestClicked) onContinueTestClicked(idx);
    };
    cb.onRestartTestClicked = [this](int idx) {
        if (onRestartTestClicked) onRestartTestClicked(idx);
    };
    cb.onSelectPointClicked = [this](int qIdx, int pIdx) {
        if (onSelectPointClicked) onSelectPointClicked(qIdx, pIdx);
    };
    cb.onClearPointClicked = [this](int qIdx, int pIdx) {
        if (onClearPointClicked) onClearPointClicked(qIdx, pIdx);
    };
    cb.onDeletePointClicked = [this](int qIdx, int pIdx) {
        if (onDeletePointClicked) onDeletePointClicked(qIdx, pIdx);
    };
    cb.onRerunSelectedClicked = [this](const std::vector<std::pair<int, int>>& pts) {
        if (onRerunSelectedClicked) onRerunSelectedClicked(pts);
    };
    cb.onQueueChanged = [this]() {
        updateSelectionButton();
        layoutRows();
        rowsContent.repaint();
    };

    eventHandler->setCallbacks(std::move(cb));
}

void SoundIdSuiteList::ensureNoiseBaselineTestPinned()
{
    modelManager.ensureNoiseBaselineTestPinned();
}

void SoundIdSuiteList::setSessionRunning(bool isRunning)
{
    isSessionRunning = isRunning;
    if (isSessionRunning)
    {
        btnRunSession.setButtonText(juce::String::fromUTF8(u8"■  STOP SESSION"));
        btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed);
    }
    else
    {
        btnRunSession.setButtonText(juce::String::fromUTF8(u8"▶  RUN SESSION TESTS"));
        btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    }
    repaint();
}

bool SoundIdSuiteList::isTestInQueue(const juce::String& signature) const noexcept
{
    return modelManager.isTestInQueue(signature);
}

void SoundIdSuiteList::addTestToQueue(const QueueItem& item)
{
    if (modelManager.isTestInQueue(item.id))
    {
        if (onDuplicateWarning)
            onDuplicateWarning("Test '" + item.title + "' is already in the session plan.");
        return;
    }

    modelManager.addTest(item);
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::updateTestInQueue(int index, const QueueItem& item)
{
    modelManager.updateTest(index, item);
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::duplicateTestInQueue(int index)
{
    modelManager.duplicateTest(index);
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::removeTestFromQueue(int index)
{
    if (index >= 0 && index < modelManager.getQueueSize())
    {
        const auto& item = modelManager.getQueue()[static_cast<size_t>(index)];
        if (item.isPinned) return;

        if (onRequestDeleteTest)
        {
            onRequestDeleteTest(index, item);
            return;
        }
        removeTestDirectly(index);
    }
}

void SoundIdSuiteList::removeTestDirectly(int index)
{
    if (modelManager.removeTestDirectly(index))
    {
        layoutRows();
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::invalidateTest(int index)
{
    modelManager.invalidateTest(index);
    rowsContent.repaint();
}

void SoundIdSuiteList::moveTestUp(int index)
{
    modelManager.moveUp(index);
    rowsContent.repaint();
}

void SoundIdSuiteList::moveTestDown(int index)
{
    modelManager.moveDown(index);
    rowsContent.repaint();
}

void SoundIdSuiteList::toggleTestExpanded(int index)
{
    modelManager.toggleExpanded(index);
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::toggleTestSkipped(int index)
{
    modelManager.toggleSkipped(index);
    rowsContent.repaint();
}

void SoundIdSuiteList::clearQueue()
{
    modelManager.clear();
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::updateTheme()
{
    btnAddStandard.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAddStandard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAddCustom.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAddCustom.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnViewMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnViewMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnRunSession.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnClear.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClear.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    rowsContent.repaint();
    repaint();
}

void SoundIdSuiteList::updateItemStatus(int index, QueueItemStatus status, int currentPoint)
{
    modelManager.updateItemStatus(index, status, currentPoint);
    rowsContent.repaint();
}

void SoundIdSuiteList::resetAllStatuses()
{
    modelManager.resetAllStatuses();
    rowsContent.repaint();
}

void SoundIdSuiteList::setPointStatus(int queueIndex, int pointIndex, PointStatus status)
{
    modelManager.setPointStatus(queueIndex, pointIndex, status);
    rowsContent.repaint();
}

PointStatus SoundIdSuiteList::getPointStatus(int queueIndex, int pointIndex) const
{
    return modelManager.getPointStatus(queueIndex, pointIndex);
}

void SoundIdSuiteList::resetPointStatuses(int queueIndex)
{
    modelManager.resetPointStatuses(queueIndex);
    rowsContent.repaint();
}

void SoundIdSuiteList::setPointSelected(int queueIndex, int pointIndex, bool isSelected)
{
    modelManager.setPointSelected(queueIndex, pointIndex, isSelected);
    updateSelectionButton();
    rowsContent.repaint();
}

bool SoundIdSuiteList::isPointSelected(int queueIndex, int pointIndex) const
{
    return modelManager.isPointSelected(queueIndex, pointIndex);
}

void SoundIdSuiteList::togglePointSelection(int queueIndex, int pointIndex)
{
    modelManager.togglePointSelection(queueIndex, pointIndex);
    updateSelectionButton();
    rowsContent.repaint();
}

void SoundIdSuiteList::selectPointRange(int queueIndex, int startPointIndex, int endPointIndex, bool isSelected)
{
    modelManager.selectPointRange(queueIndex, startPointIndex, endPointIndex, isSelected);
    updateSelectionButton();
    rowsContent.repaint();
}

void SoundIdSuiteList::selectAllPointsInTest(int queueIndex, bool isSelected)
{
    modelManager.selectAllPointsInTest(queueIndex, isSelected);
    updateSelectionButton();
    rowsContent.repaint();
}

void SoundIdSuiteList::selectAllInvalidatedPoints()
{
    modelManager.selectAllInvalidatedPoints();
    updateSelectionButton();
    rowsContent.repaint();
}

void SoundIdSuiteList::clearAllSelections()
{
    modelManager.clearAllSelections();
    updateSelectionButton();
    rowsContent.repaint();
}

std::vector<std::pair<int, int>> SoundIdSuiteList::getSelectedPoints() const
{
    return modelManager.getSelectedPoints();
}

int SoundIdSuiteList::getSelectedPointCount() const
{
    return modelManager.getSelectedPointCount();
}

void SoundIdSuiteList::invalidateSelectedPoints()
{
    modelManager.invalidateSelectedPoints();
    rowsContent.repaint();
}

void SoundIdSuiteList::updateSelectionButton()
{
    int count = modelManager.getSelectedPointCount();
    if (count > 0)
    {
        btnRerunSelected.setEnabled(true);
        btnRerunSelected.setButtonText(juce::String::fromUTF8(u8"⚡ Re-Measure (") + juce::String(count) + ")");
        btnRerunSelected.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber.withAlpha(0.25f));
        btnRerunSelected.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    }
    else
    {
        btnRerunSelected.setEnabled(false);
        btnRerunSelected.setButtonText(juce::String::fromUTF8(u8"⚡ Re-Measure (0)"));
        btnRerunSelected.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btnRerunSelected.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    }
}

void SoundIdSuiteList::showPointContextMenu(int queueIndex, int pointIndex)
{
    syncEventHandlerCallbacks();
    if (eventHandler)
        eventHandler->showPointContextMenu(queueIndex, pointIndex);
}

void SoundIdSuiteList::layoutRows()
{
    float totalH = 0.0f;
    const auto& queue = modelManager.getQueue();

    for (const auto& item : queue)
    {
        totalH += 34.0f;
        if (item.isExpanded)
        {
            totalH += 28.0f;
            if (!isCompactView)
            {
                totalH += static_cast<float>(item.totalPoints) * 24.0f + 6.0f;
            }
        }
    }

    int viewW = viewport.getViewWidth();
    rowsContent.setSize(std::max(viewW, 400), static_cast<int>(totalH));
}

void SoundIdSuiteList::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);
}

void SoundIdSuiteList::resized()
{
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop(36);

    btnToggleCollapse.setBounds(topBar.removeFromLeft(36).reduced(6, 6));

    if (isCollapsed)
    {
        viewport.setVisible(false);
        btnAddStandard.setVisible(false);
        btnAddCustom.setVisible(false);
        btnViewMode.setVisible(false);
        btnRerunSelected.setVisible(false);
        btnClear.setVisible(false);
        btnRunSession.setVisible(false);
        return;
    }

    viewport.setVisible(true);
    btnAddStandard.setVisible(true);
    btnAddCustom.setVisible(true);
    btnViewMode.setVisible(true);
    btnRerunSelected.setVisible(true);
    btnClear.setVisible(true);
    btnRunSession.setVisible(true);

    topBar.removeFromLeft(4);
    btnAddStandard.setBounds(topBar.removeFromLeft(130).reduced(2, 4));
    topBar.removeFromLeft(6);
    btnAddCustom.setBounds(topBar.removeFromLeft(120).reduced(2, 4));
    topBar.removeFromLeft(6);
    btnViewMode.setBounds(topBar.removeFromLeft(110).reduced(2, 4));
    topBar.removeFromLeft(6);
    btnRerunSelected.setBounds(topBar.removeFromLeft(140).reduced(2, 4));

    btnRunSession.setBounds(topBar.removeFromRight(180).reduced(4, 3));
    topBar.removeFromRight(6);
    btnClear.setBounds(topBar.removeFromRight(70).reduced(2, 4));

    viewport.setBounds(bounds);
    layoutRows();
}

void SoundIdSuiteList::setCollapsed(bool collapsed)
{
    isCollapsed = collapsed;
    resized();
}

void SoundIdSuiteList::setChevronGlyph(const juce::String& glyph)
{
    btnToggleCollapse.setButtonText(glyph);
}

// ---------------- RowsContentComponent Implementation ----------------

void SoundIdSuiteList::RowsContentComponent::paint(juce::Graphics& g)
{
    float currentY = 0.0f;
    float w = static_cast<float>(getWidth());
    const auto& queue = owner.modelManager.getQueue();

    for (size_t i = 0; i < queue.size(); ++i)
    {
        const auto& item = queue[i];
        auto layout = SuiteMainRowLayout::calculate(currentY, w, item, i, queue.size());
        currentY += 34.0f;

        SuiteRowRenderer::renderMainRow(g, item, layout, i, queue.size(), hoveredPos);

        if (item.isExpanded)
        {
            auto progRect = juce::Rectangle<float>(32.0f, currentY + 2.0f, w - 40.0f, 22.0f);
            currentY += 28.0f;

            SuiteRowRenderer::renderProgressBar(g, item, progRect);

            if (!owner.isCompactView)
            {
                for (int pIdx = 0; pIdx < item.totalPoints; ++pIdx)
                {
                    PointStatus ptStatus = PointStatus::Queued;
                    if (static_cast<size_t>(pIdx) < item.pointStatuses.size())
                        ptStatus = item.pointStatuses[static_cast<size_t>(pIdx)];

                    bool isSelected = false;
                    if (static_cast<size_t>(pIdx) < item.pointSelections.size())
                        isSelected = item.pointSelections[static_cast<size_t>(pIdx)];

                    auto ptLayout = SuitePointRowLayout::calculate(currentY, w, pIdx, item.totalPoints, ptStatus);
                    currentY += 24.0f;

                    SuiteRowRenderer::renderPointRow(g, pIdx, item.totalPoints, ptStatus, ptLayout, hoveredPos, isSelected);
                }
                currentY += 6.0f;
            }
        }
    }
}

void SoundIdSuiteList::RowsContentComponent::mouseMove(const juce::MouseEvent& e)
{
    hoveredPos = e.position;
    repaint();
}

void SoundIdSuiteList::RowsContentComponent::mouseExit(const juce::MouseEvent&)
{
    hoveredPos = { -1.0f, -1.0f };
    repaint();
}

void SoundIdSuiteList::RowsContentComponent::mouseDown(const juce::MouseEvent& e)
{
    owner.syncEventHandlerCallbacks();
    if (owner.eventHandler)
    {
        if (owner.eventHandler->handleMouseDown(e, static_cast<float>(getWidth()), owner.isCompactView))
            repaint();
    }
}

juce::String SoundIdSuiteList::RowsContentComponent::getTooltip()
{
    float currentY = 0.0f;
    float w = static_cast<float>(getWidth());
    const auto& queue = owner.modelManager.getQueue();

    for (size_t i = 0; i < queue.size(); ++i)
    {
        const auto& item = queue[i];
        auto layout = SuiteMainRowLayout::calculate(currentY, w, item, i, queue.size());
        currentY += 34.0f;

        if (layout.rowRect.contains(hoveredPos))
        {
            if (layout.bypassPillRect.contains(hoveredPos))
                return item.isSkipped ? "Test Bypass Active - Click to activate for session" : "Test Active - Click to skip/bypass";
            if (layout.delBtnRect.contains(hoveredPos))
                return "Delete Test - Remove test from session";
            if (layout.copyBtnRect.contains(hoveredPos))
                return "Duplicate Test - Create copy of this test configuration";
            if (layout.editBtnRect.contains(hoveredPos))
                return "Edit Test - Configure sweep parameters, stimulus and ranges";
            if (layout.reorderUpRect.contains(hoveredPos))
                return "Move Up - Advance test execution order";
            if (layout.reorderDownRect.contains(hoveredPos))
                return "Move Down - Delay test execution order";

            return item.title + " (" + item.description + ")";
        }

        if (item.isExpanded)
        {
            currentY += 28.0f;
            if (!owner.isCompactView)
            {
                for (int pIdx = 0; pIdx < item.totalPoints; ++pIdx)
                {
                    PointStatus ptStatus = PointStatus::Queued;
                    if (static_cast<size_t>(pIdx) < item.pointStatuses.size())
                        ptStatus = item.pointStatuses[static_cast<size_t>(pIdx)];

                    auto ptLayout = SuitePointRowLayout::calculate(currentY, w, pIdx, item.totalPoints, ptStatus);
                    currentY += 24.0f;

                    if (ptLayout.rowRect.contains(hoveredPos))
                    {
                        if (ptLayout.viewBtnRect.contains(hoveredPos))
                            return "Inspect Point - View captured curves and harmonic distortion";
                        if (ptLayout.clearBtnRect.contains(hoveredPos))
                            return "Invalidate Point - Mark point to be re-measured";
                        if (ptLayout.delBtnRect.contains(hoveredPos))
                            return "Delete Point - Remove captured audio and invalidate point";
                        if (ptLayout.selectBoxRect.contains(hoveredPos))
                            return "Select Point (Shift+Click for Range, Right-Click for Context Menu)";

                        return "Point #" + juce::String(pIdx + 1) + " of " + juce::String(item.totalPoints);
                    }
                }
                currentY += 6.0f;
            }
        }
    }

    return {};
}

} // namespace abdaudiolab::gui
