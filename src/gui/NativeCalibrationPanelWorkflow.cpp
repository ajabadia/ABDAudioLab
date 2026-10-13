/**
 * @file NativeCalibrationPanelWorkflow.cpp
 * @brief Step 2 state machine: 2A noise floor, 2B safety preflight, Farina sweep,
 *        result publication and bypass. All safety invariants live here.
 * @author ABDSynths
 * @date 2026
 */

#include "NativeCalibrationPanel.h"

#include "../calibration/CalibrationNoiseBaselineRunner.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace abdaudiolab::gui
{

namespace
{

constexpr double kTickSeconds { 0.05 };
constexpr double kFallbackSampleRate { 44100.0 };
constexpr double kBaselineWindowSeconds { 0.40 };
constexpr double kPreflightWindowSeconds { 0.20 };
constexpr int kBaselineWatchdogTicks { 20 };
constexpr int kPreflightTicks { 4 };
constexpr float kSweepStartHz { 20.0f };
constexpr float kMinUsablePeakDbfs { -40.0f };
constexpr float kMaxUsableFlatnessDb { 6.0f };

/** @brief Reason shown when the receiver overloads while the output is still muted. */
juce::String describeImmediateOverloadAbort(const audio::CaptureStatus& status)
{
    if (status.abortReason == audio::CaptureAbortReason::PossibleFeedbackLoop)
        return "Safety abort: Possible analog feedback loop detected (> -6 dBFS during silence). Sweep not started.";

    if (status.abortReason == audio::CaptureAbortReason::SustainedClipping)
        return "Safety abort: Input clipped while output was muted. Sweep not started.";

    return "Baseline capture aborted";
}

/** @brief Reason shown when the 400 ms baseline window itself ends in an abort. */
juce::String describeCaptureAbort(const audio::CaptureStatus& status)
{
    if (status.abortReason == audio::CaptureAbortReason::PossibleFeedbackLoop)
        return "Safety abort: Possible analog feedback loop detected. Sweep not started.";

    return "Safety abort: Input clipped while output was muted. Sweep not started.";
}

} // namespace

//==============================================================================
// Small shared helpers
//==============================================================================
void NativeCalibrationPanel::neutralizeActiveTrim()
{
    activeDraft_ = std::nullopt;
    activeSnapshot_ = std::nullopt;
    audioEngine.setInputAutoTrim(1.0f);
}

void NativeCalibrationPanel::setActiveSubView(CalibrationSubView view) noexcept
{
    if (activeSubView_ == view)
        return;

    activeSubView_ = view;
    resized();
    repaint();
}

void NativeCalibrationPanel::toggleActiveSubView()
{
    setActiveSubView(activeSubView_ == CalibrationSubView::NoiseBaseline_2A
                         ? CalibrationSubView::PhysicalLoopback_2B
                         : CalibrationSubView::NoiseBaseline_2A);
}

void NativeCalibrationPanel::runActiveSubViewAction()
{
    if (activeSubView_ == CalibrationSubView::NoiseBaseline_2A)
        startNoiseBaselineCheck();
    else
        startPhysicalLoopbackSweep();
}

double NativeCalibrationPanel::effectiveSampleRate() const
{
    const auto rate = audioEngine.getSampleRate();
    return rate > 0.0 ? rate : kFallbackSampleRate;
}

int NativeCalibrationPanel::secondsToSamples(double seconds) const
{
    return static_cast<int>(std::lround(effectiveSampleRate() * seconds));
}

double NativeCalibrationPanel::progressFromTicks(int ticks, double seconds)
{
    return std::min(1.0, (ticks * kTickSeconds) / seconds);
}

void NativeCalibrationPanel::ensurePhysicalIsolation()
{
    // Step 2A and 2B both measure the physical/driver audio interface. The RAII guard
    // isolates it from any mock synth filter or active plugin for the whole flow.
    if (!scopedCapture_)
        scopedCapture_ = std::make_unique<audio::ScopedPhysicalLoopbackCapture>(audioEngine);
}

//==============================================================================
// Entry points
//==============================================================================
void NativeCalibrationPanel::startCalibrationSweep()
{
    if (currentState == State::NoiseBaselinePassed)
        startPhysicalLoopbackSweep();
    else
        startNoiseBaselineCheck();
}

void NativeCalibrationPanel::startNoiseBaselineCheck()
{
    // Immediate Invalidation Invariant:
    // Starting a new 2A check immediately invalidates 2B, neutralizes the active calibration
    // context, deactivates latency/trim compensation and prevents continuing to Step 3.
    loopbackState_ = loopbackState_ == LoopbackState::Passed ? LoopbackState::Stale
                                                            : LoopbackState::Locked;

    activeAlignment = calibration::ActiveCalibrationAlignment::None;
    neutralizeActiveTrim();
    calibrationData.isCalibrated = false;

    currentState = State::MeasuringNoiseBaseline;
    noiseBaselineState_ = NoiseBaselineState::Checking;
    noiseReport_.statusText = "CHECKING...";

    measurementStep = 0;
    baselineTicks_ = 0;
    baselineArmed_ = false;
    progressValue = 0.0;
    btnStartMeasure.setButtonText("Measuring Baseline (2A)...");
    applyMeasuringButtonLayout();

    ensurePhysicalIsolation();

    // Guarantee the stimulus generator is stopped and the output is silent (0.0f).
    audioEngine.getStimulusGenerator().stop();
    audioEngine.getResponseReceiver().reset();

    startTimer(50); // 50 ms tick
    resized();
    repaint();
}

void NativeCalibrationPanel::startPhysicalLoopbackSweep()
{
    // Step 2B gate: 2A must have passed before physical loopback can be measured.
    if (noiseBaselineState_ != NoiseBaselineState::Passed)
    {
        setActiveSubView(CalibrationSubView::NoiseBaseline_2A);
        return;
    }

    // Step 2B preflight guard: the capture source must be the verified physical ADC with
    // mock hardware isolated, so no software passthrough or plugin can hijack the capture.
    ensurePhysicalIsolation();

    if (!audioEngine.isPhysicalLoopbackIsolationActive() || !audioEngine.isCaptureSourcePhysicalAdc())
    {
        currentState = State::Failed;
        loopbackState_ = LoopbackState::Failed;
        loopbackReport_.passed = false;
        loopbackReport_.failureReason = "Physical ADC isolation unavailable.";
        scopedCapture_.reset();
        lastDiagnostics_.failureReason = "Physical loopback calibration unavailable.\n"
                                         "Reason: Calibration input is not the physical ADC path.\n"
                                         "No sweep was played.";

        applyFailureButtonLayout();
        startTimerHz(30);
        resized();
        repaint();
        return;
    }

    // 2B opens with a 200 ms safety check under muted output to catch feedback loops or
    // dangerous energy before any sweep is played.
    loopbackState_ = LoopbackState::MeasuringPreflight;
    currentState = State::Measuring;
    preflightTicks_ = 0;
    measurementStep = 0;
    progressValue = 0.0;
    finalizingCapture_ = false;
    lastCaptureStatus_ = {};

    auto& receiver = audioEngine.getResponseReceiver();
    receiver.reset();
    audioEngine.getStimulusGenerator().stop(); // silent during preflight

    receiver.armBaselineCapture(secondsToSamples(kPreflightWindowSeconds));

    btnStartMeasure.setButtonText("Preflight Safety Check (200 ms)...");
    applyMeasuringButtonLayout();

    startTimer(50); // 50 ms tick
    resized();
    repaint();
}

//==============================================================================
// Timer dispatch
//==============================================================================
void NativeCalibrationPanel::timerCallback()
{
    // The audio interface can be closed or unplugged at any moment. That edge outranks every
    // other tick: a measurement running against a device that is no longer there is invalid.
    audioEngine.pollAudioDeviceState();
    if (audioEngine.consumeAudioDeviceLost())
    {
        handleAudioDeviceLost();
        return;
    }

    if (currentState == State::MeasuringNoiseBaseline)
    {
        // The 2A tick owns its own repaints: it repaints only when the layout changed.
        handleNoiseBaselineTick();
        return;
    }

    if (currentState == State::Measuring)
        handleLoopbackTick();
    else
        handleIdleTick();

    repaint();
}

void NativeCalibrationPanel::handleAudioDeviceLost()
{
    const bool measuring2A = currentState == State::MeasuringNoiseBaseline;
    const bool measuring2B = currentState == State::Measuring;
    const bool calibrationActive =
        activeAlignment == calibration::ActiveCalibrationAlignment::AlignedAndActive;

    // Release the hardware in a safe order whatever was in flight.
    audioEngine.getStimulusGenerator().stop();
    audioEngine.getResponseReceiver().reset();
    scopedCapture_.reset();

    if (!measuring2A && !measuring2B && !calibrationActive)
    {
        // Nothing was running and no compensation was applied: just resync the idle UI.
        if (loopbackState_ == LoopbackState::Passed)
            loopbackState_ = LoopbackState::Stale;

        repaint();
        return;
    }

    // A different device means the latency and trim figures no longer describe the chain, and
    // Step 3 must not be reachable with compensation that was measured on other hardware.
    neutralizeActiveTrim();
    activeAlignment = calibration::ActiveCalibrationAlignment::Misaligned;
    btnContinue.setEnabled(false);

    const auto reason = audioEngine.isAudioDeviceOpen()
                            ? juce::String("Audio device stopped during measurement and was reopened "
                                           "before the capture finished. Any latency and trim "
                                           "compensation has been neutralized.")
                            : juce::String("Audio device stopped during measurement: no audio device is "
                                           "available. Any latency and trim compensation has been "
                                           "neutralized.");

    if (measuring2A)
        failNoiseBaselineStep(NoiseBaselineState::DeviceStopped, "DEVICE STOPPED", reason, false);
    else
        failLoopbackStep(LoopbackState::DeviceStopped, reason, false);
}

void NativeCalibrationPanel::handleNoiseBaselineTick()
{
    ++measurementStep;
    ++baselineTicks_;

    const float inL = audioEngine.getInputPeakL();
    const float inR = audioEngine.getInputPeakR();
    liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));

    auto& receiver = audioEngine.getResponseReceiver();

    // 1. Confirm silence in the audio thread before arming the baseline capture.
    if (!baselineArmed_)
    {
        if (audioEngine.isOutputConfirmedSilent(2) || baselineTicks_ >= 4)
        {
            receiver.armBaselineCapture(secondsToSamples(kBaselineWindowSeconds));
            baselineArmed_ = true;
            baselineTicks_ = 0;
        }
        return;
    }

    progressValue = progressFromTicks(baselineTicks_, kBaselineWindowSeconds);

    // 2. Emergency abort on the first block: feedback loop or clipping under mute.
    if (receiver.isOverloadTriggered())
    {
        receiver.forceFinish();
        std::vector<float> discarded;
        const auto status = receiver.retrieveFinalizedSnapshot(discarded);

        stopTimer();
        failNoiseBaselineStep(NoiseBaselineState::SafetyAborted, "SAFETY ABORT",
                              describeImmediateOverloadAbort(status), true);
        return;
    }

    // 3. Window completed, or the 20-tick watchdog expired.
    if (!receiver.isFinished() && baselineTicks_ <= kBaselineWatchdogTicks)
        return;

    receiver.forceFinish();
    std::vector<float> baselineData;
    const auto status = receiver.retrieveFinalizedSnapshot(baselineData);

    if (status.result == audio::CaptureResult::Aborted)
    {
        stopTimer();
        failNoiseBaselineStep(NoiseBaselineState::SafetyAborted, "SAFETY ABORT",
                              describeCaptureAbort(status), true);
        return;
    }

    lastNoiseBaseline_ = calibration::CalibrationNoiseBaselineRunner::analyze(
        baselineData.data(),
        static_cast<int>(baselineData.size()),
        effectiveSampleRate(),
        true,    // output confirmed muted
        -45.0f,  // contamination threshold
        -115.0f // floor threshold
    );
    lastNoiseBaseline_.directMonitorState = "UserConfirmedOff";

    noiseReport_.rmsDbfs = lastNoiseBaseline_.rmsDbfs;
    noiseReport_.peakDbfs = lastNoiseBaseline_.peakDbfs;
    noiseReport_.spectrum32Bands = lastNoiseBaseline_.spectralBandDbfs;
    noiseReport_.inputChannel = calibrationInputChannelName;
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    noiseReport_.deviceName = device ? device->getName() : "Audio Device";
    noiseReport_.timestampIso = calibration::CalibrationProfileStore::getCurrentUtcIsoTimestamp();

    if (lastNoiseBaseline_.status == calibration::NoiseBaselineStatus::Contaminated)
    {
        stopTimer();
        failNoiseBaselineStep(NoiseBaselineState::Contaminated, "CONTAMINATED",
                              "Baseline contaminated: input level ("
                                  + juce::String(lastNoiseBaseline_.rmsDbfs, 1)
                                  + " dBFS) exceeds -45 dBFS while output muted.",
                              false);
        return;
    }

    if (lastNoiseBaseline_.status == calibration::NoiseBaselineStatus::Clipped)
    {
        stopTimer();
        failNoiseBaselineStep(NoiseBaselineState::SafetyAborted, "CLIPPED",
                              "Baseline clipped while output was muted. Sweep not started.", true);
        return;
    }

    // Valid or BelowMeasurementFloor: sub-step 2A passed, so 2B may start.
    // scopedCapture_ is intentionally PRESERVED across this transition.
    currentState = State::NoiseBaselinePassed;
    noiseBaselineState_ = NoiseBaselineState::Passed;
    noiseReport_.passed = true;
    noiseReport_.statusText = "PASSED";
    noiseReport_.failureReason = {};

    juce::Logger::writeToLog(juce::String("[Calibration 2A] Evaluated: Status=")
                             + juce::String(calibration::noiseBaselineStatusToString(lastNoiseBaseline_.status))
                             + " | RMS=" + juce::String(lastNoiseBaseline_.rmsDbfs, 1) + " dBFS"
                             + " | Peak=" + juce::String(lastNoiseBaseline_.peakDbfs, 1) + " dBFS"
                             + " | Device=" + noiseReport_.deviceName);

    // Unlock loopback only if it was locked; an already Stale 2B stays ready to measure.
    if (loopbackState_ == LoopbackState::Locked)
        loopbackState_ = LoopbackState::Ready;

    stopTimer();
    startTimerHz(30);

    applyIdleButtonLayout();
    resized();
    repaint();
}

void NativeCalibrationPanel::handleLoopbackTick()
{
    ++measurementStep;

    // Live loopback signal detection keeps the meter responsive during the sweep.
    const float inL = audioEngine.getInputPeakL();
    const float inR = audioEngine.getInputPeakR();
    liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));

    auto& receiver = audioEngine.getResponseReceiver();

    if (loopbackState_ == LoopbackState::MeasuringPreflight)
    {
        ++preflightTicks_;
        progressValue = progressFromTicks(preflightTicks_, kPreflightWindowSeconds);

        if (receiver.isOverloadTriggered() || std::max(inL, inR) > 0.5f) // > -6 dBFS
        {
            receiver.forceFinish();
            failLoopbackStep(LoopbackState::Failed,
                             "Loopback Preflight Abort: Dangerous signal level or feedback "
                             "(> -6 dBFS) detected while output was muted. Sweep not started.",
                             true);
            return;
        }

        if (receiver.isFinished() || preflightTicks_ >= kPreflightTicks)
            advanceToLoopbackSweep();

        return;
    }

    // Phase 2: Farina sweep capture.
    const double sampleRate = audioEngine.getSampleRate();
    const double totalDurationSec = (activeCaptureRequirements_.requiredSamples > 0 && sampleRate > 0.0)
                                        ? static_cast<double>(activeCaptureRequirements_.requiredSamples) / sampleRate
                                        : 1.30;
    progressValue = std::min(1.0, (measurementStep * kTickSeconds) / totalDurationSec);

    // Handshake: ask for the finalized snapshot as soon as the receiver is done or the
    // contractual UI deadline expires.
    if (!finalizingCapture_ && (receiver.isFinished() || measurementStep >= measurementDeadlineTicks_))
    {
        receiver.requestFinalizeCapture();
        finalizingCapture_ = true;
    }

    if (finalizingCapture_ && (receiver.isSnapshotReady() || measurementStep > measurementDeadlineTicks_ + 6))
    {
        stopTimer();
        processCalibrationResult();
    }
}

void NativeCalibrationPanel::handleIdleTick()
{
    const float inL = audioEngine.getInputPeakL();
    const float inR = audioEngine.getInputPeakR();
    liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));

    // Periodic runtime alignment check while an active calibration is applied.
    if (activeAlignment != calibration::ActiveCalibrationAlignment::AlignedAndActive)
        return;

    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    const auto currentSnapshot = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
        device, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

    if (calibration::CalibrationMatchEvaluator::isStillAligned(activeCalibrationRecord_, currentSnapshot))
        return;

    activeAlignment = calibration::ActiveCalibrationAlignment::Misaligned;
    neutralizeActiveTrim();
    loopbackState_ = LoopbackState::Stale;
    saveFeedbackText_ = "Audio configuration changed since last calibration. Loopback calibration marked STALE.";
    btnContinue.setEnabled(false);
    btnRetry.setVisible(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    resized();
}

void NativeCalibrationPanel::advanceToLoopbackSweep()
{
    auto& receiver = audioEngine.getResponseReceiver();
    receiver.forceFinish();
    std::vector<float> discarded;
    receiver.retrieveFinalizedSnapshot(discarded);

    // Preflight passed: start the contractual Farina sweep excitation.
    loopbackState_ = LoopbackState::MeasuringSweep;
    const auto sampleRate = effectiveSampleRate();

    activeCaptureRequirements_ = audio::CaptureRequirements::makeLoopbackRequirements(sampleRate);
    receiver.armWithRequirements(activeCaptureRequirements_);

    const int deadlineMs = audio::CaptureRequirements::computeUiDeadlineMs(activeCaptureRequirements_,
                                                                        sampleRate, 250);
    measurementDeadlineTicks_ = std::max(
        1, static_cast<int>(std::ceil(static_cast<double>(deadlineMs) / (kTickSeconds * 1000.0))));

    const auto endFreq = math::LoopbackCalibrator::computeSafeSweepMaxHz(sampleRate);
    audioEngine.getStimulusGenerator().setStimulus(audio::StimulusType::LogFarinaSweep, 1.0,
                                                   kSweepStartHz, endFreq);

    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setButtonText("Measuring Loopback Sweep (2B)...");
    resized();
    repaint();
}

//==============================================================================
// Failure paths
//==============================================================================
void NativeCalibrationPanel::failNoiseBaselineStep(NoiseBaselineState failure,
                                                   const juce::String& statusText,
                                                   const juce::String& reason,
                                                   bool clippingDetected)
{
    currentState = State::Failed;
    noiseBaselineState_ = failure;
    loopbackState_ = LoopbackState::Locked;

    noiseReport_.passed = false;
    noiseReport_.statusText = statusText;
    noiseReport_.failureReason = reason;

    scopedCapture_.reset();

    lastDiagnostics_.failureReason = reason;
    lastDiagnostics_.clippingDetected = clippingDetected;

    applyFailureButtonLayout();
    startTimerHz(30);
    resized();
    repaint();
}

void NativeCalibrationPanel::failLoopbackStep(LoopbackState failure,
                                              const juce::String& reason,
                                              bool clippingDetected)
{
    stopTimer();
    currentState = State::Failed;
    loopbackState_ = failure;
    loopbackReport_.passed = false;
    loopbackReport_.failureReason = reason;

    lastDiagnostics_.failureReason = reason;
    lastDiagnostics_.clippingDetected = clippingDetected;

    scopedCapture_.reset();

    applyFailureButtonLayout();
    startTimerHz(30);
    resized();
    repaint();
}

//==============================================================================
// Result publication
//==============================================================================
void NativeCalibrationPanel::fillDiagnosticsFromResult(const math::LoopbackCalibrationData& result)
{
    lastDiagnostics_.peakInDbfs = result.peakInDbfs;
    lastDiagnostics_.roundTripLatencyMs = result.roundTripLatencyMs;
    lastDiagnostics_.latencySamples = result.latencySamples;
    lastDiagnostics_.frequencyFlatnessDb = result.frequencyFlatnessDb;
    lastDiagnostics_.flatnessMinMagDb = result.flatnessMinMagDb;
    lastDiagnostics_.flatnessMaxMagDb = result.flatnessMaxMagDb;
    lastDiagnostics_.flatnessMinFreqHz = result.flatnessMinFreqHz;
    lastDiagnostics_.flatnessMaxFreqHz = result.flatnessMaxFreqHz;
    lastDiagnostics_.snrDb = result.snrDb;
    lastDiagnostics_.thdPercent = result.thdPlusNoisePercent;
    lastDiagnostics_.clippingDetected = result.clippingDetected;
    lastDiagnostics_.clippedSamplesCount = result.clippedSamplesCount;
    lastDiagnostics_.isCalibrated = result.isCalibrated;
    lastDiagnostics_.levelPassed = result.peakInDbfs > kMinUsablePeakDbfs;
    lastDiagnostics_.flatnessPassed = result.frequencyFlatnessDb < kMaxUsableFlatnessDb;
}

void NativeCalibrationPanel::publishLoopbackReport()
{
    loopbackReport_.roundTripLatencyMs = calibrationData.roundTripLatencyMs;
    loopbackReport_.latencySamples = calibrationData.latencySamples;
    loopbackReport_.frequencyFlatnessDb = calibrationData.frequencyFlatnessDb;
    loopbackReport_.recommendedTrimGain = calibrationData.recommendedTrimGain;
    loopbackReport_.snrDb = calibrationData.snrDb;
    loopbackReport_.snrMethod = calibrationData.snrMethod;
    loopbackReport_.thdPercent = calibrationData.thdPlusNoisePercent;
    loopbackReport_.peakInDbfs = calibrationData.peakInDbfs;
    loopbackReport_.phaseInverted = calibrationData.phaseInversionDetected;
    loopbackReport_.clippingDetected = calibrationData.clippingDetected;
    loopbackReport_.clippedSamples = calibrationData.clippedSamplesCount;
    loopbackReport_.timestampIso = calibration::CalibrationProfileStore::getCurrentUtcIsoTimestamp();
}

void NativeCalibrationPanel::processCalibrationResult()
{
    progressBar.setVisible(false);
    finalizingCapture_ = false;

    const auto sampleRate = audioEngine.getSampleRate();

    std::vector<float> captured;
    lastCaptureStatus_ = audioEngine.getResponseReceiver().retrieveFinalizedSnapshot(captured);

    lastDiagnostics_ = {};
    lastDiagnostics_.samplesCaptured = lastCaptureStatus_.samplesCaptured;
    lastDiagnostics_.samplesRequired = lastCaptureStatus_.requiredSamples;

    if (lastCaptureStatus_.result == audio::CaptureResult::Complete && !captured.empty())
    {
        std::optional<float> baselineRmsDb;
        if (lastNoiseBaseline_.status == calibration::NoiseBaselineStatus::Valid)
            baselineRmsDb = lastNoiseBaseline_.rmsDbfs;

        calibrationData = math::LoopbackCalibrator::analyzeLoopback(
            captured, sampleRate, 1.0, kSweepStartHz,
            math::LoopbackCalibrator::computeSafeSweepMaxHz(sampleRate), -3.0f, baselineRmsDb);

        fillDiagnosticsFromResult(calibrationData);

        if (!calibrationData.isCalibrated)
        {
            juce::StringArray reasons;
            if (!lastDiagnostics_.levelPassed)
                reasons.add("Peak level too low ("
                            + juce::String(calibrationData.peakInDbfs, 1) + " dBFS <= -40 dBFS)");
            if (calibrationData.clippingDetected)
                reasons.add("Clipping detected ("
                            + juce::String(calibrationData.clippedSamplesCount) + " samples)");
            if (!lastDiagnostics_.flatnessPassed)
                reasons.add("Flatness delta too large ("
                            + juce::String(calibrationData.frequencyFlatnessDb, 1) + " dB >= 6.0 dB)");

            lastDiagnostics_.failureReason = reasons.isEmpty() ? "Deconvolution analysis failed"
                                                              : reasons.joinIntoString("; ");
        }
    }
    else
    {
        calibrationData = {};
        calibrationData.isCalibrated = false;

        if (lastCaptureStatus_.result == audio::CaptureResult::TimedOutIncomplete)
        {
            lastDiagnostics_.failureReason = "Capture incomplete: "
                                             + juce::String(lastCaptureStatus_.samplesCaptured) + " / "
                                             + juce::String(lastCaptureStatus_.requiredSamples) + " samples";
        }
        else if (lastCaptureStatus_.result == audio::CaptureResult::Aborted)
        {
            const auto sustainedClipping = lastCaptureStatus_.abortReason
                                           == audio::CaptureAbortReason::SustainedClipping;
            lastDiagnostics_.failureReason = sustainedClipping ? "Aborted: sustained clipping" : "Aborted";
            calibrationData.clippingDetected = true;
        }
        else
        {
            lastDiagnostics_.failureReason = "Capture snapshot invalid or empty";
        }
    }

    // Device and channel snapshot for diagnostics.
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    lastDiagnostics_.deviceName = device != nullptr ? device->getName() : "None";
    lastDiagnostics_.driverType = device != nullptr ? device->getTypeName() : "Unknown";
    const auto inNames = device != nullptr ? device->getInputChannelNames() : juce::StringArray();
    const auto outNames = device != nullptr ? device->getOutputChannelNames() : juce::StringArray();
    lastDiagnostics_.inputChannel = inNames.isEmpty() ? "Ch 1" : inNames[0];
    lastDiagnostics_.outputChannel = outNames.isEmpty() ? "Ch 1" : outNames[0];

    // Restore any previous mock hardware through the RAII guard.
    scopedCapture_.reset();

    publishLoopbackReport();

    if (calibrationData.isCalibrated && !calibrationData.clippingDetected)
        completeSuccessfulCalibration();
    else
        completeFailedCalibration();

    juce::Logger::writeToLog(juce::String("=== CALIBRATION 2B RESULT SUMMARY ==="));
    juce::Logger::writeToLog(juce::String("Device / driver: ") + lastDiagnostics_.deviceName + " / " + lastDiagnostics_.driverType);
    juce::Logger::writeToLog(juce::String("Sample rate: ") + juce::String(sampleRate, 0) + " Hz");
    juce::Logger::writeToLog(juce::String("Buffer size: ") + juce::String(audioEngine.getCurrentBufferSizeSamples()));
    juce::Logger::writeToLog(juce::String("Input / output: ") + lastDiagnostics_.inputChannel + " -> " + lastDiagnostics_.outputChannel);
    juce::Logger::writeToLog(juce::String("2A status: ") + (noiseReport_.passed ? "PASS" : "FAIL"));
    juce::Logger::writeToLog(juce::String("2A RMS: ") + juce::String(noiseReport_.rmsDbfs, 1) + " dBFS");
    juce::Logger::writeToLog(juce::String("2A peak: ") + juce::String(noiseReport_.peakDbfs, 1) + " dBFS [INFO]");
    juce::Logger::writeToLog(juce::String("2B preflight: PASSED (200 ms muted)"));
    juce::Logger::writeToLog(juce::String("2B result: ") + (loopbackReport_.passed ? juce::String("PASS") : (juce::String("FAIL — ") + loopbackReport_.failureReason)));
    juce::Logger::writeToLog(juce::String("Samples captured / required: ") + juce::String(lastCaptureStatus_.samplesCaptured) + " / " + juce::String(lastCaptureStatus_.requiredSamples));
    juce::Logger::writeToLog(juce::String("Peak level: ") + juce::String(calibrationData.peakInDbfs, 1) + " dBFS");
    if (loopbackReport_.passed)
    {
        juce::Logger::writeToLog(juce::String("RTL: ") + juce::String(calibrationData.latencySamples) + " samples (" + juce::String(calibrationData.roundTripLatencyMs, 2) + " ms)");
        juce::Logger::writeToLog(juce::String("SNR: ") + juce::String(calibrationData.snrDb, 1) + " dB");
    }
    else
    {
        juce::Logger::writeToLog(juce::String("RTL: ") + juce::String(calibrationData.latencySamples) + " samples (" + juce::String(calibrationData.roundTripLatencyMs, 2) + " ms) [DIAGNOSTIC ONLY — CALIBRATION FAILED]");
        juce::Logger::writeToLog(juce::String("SNR: ") + juce::String(calibrationData.snrDb, 1) + " dB [PHYSICAL BASELINE, INVALID LOOPBACK]");
    }
    juce::Logger::writeToLog(juce::String("Flatness: ") + juce::String(calibrationData.frequencyFlatnessDb, 1) + " dB");
    juce::Logger::writeToLog(juce::String("SNR method: ") + juce::String(math::snrMeasurementMethodToString(calibrationData.snrMethod)));
    juce::Logger::writeToLog(juce::String("Abort reason: ") + (lastCaptureStatus_.result == audio::CaptureResult::Aborted ? "Aborted" : "None"));
    juce::Logger::writeToLog(juce::String("Step 3 unlocked: ") + (loopbackReport_.passed ? "YES" : "NO (LOCKED)"));
    juce::Logger::writeToLog(juce::String("====================================="));

    startTimerHz(30);
    resized();
    repaint();
}

void NativeCalibrationPanel::completeSuccessfulCalibration()
{
    currentState = State::Success;
    loopbackState_ = LoopbackState::Passed;
    loopbackReport_.passed = true;
    loopbackReport_.failureReason = {};
    activeAlignment = calibration::ActiveCalibrationAlignment::AlignedAndActive;

    activeCalibrationRecord_.schemaVersion = 1;
    activeCalibrationRecord_.calibrationResult = calibrationData;
    captureActiveDeviceAndRoutingSnapshot(activeCalibrationRecord_);

    audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);

    const auto compat = buildCompatibility(activeCalibrationRecord_);
    const auto captureMeta = buildCaptureMetadata(activeCaptureRequirements_.requiredSamples,
                                                  lastCaptureStatus_.samplesCaptured);

    const auto profileId = calibration::CalibrationSnapshot::generateDefaultProfileId(
        compat.deviceStableId, calibration::CalibrationSnapshot::getCurrentUtcIsoTimestamp());

    activeDraft_ = calibration::CalibrationDraft::fromMeasurement(
        buildProfileDisplayName(compat),
        profileId,
        compat,
        captureMeta,
        calibrationData,
        lastDiagnostics_.clippedSamplesCount,
        {},
        lastNoiseBaseline_);
    activeSnapshot_ = activeDraft_->sealSnapshot();

    txtDisplayName.setText(juce::String(activeDraft_->displayName), false);

    btnRetry.setButtonText("Retry Loopback (2B)");
    saveFeedbackText_ = {};
    applyResolvedButtonLayout(true, true);
    refreshSavedProfiles();

    if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);
}

void NativeCalibrationPanel::completeFailedCalibration()
{
    currentState = State::Failed;
    loopbackReport_.passed = false;

    if (calibrationData.clippingDetected)
    {
        loopbackState_ = LoopbackState::Clipped;
        loopbackReport_.failureReason = "Clipping detected during calibration sweep ("
                                        + juce::String(calibrationData.clippedSamplesCount) + " samples).";
    }
    else if (calibrationData.peakInDbfs <= kMinUsablePeakDbfs)
    {
        loopbackState_ = LoopbackState::SignalTooLow;
        loopbackReport_.failureReason = "Signal level too low ("
                                        + juce::String(calibrationData.peakInDbfs, 1)
                                        + " dBFS <= -40 dBFS). Verify cable connection.";
    }
    else
    {
        loopbackState_ = LoopbackState::Failed;
        loopbackReport_.failureReason = lastDiagnostics_.failureReason.isNotEmpty()
                                            ? lastDiagnostics_.failureReason
                                            : juce::String("Calibration analysis failed");
    }

    lastDiagnostics_.failureReason = loopbackReport_.failureReason;

    btnRetry.setButtonText("Retry Loopback (2B)");
    lblDisplayName.setVisible(false);
    txtDisplayName.setVisible(false);

    // A failed sweep must offer navigation back to 2A, retry and bypass.
    applyFailureButtonLayout();
}

void NativeCalibrationPanel::skipCalibration()
{
    stopTimer();
    audioEngine.getResponseReceiver().reset();

    calibrationData = {};
    calibrationData.isCalibrated = false;
    calibrationData.sampleRate = audioEngine.getSampleRate();
    calibrationData.recommendedTrimGain = 1.0f; // 0 dB unity gain
    calibrationData.targetHeadroomDbfs = -3.0f;
    calibrationData.roundTripLatencyMs = 0.0f;
    calibrationData.latencySamples = 0;
    calibrationData.frequencyFlatnessDb = 0.0f;
    calibrationData.deviceName = "Bypassed / Nominal (0 dB)";

    neutralizeActiveTrim();
    currentState = State::Skipped;
    activeAlignment = calibration::ActiveCalibrationAlignment::Bypassed;
    activeCalibrationRecord_ = {};

    saveFeedbackText_ = {};
    applyResolvedButtonLayout(false, false);

    if (onCalibrationSkipped)
        onCalibrationSkipped();
    else if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    repaint();
}

} // namespace abdaudiolab::gui
