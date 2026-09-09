/**
 * @file WorkflowNavigationController.h
 * @brief Autonomous navigation and step visibility controller for SoundID guided workflow.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "../soundid/SoundIdSidebarStepper.h"
#include "../soundid/SoundIdHardwareCatalogSelector.h"
#include "../NativeCalibrationPanel.h"
#include "../ExportReportPanel.h"
#include "../SoundIdCurvePlotter.h"
#include "../MeasurementHealthPanel.h"
#include "../SoundIdSuiteList.h"
#include "../OperatorStepModalDialog.h"
#include "../CenterSplitterBar.h"

namespace abdaudiolab::gui
{

/**
 * @class WorkflowNavigationController
 * @brief Coordinates the 4 steps of the SoundID workflow, manages panel visibility and geometry.
 */
class WorkflowNavigationController
{
public:
    using Step = SoundIdSidebarStepper::Step;
    using StepStatus = SoundIdSidebarStepper::StepStatus;

    WorkflowNavigationController(SoundIdSidebarStepper& stepper,
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
    void setStepLocked(Step step, bool locked);

    void resetToNewSession();
    void layoutStepViews(juce::Rectangle<int> centralBounds, float currentBottomH, bool isSplittingBalanced);

    std::function<void(Step newStep)> onStepChanged;

private:
    Step currentStep { Step::HardwareRouting };

    SoundIdSidebarStepper& sidebarStepper;
    SoundIdHardwareCatalogSelector& catalogSelector;
    NativeCalibrationPanel& nativeCalibrationPanel;
    ExportReportPanel& exportReportPanel;
    SoundIdCurvePlotter& curvePlotter;
    MeasurementHealthPanel& healthPanel;
    SoundIdSuiteList& suiteList;
    OperatorStepModalDialog& operatorStepModal;
    CenterSplitterBar& centerSplitterBar;
};

} // namespace abdaudiolab::gui
