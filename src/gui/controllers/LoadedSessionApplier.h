/**
 * @file LoadedSessionApplier.h
 * @brief Coordinates the projection and application of a loaded session model onto UI targets.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "core/SessionSerializer.h"
#include "export/LutExporter.h"
#include "gui/suite/SuiteDataModels.h"
#include "gui/WorkflowStepperBar.h"
#include "gui/soundid/SoundIdSidebarStepper.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::gui
{

enum class SessionApplicationStatus
{
    Success,
    InvalidManifest,
    TargetUnavailable,
    PartialFailure
};

struct SessionApplicationResult
{
    SessionApplicationStatus status { SessionApplicationStatus::InvalidManifest };
    juce::String message;
    int pointsApplied { 0 };
    int testsRestored { 0 };

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == SessionApplicationStatus::Success;
    }
};

/**
 * @struct SessionUiPresentationData
 * @brief Decoupled presentation data projected from manifest for environment and hardware drawers.
 */
struct SessionUiPresentationData
{
    juce::String hardwareId;
    juce::String hardwareDisplayName;
    juce::String activeFunctionId;
    juce::String activeFunctionName;
    juce::String targetModule;
    juce::String operatorNotes;
    float ambientTemperatureC { 22.0f };
    int warmupTimeMinutes { 15 };
    bool hasValidHardwareContract { false };
};

/**
 * @struct WorkflowStepState
 * @brief Target navigation and step lock states derived from session completion.
 */
struct WorkflowStepState
{
    bool isSessionComplete { false };
    WorkflowStepperBar::Step targetStepperStep { WorkflowStepperBar::Step::RunSession };
    SoundIdSidebarStepper::Step targetSidebarStep { SoundIdSidebarStepper::Step::RunSession };
    WorkflowStepperBar::StepStatus runSessionStatus { WorkflowStepperBar::StepStatus::Current };
};

/**
 * @class ILoadedSessionTarget
 * @brief Port interface allowing LoadedSessionApplier to update view components without owning them.
 */
class ILoadedSessionTarget
{
public:
    virtual ~ILoadedSessionTarget() = default;

    virtual void setSessionData(const core::SessionManifest& manifest,
                                const std::vector<exporting::MeasuredPoint>& points) = 0;

    virtual void clearPlotterAndAddPoints(const std::vector<exporting::MeasuredPoint>& points) = 0;

    virtual void updateDrawerAndEnvironment(const SessionUiPresentationData& data) = 0;

    virtual void updateHardwarePanels(const SessionUiPresentationData& data) = 0;

    virtual void rebuildTestSuiteQueue(const std::vector<core::SessionManifest>& /*manifest*/,
                                       const std::vector<gui::QueueItem>& items) = 0;

    virtual void updateWorkflowAndNavigation(const WorkflowStepState& workflowState) = 0;
};

/**
 * @class LoadedSessionApplier
 * @brief Pure presentation coordinator that validates a loaded session and applies it to UI targets.
 *
 * ## Application Contract
 *
 * ### Pre-condition gate (stop-at-first)
 * Validation of the manifest occurs **before** any target method is called.
 * If the manifest is invalid (`formatVersion` or `hardwareDisplayName` empty),
 * the result is `InvalidManifest` and **zero** target methods are invoked.
 *
 * ### Projection ordering (all-or-nothing after the gate)
 * Given a valid manifest, all six phases execute in strict deterministic order:
 *   1. `setSessionData`            — session manager state reset
 *   2. `clearPlotterAndAddPoints`  — plotter projection
 *   3. `updateDrawerAndEnvironment`— drawer + environment card
 *   4. `updateHardwarePanels`      — routing panels + catalog + header
 *   5. `rebuildTestSuiteQueue`     — queue rebuild from pre-built QueueItems
 *   6. `updateWorkflowAndNavigation` — steppers + navigation controller
 *
 * ### Rollback
 * Not available. Target methods are `void` and are considered infallible after
 * the pre-condition gate. Partial application is impossible given a valid manifest:
 * either all six phases run or none do.
 *
 * ### Responsibility of targets
 * Implementations of `ILoadedSessionTarget` must not throw or produce observable
 * side-effects beyond the projection described in their interface contract.
 */
class LoadedSessionApplier
{
public:
    [[nodiscard]] static SessionApplicationResult apply(
        const core::SessionManifest& manifest,
        const std::vector<exporting::MeasuredPoint>& points,
        bool hasHardwareContract,
        ILoadedSessionTarget& target);

    [[nodiscard]] static std::vector<gui::QueueItem> reconstructQueueItems(
        const std::vector<gui::TestConfiguration>& testConfigs,
        bool hasMeasuredPoints);

    [[nodiscard]] static WorkflowStepState computeWorkflowState(
        int totalMeasuredPointsInSession,
        size_t actualPointsCount);
};

} // namespace abdaudiolab::gui

