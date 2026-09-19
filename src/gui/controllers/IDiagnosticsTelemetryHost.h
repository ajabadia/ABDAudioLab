#pragma once

#include "DiagnosticsTelemetrySnapshot.h"

namespace abdaudiolab::gui {

/**
 * @class IDiagnosticsTelemetryHost
 * @brief Port implemented by MainContentComponent or test fixture to receive periodic diagnostics telemetry snapshots.
 */
class IDiagnosticsTelemetryHost
{
public:
    virtual ~IDiagnosticsTelemetryHost() = default;

    /**
     * @brief Receives an immutable value snapshot of diagnostic and telemetry measurements.
     */
    virtual void applyTelemetrySnapshot(const TelemetrySnapshot& snapshot) = 0;
};

} // namespace abdaudiolab::gui
