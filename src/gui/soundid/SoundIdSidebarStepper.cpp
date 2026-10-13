#include "SoundIdSidebarStepper.h"

namespace abdaudiolab::gui
{

SoundIdSidebarStepper::SoundIdSidebarStepper()
{
    stepStatuses[Step::SystemInfo]        = StepStatus::Completed;
    stepStatuses[Step::HardwareRouting]   = StepStatus::Current;
    stepStatuses[Step::CalibrateLoopback] = StepStatus::Pending;
    stepStatuses[Step::RunSession]        = StepStatus::Pending;
    stepStatuses[Step::ExportReport]      = StepStatus::Pending;

    stepTitles[Step::SystemInfo]        = "0. Studio Environment";
    stepTitles[Step::HardwareRouting]   = "1. Target & Routing";
    stepTitles[Step::CalibrateLoopback] = "2. Audio Interface Calibration";
    stepTitles[Step::RunSession]        = "3. Run Session";
    stepTitles[Step::ExportReport]      = "4. Export & Report";

    stepDescriptions[Step::SystemInfo]        = "Audio I/O & MIDI setup";
    stepDescriptions[Step::HardwareRouting]   = "Synth profile & wiring";
    stepDescriptions[Step::CalibrateLoopback] = "Loopback latency & SNR";
    stepDescriptions[Step::RunSession]        = "Acquisition & live monitor";
    stepDescriptions[Step::ExportReport]      = "Package & validation report";

    btnToggleCollapse.setCollapsed(false);
    btnToggleCollapse.setTooltip("Collapse Navigation Rail");
    btnToggleCollapse.onClick = [this] {
        setCollapsed(!collapsedState);
        if (onCollapseToggled != nullptr)
            onCollapseToggled(collapsedState);
    };
    addAndMakeVisible(btnToggleCollapse);
}

void SoundIdSidebarStepper::setCurrentStep(Step targetStep)
{
    if (currentStep == targetStep) return;

    if (stepStatuses[currentStep] == StepStatus::Current)
    {
        if (currentStep == Step::ExportReport || currentStep == Step::RunSession)
            stepStatuses[currentStep] = StepStatus::Pending;
        else
            stepStatuses[currentStep] = StepStatus::Completed;
    }

    currentStep = targetStep;
    stepStatuses[currentStep] = StepStatus::Current;
    repaint();
}

void SoundIdSidebarStepper::setStepStatus(Step step, StepStatus status)
{
    stepStatuses[step] = status;
    repaint();
}

SoundIdSidebarStepper::StepStatus SoundIdSidebarStepper::getStepStatus(Step step) const
{
    auto it = stepStatuses.find(step);
    return it != stepStatuses.end() ? it->second : StepStatus::Pending;
}

void SoundIdSidebarStepper::setStepLocked(Step step, bool locked)
{
    lockedSteps[step] = locked;
    repaint();
}

bool SoundIdSidebarStepper::isStepLocked(Step step) const
{
    auto it = lockedSteps.find(step);
    return it != lockedSteps.end() && it->second;
}

bool SoundIdSidebarStepper::canNavigateTo(Step step) const
{
    if (step == Step::ExportReport)
        return canNavigateToExportReport();
    return !isStepLocked(step);
}

bool SoundIdSidebarStepper::canNavigateToExportReport() const noexcept
{
    return !isStepLocked(Step::ExportReport);
}

bool SoundIdSidebarStepper::isExportReportCompleted() const noexcept
{
    return getStepStatus(Step::ExportReport) == StepStatus::Completed;
}

void SoundIdSidebarStepper::setCollapsed(bool collapsed)
{
    if (collapsedState == collapsed) return;
    collapsedState = collapsed;
    btnToggleCollapse.setCollapsed(collapsedState);
    btnToggleCollapse.setTooltip(collapsedState ? "Expand Navigation Rail (Click to restore)"
                                                : "Collapse Navigation Rail");
    resized();
    repaint();
}

void SoundIdSidebarStepper::setSessionSummary(const SessionSummaryInfo& info)
{
    summaryInfo = info;
    repaint();
}

void SoundIdSidebarStepper::resized()
{
    auto b = getLocalBounds();
    auto topRow = b.removeFromTop(32);
    if (collapsedState)
    {
        btnToggleCollapse.setBounds(topRow.withSizeKeepingCentre(32, 24));
    }
    else
    {
        btnToggleCollapse.setBounds(topRow.removeFromRight(32).withSizeKeepingCentre(24, 24));
    }
}

void SoundIdSidebarStepper::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // 1. Sidebar Background & Right Border (SoundID dark card or light surface)
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    // Top header label in expanded mode
    if (!collapsedState)
    {
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("WORKFLOW", juce::Rectangle<float>(12.0f, 4.0f, 100.0f, 24.0f), juce::Justification::centredLeft, false);
    }

    auto contentArea = b.reduced(collapsedState ? 4.0f : 10.0f, 8.0f);
    contentArea.removeFromTop(28.0f); // Top bar space for collapse toggle

    // 2. Render Step Rows (5 steps: 0. Información to 4. Export & Report)
    const float rowHeight = collapsedState ? 44.0f : 52.0f;
    const float stepSpacing = 6.0f;

    static constexpr Step visualOrder[5] = {
        Step::SystemInfo,
        Step::HardwareRouting,
        Step::CalibrateLoopback,
        Step::RunSession,
        Step::ExportReport
    };

    for (int i = 0; i < 5; ++i)
    {
        Step step = visualOrder[i];
        auto rowRect = contentArea.removeFromTop(rowHeight);
        bool isHovered = (hoveredStep.has_value() && *hoveredStep == step);
        drawStepRow(g, step, rowRect, isHovered);
        contentArea.removeFromTop(stepSpacing);
    }

    // 3. Render Session Summary Card at bottom if expanded
    if (!collapsedState && contentArea.getHeight() >= 90.0f)
    {
        auto summaryArea = contentArea.removeFromBottom(110.0f);
        drawSummaryCard(g, summaryArea);
    }
}

void SoundIdSidebarStepper::drawStepRow(juce::Graphics& g, Step step, juce::Rectangle<float> rowBounds, bool isHovered)
{
    const auto status = getStepStatus(step);
    const bool isCurrent = (step == currentStep);
    const bool isLocked = isStepLocked(step);
    const bool isNavigable = isStepNavigable(step);

    const juce::Colour accentGreen   = SoundIdTheme::accentGreen;
    const juce::Colour accentAmber   = SoundIdTheme::accentAmber;
    const juce::Colour textPrimary   = SoundIdTheme::textPrimary;
    const juce::Colour textSecondary = SoundIdTheme::textSecondary;
    const juce::Colour textMuted     = SoundIdTheme::textMuted;
    const juce::Colour borderSubtle  = SoundIdTheme::borderSubtle;

    // 1. Row Background, Contours & Indicators
    if (isCurrent)
    {
        // Current state: subtle accent background + outline + left vertical bar
        g.setColour(accentGreen.withAlpha(0.08f));
        g.fillRoundedRectangle(rowBounds, 6.0f);
        g.setColour(accentGreen.withAlpha(0.20f));
        g.drawRoundedRectangle(rowBounds.reduced(0.5f), 6.0f, 1.0f);

        // Vertical indicator bar on left edge (2.5 px width)
        auto barRect = juce::Rectangle<float>(rowBounds.getX() + 1.0f, rowBounds.getY() + 5.0f,
                                              getActiveIndicatorWidth(), rowBounds.getHeight() - 10.0f);
        g.setColour(accentGreen);
        g.fillRoundedRectangle(barRect, 1.25f);
    }
    else if (isHovered && isNavigable)
    {
        // Interactive hover state: subtle hover surface + accent border hint
        g.setColour(accentGreen.withAlpha(0.06f));
        g.fillRoundedRectangle(rowBounds, 6.0f);
        g.setColour(accentGreen.withAlpha(0.18f));
        g.drawRoundedRectangle(rowBounds.reduced(0.5f), 6.0f, 1.0f);
    }

    // 2. Circle Badge (24x24)
    const float badgeSize = 24.0f;
    const float badgeX = collapsedState ? (rowBounds.getCentreX() - badgeSize * 0.5f) : (rowBounds.getX() + 10.0f);
    const float badgeY = rowBounds.getCentreY() - badgeSize * 0.5f;
    const auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeSize, badgeSize);
    const float cx = badgeRect.getCentreX();
    const float cy = badgeRect.getCentreY();

    if (isLocked)
    {
        // Locked: dimmed outlined circle with muted digit (non-interactive)
        g.setColour(borderSubtle.withAlpha(0.35f));
        g.drawEllipse(badgeRect, 1.2f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.setColour(textMuted.withAlpha(0.35f));
        g.drawText(juce::String(getStepBadgeNumber(step)), badgeRect, juce::Justification::centred, false);
    }
    else if (status == StepStatus::Completed)
    {
        // Completed: solid filled green badge with white checkmark path (mojibake-free vector)
        g.setColour(accentGreen);
        g.fillEllipse(badgeRect);

        juce::Path checkmark;
        checkmark.startNewSubPath(cx - 4.5f, cy + 0.2f);
        checkmark.lineTo(cx - 1.2f, cy + 3.8f);
        checkmark.lineTo(cx + 5.0f, cy - 3.8f);
        g.setColour(juce::Colours::white);
        g.strokePath(checkmark, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (isCurrent)
    {
        // Current: green ring with expanding glowing halo and bold green digit
        g.setColour(accentGreen.withAlpha(0.15f));
        g.fillEllipse(badgeRect.expanded(3.0f));
        g.setColour(accentGreen);
        g.drawEllipse(badgeRect, 2.0f);

        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(accentGreen);
        g.drawText(juce::String(getStepBadgeNumber(step)), badgeRect, juce::Justification::centred, false);
    }
    else if (status == StepStatus::Warning || status == StepStatus::Skipped)
    {
        g.setColour(accentAmber.withAlpha(0.20f));
        g.fillEllipse(badgeRect);
        g.setColour(accentAmber);
        g.drawEllipse(badgeRect, 1.5f);

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(accentAmber);
        if (status == StepStatus::Skipped)
            g.drawText(juce::String::fromUTF8(u8"\u23ed"), badgeRect, juce::Justification::centred, false);
        else
            g.drawText(juce::String(getStepBadgeNumber(step)), badgeRect, juce::Justification::centred, false);
    }
    else
    {
        // Pending: subtle border circle with muted digit
        g.setColour(borderSubtle);
        g.drawEllipse(badgeRect, 1.5f);

        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.setColour(textMuted);
        g.drawText(juce::String(getStepBadgeNumber(step)), badgeRect, juce::Justification::centred, false);
    }

    // 3. Expanded Mode: Typography & Hierarchy
    if (!collapsedState)
    {
        const float textLeft = badgeX + badgeSize + 10.0f;
        const float textWidth = rowBounds.getRight() - textLeft - 6.0f;

        const auto titleArea = juce::Rectangle<float>(textLeft, rowBounds.getY() + 9.0f, textWidth, 18.0f);
        const auto descArea  = juce::Rectangle<float>(textLeft, rowBounds.getY() + 27.0f, textWidth, 16.0f);

        if (isLocked)
        {
            // Locked: dimmed typography, no crude "[Locked]" suffix string
            g.setFont(juce::FontOptions(getStandardTitleFontSize(), juce::Font::plain));
            g.setColour(textMuted.withAlpha(0.45f));
            g.drawText(stepTitles[step], titleArea, juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(getStandardSubtitleFontSize(), juce::Font::plain));
            g.setColour(textMuted.withAlpha(0.35f));
            g.drawText(stepDescriptions[step], descArea, juce::Justification::centredLeft, true);
        }
        else if (isCurrent)
        {
            // Current: prominent 13.0f bold title in textPrimary and 10.5f subtitle in textSecondary
            g.setFont(juce::FontOptions(getActiveTitleFontSize(), juce::Font::bold));
            g.setColour(textPrimary);
            g.drawText(stepTitles[step], titleArea, juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(getActiveSubtitleFontSize(), juce::Font::plain));
            g.setColour(textSecondary);
            g.drawText(stepDescriptions[step], descArea, juce::Justification::centredLeft, true);
        }
        else if (status == StepStatus::Completed)
        {
            // Completed: standard 12.0f plain title in textPrimary and 10.0f subtitle in textMuted
            g.setFont(juce::FontOptions(getStandardTitleFontSize(), juce::Font::plain));
            g.setColour(textPrimary);
            g.drawText(stepTitles[step], titleArea, juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(getStandardSubtitleFontSize(), juce::Font::plain));
            g.setColour(textMuted);
            g.drawText(stepDescriptions[step], descArea, juce::Justification::centredLeft, true);
        }
        else
        {
            // Pending: standard 12.0f plain title in textMuted and 10.0f subtitle in textMuted
            g.setFont(juce::FontOptions(getStandardTitleFontSize(), juce::Font::plain));
            g.setColour(textMuted);
            g.drawText(stepTitles[step], titleArea, juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(getStandardSubtitleFontSize(), juce::Font::plain));
            g.setColour(textMuted);
            g.drawText(stepDescriptions[step], descArea, juce::Justification::centredLeft, true);
        }
    }
}

void SoundIdSidebarStepper::drawSummaryCard(juce::Graphics& g, juce::Rectangle<float> cardBounds)
{
    g.setColour(SoundIdTheme::surfaceSubtle);
    g.fillRoundedRectangle(cardBounds, 6.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), 6.0f, 1.0f);

    auto inner = cardBounds.reduced(8.0f);
    
    // Header
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("SESSION SUMMARY", inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);

    // Target Hardware
    inner.removeFromTop(4.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(summaryInfo.hardwareName, inner.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    // Loopback status line
    g.setFont(juce::FontOptions(10.0f));
    if (summaryInfo.loopbackCalibrated)
    {
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x8F Calibrated (SNR ")) + juce::String(summaryInfo.loopbackSnrDb, 1) + " dB)",
                   inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    }
    else if (summaryInfo.loopbackBypassed)
    {
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x8F Loopback: Bypassed (0 dB)")),
                   inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x8F Loopback: Uncalibrated")),
                   inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    }

    // Progress line
    g.setColour(SoundIdTheme::textMuted);
    juce::String progText = "Progress: " + juce::String(summaryInfo.pointsMeasured) + " / " + juce::String(summaryInfo.totalPointsPlanned) + " pts";
    g.drawText(progText, inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
}

juce::String SoundIdSidebarStepper::getTooltip()
{
    if (collapsedState && hoveredStep.has_value())
    {
        return stepTitles[*hoveredStep] + " : " + stepDescriptions[*hoveredStep];
    }
    return {};
}

void SoundIdSidebarStepper::mouseMove(const juce::MouseEvent& event)
{
    if (event.position.y < 34.0f)
    {
        if (hoveredStep.has_value())
        {
            hoveredStep.reset();
            repaint();
        }
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

    auto b = getLocalBounds().toFloat();
    auto contentArea = b.reduced(collapsedState ? 4.0f : 10.0f, 8.0f);
    contentArea.removeFromTop(28.0f);

    const float rowHeight = collapsedState ? 44.0f : 52.0f;
    const float stepSpacing = 6.0f;

    std::optional<Step> foundStep;
    float currentY = contentArea.getY();

    static constexpr Step visualOrder[5] = {
        Step::SystemInfo,
        Step::HardwareRouting,
        Step::CalibrateLoopback,
        Step::RunSession,
        Step::ExportReport
    };

    for (int i = 0; i < 5; ++i)
    {
        auto rowRect = juce::Rectangle<float>(contentArea.getX(), currentY, contentArea.getWidth(), rowHeight);
        if (rowRect.contains(event.position))
        {
            foundStep = visualOrder[i];
            break;
        }
        currentY += rowHeight + stepSpacing;
    }

    if (hoveredStep != foundStep)
    {
        hoveredStep = foundStep;
        const bool isNavigable = hoveredStep.has_value() && isStepNavigable(*hoveredStep);
        setMouseCursor(isNavigable ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SoundIdSidebarStepper::mouseExit(const juce::MouseEvent&)
{
    hoveredStep.reset();
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void SoundIdSidebarStepper::mouseUp(const juce::MouseEvent& event)
{
    if (event.position.y < 34.0f)
    {
        setCollapsed(!collapsedState);
        if (onCollapseToggled != nullptr)
            onCollapseToggled(collapsedState);
        return;
    }

    if (hoveredStep.has_value() && isStepNavigable(*hoveredStep))
    {
        setCurrentStep(*hoveredStep);
        if (onStepSelected != nullptr)
            onStepSelected(*hoveredStep);
    }
}

void SoundIdSidebarStepper::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    setCollapsed(!collapsedState);
    if (onCollapseToggled != nullptr)
        onCollapseToggled(collapsedState);
}

} // namespace abdaudiolab::gui
