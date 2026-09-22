#include "MainContentTelemetrySource.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>

namespace abdaudiolab::gui {

MainContentTelemetrySource::MainContentTelemetrySource(audio::LabAudioEngine& audioEngineRef,
                                                       SessionExecutionCoordinator& sessionCoordinatorRef,
                                                       SoundIdSuiteList& suiteListRef,
                                                       const CanonicalCalibrationState* canonicalCalibrationRef,
                                                       const CanonicalWorkflowState* canonicalWorkflowRef)
    : audioEngine(audioEngineRef),
      sessionCoordinator(sessionCoordinatorRef),
      suiteList(suiteListRef),
      canonicalCalibration(canonicalCalibrationRef),
      canonicalWorkflow(canonicalWorkflowRef)
{
}

void MainContentTelemetrySource::setCanonicalCalibrationState(const CanonicalCalibrationState* state) noexcept
{
    canonicalCalibration = state;
}

void MainContentTelemetrySource::setCanonicalWorkflowState(const CanonicalWorkflowState* state) noexcept
{
    canonicalWorkflow = state;
}

TelemetryAudioLevels MainContentTelemetrySource::readAudioLevels() const
{
    TelemetryAudioLevels levels;
    levels.inPeakL  = audioEngine.getInputPeakL();
    levels.inPeakR  = audioEngine.getInputPeakR();
    levels.inRmsL   = audioEngine.getInputRmsL();
    levels.inRmsR   = 0.0f; // mono/stereo L reference
    levels.outPeakL = audioEngine.getOutputPeakL();
    levels.outPeakR = audioEngine.getOutputPeakR();
    levels.outRmsL  = audioEngine.getOutputRmsL();
    levels.outRmsR  = 0.0f;
    return levels;
}

bool MainContentTelemetrySource::isSpectrumReady() const
{
    return audioEngine.isSpectrumReady();
}

std::size_t MainContentTelemetrySource::readSpectrumMagnitudes(float* destBuffer, std::size_t maxBins) const
{
    if (!destBuffer || maxBins == 0)
        return 0;

    std::array<float, audio::LabAudioEngine::kSpectrumBins> fftData;
    audioEngine.getSpectrumMagnitudes(fftData);

    std::size_t count = std::min(maxBins, static_cast<std::size_t>(audio::LabAudioEngine::kSpectrumBins));
    for (std::size_t i = 0; i < count; ++i)
    {
        destBuffer[i] = fftData[i];
    }
    return audio::LabAudioEngine::kSpectrumBins;
}

TelemetryDeviceMetrics MainContentTelemetrySource::readDeviceMetrics() const
{
    TelemetryDeviceMetrics metrics;
    metrics.sampleRate = audioEngine.getCurrentSampleRate();
    metrics.bufferSizeSamples = 256;
    if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
    {
        metrics.bufferSizeSamples = dev->getCurrentBufferSizeSamples();
    }
    metrics.cpuUsagePercent = static_cast<float>(audioEngine.getDeviceManager().getCpuUsage() * 100.0);
    return metrics;
}

TelemetryCalibrationData MainContentTelemetrySource::readCalibrationData() const
{
    TelemetryCalibrationData data;

    // Calibration measurement parameters (SEAM-04: Canonical Calibration)
    if (canonicalCalibration != nullptr)
    {
        data.isCalibrated = canonicalCalibration->isCalibrated;
        data.sampleRate = canonicalCalibration->sampleRate;
    }

    // Workflow calibration skip status (SEAM-05: Canonical Workflow)
    if (canonicalWorkflow != nullptr)
    {
        data.isSkipped = canonicalWorkflow->isCalibrationSkipped;
    }
    else if (canonicalCalibration != nullptr)
    {
        data.isSkipped = canonicalCalibration->isSkipped;
    }

    return data;
}

TelemetrySessionProgress MainContentTelemetrySource::readSessionProgress() const
{
    TelemetrySessionProgress progress;
    progress.currentTrial = sessionCoordinator.getTotalPointsMeasured();
    progress.totalTrials = suiteList.getQueueSize();

    float rms = audioEngine.getLastPluginOutputRms();
    progress.lastPluginOutputRmsDb = (rms > 0.00001f) ? juce::Decibels::gainToDecibels(rms) : -120.0f;
    progress.activeMidiNoteNumber = audioEngine.getLastNoteOnNumber();

    if (sessionCoordinator.isRunningSession())
    {
        progress.sessionStateCode = sessionCoordinator.isSessionPaused() ? 2 : 1; // 2: Paused, 1: Profiling
    }
    else if (sessionCoordinator.getCoordinatorState() == measurement::CoordinatorState::SessionCompleted)
    {
        progress.sessionStateCode = 3; // 3: Completed
    }
    else if (sessionCoordinator.getCoordinatorState() == measurement::CoordinatorState::Aborted)
    {
        progress.sessionStateCode = 4; // 4: Cancelled
    }
    else
    {
        progress.sessionStateCode = 0; // 0: ReadyToProfile
    }

    return progress;
}

} // namespace abdaudiolab::gui
