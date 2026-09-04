#include "SoundIdSuiteList.h"

namespace {

void drawTrashIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    // Top lid separated from body by 1.5 px (UI-15)
    float lidY = c.y - 4.5f;
    g.drawLine(c.x - 5.5f, lidY, c.x + 5.5f, lidY, 1.2f);
    g.drawLine(c.x - 2.0f, lidY - 1.5f, c.x + 2.0f, lidY - 1.5f, 1.1f);

    // Body with 1.5px gap below lid
    float bodyTopY = lidY + 1.5f;
    juce::Path bin;
    bin.startNewSubPath(c.x - 4.2f, bodyTopY);
    bin.lineTo(c.x - 3.2f, c.y + 5.0f);
    bin.lineTo(c.x + 3.2f, c.y + 5.0f);
    bin.lineTo(c.x + 4.2f, bodyTopY);
    bin.closeSubPath();
    g.strokePath(bin, juce::PathStrokeType(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Two subtle interior vertical lines
    g.drawLine(c.x - 1.4f, bodyTopY + 2.0f, c.x - 1.2f, c.y + 3.5f, 0.9f);
    g.drawLine(c.x + 1.4f, bodyTopY + 2.0f, c.x + 1.2f, c.y + 3.5f, 0.9f);
}

void drawCopyIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col.withAlpha(0.5f));
    g.drawRoundedRectangle(c.x - 5.0f, c.y - 5.0f, 7.0f, 8.5f, 1.0f, 1.0f);
    g.setColour(col);
    g.drawRoundedRectangle(c.x - 2.5f, c.y - 2.5f, 7.0f, 8.5f, 1.0f, 1.1f);
}

void drawEditIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);
    juce::Path p;
    p.startNewSubPath(c.x - 4.5f, c.y + 4.5f);
    p.lineTo(c.x - 4.5f, c.y + 2.0f);
    p.lineTo(c.x + 2.5f, c.y - 5.0f);
    p.lineTo(c.x + 5.0f, c.y - 2.5f);
    p.lineTo(c.x - 2.0f, c.y + 4.5f);
    p.closeSubPath();
    g.strokePath(p, juce::PathStrokeType(1.1f));
    g.drawLine(c.x - 4.5f, c.y + 4.5f, c.x - 3.5f, c.y + 2.0f, 0.9f);
}

void drawEyeIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    // Minimalist Lucide/Feather outline using addCentredArc top and bottom (UI-15)
    juce::Path eyeTop, eyeBot;
    eyeTop.addCentredArc(c.x, c.y + 3.2f, 6.2f, 6.2f, 0.0f, -0.92f, 0.92f, true);
    eyeBot.addCentredArc(c.x, c.y - 3.2f, 6.2f, 6.2f, 0.0f, 3.14159f - 0.92f, 3.14159f + 0.92f, true);

    g.strokePath(eyeTop, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(eyeBot, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2px filled circular pupil in center
    g.fillEllipse(c.x - 1.0f, c.y - 1.0f, 2.0f, 2.0f);
}

void drawClearIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour col)
{
    auto c = bounds.getCentre();
    g.setColour(col);

    // Continuous 270-degree circular arc with sharp triangular arrowhead (UI-15)
    juce::Path arc;
    float r = 4.2f;
    arc.addCentredArc(c.x, c.y, r, r, 0.0f, 0.0f, 4.71239f, true);
    g.strokePath(arc, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float endX = c.x - r;
    float endY = c.y;
    juce::Path arrow;
    arrow.startNewSubPath(endX - 2.5f, endY - 2.0f);
    arrow.lineTo(endX + 0.5f, endY + 1.5f);
    arrow.lineTo(endX + 3.0f, endY - 2.0f);
    arrow.closeSubPath();
    g.fillPath(arrow);
}

} // namespace

namespace abdaudiolab::gui
{

SoundIdSuiteList::SoundIdSuiteList()
{
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

    btnRunSession.setButtonText(juce::String::fromUTF8(u8"\u25b6  RUN SESSION TESTS"));
    btnRunSession.setTooltip("Run Session Tests - Execute all queued measurement tests sequentially (or stop running session)");
    btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnRunSession.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnRunSession.onClick = [this] {
        if (onToggleSessionRunClicked)
            onToggleSessionRunClicked(!isSessionRunning);
    };
    addAndMakeVisible(btnRunSession);

    btnClear.setTooltip("Clear Queue - Remove all completed or queued tests (preserves pinned baseline)");
    btnClear.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClear.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    btnClear.onClick = [this] { clearQueue(); };
    addAndMakeVisible(btnClear);

    btnToggleCollapse.setButtonText(juce::String::fromUTF8(u8"\u25b2")); // ▲ (pointing up to expand upwards)
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
        btnRunSession.setButtonText(juce::String::fromUTF8(u8"\u25a0  STOP SESSION"));
        btnRunSession.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed);
    }
    else
    {
        btnRunSession.setButtonText(juce::String::fromUTF8(u8"\u25b6  RUN SESSION TESTS"));
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
        else if (status == QueueItemStatus::Running)
        {
            for (int p = 0; p < item.totalPoints; ++p)
            {
                if (item.pointStatuses[static_cast<size_t>(p)] == PointStatus::Annulled)
                    continue;

                if (p < currentPoint)
                    item.pointStatuses[static_cast<size_t>(p)] = PointStatus::Completed;
                else if (p == currentPoint)
                    item.pointStatuses[static_cast<size_t>(p)] = PointStatus::Running;
                else
                    item.pointStatuses[static_cast<size_t>(p)] = PointStatus::Queued;
            }
        }
        else if (status == QueueItemStatus::Queued)
        {
            for (auto& pSt : item.pointStatuses)
                pSt = PointStatus::Queued;
        }
        else if (status == QueueItemStatus::Invalidated)
        {
            for (auto& pSt : item.pointStatuses)
            {
                if (pSt != PointStatus::Annulled)
                    pSt = PointStatus::Invalidated;
            }
        }

        rowsContent.repaint();
    }
}

void SoundIdSuiteList::setPointStatus(int queueIndex, int pointIndex, PointStatus status)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        if (item.pointStatuses.size() != static_cast<size_t>(item.totalPoints))
            item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);

        if (pointIndex >= 0 && pointIndex < static_cast<int>(item.pointStatuses.size()))
        {
            item.pointStatuses[static_cast<size_t>(pointIndex)] = status;
            rowsContent.repaint();
        }
    }
}

PointStatus SoundIdSuiteList::getPointStatus(int queueIndex, int pointIndex) const
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        const auto& item = queue[static_cast<size_t>(queueIndex)];
        if (pointIndex >= 0 && pointIndex < static_cast<int>(item.pointStatuses.size()))
            return item.pointStatuses[static_cast<size_t>(pointIndex)];
    }
    return PointStatus::Queued;
}

void SoundIdSuiteList::resetPointStatuses(int queueIndex)
{
    if (queueIndex >= 0 && queueIndex < static_cast<int>(queue.size()))
    {
        auto& item = queue[static_cast<size_t>(queueIndex)];
        item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);
        rowsContent.repaint();
    }
}

void SoundIdSuiteList::resetAllStatuses()
{
    for (auto& item : queue)
    {
        item.status = QueueItemStatus::Queued;
        item.currentRunningPoint = 0;
        item.pointStatuses.assign(static_cast<size_t>(item.totalPoints), PointStatus::Queued);
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
            totalH += 28; // Progress bar row
            if (!isCompactView)
            {
                totalH += item.totalPoints * 24 + 6;
            }
        }
    }
    rowsContent.setSize(viewport.getWidth() > 0 ? viewport.getWidth() - 14 : 700, std::max(totalH, 100));
}

void SoundIdSuiteList::setCollapsed(bool collapsed)
{
    isCollapsed = collapsed;
    btnToggleCollapse.setButtonText(isCollapsed ? juce::String::fromUTF8(u8"\u25b2") : juce::String::fromUTF8(u8"\u25bc"));
    viewport.setVisible(!isCollapsed);
    repaint();
    resized();
}

void SoundIdSuiteList::setChevronGlyph(const juce::String& glyph)
{
    btnToggleCollapse.setButtonText(glyph);
}

void SoundIdSuiteList::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(34).reduced(8, 4);

    btnToggleCollapse.setBounds(header.removeFromRight(26).withSizeKeepingCentre(22, 22));
    header.removeFromRight(6);

    btnClear.setBounds(header.removeFromRight(75));
    header.removeFromRight(8);

    btnViewMode.setBounds(header.removeFromRight(105));
    header.removeFromRight(12);

    // Hero Action Button: dominant placement
    btnRunSession.setBounds(header.removeFromLeft(195));
    header.removeFromLeft(16);

    // Secondary actions
    btnAddStandard.setBounds(header.removeFromLeft(145));
    header.removeFromLeft(8);
    btnAddCustom.setBounds(header.removeFromLeft(140));

    bounds.removeFromTop(2);
    if (!isCollapsed)
    {
        viewport.setVisible(true);
        viewport.setBounds(bounds.reduced(6, 4));
        layoutRows();
    }
    else
    {
        viewport.setVisible(false);
    }
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
            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
            g.fillRoundedRectangle(rowRect, 4.0f);
            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.6f));
            g.drawRoundedRectangle(rowRect.reduced(0.5f), 4.0f, 1.0f);
        }
        else if (item.isSkipped)
        {
            g.setColour(SoundIdTheme::surfaceSubtle.withAlpha(0.5f));
            g.fillRoundedRectangle(rowRect, 4.0f);
        }
        else
        {
            g.setColour(i % 2 == 0 ? SoundIdTheme::bgCardHover : SoundIdTheme::bgCard);
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
            auto delBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
            g.setColour(SoundIdTheme::accentRed.withAlpha(0.18f));
            g.fillRoundedRectangle(delBtn, 4.0f);
            drawTrashIcon(g, delBtn, SoundIdTheme::accentRed);

            auto copyBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
            g.setColour(SoundIdTheme::surfaceSubtle);
            g.fillRoundedRectangle(copyBtn, 4.0f);
            drawCopyIcon(g, copyBtn, SoundIdTheme::textSecondary);

            auto editBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
            g.setColour(SoundIdTheme::surfaceSubtle);
            g.fillRoundedRectangle(editBtn, 4.0f);
            drawEditIcon(g, editBtn, SoundIdTheme::textPrimary);
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
            g.setColour(SoundIdTheme::surfaceSubtle);
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
        g.setColour(item.isSkipped ? SoundIdTheme::surfaceSubtle : SoundIdTheme::accentGreen.withAlpha(0.2f));
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
            auto progRect = juce::Rectangle<float>(32.0f, currentY + 2.0f, static_cast<float>(getWidth()) - 40.0f, 22.0f);
            currentY += 28.0f;

            int completedPoints = 0;
            for (auto s : item.pointStatuses)
            {
                if (s == PointStatus::Completed)
                    completedPoints++;
            }
            if (item.status == QueueItemStatus::Completed)
                completedPoints = item.totalPoints;
            else if (item.currentRunningPoint > completedPoints)
                completedPoints = item.currentRunningPoint;

            float pct = item.totalPoints > 0 ? static_cast<float>(completedPoints) / static_cast<float>(item.totalPoints) : 0.0f;

            g.setColour(SoundIdTheme::bgCardHover);
            g.fillRoundedRectangle(progRect, 4.0f);
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawRoundedRectangle(progRect, 4.0f, 1.0f);

            if (pct > 0.0f)
            {
                auto fillRect = progRect.withWidth(progRect.getWidth() * std::clamp(pct, 0.0f, 1.0f));
                g.setColour(item.status == QueueItemStatus::Invalidated ? SoundIdTheme::accentAmber.withAlpha(0.8f) : SoundIdTheme::accentGreen.withAlpha(0.85f));
                g.fillRoundedRectangle(fillRect, 4.0f);
            }

            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.setColour(pct > 0.5f ? juce::Colours::white : SoundIdTheme::textPrimary);
            juce::String progText = "Progress: " + juce::String(completedPoints) + " / " + juce::String(item.totalPoints) + 
                                    " Points Completed (" + juce::String(static_cast<int>(pct * 100.0f)) + "%)";
            g.drawText(progText, progRect, juce::Justification::centred, true);

            if (!owner.isCompactView)
            {
                for (int pIdx = 0; pIdx < item.totalPoints; ++pIdx)
                {
                    auto subRect = juce::Rectangle<float>(32.0f, currentY + 1.0f, static_cast<float>(getWidth()) - 36.0f, 22.0f);
                    currentY += 24.0f;

                    g.setColour(SoundIdTheme::bgCardHover.withAlpha(0.45f));
                    g.fillRoundedRectangle(subRect, 3.0f);
                    g.setColour(SoundIdTheme::borderSubtle);
                    g.drawRoundedRectangle(subRect.reduced(0.5f), 3.0f, 1.0f);

                    auto subArea = subRect.reduced(6.0f, 2.0f);
                    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
                    g.setColour(SoundIdTheme::textPrimary);
                    
                    float stepPct = (item.totalPoints > 1) ? (static_cast<float>(pIdx) / static_cast<float>(item.totalPoints - 1) * 100.0f) : 0.0f;
                    juce::String stepLabel = "Point #" + juce::String(pIdx + 1) + " / " + juce::String(item.totalPoints) + " (" + juce::String(stepPct, 1) + "% Pos)";
                    g.drawText(stepLabel, subArea.removeFromLeft(200.0f), juce::Justification::centredLeft, true);

                    // Subtest / Point Status Pill
                    PointStatus ptStatus = PointStatus::Queued;
                    if (static_cast<size_t>(pIdx) < item.pointStatuses.size())
                        ptStatus = item.pointStatuses[static_cast<size_t>(pIdx)];

                    auto ptStatusRect = subArea.removeFromLeft(86.0f).withSizeKeepingCentre(80.0f, 16.0f);

                    if (ptStatus == PointStatus::Queued)
                    {
                        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                        g.setColour(SoundIdTheme::textSecondary);
                        g.drawText("QUEUED", ptStatusRect, juce::Justification::centredLeft, false);
                    }
                    else
                    {
                        juce::Colour bgPill, textPill;
                        juce::String statusLabel;

                        switch (ptStatus)
                        {
                            case PointStatus::Completed:
                                bgPill = SoundIdTheme::accentGreen.withAlpha(0.2f);
                                textPill = SoundIdTheme::accentGreen;
                                statusLabel = "DONE";
                                break;
                            case PointStatus::Running:
                                bgPill = juce::Colour(0xff0284c7).withAlpha(0.2f);
                                textPill = juce::Colour(0xff0284c7);
                                statusLabel = "MEASURING";
                                break;
                            case PointStatus::Invalidated:
                                bgPill = SoundIdTheme::accentAmber.withAlpha(0.2f);
                                textPill = SoundIdTheme::accentAmber;
                                statusLabel = "RE-RUN";
                                break;
                            case PointStatus::Annulled:
                                bgPill = SoundIdTheme::accentRed.withAlpha(0.2f);
                                textPill = SoundIdTheme::accentRed;
                                statusLabel = "ANNULLED";
                                break;
                            default:
                                break;
                        }

                        g.setColour(bgPill);
                        g.fillRoundedRectangle(ptStatusRect, 3.0f);
                        g.setColour(textPill);
                        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                        g.drawText(statusLabel, ptStatusRect, juce::Justification::centred, false);
                    }

                    auto subActions = subArea.removeFromRight(150.0f);

                    auto subDel = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
                    subActions.removeFromRight(6.0f);

                    auto subClear = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
                    subActions.removeFromRight(6.0f);

                    auto subView = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);

                    bool isViewHovered = subView.contains(hoveredPos);
                    if (isViewHovered)
                    {
                        g.setColour(SoundIdTheme::bgCardHover);
                        g.fillRoundedRectangle(subView, 4.0f);
                    }
                    drawEyeIcon(g, subView, SoundIdTheme::accentGreen);

                    bool isClearHovered = subClear.contains(hoveredPos);
                    if (isClearHovered)
                    {
                        g.setColour(SoundIdTheme::bgCardHover);
                        g.fillRoundedRectangle(subClear, 4.0f);
                    }
                    drawClearIcon(g, subClear, isClearHovered ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);

                    bool isDelHovered = subDel.contains(hoveredPos);
                    if (isDelHovered)
                    {
                        g.setColour(SoundIdTheme::accentRed.withAlpha(0.15f));
                        g.fillRoundedRectangle(subDel, 4.0f);
                    }
                    drawTrashIcon(g, subDel, isDelHovered ? SoundIdTheme::accentRed : SoundIdTheme::textSecondary);
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
                auto delBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
                if (delBtn.contains(e.position))
                {
                    owner.removeTestFromQueue(static_cast<int>(i));
                    return;
                }

                auto copyBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
                if (copyBtn.contains(e.position))
                {
                    owner.duplicateTestInQueue(static_cast<int>(i));
                    return;
                }

                auto editBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
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
            currentY += 28.0f; // Skip progress bar

            if (!owner.isCompactView)
            {
                for (int pIdx = 0; pIdx < owner.queue[i].totalPoints; ++pIdx)
                {
                    auto subRect = juce::Rectangle<float>(32.0f, currentY + 1.0f, static_cast<float>(getWidth()) - 36.0f, 22.0f);
                    currentY += 24.0f;

                    if (subRect.contains(e.position))
                    {
                        auto subArea = subRect.reduced(6.0f, 2.0f);
                        subArea.removeFromLeft(220.0f);
                        auto subActions = subArea.removeFromRight(150.0f);

                        auto subDel = subActions.removeFromRight(28.0f).withSizeKeepingCentre(24.0f, 18.0f);
                        if (subDel.contains(e.position))
                        {
                            if (owner.onDeletePointClicked) owner.onDeletePointClicked(static_cast<int>(i), pIdx);
                            return;
                        }
                        subActions.removeFromRight(4.0f);

                        auto subClear = subActions.removeFromRight(28.0f).withSizeKeepingCentre(24.0f, 18.0f);
                        if (subClear.contains(e.position))
                        {
                            if (owner.onClearPointClicked) owner.onClearPointClicked(static_cast<int>(i), pIdx);
                            return;
                        }
                        subActions.removeFromRight(4.0f);

                        auto subView = subActions.removeFromRight(28.0f).withSizeKeepingCentre(24.0f, 18.0f);
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
}

juce::String SoundIdSuiteList::RowsContentComponent::getTooltip()
{
    auto pos = getMouseXYRelative().toFloat();
    float currentY = 0.0f;

    for (size_t i = 0; i < owner.queue.size(); ++i)
    {
        const auto& item = owner.queue[i];
        auto rowRect = juce::Rectangle<float>(0.0f, currentY, static_cast<float>(getWidth()), 34.0f).reduced(0.0f, 1.0f);
        currentY += 34.0f;

        if (rowRect.contains(pos))
        {
            auto area = rowRect.reduced(6.0f, 2.0f);

            if (!item.isPinned)
            {
                auto reorderArea = area.removeFromLeft(36.0f);
                auto upRect = reorderArea.removeFromLeft(16.0f);
                if (upRect.contains(pos))
                    return "Move test up in session execution order";

                auto downRect = reorderArea.removeFromLeft(16.0f);
                if (downRect.contains(pos))
                    return "Move test down in session execution order";
            }
            else
            {
                auto pinRect = area.removeFromLeft(36.0f);
                if (pinRect.contains(pos))
                    return "Pinned baseline test (always runs first to calibrate noise floor)";
            }

            auto badgeRect = area.removeFromLeft(38.0f);
            if (badgeRect.contains(pos))
                return "Module: " + item.badgeText + " (" + item.title + ")";

            auto rightActions = area.removeFromRight(290.0f);
            if (!item.isPinned)
            {
                auto delBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
                if (delBtn.contains(pos))
                    return "Delete Test - Remove this test from the session plan";

                auto copyBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
                if (copyBtn.contains(pos))
                    return "Duplicate Test - Create a copy of this test in queue";

                auto editBtn = rightActions.removeFromRight(30.0f).withSizeKeepingCentre(26.0f, 22.0f);
                if (editBtn.contains(pos))
                    return "Edit Test - Configure sweep resolution, stimulus duration, capture mode and ranges";
            }
            else
            {
                rightActions.removeFromRight(112.0f);
            }

            rightActions.removeFromRight(6.0f);

            if (item.status == QueueItemStatus::Incomplete)
            {
                auto contBtn = rightActions.removeFromRight(58.0f).withSizeKeepingCentre(54.0f, 20.0f);
                if (contBtn.contains(pos))
                    return "Resume Session - Continue running this test from last unfinished point";

                auto resetBtn = rightActions.removeFromRight(50.0f).withSizeKeepingCentre(46.0f, 20.0f);
                if (resetBtn.contains(pos))
                    return "Reset Test - Reset measurement state to Queued";
            }
            else if (item.status == QueueItemStatus::Invalidated)
            {
                auto rerunBtn = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
                if (rerunBtn.contains(pos))
                    return "Re-run Test - Execute this invalidated test again";
            }

            rightActions.removeFromRight(6.0f);
            auto bypassPill = rightActions.removeFromRight(60.0f).withSizeKeepingCentre(56.0f, 20.0f);
            if (bypassPill.contains(pos))
                return item.isSkipped ? "Click to set ACTIVE (include in session run)" : "Click to set BYPASS (skip during session run)";

            auto expandBtn = area.removeFromLeft(16.0f);
            if (expandBtn.contains(pos))
                return item.isExpanded ? "Collapse point measurements list" : "Expand to view individual point measurements";

            return item.title + (item.description.isNotEmpty() ? (" (" + item.description + ")") : "");
        }

        if (item.isExpanded)
        {
            if (owner.isCompactView)
            {
                auto progRect = juce::Rectangle<float>(32.0f, currentY + 2.0f, static_cast<float>(getWidth()) - 40.0f, 22.0f);
                currentY += 28.0f;
                if (progRect.contains(pos))
                    return "Batch Test Suite Progress - Toggle View Mode in toolbar to expand full detailed points";
            }
            else
            {
                int shownSubRows = std::min(item.totalPoints, 16);
                for (int pIdx = 0; pIdx < shownSubRows; ++pIdx)
                {
                    auto subRect = juce::Rectangle<float>(32.0f, currentY + 1.0f, static_cast<float>(getWidth()) - 36.0f, 22.0f);
                    currentY += 24.0f;

                    if (subRect.contains(pos))
                    {
                        auto subArea = subRect.reduced(6.0f, 2.0f);
                        subArea.removeFromLeft(220.0f);
                        auto subActions = subArea.removeFromRight(150.0f);

                        auto subDel = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
                        if (subDel.contains(pos))
                            return "Annul Point - Mark point as invalid without repeating";
                        subActions.removeFromRight(6.0f);

                        auto subClear = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
                        if (subClear.contains(pos))
                            return "Reset Point - Re-queue point to be re-measured";
                        subActions.removeFromRight(6.0f);

                        auto subView = subActions.removeFromRight(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
                        if (subView.contains(pos))
                            return "View Point Curve - Display response in curve plotter";

                        return "Measurement Point #" + juce::String(pIdx + 1);
                    }
                }
                currentY += 6.0f;
            }
        }
    }

    return {};
}

} // namespace abdaudiolab::gui

