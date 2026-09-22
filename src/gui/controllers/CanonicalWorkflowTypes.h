#pragma once

namespace abdaudiolab::gui {

/**
 * @enum CanonicalStep
 * @brief Canonical 5-step workflow sequence for ABDAudioLab Lab Bench (Steps 0..4).
 */
enum class CanonicalStep
{
    SystemInfo = 0,
    CalibrateLoopback,
    HardwareRouting,
    RunSession,
    ExportReport
};

/**
 * @enum CanonicalStepStatus
 * @brief Status enumeration for each step in the workflow.
 */
enum class CanonicalStepStatus
{
    Pending,
    Current,
    Completed,
    Skipped,
    Warning
};

using Step = CanonicalStep;
using StepStatus = CanonicalStepStatus;

// Temporary compatibility adapter for migration from legacy WorkflowStepperBar to canonical types
struct WorkflowStepperBar
{
    using Step = CanonicalStep;
    using StepStatus = CanonicalStepStatus;
};

} // namespace abdaudiolab::gui
