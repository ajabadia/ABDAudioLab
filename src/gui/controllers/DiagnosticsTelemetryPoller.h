#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DiagnosticsTelemetrySnapshot.h"
#include "IDiagnosticsTelemetrySource.h"
#include "IDiagnosticsTelemetryHost.h"

namespace abdaudiolab::gui {

/**
 * @class DiagnosticsTelemetryPoller
 * @brief Periodically samples and normalizes diagnostic telemetry (audio levels, spectrum, CPU, session progress, MIDI).
 *
 * Enforces:
 * - Zero heap allocation during periodic polling using pre-allocated arrays.
 * - Deterministic, synchronously testable execution via pollNow().
 * - Non-owning references to source and host ports (no direct widget coupling).
 * - Finite number sanitization (NaN/Inf protection) and strict bounds clamping.
 */
class DiagnosticsTelemetryPoller : public juce::Timer
{
public:
    DiagnosticsTelemetryPoller(IDiagnosticsTelemetrySource& sourceRef,
                               IDiagnosticsTelemetryHost& hostRef,
                               const TelemetryPollerConfig& config = {});
    ~DiagnosticsTelemetryPoller() override;

    DiagnosticsTelemetryPoller(const DiagnosticsTelemetryPoller&) = delete;
    DiagnosticsTelemetryPoller& operator=(const DiagnosticsTelemetryPoller&) = delete;

    /**
     * @brief Performs one synchronous poll cycle: acquires from source, sanitizes, and publishes to host.
     * Does NOT start or stop the periodic Timer.
     */
    void pollNow();

    /**
     * @brief Starts periodic sampling on the JUCE Message Thread at the given frequency (default 60 Hz).
     */
    void startPolling(int frequencyHz = 60);

    /**
     * @brief Stops periodic sampling.
     */
    void stopPolling();

    /**
     * @brief Returns true if periodic timer is actively running.
     */
    [[nodiscard]] bool isPolling() const noexcept;

    /**
     * @brief Access the configured poller parameters.
     */
    [[nodiscard]] const TelemetryPollerConfig& getConfig() const noexcept { return pollerConfig; }

private:
    void timerCallback() override;

    IDiagnosticsTelemetrySource& source;
    IDiagnosticsTelemetryHost& host;
    TelemetryPollerConfig pollerConfig;
    int tickCounter { 0 };
};

} // namespace abdaudiolab::gui
