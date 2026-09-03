#include "SoundIdSuiteList.h"

namespace abdaudiolab::gui
{

SoundIdSuiteList::SoundIdSuiteList()
{
    btnAddStandard.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnAddStandard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAddStandard.onClick = [this] { if (onAddStandardClicked) onAddStandardClicked(); };
    addAndMakeVisible(btnAddStandard);

    btnAddCustom.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnAddCustom.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAddCustom.onClick = [this] { if (onAddCustomClicked) onAddCustomClicked(); };
    addAndMakeVisible(btnAddCustom);

    btnRunSession.setButtonText("RUN SESSION TESTS");
    btnRunSession.setTooltip("Start executing all active tests in the plan");
    btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnRunSession.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnRunSession.onClick = [this] {
        if (onToggleSessionRunClicked)
            onToggleSessionRunClicked(!isSessionRunning);
    };
    addAndMakeVisible(btnRunSession);

    btnClear.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClear.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    btnClear.onClick = [this] { clearQueue(); };
    addAndMakeVisible(btnClear);

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
    defaultItem.badgeColor = juce::Colour(0xff10b981);
    defaultItem.title = "Resonant Low-Pass Filter (VCF)";
    defaultItem.description = "Farina Log Sweep, 8 x 4 = 32 points (~43s)";
    defaultItem.stimulusType = audio::StimulusType::LogFarinaSweep;
    defaultItem.burstDurationSec = 1.0f;
    defaultItem.totalPoints = 32;
    defaultItem.status = QueueItemStatus::Queued;
    queue.push_back(defaultItem);

    layoutRows();
}

void SoundIdSuiteList::ensureNoiseBaselineTestPinned()
{
    if (queue.empty() || !queue[0].isPinned)
    {
        QueueItem noiseItem;
        noiseItem.id = "system:noise_floor_baseline";
        noiseItem.hwId = "system";
        noiseItem.funcId = "noise_baseline";
        noiseItem.badgeText = "NOI";
        noiseItem.badgeColor = juce::Colour(0xff64748b);
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

void SoundIdSuiteList::setSessionRunning(bool isRunning)
{
    isSessionRunning = isRunning;
    if (isSessionRunning)
    {
        btnRunSession.setButtonText("STOP SESSION");
        btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed);
    }
    else
    {
        btnRunSession.setButtonText("RUN SESSION TESTS");
        btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    }
    repaint();
}

bool SoundIdSuiteList::isTestInQueue(const juce::String& signature) const noexcept
{
    for (const auto& item : queue)
    {
        if (item.id == signature)
            return true;
    }
    return false;
}

void SoundIdSuiteList::addTestToQueue(const QueueItem& item)
{
    if (isTestInQueue(item.id))
    {
        if (onDuplicateWarning)
            onDuplicateWarning("Test '" + item.title + "' is already in the session plan.");
        return;
    }

    queue.push_back(item);
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::updateTestInQueue(int index, const QueueItem& item)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)] = item;
        if (queue[static_cast<size_t>(index)].status == QueueItemStatus::Completed)
        {
            queue[static_cast<size_t>(index)].status = QueueItemStatus::Invalidated;
        }
        layoutRows();
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::duplicateTestInQueue(int index)
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
        layoutRows();
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::removeTestFromQueue(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        if (queue[static_cast<size_t>(index)].isPinned) return;
        
        if (onRequestDeleteTest)
        {
            onRequestDeleteTest(index, queue[static_cast<size_t>(index)]);
            return;
        }
        removeTestDirectly(index);
    }
}

void SoundIdSuiteList::removeTestDirectly(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        if (queue[static_cast<size_t>(index)].isPinned) return;
        queue.erase(queue.begin() + index);
        layoutRows();
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::invalidateTest(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].status = QueueItemStatus::Invalidated;
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::moveTestUp(int index)
{
    if (index > 1 && index < static_cast<int>(queue.size()))
    {
        std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index - 1)]);
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::moveTestDown(int index)
{
    if (index >= 1 && index < static_cast<int>(queue.size()) - 1)
    {
        std::swap(queue[static_cast<size_t>(index)], queue[static_cast<size_t>(index + 1)]);
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::toggleTestExpanded(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].isExpanded = !queue[static_cast<size_t>(index)].isExpanded;
        layoutRows();
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::toggleTestSkipped(int index)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].isSkipped = !queue[static_cast<size_t>(index)].isSkipped;
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::clearQueue()
{
    queue.clear();
    ensureNoiseBaselineTestPinned();
    layoutRows();
    rowsContent.repaint();
}

void SoundIdSuiteList::updateItemStatus(int index, QueueItemStatus status, int currentPoint)
{
    if (index >= 0 && index < static_cast<int>(queue.size()))
    {
        queue[static_cast<size_t>(index)].status = status;
        queue[static_cast<size_t>(index)].currentRunningPoint = currentPoint;
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::resetAllStatuses()
{
    for (auto& item : queue)
    {
        item.status = QueueItemStatus::Queued;
        item.currentRunningPoint = 0;
    }
    rowsContent.repaint();
}

void SoundIdSuiteList::layoutRows()
{
    int totalH = 0;
    for (const auto& item : queue)
    {
        totalH += 34; // Main row height
        if (item.isExpanded)
        {
            int shownSubRows = std::min(item.totalPoints, 16);
            totalH += shownSubRows * 24 + 6;
        }
    }
    rowsContent.setSize(viewport.getWidth() > 0 ? viewport.getWidth() - 14 : 700, std::max(totalH, 100));
}

void SoundIdSuiteList::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(34).reduced(8, 4);

    btnAddStandard.setBounds(header.removeFromLeft(150));
    header.removeFromLeft(8);
    btnAddCustom.setBounds(header.removeFromLeft(145));
    header.removeFromLeft(16);

    btnRunSession.setBounds(header.removeFromLeft(175));

    btnClear.setBounds(header.removeFromRight(75));

    bounds.removeFromTop(2);
    viewport.setBounds(bounds.reduced(6, 4));
    layoutRows();
}

void SoundIdSuiteList::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);
}

void SoundIdSuiteList::RowsContentComponent::paint(juce::Graphics& g)
{
    float currentY = 0.0f;

    for (size_t i = 0; i < owner.queue.size(); ++i)
    {
        const auto& item = owner.queue[i];
        auto rowRect = juce::Rectangle<float>(0.0f, currentY, static_cast<float>(getWidth()), 34.0f).reduced(0.0f, 1.0f);
        currentY += 34.0f;

        // Row background
        if (item.status == QueueItemStatus::Running)
        {
            g.setColour(juce::Colour(0xffecfdf5));
            g.fillRoundedRectangle(rowRect, 4.0f);
            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.6f));
            g.drawRoundedRectangle(rowRect.reduced(0.5f), 4.0f, 1.0f);
        }
        else if (item.isSkipped)
        {
            g.setColour(juce::Colour(0xfff1f5f9));
            g.fillRoundedRectangle(rowRect, 4.0f);
        }
        else
        {
            g.setColour(i % 2 == 0 ? juce::Colour(0xfff8fafc) : juce::Colours::white);
            g.fillRoundedRectangle(rowRect, 4.0f);
        }

        auto area = rowRect.reduced(6.0f, 2.0f);

        // 1. Order controls [^] [v]
        auto reorderArea = area.removeFromLeft(36.0f);
        if (!item.isPinned)
        {
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            auto upRect = reorderArea.removeFromLeft(16.0f);
            g.setColour(i > 1 ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted.withAlpha(0.3f));
            g.drawText("^", upRect, juce::Justification::centred, false);

            auto downRect = reorderArea.removeFromLeft(16.0f);
            g.setColour(i < owner.queue.size() - 1 ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted.withAlpha(0.3f));
            g.drawText("v", downRect, juce::Justification::centred, false);
        }
        else
        {
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textMuted);
            g.drawText("PIN", reorderArea, juce::Justification::centred, false);
        }
        area.removeFromLeft(4.0f);

        // 2. Badge (FLT, ENV, etc.)
        auto badgeRect = area.removeFromLeft(38.0f).withSizeKeepingCentre(36.0f, 20.0f);
        g.setColour(item.isSkipped ? juce::Colour(0xffcbd5e1) : item.badgeColor.withAlpha(0.18f));
        g.fillRoundedRectangle(badgeRect, 3.0f);
        g.setColour(item.isSkipped ? SoundIdTheme::textMuted : item.badgeColor);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(item.badgeText, badgeRect, juce::Justification::centred, false);

        area.removeFromLeft(8.0f);

        // 3. Actions & Status on Right
        auto rightActions = area.removeFromRight(290.0f);

        // Standard CRUD Buttons: [Edit] [Copy] [Del]
        if (!item.isPinned)
        {
            auto delBtn = rightActions.removeFromRight(36.0f).withSizeKeepingCentre(32.0f, 20.0f);
            g.setColour(juce::Colour(0xfffee2e2));
            g.fillRoundedRectangle(delBtn, 3.0f);
            g.setColour(SoundIdTheme::accentRed);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("Del", delBtn, juce::Justification::centred, false);

            auto copyBtn = rightActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 20.0f);
            g.setColour(juce::Colour(0xfff1f5f9));
            g.fillRoundedRectangle(copyBtn, 3.0f);
            g.setColour(SoundIdTheme::textSecondary);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("Copy", copyBtn, juce::Justification::centred, false);

            auto editBtn = rightActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 20.0f);
            g.setColour(juce::Colour(0xfff1f5f9));
            g.fillRoundedRectangle(editBtn, 3.0f);
            g.setColour(SoundIdTheme::textPrimary);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("Edit", editBtn, juce::Justification::centred, false);
        }
        else
        {
            rightActions.removeFromRight(112.0f);
        }

        rightActions.removeFromRight(6.0f);

        // State Pill & Retry Actions
        if (item.status == QueueItemStatus::Incomplete)
        {
            auto contBtn = rightActions.removeFromRight(58.0f).withSizeKeepingCentre(54.0f, 20.0f);
            g.setColour(SoundIdTheme::accentAmber.withAlpha(0.2f));
            g.fillRoundedRectangle(contBtn, 3.0f);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentAmber);
            g.drawText("RESUME", contBtn, juce::Justification::centred, false);

            auto resetBtn = rightActions.removeFromRight(50.0f).withSizeKeepingCentre(46.0f, 20.0f);
            g.setColour(juce::Colour(0xffe2e8f0));
            g.fillRoundedRectangle(resetBtn, 3.0f);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("RESET", resetBtn, juce::Justification::centred, false);
        }
        else if (item.status == QueueItemStatus::Invalidated)
        {
            auto rerunBtn = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
            g.setColour(SoundIdTheme::accentRed.withAlpha(0.2f));
            g.fillRoundedRectangle(rerunBtn, 3.0f);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText("RE-RUN", rerunBtn, juce::Justification::centred, false);
        }

        // Status Text
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        if (item.isSkipped)
        {
            g.setColour(SoundIdTheme::textMuted);
            g.drawText("BYPASSED", rightActions.removeFromRight(70.0f), juce::Justification::centredRight, true);
        }
        else if (item.status == QueueItemStatus::Running)
        {
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText("RUNNING " + juce::String(item.currentRunningPoint) + "/" + juce::String(item.totalPoints), rightActions.removeFromRight(90.0f), juce::Justification::centredRight, true);
        }
        else if (item.status == QueueItemStatus::Completed)
        {
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText("DONE [OK]", rightActions.removeFromRight(70.0f), juce::Justification::centredRight, true);
        }
        else if (item.status == QueueItemStatus::Invalidated)
        {
            g.setColour(SoundIdTheme::accentRed);
            g.drawText("INVALID", rightActions.removeFromRight(65.0f), juce::Justification::centredRight, true);
        }
        else
        {
            g.setColour(SoundIdTheme::textMuted);
            g.drawText("QUEUED", rightActions.removeFromRight(60.0f), juce::Justification::centredRight, true);
        }

        rightActions.removeFromRight(6.0f);

        // ACTIVE / BYPASS Toggle Pill
        auto bypassPill = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
        g.setColour(item.isSkipped ? juce::Colour(0xffe2e8f0) : juce::Colour(0xffdcfce7));
        g.fillRoundedRectangle(bypassPill, 3.0f);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(item.isSkipped ? SoundIdTheme::textMuted : SoundIdTheme::accentGreen);
        g.drawText(item.isSkipped ? "BYPASS" : "ACTIVE", bypassPill, juce::Justification::centred, false);

        // Expand Arrow Button
        auto expandBtn = area.removeFromLeft(16.0f);
        g.setColour(SoundIdTheme::textSecondary);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(item.isExpanded ? juce::String::fromUTF8(u8"▼") : juce::String::fromUTF8(u8"▶"), expandBtn, juce::Justification::centred, false);
        area.removeFromLeft(4.0f);

        // Title and Single-line info
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(item.isSkipped ? SoundIdTheme::textMuted : SoundIdTheme::textPrimary);
        
        juce::String fullText = item.title;
        if (item.description.isNotEmpty())
            fullText += "  --  " + item.description;

        g.drawText(fullText, area, juce::Justification::centredLeft, true);

        // Sub-rows when expanded
        if (item.isExpanded)
        {
            int shownSubRows = std::min(item.totalPoints, 16);
            for (int pIdx = 0; pIdx < shownSubRows; ++pIdx)
            {
                auto subRect = juce::Rectangle<float>(32.0f, currentY + 1.0f, static_cast<float>(getWidth()) - 36.0f, 22.0f);
                currentY += 24.0f;

                g.setColour(juce::Colour(0xfff8fafc));
                g.fillRoundedRectangle(subRect, 3.0f);
                g.setColour(SoundIdTheme::borderSubtle);
                g.drawRoundedRectangle(subRect.reduced(0.5f), 3.0f, 1.0f);

                auto subArea = subRect.reduced(6.0f, 2.0f);
                g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
                g.setColour(SoundIdTheme::textPrimary);
                
                float stepPct = (item.totalPoints > 1) ? (static_cast<float>(pIdx) / static_cast<float>(item.totalPoints - 1) * 100.0f) : 0.0f;
                juce::String stepLabel = "Point #" + juce::String(pIdx + 1) + " / " + juce::String(item.totalPoints) + " (" + juce::String(stepPct, 1) + "% Pos)";
                g.drawText(stepLabel, subArea.removeFromLeft(220.0f), juce::Justification::centredLeft, true);

                auto subActions = subArea.removeFromRight(150.0f);

                auto subDel = subActions.removeFromRight(36.0f).withSizeKeepingCentre(32.0f, 18.0f);
                g.setColour(juce::Colour(0xfffee2e2));
                g.fillRoundedRectangle(subDel, 2.0f);
                g.setColour(SoundIdTheme::accentRed);
                g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText("Del", subDel, juce::Justification::centred, false);

                auto subClear = subActions.removeFromRight(42.0f).withSizeKeepingCentre(38.0f, 18.0f);
                g.setColour(juce::Colour(0xfff1f5f9));
                g.fillRoundedRectangle(subClear, 2.0f);
                g.setColour(SoundIdTheme::textSecondary);
                g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText("Clear", subClear, juce::Justification::centred, false);

                auto subView = subActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 18.0f);
                g.setColour(juce::Colour(0xffdcfce7));
                g.fillRoundedRectangle(subView, 2.0f);
                g.setColour(SoundIdTheme::accentGreen);
                g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText("View", subView, juce::Justification::centred, false);
            }
            currentY += 6.0f;
        }
    }
}

void SoundIdSuiteList::RowsContentComponent::mouseDown(const juce::MouseEvent& e)
{
    float currentY = 0.0f;

    for (size_t i = 0; i < owner.queue.size(); ++i)
    {
        auto rowRect = juce::Rectangle<float>(0.0f, currentY, static_cast<float>(getWidth()), 34.0f).reduced(0.0f, 1.0f);
        currentY += 34.0f;

        if (rowRect.contains(e.position))
        {
            auto area = rowRect.reduced(6.0f, 2.0f);

            // Reorder buttons
            if (!owner.queue[i].isPinned)
            {
                auto reorderArea = area.removeFromLeft(36.0f);
                auto upRect = reorderArea.removeFromLeft(16.0f);
                if (upRect.contains(e.position))
                {
                    owner.moveTestUp(static_cast<int>(i));
                    return;
                }
                auto downRect = reorderArea.removeFromLeft(16.0f);
                if (downRect.contains(e.position))
                {
                    owner.moveTestDown(static_cast<int>(i));
                    return;
                }
            }
            else
            {
                area.removeFromLeft(36.0f);
            }

            area.removeFromLeft(46.0f); // badge

            auto rightActions = area.removeFromRight(290.0f);

            if (!owner.queue[i].isPinned)
            {
                auto delBtn = rightActions.removeFromRight(36.0f).withSizeKeepingCentre(32.0f, 20.0f);
                if (delBtn.contains(e.position))
                {
                    owner.removeTestFromQueue(static_cast<int>(i));
                    return;
                }

                auto copyBtn = rightActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 20.0f);
                if (copyBtn.contains(e.position))
                {
                    owner.duplicateTestInQueue(static_cast<int>(i));
                    return;
                }

                auto editBtn = rightActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 20.0f);
                if (editBtn.contains(e.position))
                {
                    if (owner.onEditTestClicked)
                        owner.onEditTestClicked(static_cast<int>(i), owner.queue[i]);
                    return;
                }
            }
            else
            {
                rightActions.removeFromRight(112.0f);
            }

            rightActions.removeFromRight(6.0f);

            if (owner.queue[i].status == QueueItemStatus::Incomplete)
            {
                auto contBtn = rightActions.removeFromRight(58.0f).withSizeKeepingCentre(54.0f, 20.0f);
                if (contBtn.contains(e.position))
                {
                    if (owner.onContinueTestClicked) owner.onContinueTestClicked(static_cast<int>(i));
                    return;
                }
                auto resetBtn = rightActions.removeFromRight(50.0f).withSizeKeepingCentre(46.0f, 20.0f);
                if (resetBtn.contains(e.position))
                {
                    if (owner.onRestartTestClicked) owner.onRestartTestClicked(static_cast<int>(i));
                    return;
                }
            }
            else if (owner.queue[i].status == QueueItemStatus::Invalidated)
            {
                auto rerunBtn = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
                if (rerunBtn.contains(e.position))
                {
                    if (owner.onRestartTestClicked) owner.onRestartTestClicked(static_cast<int>(i));
                    return;
                }
            }

            // Bypass pill
            auto bypassPill = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
            if (bypassPill.contains(e.position))
            {
                owner.toggleTestSkipped(static_cast<int>(i));
                return;
            }

            // Expand arrow or title click toggles expansion
            owner.toggleTestExpanded(static_cast<int>(i));
            return;
        }

        // Sub-rows
        if (owner.queue[i].isExpanded)
        {
            int shownSubRows = std::min(owner.queue[i].totalPoints, 16);
            for (int pIdx = 0; pIdx < shownSubRows; ++pIdx)
            {
                auto subRect = juce::Rectangle<float>(32.0f, currentY + 1.0f, static_cast<float>(getWidth()) - 36.0f, 22.0f);
                currentY += 24.0f;

                if (subRect.contains(e.position))
                {
                    auto subArea = subRect.reduced(6.0f, 2.0f);
                    subArea.removeFromLeft(220.0f);
                    auto subActions = subArea.removeFromRight(150.0f);

                    auto subDel = subActions.removeFromRight(36.0f).withSizeKeepingCentre(32.0f, 18.0f);
                    if (subDel.contains(e.position))
                    {
                        if (owner.onDeletePointClicked) owner.onDeletePointClicked(static_cast<int>(i), pIdx);
                        return;
                    }

                    auto subClear = subActions.removeFromRight(42.0f).withSizeKeepingCentre(38.0f, 18.0f);
                    if (subClear.contains(e.position))
                    {
                        if (owner.onClearPointClicked) owner.onClearPointClicked(static_cast<int>(i), pIdx);
                        return;
                    }

                    auto subView = subActions.removeFromRight(38.0f).withSizeKeepingCentre(34.0f, 18.0f);
                    if (subView.contains(e.position))
                    {
                        if (owner.onSelectPointClicked) owner.onSelectPointClicked(static_cast<int>(i), pIdx);
                        return;
                    }
                }
            }
            currentY += 6.0f;
        }
    }
}

} // namespace abdaudiolab::gui
