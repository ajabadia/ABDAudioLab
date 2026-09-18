/**
 * @file CoordinatorStateMachine.h
 * @brief Pure decoupled state machine for Measurement Workspace orchestration:
 *        Session Lifecycle, Point Execution, Terminal States, Typed Events, Guards,
 *        and Immutable Transition Audit Trail.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementSessionContracts.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Public unified lifecycle states (13 canonical phases).
 */
enum class CoordinatorState
{
    // Session Lifecycle
    NoSession,
    ProfileSelected,
    SessionReady,
    SessionCompleted,
    ReanalysisAvailable,

    // Point Execution
    AwaitingManualConfirmation,
    ApplyingAutomation,
    Capturing,
    Validating,
    Persisting,
    PointCompleted,

    // Terminal Status
    Error,
    Aborted
};

[[nodiscard]] inline std::string coordinatorStateToString(CoordinatorState s) noexcept
{
    switch (s)
    {
        case CoordinatorState::NoSession:                  return "NoSession";
        case CoordinatorState::ProfileSelected:            return "ProfileSelected";
        case CoordinatorState::SessionReady:               return "SessionReady";
        case CoordinatorState::AwaitingManualConfirmation: return "AwaitingManualConfirmation";
        case CoordinatorState::ApplyingAutomation:         return "ApplyingAutomation";
        case CoordinatorState::Capturing:                  return "Capturing";
        case CoordinatorState::Validating:                 return "Validating";
        case CoordinatorState::Persisting:                 return "Persisting";
        case CoordinatorState::PointCompleted:             return "PointCompleted";
        case CoordinatorState::SessionCompleted:           return "SessionCompleted";
        case CoordinatorState::ReanalysisAvailable:        return "ReanalysisAvailable";
        case CoordinatorState::Error:                      return "Error";
        case CoordinatorState::Aborted:                    return "Aborted";
    }
    return "Unknown";
}

/**
 * @brief Interaction mode for the measurement workspace.
 */
enum class WorkspaceInteractionMode
{
    Guided,   /**< Step-by-step assistant with mandatory operator confirmation prompts */
    Free      /**< Laboratory bench mode with ad-hoc direct capture commands */
};

[[nodiscard]] inline std::string workspaceInteractionModeToString(WorkspaceInteractionMode mode) noexcept
{
    switch (mode)
    {
        case WorkspaceInteractionMode::Guided: return "Guided";
        case WorkspaceInteractionMode::Free:   return "Free";
    }
    return "Unknown";
}

/**
 * @brief Typed discrete events driving state transitions.
 */
enum class CoordinatorEvent
{
    SelectProfile,
    PrepareSession,
    AwaitingManualPrompt,
    ConfirmManualControl,
    ApplyAutomation,
    AutomationAcknowledged,
    AutomationFailed,
    CaptureStarted,
    CaptureFinished,
    ValidationPassed,
    ValidationFailed,
    PersistenceSucceeded,
    PersistenceFailed,
    NextPointOrFinish,
    CancelRequested,
    ReanalysisRequested,
    Reset
};

[[nodiscard]] inline std::string coordinatorEventToString(CoordinatorEvent e) noexcept
{
    switch (e)
    {
        case CoordinatorEvent::SelectProfile:         return "SelectProfile";
        case CoordinatorEvent::PrepareSession:        return "PrepareSession";
        case CoordinatorEvent::AwaitingManualPrompt:  return "AwaitingManualPrompt";
        case CoordinatorEvent::ConfirmManualControl:  return "ConfirmManualControl";
        case CoordinatorEvent::ApplyAutomation:       return "ApplyAutomation";
        case CoordinatorEvent::AutomationAcknowledged: return "AutomationAcknowledged";
        case CoordinatorEvent::AutomationFailed:       return "AutomationFailed";
        case CoordinatorEvent::CaptureStarted:        return "CaptureStarted";
        case CoordinatorEvent::CaptureFinished:       return "CaptureFinished";
        case CoordinatorEvent::ValidationPassed:      return "ValidationPassed";
        case CoordinatorEvent::ValidationFailed:      return "ValidationFailed";
        case CoordinatorEvent::PersistenceSucceeded:  return "PersistenceSucceeded";
        case CoordinatorEvent::PersistenceFailed:     return "PersistenceFailed";
        case CoordinatorEvent::NextPointOrFinish:     return "NextPointOrFinish";
        case CoordinatorEvent::CancelRequested:       return "CancelRequested";
        case CoordinatorEvent::ReanalysisRequested:   return "ReanalysisRequested";
        case CoordinatorEvent::Reset:                 return "Reset";
    }
    return "Unknown";
}

/**
 * @brief Contextual data inspected by state machine guards before authorizing transitions.
 */
struct CoordinatorContext
{
    WorkspaceInteractionMode mode { WorkspaceInteractionMode::Guided };
    bool hasActiveSession { false };
    bool profileSha256Verified { false };
    bool isManualControlRequired { false };
    bool isCapturingActive { false };
    bool stimulusReady { false };
    bool rawSha256Verified { false };
    bool hasRemainingPoints { false };
    bool allTakesValidForReanalysis { false };
    std::string actor { "system" }; /**< "operator", "midi", "vst3", "system" */
    std::string currentPointId;
};

/**
 * @brief Result of applying an event to the state machine through the pure reducer.
 */
struct TransitionResult
{
    bool accepted { false };
    CoordinatorState newState { CoordinatorState::NoSession };
    std::string rejectionReason;
};

/**
 * @brief Immutable audit record of an executed transition.
 */
struct CoordinatorTransitionRecord
{
    std::string sessionId;
    std::string fromState;
    std::string event;
    std::string toState;
    std::string reason;
    std::string actor;
    std::string contextHash;
    std::string timestampUtc;

    [[nodiscard]] nlohmann::ordered_json toJson() const
    {
        nlohmann::ordered_json j;
        j["actor"] = actor;
        j["contextHash"] = contextHash;
        j["event"] = event;
        j["fromState"] = fromState;
        j["reason"] = reason;
        j["sessionId"] = sessionId;
        j["timestampUtc"] = timestampUtc;
        j["toState"] = toState;
        return j;
    }
};

/**
 * @class CoordinatorStateMachine
 * @brief Pure state machine engine implementing reducer, guards, and transition logging.
 */
class CoordinatorStateMachine
{
public:
    CoordinatorStateMachine() = default;

    /**
     * @brief Pure transition function with guard evaluation.
     */
    [[nodiscard]] static TransitionResult reduce(
        CoordinatorState current,
        CoordinatorEvent event,
        const CoordinatorContext& context);

    /**
     * @brief Dispatches an event into the active state machine instance.
     * @return true if the transition was accepted and logged; false if rejected by guard.
     */
    bool dispatch(CoordinatorEvent event, const CoordinatorContext& context, const std::string& sessionId = {});

    [[nodiscard]] CoordinatorState getState() const noexcept { return currentState; }
    [[nodiscard]] const std::vector<CoordinatorTransitionRecord>& getTransitionHistory() const noexcept { return transitionHistory; }

    // Action permission queries
    [[nodiscard]] bool isManualConfirmationAllowed() const noexcept;
    [[nodiscard]] bool isProfileChangeAllowed() const noexcept;
    [[nodiscard]] bool isCancellationAllowed() const noexcept;
    [[nodiscard]] bool isReanalysisAllowed() const noexcept;

    void reset() noexcept
    {
        currentState = CoordinatorState::NoSession;
        transitionHistory.clear();
    }

private:
    CoordinatorState currentState { CoordinatorState::NoSession };
    std::vector<CoordinatorTransitionRecord> transitionHistory;
};

} // namespace abdaudiolab::measurement
