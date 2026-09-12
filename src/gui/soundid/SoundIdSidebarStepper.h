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

    void setStepStatus(Step step, StepStatus status);
    [[nodiscard]] StepStatus getStepStatus(Step step) const;

    void setStepLocked(Step step, bool locked);
    [[nodiscard]] bool isStepLocked(Step step) const;
    [[nodiscard]] bool canNavigateTo(Step step) const;

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

private:
    Step currentStep { Step::SystemInfo };
    bool collapsedState { false };
    std::optional<Step> hoveredStep;

    std::map<Step, StepStatus> stepStatuses;
    std::map<Step, juce::String> stepTitles;
    std::map<Step, juce::String> stepDescriptions;
    std::map<Step, bool> lockedSteps;

    SessionSummaryInfo summaryInfo;

    juce::TextButton btnToggleCollapse;

    void drawStepRow(juce::Graphics& g, Step step, juce::Rectangle<float> rowBounds, bool isHovered);
    void drawSummaryCard(juce::Graphics& g, juce::Rectangle<float> cardBounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdSidebarStepper)
};

} // namespace abdaudiolab::gui
