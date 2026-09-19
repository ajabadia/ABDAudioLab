#pragma once

#include "IDiagnosticsTelemetrySource.h"
#include "../../audio/LabAudioEngine.h"
#include "../SessionExecutionCoordinator.h"
#include "../SoundIdSuiteList.h"
#include "../LoopbackCalibrationModal.h"
#include "../WorkflowStepperBar.h"

namespace abdaudiolab::gui {

/**
 * @class MainContentTelemetrySource
 * @brief Concrete adapter collecting raw diagnostic metrics from AudioEngine, SessionCoordinator, and Modals.
 */
class MainContentTelemetrySource final : public IDiagnosticsTelemetrySource
{
public:
    MainContentTelemetrySource(audio::LabAudioEngine& audioEngineRef,
                               SessionExecutionCoordinator& sessionCoordinatorRef,
                               SoundIdSuiteList& suiteListRef,
                               LoopbackCalibrationModal& loopbackModalRef,
                               WorkflowStepperBar& stepperBarRef);
    ~MainContentTelemetrySource() override = default;

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
    LoopbackCalibrationModal& loopbackModal;
    WorkflowStepperBar& stepperBar;
};

} // namespace abdaudiolab::gui
