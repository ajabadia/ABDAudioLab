/**
 * @file WorkflowNavigationController.h
 * @brief Autonomous navigation and step visibility controller for SoundID guided workflow.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <map>

#include "CanonicalWorkflowState.h"
#include "../soundid/SoundIdSidebarStepper.h"
#include "../soundid/SoundIdHardwareCatalogSelector.h"
#include "../drawers/DrawerSetupTab.h"
#include "../NativeCalibrationPanel.h"
#include "../ExportReportPanel.h"
#include "../SoundIdCurvePlotter.h"
#include "../MeasurementHealthPanel.h"
#include "../SoundIdSuiteList.h"
#include "../OperatorStepModalDialog.h"
#include "../CenterSplitterBar.h"

namespace abdaudiolab::gui
{

namespace soundid
{
    class SoundIdTargetView;
    class SoundIdExcitationConfigPanel;
}

/**
 * @enum TargetViewIntegrationMode
 * @brief Runtime feature flag for SoundIdTargetView integration in Step 1 (HardwareRouting).
 */
enum class TargetViewIntegrationMode
{
    Disabled,
    ClassicStep1
};

/**
 * @class WorkflowNavigationController
 * @brief Coordinates the 5 steps of the SoundID workflow (Step 0: Info to Step 4: Export), manages panel visibility and geometry.
 */
class WorkflowNavigationController
{
public:
    using Step = SoundIdSidebarStepper::Step;
    using StepStatus = SoundIdSidebarStepper::StepStatus;

    WorkflowNavigationController(SoundIdSidebarStepper& stepper,
                                 DrawerSetupTab& infoTab,
                                 SoundIdHardwareCatalogSelector& catalog,
                                 NativeCalibrationPanel& calPanel,
                                 ExportReportPanel& exportPanel,
                                 SoundIdCurvePlotter& plotter,
                                 MeasurementHealthPanel& health,
                                 SoundIdSuiteList& suites,
                                 OperatorStepModalDialog& opModal,
                                 CenterSplitterBar& splitter);

    ~WorkflowNavigationController() = default;

    void setStep(Step targetStep);
    [[nodiscard]] Step getCurrentStep() const noexcept { return currentStep; }

    void setStepStatus(Step step, StepStatus status);
    [[nodiscard]] StepStatus getStepStatus(Step step) const;
    [[nodiscard]] bool isCalibrationSkipped() const noexcept { return canonicalWorkflowState.isCalibrationSkipped; }
    [[nodiscard]] const CanonicalWorkflowState& getCanonicalWorkflowState() const noexcept { return canonicalWorkflowState; }

    void setStepLocked(Step step, bool locked);

    void resetToNewSession();
    void layoutStepViews(juce::Rectangle<int> centralBounds, float currentBottomH, bool isSplittingBalanced);

    void setTargetView(soundid::SoundIdTargetView* view) noexcept;
    void setTargetViewIntegrationMode(TargetViewIntegrationMode mode) noexcept;
    [[nodiscard]] TargetViewIntegrationMode getTargetViewIntegrationMode() const noexcept { return targetViewIntegrationMode; }

    void setExcitationConfigPanel(soundid::SoundIdExcitationConfigPanel* panel) noexcept;

    std::function<void(Step newStep)> onStepChanged;

private:
    Step currentStep { Step::HardwareRouting };
    CanonicalWorkflowState canonicalWorkflowState;
    std::map<Step, StepStatus> stepStatuses;

    SoundIdSidebarStepper& sidebarStepper;
    DrawerSetupTab& setupTab;
    SoundIdHardwareCatalogSelector& catalogSelector;
    NativeCalibrationPanel& nativeCalibrationPanel;
    ExportReportPanel& exportReportPanel;
    SoundIdCurvePlotter& curvePlotter;
    MeasurementHealthPanel& healthPanel;
    SoundIdSuiteList& suiteList;
    OperatorStepModalDialog& operatorStepModal;
    CenterSplitterBar& centerSplitterBar;

    soundid::SoundIdTargetView* targetView { nullptr };
    soundid::SoundIdExcitationConfigPanel* excitationConfigPanel { nullptr };
    TargetViewIntegrationMode targetViewIntegrationMode { TargetViewIntegrationMode::ClassicStep1 };
};

} // namespace abdaudiolab::gui
