#pragma once

#include "IDiagnosticsTelemetrySource.h"
#include "CanonicalCalibrationState.h"
#include "CanonicalWorkflowState.h"
#include "../../audio/LabAudioEngine.h"
#include "../SessionExecutionCoordinator.h"
#include "../SoundIdSuiteList.h"

namespace abdaudiolab::gui {

/**
 * @class MainContentTelemetrySource
 * @brief Concrete adapter collecting raw diagnostic metrics from AudioEngine, SessionCoordinator, and Canonical States.
 *
 * Fully decoupled from visual GUI widgets (LoopbackCalibrationModal, WorkflowStepperBar).
 */
class MainContentTelemetrySource final : public IDiagnosticsTelemetrySource
{
public:
    MainContentTelemetrySource(audio::LabAudioEngine& audioEngineRef,
                               SessionExecutionCoordinator& sessionCoordinatorRef,
                               SoundIdSuiteList& suiteListRef,
                               const CanonicalCalibrationState* canonicalCalibrationRef = nullptr,
                               const CanonicalWorkflowState* canonicalWorkflowRef = nullptr);

    ~MainContentTelemetrySource() override = default;

    void setCanonicalCalibrationState(const CanonicalCalibrationState* state) noexcept;
    void setCanonicalWorkflowState(const CanonicalWorkflowState* state) noexcept;

    TelemetryAudioLevels readAudioLevels() const override;
    bool isSpectrumReady() const override;
    std::size_t readSpectrumMagnitudes(float* destBuffer, std::size_t maxBins) const override;
    TelemetryDeviceMetrics readDeviceMetrics() const override;
    TelemetryCalibrationData readCalibrationData() const override;
    TelemetrySessionProgress readSessionProgress() const override;

private:
    audio::LabAudioEngine& audioEngine;
    SessionExecutionCoordinator& sessionCoordinator;
    SoundIdSuiteList& suiteList;
    const CanonicalCalibrationState* canonicalCalibration { nullptr };
    const CanonicalWorkflowState* canonicalWorkflow { nullptr };
};

} // namespace abdaudiolab::gui
