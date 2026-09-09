#include "SoundIdSidebarStepper.h"

namespace abdaudiolab::gui
{

SoundIdSidebarStepper::SoundIdSidebarStepper()
{
    stepStatuses[Step::HardwareRouting]   = StepStatus::Current;
    stepStatuses[Step::CalibrateLoopback] = StepStatus::Pending;
    stepStatuses[Step::RunSession]        = StepStatus::Pending;
    stepStatuses[Step::ExportReport]      = StepStatus::Pending;

    stepTitles[Step::HardwareRouting]   = "1. Hardware & Routing";
    stepTitles[Step::CalibrateLoopback] = "2. Calibrate Loopback";
    stepTitles[Step::RunSession]        = "3. Run Session";
    stepTitles[Step::ExportReport]      = "4. Export & Report";

    stepDescriptions[Step::HardwareRouting]   = "Target, I/O & Wiring";
    stepDescriptions[Step::CalibrateLoopback] = "Interface DAC/ADC Check";
    stepDescriptions[Step::RunSession]        = "Excitation & Profiling";
    stepDescriptions[Step::ExportReport]      = "NAM, LUT & Certification";

    btnToggleCollapse.setButtonText(juce::String::fromUTF8(u8"\u25c0")); // ◀ (collapse to left)
    btnToggleCollapse.setTooltip("Collapse / Expand Navigation Rail");
    btnToggleCollapse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
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
        stepStatuses[currentStep] = StepStatus::Completed;

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
    return !isStepLocked(step);
}

void SoundIdSidebarStepper::setCollapsed(bool collapsed)
{
    if (collapsedState == collapsed) return;
    collapsedState = collapsed;
    btnToggleCollapse.setButtonText(collapsedState ? juce::String::fromUTF8(u8"\u25b6") : juce::String::fromUTF8(u8"\u25c0"));
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
    auto topRow = b.removeFromTop(32).reduced(4, 4);
    btnToggleCollapse.setBounds(topRow.removeFromRight(24));
}

void SoundIdSidebarStepper::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // 1. Sidebar Background & Right Border (SoundID dark card or light surface)
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    auto contentArea = b.reduced(collapsedState ? 4.0f : 10.0f, 8.0f);
    contentArea.removeFromTop(28.0f); // Top bar space for collapse toggle

    // 2. Render Step Rows (4 steps)
    const float rowHeight = collapsedState ? 46.0f : 56.0f;
    const float stepSpacing = 8.0f;

    for (int i = 0; i < 4; ++i)
    {
        Step step = static_cast<Step>(i);
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
    auto status = getStepStatus(step);
    bool isCurrent = (step == currentStep);
    bool isLocked = isStepLocked(step);

    const juce::Colour accentGreen  = SoundIdTheme::accentGreen;
    const juce::Colour accentAmber  = SoundIdTheme::accentAmber;
    const juce::Colour textPrimary  = SoundIdTheme::textPrimary;
    const juce::Colour textMuted    = SoundIdTheme::textMuted;
    const juce::Colour borderSubtle = SoundIdTheme::borderSubtle;

    // Hover background
    if (isHovered && canNavigateTo(step))
    {
        g.setColour(accentGreen.withAlpha(0.08f));
        g.fillRoundedRectangle(rowBounds, 6.0f);
        g.setColour(accentGreen.withAlpha(0.20f));
        g.drawRoundedRectangle(rowBounds.reduced(0.5f), 6.0f, 1.0f);
    }
    else if (isCurrent)
    {
        g.setColour(SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(rowBounds, 6.0f);
        g.setColour(borderSubtle);
        g.drawRoundedRectangle(rowBounds.reduced(0.5f), 6.0f, 1.0f);
    }

    // Circle Badge
    float badgeSize = 24.0f;
    float badgeX = collapsedState ? (rowBounds.getCentreX() - badgeSize * 0.5f) : (rowBounds.getX() + 8.0f);
    float badgeY = rowBounds.getCentreY() - badgeSize * 0.5f;
    auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeSize, badgeSize);

    juce::Colour badgeColour = borderSubtle;
    juce::Colour badgeTextColour = textMuted;

    if (status == StepStatus::Completed)
    {
        badgeColour = accentGreen;
        badgeTextColour = juce::Colours::white;
        g.setColour(badgeColour);
        g.fillEllipse(badgeRect);
    }
    else if (status == StepStatus::Current)
    {
        badgeColour = accentGreen;
        badgeTextColour = accentGreen;
        g.setColour(badgeColour.withAlpha(0.15f));
        g.fillEllipse(badgeRect.expanded(3.0f));
        g.setColour(badgeColour);
        g.drawEllipse(badgeRect, 2.0f);
    }
    else if (status == StepStatus::Warning || status == StepStatus::Skipped)
    {
        badgeColour = accentAmber;
        badgeTextColour = accentAmber;
        g.setColour(badgeColour.withAlpha(0.20f));
        g.fillEllipse(badgeRect);
        g.setColour(badgeColour);
        g.drawEllipse(badgeRect, 1.5f);
    }
    else
    {
        g.setColour(borderSubtle);
        g.drawEllipse(badgeRect, 1.5f);
    }

    if (isLocked)
    {
        badgeColour = badgeColour.withAlpha(0.35f);
        badgeTextColour = badgeTextColour.withAlpha(0.35f);
    }

    // Inner Glyph
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    if (status == StepStatus::Completed)
    {
        g.setColour(juce::Colours::white);
        g.drawText(juce::String::fromUTF8(u8"\u2713"), badgeRect, juce::Justification::centred, false);
    }
    else if (status == StepStatus::Skipped)
    {
        g.setColour(badgeTextColour);
        g.drawText(juce::String::fromUTF8(u8"\u23ed"), badgeRect, juce::Justification::centred, false);
    }
    else
    {
        g.setColour(badgeTextColour);
        g.drawText(juce::String(static_cast<int>(step) + 1), badgeRect, juce::Justification::centred, false);
    }

    // If expanded, draw title and subtitle description
    if (!collapsedState)
    {
        float textLeft = badgeX + badgeSize + 10.0f;
        float textWidth = rowBounds.getRight() - textLeft - 6.0f;

        auto titleArea = juce::Rectangle<float>(textLeft, rowBounds.getY() + 10.0f, textWidth, 18.0f);
        auto descArea  = juce::Rectangle<float>(textLeft, rowBounds.getY() + 28.0f, textWidth, 16.0f);

        g.setFont(juce::FontOptions(12.0f, isCurrent ? juce::Font::bold : juce::Font::plain));
        g.setColour(isCurrent ? textPrimary : (status == StepStatus::Completed ? textPrimary : textMuted));
        
        juce::String title = stepTitles[step];
        if (isLocked) title += juce::String::fromUTF8(u8" \U0001f512");
        g.drawText(title, titleArea, juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions(10.0f));
        g.setColour(textMuted);
        g.drawText(stepDescriptions[step], descArea, juce::Justification::centredLeft, true);
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
        g.drawText(juce::String::fromUTF8(u8"\u25cf Calibrated (SNR ") + juce::String(summaryInfo.loopbackSnrDb, 1) + " dB)",
                   inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String::fromUTF8(u8"\u25cf Loopback: Uncalibrated"),
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
        return stepTitles[*hoveredStep] + " \u2014 " + stepDescriptions[*hoveredStep];
    }
    return {};
}

void SoundIdSidebarStepper::mouseMove(const juce::MouseEvent& event)
{
    auto b = getLocalBounds().toFloat();
    auto contentArea = b.reduced(collapsedState ? 4.0f : 10.0f, 8.0f);
    contentArea.removeFromTop(28.0f);

    const float rowHeight = collapsedState ? 46.0f : 56.0f;
    const float stepSpacing = 8.0f;

    std::optional<Step> foundStep;
    float currentY = contentArea.getY();

    for (int i = 0; i < 4; ++i)
    {
        auto rowRect = juce::Rectangle<float>(contentArea.getX(), currentY, contentArea.getWidth(), rowHeight);
        if (rowRect.contains(event.position))
        {
            foundStep = static_cast<Step>(i);
            break;
        }
        currentY += rowHeight + stepSpacing;
    }

    if (hoveredStep != foundStep)
    {
        hoveredStep = foundStep;
        setMouseCursor(hoveredStep.has_value() && canNavigateTo(*hoveredStep)
                       ? juce::MouseCursor::PointingHandCursor
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
    if (btnToggleCollapse.getBounds().contains(event.getPosition()))
        return;

    if (hoveredStep.has_value() && canNavigateTo(*hoveredStep))
    {
        setCurrentStep(*hoveredStep);
        if (onStepSelected != nullptr)
            onStepSelected(*hoveredStep);
    }
}

} // namespace abdaudiolab::gui
