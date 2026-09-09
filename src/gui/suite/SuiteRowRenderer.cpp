/**
 * @file SuiteRowRenderer.cpp
 * @brief Implementation of SuiteRowRenderer.
 * @author ABDSynths
 * @date 2026
 */

#include "SuiteRowRenderer.h"
#include <algorithm>

namespace abdaudiolab::gui
{

void SuiteRowRenderer::renderMainRow(juce::Graphics& g,
                                     const QueueItem& item,
                                     const SuiteMainRowLayout& layout,
                                     size_t index,
                                     size_t totalQueueSize,
                                     juce::Point<float> hoveredPos)
{
    juce::ignoreUnused(hoveredPos);

    // 1. Row background
    if (item.status == QueueItemStatus::Running)
    {
        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillRoundedRectangle(layout.rowRect, 4.0f);
        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.6f));
        g.drawRoundedRectangle(layout.rowRect.reduced(0.5f), 4.0f, 1.0f);
    }
    else if (item.isSkipped)
    {
        g.setColour(SoundIdTheme::surfaceSubtle.withAlpha(0.5f));
        g.fillRoundedRectangle(layout.rowRect, 4.0f);
    }
    else
    {
        g.setColour(index % 2 == 0 ? SoundIdTheme::bgCardHover : SoundIdTheme::bgCard);
        g.fillRoundedRectangle(layout.rowRect, 4.0f);
    }

    // 2. Order controls [^] [v] or [PIN]
    if (!item.isPinned)
    {
        auto upColor = index > 1 ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted.withAlpha(0.3f);
        suite_icons::drawReorderChevron(g, layout.reorderUpRect, true, upColor);

        auto downColor = index < totalQueueSize - 1 ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted.withAlpha(0.3f);
        suite_icons::drawReorderChevron(g, layout.reorderDownRect, false, downColor);
    }
    else
    {
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("PIN", layout.reorderArea, juce::Justification::centred, false);
    }

    // 3. Badge (FLT, ENV, etc.)
    g.setColour(item.isSkipped ? SoundIdTheme::borderCard : item.badgeColor.withAlpha(0.18f));
    g.fillRoundedRectangle(layout.badgeRect, 3.5f);
    g.setColour(item.isSkipped ? SoundIdTheme::textMuted : item.badgeColor);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(item.badgeText, layout.badgeRect, juce::Justification::centred, false);

    // 4. CRUD Action Buttons
    if (!item.isPinned)
    {
        // Delete button
        g.setColour(SoundIdTheme::accentRed.withAlpha(0.18f));
        g.fillRoundedRectangle(layout.delBtnRect, 4.0f);
        suite_icons::drawTrash(g, layout.delBtnRect, SoundIdTheme::accentRed);

        // Copy button
        g.setColour(SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(layout.copyBtnRect, 4.0f);
        suite_icons::drawCopy(g, layout.copyBtnRect, SoundIdTheme::textSecondary);

        // Edit button
        g.setColour(SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(layout.editBtnRect, 4.0f);
        suite_icons::drawEdit(g, layout.editBtnRect, SoundIdTheme::textPrimary);
    }

    // 5. State action buttons (Incomplete / Invalidated)
    if (item.status == QueueItemStatus::Incomplete)
    {
        g.setColour(SoundIdTheme::accentAmber.withAlpha(0.2f));
        g.fillRoundedRectangle(layout.contBtnRect, 3.5f);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("RESUME", layout.contBtnRect, juce::Justification::centred, false);

        g.setColour(SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(layout.resetBtnRect, 3.5f);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("RESET", layout.resetBtnRect, juce::Justification::centred, false);
    }
    else if (item.status == QueueItemStatus::Invalidated)
    {
        g.setColour(SoundIdTheme::accentRed.withAlpha(0.2f));
        g.fillRoundedRectangle(layout.rerunBtnRect, 3.5f);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("RE-RUN", layout.rerunBtnRect, juce::Justification::centred, false);
    }

    // 6. Status text
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    if (item.isSkipped)
    {
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("BYPASSED", layout.statusTextRect, juce::Justification::centredRight, true);
    }
    else if (item.status == QueueItemStatus::Running)
    {
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("RUNNING " + juce::String(item.currentRunningPoint) + "/" + juce::String(item.totalPoints),
                   layout.statusTextRect, juce::Justification::centredRight, true);
    }
    else if (item.status == QueueItemStatus::Completed)
    {
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("DONE [OK]", layout.statusTextRect, juce::Justification::centredRight, true);
    }
    else if (item.status == QueueItemStatus::Invalidated)
    {
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("INVALID", layout.statusTextRect, juce::Justification::centredRight, true);
    }
    else
    {
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("QUEUED", layout.statusTextRect, juce::Justification::centredRight, true);
    }

    // 7. Active / Bypass toggle pill
    g.setColour(item.isSkipped ? SoundIdTheme::surfaceSubtle : SoundIdTheme::accentGreen.withAlpha(0.2f));
    g.fillRoundedRectangle(layout.bypassPillRect, 3.5f);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.setColour(item.isSkipped ? SoundIdTheme::textMuted : SoundIdTheme::accentGreen);
    g.drawText(item.isSkipped ? "BYPASS" : "ACTIVE", layout.bypassPillRect, juce::Justification::centred, false);

    // 8. Expand arrow
    suite_icons::drawExpandArrow(g, layout.expandBtnRect, item.isExpanded, SoundIdTheme::textSecondary);

    // 9. Title and description
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(item.isSkipped ? SoundIdTheme::textMuted : SoundIdTheme::textPrimary);

    juce::String fullText = item.title;
    if (item.description.isNotEmpty())
        fullText += "  --  " + item.description;

    g.drawText(fullText, layout.titleRect, juce::Justification::centredLeft, true);
}

void SuiteRowRenderer::renderProgressBar(juce::Graphics& g,
                                        const QueueItem& item,
                                        juce::Rectangle<float> bounds)
{
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
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    if (pct > 0.0f)
    {
        auto fillRect = bounds.withWidth(bounds.getWidth() * std::clamp(pct, 0.0f, 1.0f));
        g.setColour(item.status == QueueItemStatus::Invalidated ? SoundIdTheme::accentAmber.withAlpha(0.8f) : SoundIdTheme::accentGreen.withAlpha(0.85f));
        g.fillRoundedRectangle(fillRect, 4.0f);
    }

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(pct > 0.5f ? juce::Colours::white : SoundIdTheme::textPrimary);
    juce::String progText = "Progress: " + juce::String(completedPoints) + " / " + juce::String(item.totalPoints) + 
                            " Points Completed (" + juce::String(static_cast<int>(pct * 100.0f)) + "%)";
    g.drawText(progText, bounds, juce::Justification::centred, true);
}

void SuiteRowRenderer::renderPointRow(juce::Graphics& g,
                                     int pointIndex,
                                     int totalPoints,
                                     PointStatus status,
                                     const SuitePointRowLayout& layout,
                                     juce::Point<float> hoveredPos,
                                     bool isSelected)
{
    if (isSelected)
    {
        g.setColour(SoundIdTheme::accentBlue.withAlpha(0.18f));
        g.fillRoundedRectangle(layout.rowRect, 3.5f);
        g.setColour(SoundIdTheme::accentBlue.withAlpha(0.65f));
        g.drawRoundedRectangle(layout.rowRect.reduced(0.5f), 3.5f, 1.2f);
    }
    else
    {
        g.setColour(SoundIdTheme::bgCardHover.withAlpha(0.45f));
        g.fillRoundedRectangle(layout.rowRect, 3.5f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(layout.rowRect.reduced(0.5f), 3.5f, 1.0f);
    }

    // Checkbox / Select Box
    bool isSelectBoxHovered = layout.selectBoxRect.contains(hoveredPos);
    if (isSelected)
    {
        g.setColour(SoundIdTheme::accentBlue);
        g.fillRoundedRectangle(layout.selectBoxRect, 2.5f);
        g.setColour(juce::Colours::white);
        juce::Path tick;
        auto r = layout.selectBoxRect;
        tick.startNewSubPath(r.getX() + r.getWidth() * 0.22f, r.getY() + r.getHeight() * 0.52f);
        tick.lineTo(r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.76f);
        tick.lineTo(r.getX() + r.getWidth() * 0.78f, r.getY() + r.getHeight() * 0.26f);
        g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour(isSelectBoxHovered ? SoundIdTheme::accentBlue.withAlpha(0.4f) : SoundIdTheme::surfaceSubtle.withAlpha(0.6f));
        g.fillRoundedRectangle(layout.selectBoxRect, 2.5f);
        g.setColour(isSelectBoxHovered ? SoundIdTheme::accentBlue : SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(layout.selectBoxRect, 2.5f, 1.0f);
    }

    // Step label
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.setColour(isSelected ? juce::Colours::white : SoundIdTheme::textPrimary);
    float stepPct = (totalPoints > 1) ? (static_cast<float>(pointIndex) / static_cast<float>(totalPoints - 1) * 100.0f) : 0.0f;
    juce::String stepLabel = "Point #" + juce::String(pointIndex + 1) + " / " + juce::String(totalPoints) + " (" + juce::String(stepPct, 1) + "% Pos)";
    g.drawText(stepLabel, layout.labelRect, juce::Justification::centredLeft, true);

    // Status pill
    if (status == PointStatus::Queued)
    {
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("QUEUED", layout.statusPillRect, juce::Justification::centredLeft, false);
    }
    else
    {
        juce::Colour bgPill, textPill;
        juce::String statusLabel;

        switch (status)
        {
            case PointStatus::Completed:
                bgPill = SoundIdTheme::accentGreen.withAlpha(0.2f);
                textPill = SoundIdTheme::accentGreen;
                statusLabel = "DONE";
                break;
            case PointStatus::Running:
                bgPill = SoundIdTheme::accentBlue.withAlpha(0.2f);
                textPill = SoundIdTheme::accentBlue;
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
        g.fillRoundedRectangle(layout.statusPillRect, 3.5f);
        g.setColour(textPill);
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText(statusLabel, layout.statusPillRect, juce::Justification::centred, false);
    }

    // Sub actions with hover highlights
    bool isViewHovered = layout.viewBtnRect.contains(hoveredPos);
    if (isViewHovered)
    {
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(layout.viewBtnRect, 4.0f);
    }
    suite_icons::drawEye(g, layout.viewBtnRect, SoundIdTheme::accentGreen);

    bool isClearHovered = layout.clearBtnRect.contains(hoveredPos);
    if (isClearHovered)
    {
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(layout.clearBtnRect, 4.0f);
    }
    suite_icons::drawReset(g, layout.clearBtnRect, isClearHovered ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);

    bool isDelHovered = layout.delBtnRect.contains(hoveredPos);
    if (isDelHovered)
    {
        g.setColour(SoundIdTheme::accentRed.withAlpha(0.15f));
        g.fillRoundedRectangle(layout.delBtnRect, 4.0f);
    }
    suite_icons::drawTrash(g, layout.delBtnRect, isDelHovered ? SoundIdTheme::accentRed : SoundIdTheme::textSecondary);
}

} // namespace abdaudiolab::gui
