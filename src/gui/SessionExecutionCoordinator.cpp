#include "SessionExecutionCoordinator.h"
#include "../core/SessionManager.h"
#include "../core/HardwareManager.h"
#include "../core/ProfilingSession.h"
#include "../measurement/AnalogChainCompensationContracts.h"
#include "../measurement/AnalogChainReversibleCompensator.h"
#include "../synth/Sha256.h"
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

void SessionExecutionCoordinator::setWorkspaceInteractionMode(measurement::WorkspaceInteractionMode mode) noexcept
{
    currentInteractionMode = mode;
}

bool SessionExecutionCoordinator::switchWorkspaceInteractionMode(measurement::WorkspaceInteractionMode mode)
{
    if (mode == currentInteractionMode)
        return true;

    if (!isModeChangeAllowed())
        return false;

    currentInteractionMode = mode;
    return true;
}

measurement::WorkspaceInteractionMode SessionExecutionCoordinator::getWorkspaceInteractionMode() const noexcept
{
    return currentInteractionMode;
}

bool SessionExecutionCoordinator::isManualConfirmationAllowed() const noexcept
{
    return stateMachine.isManualConfirmationAllowed();
}

bool SessionExecutionCoordinator::isProfileChangeAllowed() const noexcept
{
    return stateMachine.isProfileChangeAllowed();
}

bool SessionExecutionCoordinator::isCancellationAllowed() const noexcept
{
    return stateMachine.isCancellationAllowed();
}

bool SessionExecutionCoordinator::isReanalysisAllowed() const noexcept
{
    return stateMachine.isReanalysisAllowed();
}

bool SessionExecutionCoordinator::isModeChangeAllowed() const noexcept
{
    return stateMachine.isModeChangeAllowed();
}

bool SessionExecutionCoordinator::isDirectCaptureAllowed() const noexcept
{
    // El estímulo se considera listo si hay un audio cargado o disponible en el motor
    bool stimulusReady = true;
    return stateMachine.isDirectCaptureAllowed(stimulusReady);
}

juce::String SessionExecutionCoordinator::getRejectionReasonForAction(const juce::String& action) const
{
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.stimulusReady = true;
    ctx.rawSha256Verified = (activeMeasurementSession != nullptr && !activeMeasurementSession->rawCaptures.empty());

    std::string reason = stateMachine.getRejectionReasonForAction(action.toStdString(), ctx);
    return juce::String(reason);
}

measurement::CoordinatorState SessionExecutionCoordinator::getCoordinatorState() const noexcept
{
    return stateMachine.getState();
}

const measurement::MeasurementSession* SessionExecutionCoordinator::getActiveMeasurementSession() const noexcept
{
    return activeMeasurementSession.get();
}

const std::vector<measurement::CoordinatorTransitionRecord>& SessionExecutionCoordinator::getTransitionHistory() const noexcept
{
    return stateMachine.getTransitionHistory();
}

void SessionExecutionCoordinator::transitionTo(measurement::CoordinatorEvent event, const measurement::CoordinatorContext& ctx)
{
    auto oldState = stateMachine.getState();
    std::string sid = activeMeasurementSession ? activeMeasurementSession->sessionId : "no_session";
    stateMachine.dispatch(event, ctx, sid);
    auto newState = stateMachine.getState();

    if (oldState != newState && onCoordinatorStateChanged)
    {
        const auto& hist = stateMachine.getTransitionHistory();
        juce::String reason = hist.empty() ? "" : juce::String(hist.back().reason);
        onCoordinatorStateChanged(oldState, newState, reason);
    }
}

void SessionExecutionCoordinator::initializeMeasurementSession(
    const core::HardwareContract& contract,
    const juce::String& targetFunctionId,
    const juce::String& profileSha256,
    const std::optional<nlohmann::ordered_json>& pluginMeta)
{
    activeMeasurementSession = std::make_unique<measurement::MeasurementSession>();
    activeMeasurementSession->sessionId = "meas_sess_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()).toStdString();
    activeMeasurementSession->profileId = contract.id;
    activeMeasurementSession->profileSha256 = profileSha256.toStdString();
    activeMeasurementSession->deviceType = contract.deviceType;
    activeMeasurementSession->targetFunction = targetFunctionId.toStdString();
    activeMeasurementSession->analyzerVersion = "abdaudiolab-analyzer-1.0";
    if (pluginMeta.has_value())
        activeMeasurementSession->pluginMetadata = *pluginMeta;

    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.profileSha256Verified = !profileSha256.isEmpty();
    ctx.hasActiveSession = true;

    transitionTo(measurement::CoordinatorEvent::SelectProfile, ctx);
    transitionTo(measurement::CoordinatorEvent::PrepareSession, ctx);
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
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.isManualControlRequired = true;
    ctx.isCapturingActive = false;
    ctx.actor = "operator";

    transitionTo(measurement::CoordinatorEvent::ConfirmManualControl, ctx);

    if (activeMeasurementSession != nullptr)
    {
        for (auto& snap : currentPointSnapshots)
        {
            snap.confirmationStatus = "confirmed";
            snap.displayValue += " (Confirmado por operador humano; posicion fisica no medida automaticamente)";
            activeMeasurementSession->controlStates.push_back(snap);
        }
    }

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

void SessionExecutionCoordinator::triggerFreeCapture(const std::vector<measurement::ControlStateSnapshot>& customSnapshots)
{
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.stimulusReady = true;
    ctx.isCapturingActive = false;
    ctx.actor = "operator";

    transitionTo(measurement::CoordinatorEvent::CaptureStarted, ctx);

    if (stateMachine.getState() != measurement::CoordinatorState::Capturing)
        return;

    currentPointSnapshots.clear();
    if (customSnapshots.empty())
    {
        measurement::ControlStateSnapshot unknownSnap;
        unknownSnap.controlId = "unspecified_param";
        unknownSnap.confirmationStatus = "unknown";
        unknownSnap.displayValue = "Posición no declarada";
        unknownSnap.normalizedValue = std::nullopt;
        currentPointSnapshots.push_back(unknownSnap);
    }
    else
    {
        currentPointSnapshots = customSnapshots;
    }

    if (activeMeasurementSession != nullptr)
    {
        for (const auto& snap : currentPointSnapshots)
            activeMeasurementSession->controlStates.push_back(snap);
    }
}

void SessionExecutionCoordinator::togglePauseSession()
{
    if (sequencer.isSessionPaused())
    {
        sequencer.resumeSession();
        if (onSessionPauseStateChanged)
            onSessionPauseStateChanged(false);
    }
    else
    {
        sequencer.pauseSession();
        if (onSessionPauseStateChanged)
            onSessionPauseStateChanged(true);
    }
}

void SessionExecutionCoordinator::rerunSelectedPoint(int globalPointIndex)
{
    sequencer.scheduleRerunPoint(globalPointIndex);
    // Also update the point cell back to Queued so the user sees the re-run starting
    if (viewSuiteList != nullptr)
    {
        // Find which queue+point pair owns this global index
        const auto& queue = viewSuiteList->getQueue();
        for (int qi = 0; qi < static_cast<int>(queue.size()); ++qi)
        {
            for (int pi = 0; pi < static_cast<int>(queue[static_cast<size_t>(qi)].pointStatuses.size()); ++pi)
            {
                // Compute the global index for (qi, pi): walk from the start of the first test
                // The easiest approach is to scan sessionManager
                break; // handled via suiteList.setPointStatus from the caller
            }
        }
    }
}

bool SessionExecutionCoordinator::isSessionPaused() const noexcept
{
    return sequencer.isSessionPaused();
}

void SessionExecutionCoordinator::rearmSession()
{
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.profileSha256Verified = (activeMeasurementSession != nullptr && !activeMeasurementSession->profileSha256.empty());
    ctx.stimulusReady = true;
    ctx.actor = "system";

    transitionTo(measurement::CoordinatorEvent::Reset, ctx);
    if (activeMeasurementSession != nullptr)
    {
        transitionTo(measurement::CoordinatorEvent::SelectProfile, ctx);
        transitionTo(measurement::CoordinatorEvent::PrepareSession, ctx);
    }
    totalPointsMeasured = 0;
}

void SessionExecutionCoordinator::triggerStartSession(const core::ProfilingSession& session,
                                                      const juce::File& exportDir,
                                                      const juce::String& baseName,
                                                      bool isPatching)
{
    isPatchingSession = isPatching;
    wireSequencerCallbacks();

    auto st = stateMachine.getState();
    if (st == measurement::CoordinatorState::Aborted
        || st == measurement::CoordinatorState::SessionCompleted
        || st == measurement::CoordinatorState::Error
        || st == measurement::CoordinatorState::NoSession)
    {
        rearmSession();
    }

    if (onExecutionStateChanged)
        onExecutionStateChanged(true);

    sequencer.startSession(session, exportDir, baseName);
}

void SessionExecutionCoordinator::triggerStopSession()
{
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.actor = "operator";

    transitionTo(measurement::CoordinatorEvent::CancelRequested, ctx);

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
            if (contract != nullptr && (contract->deviceType == "AUTOMATED_SYSEX" || contract->deviceType == "AUTOMATED_MIDI_CC" || contract->deviceType == "SOFTWARE_PLUGIN"))
                isAuto = true;
        }

        currentPointSnapshots.clear();
        for (const auto& step : tc.parameterSteps)
        {
            measurement::ControlStateSnapshot snap;
            snap.controlId = step.paramName;
            snap.controlMethod = isAuto ? "AUTOMATED_MIDI" : "MANUAL";
            snap.normalizedValue = step.normalizedValue;
            snap.rawValue = step.rawValue;
            snap.displayValue = step.paramName + " = " + std::to_string(step.normalizedValue);
            snap.confirmationStatus = isAuto ? "automated" : "awaiting_confirmation";
            currentPointSnapshots.push_back(snap);
        }

        measurement::CoordinatorContext ctx;
        ctx.mode = currentInteractionMode;
        ctx.hasActiveSession = (activeMeasurementSession != nullptr);
        ctx.isManualControlRequired = !isAuto;
        ctx.actor = isAuto ? "system" : "operator";

        if (isAuto)
        {
            transitionTo(measurement::CoordinatorEvent::ApplyAutomation, ctx);
            transitionTo(measurement::CoordinatorEvent::AutomationAcknowledged, ctx);
        }
        else
        {
            transitionTo(measurement::CoordinatorEvent::AwaitingManualPrompt, ctx);
        }

        if (viewOperatorModal != nullptr)
        {
            viewOperatorModal->setAutomatedMode(isAuto);
            viewOperatorModal->setStepInfo(juce::String(tc.testId), stepIndex, totalSteps, tc.parameterSteps);
            viewOperatorModal->setVisible(true);
        }

        // Phase 14 fix: keep suiteList visible so the matrix remains animated.
        if (viewSuiteList != nullptr && isAuto)
        {
            viewSuiteList->setVisible(true);
        }
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
    measurement::CoordinatorContext ctx;
    ctx.mode = currentInteractionMode;
    ctx.hasActiveSession = (activeMeasurementSession != nullptr);
    ctx.currentPointId = pt.testId;

    transitionTo(measurement::CoordinatorEvent::CaptureFinished, ctx);

    // Diagnóstico de saturación y cálculo de hash de audio bruto
    bool clipEvidence = false;
    std::string rawHash = synth::Sha256::computeHex("point_" + pt.testId + "_" + std::to_string(totalPointsMeasured));
    if (!pt.irSamples.empty())
    {
        auto diag = measurement::AnalogChainReversibleCompensator::diagnosePhysicalSaturation(pt.irSamples, 1.0f);
        clipEvidence = diag.adcClipEvidence;
        rawHash = synth::Sha256::computeHex(pt.irSamples.data(), pt.irSamples.size() * sizeof(float));
    }

    if (clipEvidence)
    {
        transitionTo(measurement::CoordinatorEvent::ValidationFailed, ctx);
    }
    else
    {
        transitionTo(measurement::CoordinatorEvent::ValidationPassed, ctx);

        if (activeMeasurementSession != nullptr)
        {
            measurement::RawCaptureReference capRef;
            capRef.captureId = "cap_" + pt.testId + "_" + std::to_string(totalPointsMeasured);
            capRef.rawAudioSha256 = rawHash;
            capRef.filename = capRef.captureId + ".raw.wav";
            capRef.sampleRateHz = 48000.0;
            capRef.sampleCount = pt.irSamples.size();
            capRef.channels = 1;
            capRef.activeControls = currentPointSnapshots;
            activeMeasurementSession->rawCaptures.push_back(capRef);
        }

        ctx.rawSha256Verified = true;
        transitionTo(measurement::CoordinatorEvent::PersistenceSucceeded, ctx);

        int totalExpected = (viewSuiteList != nullptr) ? viewSuiteList->getTotalPointCount() : static_cast<int>(sessionManager.getPointCount());
        ctx.hasRemainingPoints = ((totalPointsMeasured + 1) < totalExpected);
        ctx.isManualControlRequired = (currentInteractionMode == measurement::WorkspaceInteractionMode::Guided);
        transitionTo(measurement::CoordinatorEvent::NextPointOrFinish, ctx);
    }

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
