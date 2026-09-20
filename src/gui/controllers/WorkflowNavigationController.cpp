/**
 * @file WorkflowNavigationController.cpp
 * @brief Implementation of autonomous navigation and step visibility controller.
 * @author ABDSynths
 * @date 2026
 */

#include "WorkflowNavigationController.h"
#include "../soundid/SoundIdTargetView.h"
#include "../soundid/SoundIdExcitationConfigPanel.h"

namespace abdaudiolab::gui
{

WorkflowNavigationController::WorkflowNavigationController(SoundIdSidebarStepper& stepper,
                                                           DrawerSetupTab& infoTab,
                                                           SoundIdHardwareCatalogSelector& catalog,
                                                           NativeCalibrationPanel& calPanel,
                                                           ExportReportPanel& exportPanel,
                                                           SoundIdCurvePlotter& plotter,
                                                           MeasurementHealthPanel& health,
                                                           SoundIdSuiteList& suites,
                                                           OperatorStepModalDialog& opModal,
                                                           CenterSplitterBar& splitter)
    : sidebarStepper(stepper),
      setupTab(infoTab),
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

void WorkflowNavigationController::setTargetView(soundid::SoundIdTargetView* view) noexcept
{
    targetView = view;
}

void WorkflowNavigationController::setTargetViewIntegrationMode(TargetViewIntegrationMode mode) noexcept
{
    targetViewIntegrationMode = mode;
}

void WorkflowNavigationController::setExcitationConfigPanel(soundid::SoundIdExcitationConfigPanel* panel) noexcept
{
    excitationConfigPanel = panel;
}

void WorkflowNavigationController::setStep(Step targetStep)
{
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
    sidebarStepper.setStepStatus(Step::SystemInfo, StepStatus::Completed);
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
    if (currentStep == Step::SystemInfo)
    {
        catalogSelector.setVisible(false);
        if (targetView != nullptr)
            targetView->setVisible(false);
        if (excitationConfigPanel != nullptr)
            excitationConfigPanel->setVisible(false);
        nativeCalibrationPanel.setVisible(false);
        exportReportPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        setupTab.setVisible(true);
        // Center studio topology card nicely with responsive width
        auto infoBounds = bounds;
        int maxW = std::min(infoBounds.getWidth(), 880);
        int x = infoBounds.getX() + (infoBounds.getWidth() - maxW) / 2;
        setupTab.setBounds(x, infoBounds.getY(), maxW, infoBounds.getHeight());
    }
    else if (currentStep == Step::HardwareRouting)
    {
        setupTab.setVisible(false);
        nativeCalibrationPanel.setVisible(false);
        if (excitationConfigPanel != nullptr)
            excitationConfigPanel->setVisible(false);
        exportReportPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        catalogSelector.setVisible(true);

        if (targetView != nullptr && targetViewIntegrationMode == TargetViewIntegrationMode::ClassicStep1)
        {
            int targetW = std::min(520, std::max(360, bounds.getWidth() * 42 / 100));
            auto targetArea = bounds.removeFromRight(targetW);
            bounds.removeFromRight(12);

            catalogSelector.setBounds(bounds);
            targetView->setBounds(targetArea);
            targetView->setVisible(true);
        }
        else
        {
            if (targetView != nullptr)
                targetView->setVisible(false);

            catalogSelector.setBounds(bounds);
        }
    }
    else if (currentStep == Step::CalibrateLoopback)
    {
        setupTab.setVisible(false);
        catalogSelector.setVisible(false);
        if (targetView != nullptr)
            targetView->setVisible(false);
        exportReportPanel.setVisible(false);
        suiteList.setVisible(false);
        operatorStepModal.setVisible(false);
        centerSplitterBar.setVisible(false);
        healthPanel.setVisible(false);
        curvePlotter.setVisible(false);

        if (excitationConfigPanel != nullptr)
        {
            int panelW = std::min(580, std::max(380, bounds.getWidth() * 45 / 100));
            auto panelArea = bounds.removeFromRight(panelW);
            bounds.removeFromRight(12);

            nativeCalibrationPanel.setVisible(true);
            nativeCalibrationPanel.setBounds(bounds);

            excitationConfigPanel->setVisible(true);
            excitationConfigPanel->setBounds(panelArea);
        }
        else
        {
            nativeCalibrationPanel.setVisible(true);
            nativeCalibrationPanel.setBounds(bounds);
        }
    }
    else if (currentStep == Step::ExportReport)
    {
        setupTab.setVisible(false);
        catalogSelector.setVisible(false);
        if (targetView != nullptr)
            targetView->setVisible(false);
        if (excitationConfigPanel != nullptr)
            excitationConfigPanel->setVisible(false);
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
        setupTab.setVisible(false);
        catalogSelector.setVisible(false);
        if (targetView != nullptr)
            targetView->setVisible(false);
        if (excitationConfigPanel != nullptr)
            excitationConfigPanel->setVisible(false);
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
