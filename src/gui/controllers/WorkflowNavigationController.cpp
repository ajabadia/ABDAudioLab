/**
 * @file WorkflowNavigationController.cpp
 * @brief Implementation of autonomous navigation and step visibility controller.
 * @author ABDSynths
 * @date 2026
 */

#include "WorkflowNavigationController.h"

namespace abdaudiolab::gui
{

WorkflowNavigationController::WorkflowNavigationController(SoundIdSidebarStepper& stepper,
                                                           SoundIdHardwareCatalogSelector& catalog,
                                                           NativeCalibrationPanel& calPanel,
                                                           ExportReportPanel& exportPanel,
                                                           SoundIdCurvePlotter& plotter,
                                                           MeasurementHealthPanel& health,
                                                           SoundIdSuiteList& suites,
                                                           OperatorStepModalDialog& opModal,
                                                           CenterSplitterBar& splitter)
    : sidebarStepper(stepper),
      catalogSelector(catalog),
      nativeCalibrationPanel(calPanel),
      exportReportPanel(exportPanel),
      curvePlotter(plotter),
      healthPanel(health),
      suiteList(suites),
      operatorStepModal(opModal),
      centerSplitterBar(splitter)
{
    sidebarStepper.onStepSelected = [this](Step step) {
        setStep(step);
    };
}

void WorkflowNavigationController::setStep(Step targetStep)
{
    if (currentStep == targetStep) return;

    currentStep = targetStep;
    sidebarStepper.setCurrentStep(targetStep);

    if (onStepChanged)
        onStepChanged(targetStep);
}

void WorkflowNavigationController::setStepStatus(Step step, StepStatus status)
{
    sidebarStepper.setStepStatus(step, status);
}

void WorkflowNavigationController::setStepLocked(Step step, bool locked)
{
    juce::ignoreUnused(step, locked);
    if (step == Step::HardwareRouting)
        catalogSelector.setHardwareLocked(locked);
}

void WorkflowNavigationController::resetToNewSession()
{
    catalogSelector.setHardwareLocked(false);
    sidebarStepper.setStepStatus(Step::HardwareRouting, StepStatus::Current);
    sidebarStepper.setStepStatus(Step::CalibrateLoopback, StepStatus::Pending);
    sidebarStepper.setStepStatus(Step::RunSession, StepStatus::Pending);
    sidebarStepper.setStepStatus(Step::ExportReport, StepStatus::Pending);

    currentStep = Step::HardwareRouting;
    sidebarStepper.setCurrentStep(Step::HardwareRouting);

    gui::SoundIdSidebarStepper::SessionSummaryInfo emptySummary;
    sidebarStepper.setSessionSummary(emptySummary);

    if (onStepChanged)
        onStepChanged(Step::HardwareRouting);
}

void WorkflowNavigationController::layoutStepViews(juce::Rectangle<int> bounds, float currentBottomH, bool isSplittingBalanced)
{
    if (currentStep == Step::HardwareRouting)
    {
        nativeCalibrationPanel.setVisible(false);
        exportReportPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        catalogSelector.setVisible(true);
        catalogSelector.setBounds(bounds);
    }
    else if (currentStep == Step::CalibrateLoopback)
    {
        catalogSelector.setVisible(false);
        exportReportPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        nativeCalibrationPanel.setVisible(true);
        nativeCalibrationPanel.setBounds(bounds);
    }
    else if (currentStep == Step::ExportReport)
    {
        catalogSelector.setVisible(false);
        nativeCalibrationPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        exportReportPanel.setVisible(true);
        exportReportPanel.setBounds(bounds);
    }
    else // Step::RunSession
    {
        catalogSelector.setVisible(false);
        nativeCalibrationPanel.setVisible(false);
        exportReportPanel.setVisible(false);

        if (!operatorStepModal.isVisible())
            suiteList.setVisible(true);
        curvePlotter.setVisible(true);
        healthPanel.setVisible(true);

        // Bottom Area Layout
        int bottomH = static_cast<int>(std::round(currentBottomH));
        if (operatorStepModal.isVisible() && operatorStepModal.isCollapsed)
        {
            bottomH = 34;
        }
        else
        {
            int minGraphAreaH = 180 + 32 + 12;
            int maxBottomH = bounds.getHeight() - minGraphAreaH;
            bottomH = juce::jlimit(36, std::max(36, maxBottomH), bottomH);
        }

        auto bottomArea = bounds.removeFromBottom(bottomH);
        suiteList.setBounds(bottomArea);
        operatorStepModal.setBounds(bottomArea);

        // Splitter bar
        if (isSplittingBalanced && (!operatorStepModal.isVisible() || !operatorStepModal.isCollapsed))
        {
            centerSplitterBar.setVisible(true);
            centerSplitterBar.setBounds(bounds.removeFromBottom(8));
            bounds.removeFromBottom(4);
        }
        else
        {
            centerSplitterBar.setVisible(false);
            bounds.removeFromBottom(8);
        }

        // Center visualizer
        healthPanel.setBounds(bounds.removeFromTop(26));
        bounds.removeFromTop(6);
        curvePlotter.setBounds(bounds);
    }
}

} // namespace abdaudiolab::gui
