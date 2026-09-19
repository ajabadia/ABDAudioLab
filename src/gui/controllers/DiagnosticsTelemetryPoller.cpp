#include "DiagnosticsTelemetryPoller.h"
#include <algorithm>

namespace abdaudiolab::gui {

DiagnosticsTelemetryPoller::DiagnosticsTelemetryPoller(IDiagnosticsTelemetrySource& sourceRef,
                                                       IDiagnosticsTelemetryHost& hostRef,
                                                       const TelemetryPollerConfig& config)
    : source(sourceRef),
      host(hostRef),
      pollerConfig(config)
{
    if (pollerConfig.calibrationPeriodTicks <= 0)
        pollerConfig.calibrationPeriodTicks = 15;
    if (pollerConfig.fftCapacity <= 0 || pollerConfig.fftCapacity > static_cast<int>(kMaxTelemetryFftBins))
        pollerConfig.fftCapacity = static_cast<int>(kMaxTelemetryFftBins);
}

DiagnosticsTelemetryPoller::~DiagnosticsTelemetryPoller()
{
    stopTimer();
}

void DiagnosticsTelemetryPoller::startPolling(int frequencyHz)
{
    if (frequencyHz <= 0)
        frequencyHz = 60;
    startTimerHz(frequencyHz);
}

void DiagnosticsTelemetryPoller::stopPolling()
{
    stopTimer();
}

bool DiagnosticsTelemetryPoller::isPolling() const noexcept
{
    return isTimerRunning();
}

void DiagnosticsTelemetryPoller::timerCallback()
{
    pollNow();
}

void DiagnosticsTelemetryPoller::pollNow()
{
    TelemetrySnapshot snapshot;

    // 1. Audio Meter Levels
    const auto levels = source.readAudioLevels();
    snapshot.inputPeakL = sanitizeFloat(levels.inPeakL, 0.0f);
    snapshot.inputPeakR = sanitizeFloat(levels.inPeakR, 0.0f);
    snapshot.inputRmsL  = sanitizeFloat(levels.inRmsL, 0.0f);
    snapshot.inputRmsR  = sanitizeFloat(levels.inRmsR, 0.0f);

    snapshot.outputPeakL = sanitizeFloat(levels.outPeakL, 0.0f);
    snapshot.outputPeakR = sanitizeFloat(levels.outPeakR, 0.0f);
    snapshot.outputRmsL  = sanitizeFloat(levels.outRmsL, 0.0f);
    snapshot.outputRmsR  = sanitizeFloat(levels.outRmsR, 0.0f);

    // 2. FFT Spectrum
    snapshot.spectrumReady = source.isSpectrumReady();
    if (snapshot.spectrumReady)
    {
        std::size_t binsToRead = static_cast<std::size_t>(pollerConfig.fftCapacity);
        std::size_t actualBins = source.readSpectrumMagnitudes(snapshot.fftMagnitudes.data(), binsToRead);
        snapshot.fftBinCount = std::min(actualBins, kMaxTelemetryFftBins);

        for (std::size_t i = 0; i < snapshot.fftBinCount; ++i)
        {
            snapshot.fftMagnitudes[i] = sanitizeFloat(snapshot.fftMagnitudes[i], 0.0f);
        }
    }
    else
    {
        snapshot.fftBinCount = 0;
    }

    // 3. Device & CPU Metrics
    const auto dev = source.readDeviceMetrics();
    snapshot.cpuUsagePercent = std::clamp(sanitizeFloat(dev.cpuUsagePercent, 0.0f), 0.0f, 100.0f);
    snapshot.sampleRate = (dev.sampleRate > 0.0 && std::isfinite(dev.sampleRate)) ? dev.sampleRate : 0.0;
    snapshot.bufferSizeSamples = (dev.bufferSizeSamples >= 0) ? dev.bufferSizeSamples : 0;

    // 4. Calibration Cadence
    bool isDue = (tickCounter++ % pollerConfig.calibrationPeriodTicks) == 0;
    snapshot.calibrationTickDue = isDue;
    if (isDue)
    {
        const auto cal = source.readCalibrationData();
        snapshot.isCalibrated = cal.isCalibrated;
        snapshot.calibrationSampleRate = sanitizeDouble(cal.sampleRate, 0.0);
        snapshot.isCalibrationSkipped = cal.isSkipped;
    }

    // 5. Session Progress & MIDI
    const auto sess = source.readSessionProgress();
    snapshot.currentTrial = std::max(0, sess.currentTrial);
    snapshot.totalTrials = std::max(0, sess.totalTrials);

    if (snapshot.totalTrials > 0)
    {
        float ratio = static_cast<float>(snapshot.currentTrial) / static_cast<float>(snapshot.totalTrials);
        snapshot.progressPercent = std::clamp(ratio * 100.0f, 0.0f, 100.0f);
    }
    else
    {
        snapshot.progressPercent = 0.0f;
    }

    snapshot.lastPluginOutputRmsDb = sanitizeFloat(sess.lastPluginOutputRmsDb, -120.0f);
    snapshot.sessionStateCode = sess.sessionStateCode;

    // MIDI Note number & name formatting
    if (sess.activeMidiNoteNumber >= 0 && sess.activeMidiNoteNumber <= 127)
    {
        snapshot.activeMidiNoteNumber = sess.activeMidiNoteNumber;
        snapshot.activeMidiNoteName = formatMidiNoteName(sess.activeMidiNoteNumber);
        snapshot.stimulusDescription = "MIDI " + snapshot.activeMidiNoteName.toStdString()
                                     + " (Note #" + std::to_string(sess.activeMidiNoteNumber) + ")";
    }
    else
    {
        snapshot.activeMidiNoteNumber = -1;
        snapshot.activeMidiNoteName = "No MIDI note";
        snapshot.stimulusDescription = "";
    }

    // Deliver snapshot to host
    host.applyTelemetrySnapshot(snapshot);
}

} // namespace abdaudiolab::gui
