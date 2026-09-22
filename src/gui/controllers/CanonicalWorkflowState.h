#pragma once

namespace abdaudiolab::gui {

/**
 * @struct CanonicalWorkflowState
 * @brief Thread-safe, non-visual data model capturing workflow progress and step status.
 *
 * Owned by WorkflowNavigationController / Session and consumed by telemetry without GUI widget coupling.
 */
struct CanonicalWorkflowState
{
    bool isCalibrationSkipped { false };
};

} // namespace abdaudiolab::gui
