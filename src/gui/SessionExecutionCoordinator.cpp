#include "SessionExecutionCoordinator.h"
#include "../core/SessionManager.h"
#include "../core/HardwareManager.h"
#include "../core/ProfilingSession.h"
#include "SoundIdSuiteList.h"
#include "MeasurementHealthPanel.h"
#include "OperatorStepModalDialog.h"
#include "SoundIdTheme.h"
#include <cmath>

namespace abdaudiolab {
namespace gui {

SessionExecutionCoordinator::SessionExecutionCoordinator(core::ProfilingSequencer& seq,
                                                         core::SessionManager& sm,
                                                         SoundIdCurvePlotter& plotter)
    : sequencer(seq), sessionManager(sm), curvePlotter(plotter)
{
}

SessionExecutionCoordinator::~SessionExecutionCoordinator()
{
    unbindSequencerCallbacks();
}

void SessionExecutionCoordinator::setCoordinatedViews(SoundIdSuiteList* suiteList,
                                                     MeasurementHealthPanel* healthPanel,
                                                     OperatorStepModalDialog* operatorModal,
                                                     juce::Label* manualPromptLabel,
                                                     juce::Button* btnStepBack,
                                                     juce::Button* btnRepeatStep,
                                                     juce::Button* confirmManualButton)
{
    viewSuiteList = suiteList;
    viewHealthPanel = healthPanel;
    viewOperatorModal = operatorModal;
    viewManualPromptLabel = manualPromptLabel;
    viewBtnStepBack = btnStepBack;
    viewBtnRepeatStep = btnRepeatStep;
    viewConfirmManualButton = confirmManualButton;
}

void SessionExecutionCoordinator::setHardwareContext(core::HardwareManager* hwMgr, const juce::String& selectedHwId)
{
    hardwareManager = hwMgr;
    currentHardwareId = selectedHwId;
}

void SessionExecutionCoordinator::wireSequencerCallbacks()
{
    // 1. Operator Step Callback
    sequencer.setOperatorStepCallback([this](const core::TestCase& tc, int stepIndex, int totalSteps) {
        handleOperatorStep(tc, stepIndex, totalSteps);
    });

    // 2. Progress and State Transition Callback
    sequencer.setProgressCallback([this](float progress, const juce::String& task, core::SequencerState state) {
        handleProgress(progress, task, state);
    });

    // 3. PreScan Callback
    sequencer.setPreScanCallback([this](const math::PreScanResult& preScan) {
        handlePreScan(preScan);
    });

    // 4. Test Index Progress Callback
    sequencer.setTestIndexCallback([this](int queueIndex, int currentPoint, int totalPoints) {
        handleTestIndex(queueIndex, currentPoint, totalPoints);
    });

    // 5. Point Measured Callback
    sequencer.setPointMeasuredCallback([this](const exporting::MeasuredPoint& pt) {
        handlePointMeasured(pt);
    });

    // 6. Modulation Node Measured Callback
    sequencer.setModulationNodeMeasuredCallback([this](const math::ModulationNode& node) {
        handleModulationNodeMeasured(node);
    });
}

void SessionExecutionCoordinator::unbindSequencerCallbacks()
{
    sequencer.setOperatorStepCallback(nullptr);
    sequencer.setProgressCallback(nullptr);
    sequencer.setPreScanCallback(nullptr);
    sequencer.setTestIndexCallback(nullptr);
    sequencer.setPointMeasuredCallback(nullptr);
    sequencer.setModulationNodeMeasuredCallback(nullptr);
}

void SessionExecutionCoordinator::confirmOperatorStep()
{
    sequencer.confirmOperatorStep();
    if (viewConfirmManualButton != nullptr)
        viewConfirmManualButton->setEnabled(false);
}

void SessionExecutionCoordinator::repeatCurrentStep()
{
    sequencer.repeatCurrentStep();
}

void SessionExecutionCoordinator::stepBack()
{
    sequencer.stepBack();
}

void SessionExecutionCoordinator::triggerStartSession(const core::ProfilingSession& session,
                                                      const juce::File& exportDir,
                                                      const juce::String& baseName,
                                                      bool isPatching)
{
    isPatchingSession = isPatching;
    wireSequencerCallbacks();

    if (onExecutionStateChanged)
        onExecutionStateChanged(true);

    sequencer.startSession(session, exportDir, baseName);
}

void SessionExecutionCoordinator::triggerStopSession()
{
    sequencer.stopSession();
    unbindSequencerCallbacks();

    if (viewConfirmManualButton != nullptr) viewConfirmManualButton->setVisible(false);
    if (viewBtnStepBack != nullptr)         viewBtnStepBack->setVisible(false);
    if (viewBtnRepeatStep != nullptr)       viewBtnRepeatStep->setVisible(false);
    if (viewManualPromptLabel != nullptr)   viewManualPromptLabel->setVisible(false);
    if (viewOperatorModal != nullptr)       viewOperatorModal->setVisible(false);
    if (viewSuiteList != nullptr)
    {
        viewSuiteList->setVisible(true);
        viewSuiteList->setSessionRunning(false);

        // If stopped midway, mark any running item as Incomplete
        for (int i = 0; i < viewSuiteList->getQueueSize(); ++i)
        {
            if (viewSuiteList->getQueue()[static_cast<size_t>(i)].status == gui::QueueItemStatus::Running)
            {
                viewSuiteList->updateItemStatus(i, gui::QueueItemStatus::Incomplete, totalPointsMeasured);
                break;
            }
        }
    }

    curvePlotter.setMeasuringState(false);

    if (onExecutionStateChanged)
        onExecutionStateChanged(false);
}

void SessionExecutionCoordinator::handleOperatorStep(const core::TestCase& tc, int stepIndex, int totalSteps)
{
    juce::MessageManager::callAsync([this, tc, stepIndex, totalSteps] {
        bool isAuto = false;
        if (hardwareManager != nullptr)
        {
            const auto* contract = hardwareManager->findContractById(currentHardwareId.toStdString());
            if (contract != nullptr && (contract->deviceType == "AUTOMATED_SYSEX" || contract->deviceType == "AUTOMATED_MIDI_CC"))
                isAuto = true;
        }

        if (viewOperatorModal != nullptr)
        {
            viewOperatorModal->setAutomatedMode(isAuto);
            viewOperatorModal->setStepInfo(juce::String(tc.testId), stepIndex, totalSteps, tc.parameterSteps);
            viewOperatorModal->setVisible(true);
        }

        if (viewSuiteList != nullptr)
            viewSuiteList->setVisible(false);
    });
}

void SessionExecutionCoordinator::handleProgress(float progress, const juce::String& task, core::SequencerState state)
{
    juce::MessageManager::callAsync([this, progress, task, state] {
        juce::ignoreUnused(progress);

        if (state == core::SequencerState::WaitingForOperator)
        {
            if (viewOperatorModal != nullptr)
                viewOperatorModal->setMeasuringState(false);

            if (viewManualPromptLabel != nullptr)
            {
                viewManualPromptLabel->setText(task, juce::dontSendNotification);
                viewManualPromptLabel->setVisible(true);
            }

            bool modalVisible = (viewOperatorModal != nullptr && viewOperatorModal->isVisible());
            if (modalVisible)
            {
                if (viewConfirmManualButton != nullptr) viewConfirmManualButton->setVisible(false);
                if (viewBtnRepeatStep != nullptr)       viewBtnRepeatStep->setVisible(false);
                if (viewBtnStepBack != nullptr)         viewBtnStepBack->setVisible(false);
            }
            else
            {
                if (viewConfirmManualButton != nullptr)
                {
                    viewConfirmManualButton->setVisible(true);
                    viewConfirmManualButton->setEnabled(true);
                }
                if (viewBtnRepeatStep != nullptr)
                {
                    viewBtnRepeatStep->setVisible(true);
                    viewBtnRepeatStep->setEnabled(true);
                }
                if (viewBtnStepBack != nullptr)
                {
                    viewBtnStepBack->setVisible(totalPointsMeasured > 1);
                    viewBtnStepBack->setEnabled(totalPointsMeasured > 1);
                }
            }
        }
        else if (state == core::SequencerState::CaptureAndAnalyze ||
                 state == core::SequencerState::InjectStimulus ||
                 state == core::SequencerState::InitiateTestCase)
        {
            if (viewOperatorModal != nullptr)
                viewOperatorModal->setMeasuringState(true);

            if (viewConfirmManualButton != nullptr) viewConfirmManualButton->setEnabled(false);
            if (viewBtnRepeatStep != nullptr)       viewBtnRepeatStep->setEnabled(false);
            if (viewBtnStepBack != nullptr)         viewBtnStepBack->setEnabled(false);

            if (viewManualPromptLabel != nullptr)
            {
                viewManualPromptLabel->setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
                viewManualPromptLabel->setText(task, juce::dontSendNotification);
                viewManualPromptLabel->setVisible(true);
            }

            if (viewSuiteList != nullptr && viewSuiteList->getQueueSize() > 0 && state != core::SequencerState::InitiateTestCase)
            {
                viewSuiteList->updateItemStatus(0, gui::QueueItemStatus::Running, totalPointsMeasured);
            }
        }
        else if (state == core::SequencerState::Finished)
        {
            if (viewOperatorModal != nullptr)
                viewOperatorModal->dismiss();

            if (viewSuiteList != nullptr)
            {
                viewSuiteList->setVisible(true);
                viewSuiteList->setSessionRunning(false);
            }

            if (viewConfirmManualButton != nullptr) viewConfirmManualButton->setVisible(false);
            if (viewBtnRepeatStep != nullptr)       viewBtnRepeatStep->setVisible(false);
            if (viewBtnStepBack != nullptr)         viewBtnStepBack->setVisible(false);

            curvePlotter.setMeasuringState(false);

            if (isPatchingSession)
            {
                isPatchingSession = false;
                if (viewManualPromptLabel != nullptr)
                {
                    viewManualPromptLabel->setText("Re-Measurement Patching completed successfully.", juce::dontSendNotification);
                    viewManualPromptLabel->setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
                    viewManualPromptLabel->setVisible(true);
                }

                if (viewSuiteList != nullptr)
                {
                    for (int i = 0; i < viewSuiteList->getQueueSize(); ++i)
                    {
                        const auto& item = viewSuiteList->getQueue()[static_cast<size_t>(i)];
                        bool allDone = true;
                        for (auto st : item.pointStatuses)
                        {
                            if (st != gui::PointStatus::Completed) { allDone = false; break; }
                        }
                        if (allDone && !item.isSkipped && !item.pointStatuses.empty())
                        {
                            viewSuiteList->updateItemStatus(i, gui::QueueItemStatus::Completed);
                        }
                    }
                }
            }
            else
            {
                if (viewManualPromptLabel != nullptr)
                {
                    if (sequencer.isLinearBypassDetected())
                    {
                        viewManualPromptLabel->setText("Module linear bypass detected. Calibration succeeded.", juce::dontSendNotification);
                        viewManualPromptLabel->setColour(juce::Label::textColourId, gui::SoundIdTheme::accentGreen);
                        viewManualPromptLabel->setVisible(true);
                    }
                    else
                    {
                        viewManualPromptLabel->setVisible(false);
                    }
                }

                if (viewSuiteList != nullptr)
                {
                    for (int i = 0; i < viewSuiteList->getQueueSize(); ++i)
                    {
                        if (!viewSuiteList->getQueue()[static_cast<size_t>(i)].isSkipped)
                            viewSuiteList->updateItemStatus(i, gui::QueueItemStatus::Completed);
                    }
                }
            }

            if (onExecutionStateChanged)
                onExecutionStateChanged(false);

            if (onSessionFinished)
                onSessionFinished(isPatchingSession);

            if (onSessionAutoSaveRequested)
                onSessionAutoSaveRequested();
        }
        else if (state == core::SequencerState::ErrorState)
        {
            juce::String errorMsg = task.isNotEmpty() ? task : "Session Halted: Insufficient Audio Signal. Connect patch cable (DAC Out 1 -> ADC In 1) & retry.";

            if (viewManualPromptLabel != nullptr)
            {
                viewManualPromptLabel->setText(errorMsg, juce::dontSendNotification);
                viewManualPromptLabel->setColour(juce::Label::textColourId, gui::SoundIdTheme::accentRed);
                viewManualPromptLabel->setVisible(true);
            }

            if (viewSuiteList != nullptr)
            {
                viewSuiteList->setSessionRunning(false);
                if (viewSuiteList->getQueueSize() > 0)
                    viewSuiteList->updateItemStatus(0, gui::QueueItemStatus::Invalidated);
            }

            curvePlotter.setMeasuringState(false);

            if (viewConfirmManualButton != nullptr) viewConfirmManualButton->setVisible(false);
            if (viewBtnRepeatStep != nullptr)       viewBtnRepeatStep->setVisible(false);
            if (viewBtnStepBack != nullptr)         viewBtnStepBack->setVisible(false);

            if (viewOperatorModal != nullptr)
                viewOperatorModal->dismiss();

            if (onExecutionStateChanged)
                onExecutionStateChanged(false);

            if (onExecutionErrorTriggered)
                onExecutionErrorTriggered(errorMsg);
        }
    });
}

void SessionExecutionCoordinator::handlePreScan(const math::PreScanResult& preScan)
{
    juce::MessageManager::callAsync([this, preScan] {
        curvePlotter.setPreScanTrajectory(preScan);
    });
}

void SessionExecutionCoordinator::handleTestIndex(int queueIndex, int currentPoint, int totalPoints)
{
    juce::MessageManager::callAsync([this, queueIndex, currentPoint, totalPoints] {
        if (!isPatchingSession && viewSuiteList != nullptr)
        {
            for (int i = 0; i < queueIndex; ++i)
            {
                if (!viewSuiteList->getQueue()[static_cast<size_t>(i)].isSkipped &&
                    viewSuiteList->getQueue()[static_cast<size_t>(i)].status != gui::QueueItemStatus::Completed)
                {
                    viewSuiteList->updateItemStatus(i, gui::QueueItemStatus::Completed);
                }
            }
        }

        if (viewSuiteList != nullptr && queueIndex >= 0 && queueIndex < viewSuiteList->getQueueSize())
        {
            if (isPatchingSession)
            {
                viewSuiteList->setPointStatus(queueIndex, currentPoint - 1, gui::PointStatus::Running);
            }
            else
            {
                viewSuiteList->updateItemStatus(queueIndex, gui::QueueItemStatus::Running, currentPoint);
            }
        }

        float progress = totalPoints > 1 ? (static_cast<float>(currentPoint) / static_cast<float>(totalPoints)) : 0.5f;
        curvePlotter.setMeasuringState(true, progress);
    });
}

void SessionExecutionCoordinator::handlePointMeasured(const exporting::MeasuredPoint& pt)
{
    if (isPatchingSession && pt.globalIndex >= 0 && pt.globalIndex < static_cast<int>(sessionManager.getPointCount()))
    {
        sessionManager.patchMeasuredPoint(static_cast<size_t>(pt.globalIndex), pt);
        curvePlotter.patchPoint(pt.globalIndex, pt);
    }
    else
    {
        curvePlotter.addMeasuredPoint(pt);
        sessionManager.addMeasuredPoint(pt);
        totalPointsMeasured++;
    }

    if (viewSuiteList != nullptr && pt.queueIndex >= 0 && pt.pointIndexInTest >= 1)
    {
        viewSuiteList->setPointStatus(pt.queueIndex, pt.pointIndexInTest - 1, gui::PointStatus::Completed);
        viewSuiteList->setPointSelected(pt.queueIndex, pt.pointIndexInTest - 1, false);
    }

    float snrDb = pt.muSigmaValue.stdDev > 0.0001f ? (20.0f * std::log10(std::max(1e-4f, pt.muSigmaValue.mean) / pt.muSigmaValue.stdDev)) : 32.0f;
    float noiseDb = (pt.secondaryValue.mean < 0.0f) ? pt.secondaryValue.mean : -92.0f;

    if (viewHealthPanel != nullptr)
    {
        int totalPts = (viewSuiteList != nullptr) ? viewSuiteList->getTotalPointCount() : static_cast<int>(sessionManager.getPointCount());
        viewHealthPanel->setMeasurementHealth(snrDb, noiseDb, static_cast<int>(sessionManager.getPointCount()), totalPts);
        viewHealthPanel->setLatestTestId(pt.testId);
    }

    if (onSessionAutoSaveRequested)
        onSessionAutoSaveRequested();
}

void SessionExecutionCoordinator::handleModulationNodeMeasured(const math::ModulationNode& node)
{
    curvePlotter.updateModulationNode(node);
}

} // namespace gui
} // namespace abdaudiolab
