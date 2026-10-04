#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../SoundIdTheme.h"
#include <map>
#include <vector>
#include <optional>
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @brief Sonarworks SoundID Measure inspired collapsible vertical workflow stepper.
 *
 * Features:
 * - 240px wide in expanded mode:
 *   - Clean step numbers / icons, titles, and subtitle states
 *   - Non-editable session summary card pinned at bottom (Active hardware, loopback calibration SNR, session progress)
 * - 56px wide in collapsed mode:
 *   - Centered circular badges with numbers/glyphs (✓ for completed)
 *   - Hover tooltip card styled like SoundID floating dark pills
 * - Expand / Collapse toggle chevron button with smooth layout transition
 * - Emits onStepSelected callback, 100% API compatible with WorkflowStepperBar
 */
class SoundIdSidebarStepper : public juce::Component,
                              public juce::TooltipClient
{
public:
    juce::String getTooltip() override;
    enum class Step 
    { 
        SystemInfo = 0,
        CalibrateLoopback, 
        HardwareRouting, 
        RunSession, 
        ExportReport 
    };

    enum class StepStatus 
    { 
        Pending, 
        Current, 
        Completed, 
        Skipped,
        Warning 
    };

    struct SessionSummaryInfo
    {
        juce::String hardwareName { "None Selected" };
        juce::String hardwareCategory { "-" };
        bool loopbackCalibrated { false };
        bool loopbackBypassed { false };
        float loopbackSnrDb { 0.0f };
        int pointsMeasured { 0 };
        int totalPointsPlanned { 0 };
    };

    SoundIdSidebarStepper();
    ~SoundIdSidebarStepper() override = default;

    // --- State Machine Logic & Query ---
    [[nodiscard]] Step getCurrentStep() const noexcept { return currentStep; }
    void setCurrentStep(Step targetStep);
    [[nodiscard]] static int getStepBadgeNumber(Step step) noexcept
    {
        switch (step)
        {
            case Step::SystemInfo:        return 0;
            case Step::HardwareRouting:   return 1;
            case Step::CalibrateLoopback: return 2;
            case Step::RunSession:        return 3;
            case Step::ExportReport:      return 4;
            default:                      return 0;
        }
    }

    void setStepStatus(Step step, StepStatus status);
    [[nodiscard]] StepStatus getStepStatus(Step step) const;

    void setStepLocked(Step step, bool locked);
    [[nodiscard]] bool isStepLocked(Step step) const;
    [[nodiscard]] bool canNavigateTo(Step step) const;
    [[nodiscard]] juce::String getStepTitle(Step step) const { auto it = stepTitles.find(step); return it != stepTitles.end() ? it->second : ""; }
    [[nodiscard]] juce::String getStepDescription(Step step) const { auto it = stepDescriptions.find(step); return it != stepDescriptions.end() ? it->second : ""; }

    // --- Collapsible Rail Control ---
    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const noexcept { return collapsedState; }
    [[nodiscard]] int getDesiredWidth() const noexcept { return collapsedState ? 56 : 240; }

    // --- Session Summary Card ---
    void setSessionSummary(const SessionSummaryInfo& info);
    [[nodiscard]] const SessionSummaryInfo& getSessionSummary() const noexcept { return summaryInfo; }

    // Callbacks
    std::function<void(Step targetStep)> onStepSelected;
    std::function<void(bool collapsed)> onCollapseToggled;

protected:
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    class CollapseToggleButton : public juce::Button
    {
    public:
        CollapseToggleButton() : juce::Button("collapseToggle") {}

        void setCollapsed(bool isCollapsed)
        {
            collapsed = isCollapsed;
            repaint();
        }

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);

            juce::Colour bg = shouldDrawButtonAsDown ? AppTheme::SurfaceHover.darker(0.08f)
                            : (shouldDrawButtonAsHighlighted ? AppTheme::SurfaceHover : AppTheme::SurfaceSubtle);
            g.setColour(bg);
            g.fillRoundedRectangle(bounds, 4.0f);

            g.setColour(shouldDrawButtonAsHighlighted ? AppTheme::AccentActive : AppTheme::BorderSubtle);
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

            juce::Path chevron;
            float cx = bounds.getCentreX();
            float cy = bounds.getCentreY();
            g.setColour(shouldDrawButtonAsHighlighted ? AppTheme::AccentActive : AppTheme::TextSecondary);

            if (collapsed)
            {
                chevron.startNewSubPath(cx - 2.5f, cy - 4.5f);
                chevron.lineTo(cx + 2.5f, cy);
                chevron.lineTo(cx - 2.5f, cy + 4.5f);
            }
            else
            {
                chevron.startNewSubPath(cx + 2.5f, cy - 4.5f);
                chevron.lineTo(cx - 2.5f, cy);
                chevron.lineTo(cx + 2.5f, cy + 4.5f);
            }

            g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    private:
        bool collapsed { false };
    };

    Step currentStep { Step::HardwareRouting };
    bool collapsedState { false };
    std::optional<Step> hoveredStep;

    std::map<Step, StepStatus> stepStatuses;
    std::map<Step, juce::String> stepTitles;
    std::map<Step, juce::String> stepDescriptions;
    std::map<Step, bool> lockedSteps;

    SessionSummaryInfo summaryInfo;

    CollapseToggleButton btnToggleCollapse;

    void drawStepRow(juce::Graphics& g, Step step, juce::Rectangle<float> rowBounds, bool isHovered);
    void drawSummaryCard(juce::Graphics& g, juce::Rectangle<float> cardBounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSidebarStepper)
};

} // namespace abdaudiolab::gui
