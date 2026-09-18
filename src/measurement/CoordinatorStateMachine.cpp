/**
 * @file CoordinatorStateMachine.cpp
 * @brief Pure decoupled state machine implementation for Measurement Workspace orchestration.
 * @author ABDSynths
 * @date 2026
 */

#include "CoordinatorStateMachine.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::measurement
{

namespace
{
    std::string getCurrentTimestampUtc()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
}

TransitionResult CoordinatorStateMachine::reduce(
    CoordinatorState current,
    CoordinatorEvent event,
    const CoordinatorContext& context)
{
    TransitionResult res;
    res.accepted = false;
    res.newState = current;

    // 1. Manejo Universal de Cancelación (Idempotente y Transaccional)
    if (event == CoordinatorEvent::CancelRequested)
    {
        if (current == CoordinatorState::Aborted)
        {
            res.accepted = true;
            res.newState = CoordinatorState::Aborted;
            res.rejectionReason = "Idempotent cancellation: session already aborted.";
            return res;
        }

        if (current == CoordinatorState::NoSession)
        {
            res.accepted = false;
            res.rejectionReason = "Cannot cancel: no active session exists.";
            return res;
        }

        // Cancelación autorizada desde cualquier estado operativo
        res.accepted = true;
        res.newState = CoordinatorState::Aborted;
        return res;
    }

    // 2. Manejo Universal de Reset
    if (event == CoordinatorEvent::Reset)
    {
        res.accepted = true;
        res.newState = CoordinatorState::NoSession;
        return res;
    }

    // 3. Reducción por Estado Origen con Evaluación Estricta de Guardas
    switch (current)
    {
        case CoordinatorState::NoSession:
        {
            if (event == CoordinatorEvent::SelectProfile)
            {
                if (!context.profileSha256Verified)
                {
                    res.rejectionReason = "Guard failed: profileSha256 must be verified before selecting profile.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::ProfileSelected;
                return res;
            }
            res.rejectionReason = "Invalid event for NoSession state: expected SelectProfile.";
            return res;
        }

        case CoordinatorState::ProfileSelected:
        {
            if (event == CoordinatorEvent::SelectProfile)
            {
                // Permitir cambiar perfil mientras esté en ProfileSelected
                if (!context.profileSha256Verified)
                {
                    res.rejectionReason = "Guard failed: new profileSha256 must be verified.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::ProfileSelected;
                return res;
            }
            if (event == CoordinatorEvent::PrepareSession)
            {
                if (!context.hasActiveSession)
                {
                    res.rejectionReason = "Guard failed: active session instance must be created.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::SessionReady;
                return res;
            }
            res.rejectionReason = "Invalid event for ProfileSelected state: expected PrepareSession or SelectProfile.";
            return res;
        }

        case CoordinatorState::SessionReady:
        {
            if (event == CoordinatorEvent::AwaitingManualPrompt)
            {
                if (!context.isManualControlRequired)
                {
                    res.rejectionReason = "Guard failed: point does not require manual operator intervention.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::AwaitingManualConfirmation;
                return res;
            }
            if (event == CoordinatorEvent::ApplyAutomation)
            {
                if (context.isManualControlRequired)
                {
                    res.rejectionReason = "Guard failed: manual control required; cannot apply automation.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::ApplyingAutomation;
                return res;
            }
            // En modo Free (sin guion), se puede iniciar captura directa si el estímulo está listo
            if (context.mode == WorkspaceInteractionMode::Free && event == CoordinatorEvent::CaptureStarted)
            {
                if (!context.stimulusReady)
                {
                    res.rejectionReason = "Guard failed: stimulus is not ready for direct capture.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::Capturing;
                return res;
            }
            res.rejectionReason = "Invalid event for SessionReady state.";
            return res;
        }

        case CoordinatorState::AwaitingManualConfirmation:
        {
            if (event == CoordinatorEvent::ConfirmManualControl)
            {
                if (!context.hasActiveSession)
                {
                    res.rejectionReason = "Guard failed: missing active session.";
                    return res;
                }
                if (!context.isManualControlRequired)
                {
                    res.rejectionReason = "Guard failed: current point does not require manual control.";
                    return res;
                }
                if (context.isCapturingActive)
                {
                    res.rejectionReason = "Guard failed: cannot confirm manual step while capture is active.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::Capturing;
                return res;
            }
            res.rejectionReason = "Invalid event while awaiting manual confirmation: expected ConfirmManualControl or CancelRequested.";
            return res;
        }

        case CoordinatorState::ApplyingAutomation:
        {
            if (event == CoordinatorEvent::AutomationAcknowledged)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Capturing;
                return res;
            }
            if (event == CoordinatorEvent::AutomationFailed)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Error;
                res.rejectionReason = "Transport or plugin automation failed.";
                return res;
            }
            res.rejectionReason = "Invalid event while applying automation: expected AutomationAcknowledged or AutomationFailed.";
            return res;
        }

        case CoordinatorState::Capturing:
        {
            if (event == CoordinatorEvent::CaptureFinished)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Validating;
                return res;
            }
            res.rejectionReason = "Invalid event while capturing: expected CaptureFinished or CancelRequested.";
            return res;
        }

        case CoordinatorState::Validating:
        {
            if (event == CoordinatorEvent::ValidationPassed)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Persisting;
                return res;
            }
            if (event == CoordinatorEvent::ValidationFailed)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Error;
                res.rejectionReason = "Audio validation failed (e.g. ADC clipping evidence or severe distortion).";
                return res;
            }
            res.rejectionReason = "Invalid event while validating: expected ValidationPassed or ValidationFailed.";
            return res;
        }

        case CoordinatorState::Persisting:
        {
            if (event == CoordinatorEvent::PersistenceSucceeded)
            {
                if (!context.rawSha256Verified)
                {
                    res.rejectionReason = "Guard failed: raw audio SHA-256 fixity must be verified from disk before completing point.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::PointCompleted;
                return res;
            }
            if (event == CoordinatorEvent::PersistenceFailed)
            {
                res.accepted = true;
                res.newState = CoordinatorState::Error;
                res.rejectionReason = "Disk I/O failure while persisting raw capture and manifests.";
                return res;
            }
            res.rejectionReason = "Invalid event while persisting: expected PersistenceSucceeded or PersistenceFailed.";
            return res;
        }

        case CoordinatorState::PointCompleted:
        {
            if (event == CoordinatorEvent::NextPointOrFinish)
            {
                if (context.hasRemainingPoints)
                {
                    if (context.mode == WorkspaceInteractionMode::Free)
                    {
                        res.accepted = true;
                        res.newState = CoordinatorState::SessionReady;
                        return res;
                    }
                    if (context.isManualControlRequired)
                    {
                        res.accepted = true;
                        res.newState = CoordinatorState::AwaitingManualConfirmation;
                        return res;
                    }
                    res.accepted = true;
                    res.newState = CoordinatorState::ApplyingAutomation;
                    return res;
                }

                // Fin de la campaña
                res.accepted = true;
                res.newState = CoordinatorState::SessionCompleted;
                return res;
            }
            res.rejectionReason = "Invalid event for PointCompleted: expected NextPointOrFinish.";
            return res;
        }

        case CoordinatorState::SessionCompleted:
        {
            if (event == CoordinatorEvent::NextPointOrFinish)
            {
                // Idempotente: sesión ya completada bloquea nuevas tomas
                res.accepted = true;
                res.newState = CoordinatorState::SessionCompleted;
                res.rejectionReason = "Session already completed: no new points authorized.";
                return res;
            }
            if (event == CoordinatorEvent::ReanalysisRequested)
            {
                if (!context.allTakesValidForReanalysis)
                {
                    res.rejectionReason = "Guard failed: reanalysis requires all raw captures to maintain valid cryptographic fixity.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::ReanalysisAvailable;
                return res;
            }
            res.rejectionReason = "Invalid event for SessionCompleted: expected ReanalysisRequested or Reset.";
            return res;
        }

        case CoordinatorState::ReanalysisAvailable:
        {
            if (event == CoordinatorEvent::ReanalysisRequested)
            {
                if (!context.allTakesValidForReanalysis)
                {
                    res.rejectionReason = "Guard failed: fixity check mismatch during reanalysis.";
                    return res;
                }
                res.accepted = true;
                res.newState = CoordinatorState::ReanalysisAvailable;
                return res;
            }
            res.rejectionReason = "Invalid event for ReanalysisAvailable: expected ReanalysisRequested or Reset.";
            return res;
        }

        case CoordinatorState::Error:
        case CoordinatorState::Aborted:
        {
            res.rejectionReason = "Session in terminal state (" + coordinatorStateToString(current) + "): requires Reset.";
            return res;
        }
    }

    res.rejectionReason = "Unhandled state transition.";
    return res;
}

bool CoordinatorStateMachine::dispatch(CoordinatorEvent event, const CoordinatorContext& context, const std::string& sessionId)
{
    auto result = reduce(currentState, event, context);

    CoordinatorTransitionRecord record;
    record.sessionId = sessionId;
    record.fromState = coordinatorStateToString(currentState);
    record.event = coordinatorEventToString(event);
    record.toState = coordinatorStateToString(result.newState);
    record.reason = result.accepted ? (result.rejectionReason.empty() ? "Transition accepted" : result.rejectionReason)
                                   : ("Transition rejected: " + result.rejectionReason);
    record.actor = context.actor;
    record.contextHash = synth::Sha256::computeHex("point=" + context.currentPointId + "&actor=" + context.actor);
    record.timestampUtc = getCurrentTimestampUtc();

    transitionHistory.push_back(record);

    if (result.accepted)
    {
        currentState = result.newState;
        return true;
    }
    return false;
}

bool CoordinatorStateMachine::isManualConfirmationAllowed() const noexcept
{
    return currentState == CoordinatorState::AwaitingManualConfirmation;
}

bool CoordinatorStateMachine::isProfileChangeAllowed() const noexcept
{
    return currentState == CoordinatorState::NoSession || currentState == CoordinatorState::ProfileSelected;
}

bool CoordinatorStateMachine::isCancellationAllowed() const noexcept
{
    return currentState != CoordinatorState::NoSession &&
           currentState != CoordinatorState::SessionCompleted &&
           currentState != CoordinatorState::ReanalysisAvailable &&
           currentState != CoordinatorState::Aborted;
}

bool CoordinatorStateMachine::isReanalysisAllowed() const noexcept
{
    return currentState == CoordinatorState::SessionCompleted || currentState == CoordinatorState::ReanalysisAvailable;
}

} // namespace abdaudiolab::measurement
