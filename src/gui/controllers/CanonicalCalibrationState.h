#pragma once

namespace abdaudiolab::gui {

/**
 * @struct CanonicalCalibrationState
 * @brief Thread-safe, non-visual data model capturing calibration status and parameters.
 *
 * Owned by session/application lifecycle and consumed by telemetry without GUI widget coupling.
 */
struct CanonicalCalibrationState
{
    bool isCalibrated { false };
    double sampleRate { 0.0 };
    bool isSkipped { false };
};

} // namespace abdaudiolab::gui
