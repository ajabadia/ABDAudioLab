/**
 * @file NativeCalibrationPanelProfiles.cpp
 * @brief Saved-calibration profile handling: listing, matching, reuse, persistence and
 *        the shared builders that shape a calibration record / snapshot.
 * @author ABDSynths
 * @date 2026
 */

#include "NativeCalibrationPanel.h"

#include "../BuildVersion.h"

#include <cmath>

namespace abdaudiolab::gui
{

//==============================================================================
// Record shaping helpers (shared by reuse, save and result publication).
//==============================================================================
void NativeCalibrationPanel::captureActiveDeviceAndRoutingSnapshot(calibration::CalibrationRecord& record) const
{
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();

    record.deviceSnapshot.deviceName = device != nullptr ? device->getName().toStdString() : "Audio Device";
    record.deviceSnapshot.driverType = device != nullptr ? device->getTypeName().toStdString() : "Unknown";
    record.deviceSnapshot.sampleRate = device != nullptr ? device->getCurrentSampleRate() : audioEngine.getSampleRate();
    record.deviceSnapshot.bufferSizeSamples = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;

    record.routingSnapshot.inputChannelIndex = 0;
    record.routingSnapshot.inputChannelLabel = calibrationInputChannelName.toStdString();
    record.routingSnapshot.outputChannelIndex = 0;
    record.routingSnapshot.outputChannelLabel = calibrationOutputChannelName.toStdString();
}

calibration::CalibrationCompatibility
NativeCalibrationPanel::buildCompatibility(const calibration::CalibrationRecord& record)
{
    calibration::CalibrationCompatibility compat;

    compat.deviceStableId = record.deviceSnapshot.deviceName;
    compat.driverType = record.deviceSnapshot.driverType;
    compat.sampleRateHz = record.deviceSnapshot.sampleRate;
    compat.bufferSamples = record.deviceSnapshot.bufferSizeSamples;
    compat.inputChannelIndex = record.routingSnapshot.inputChannelIndex;
    compat.outputChannelIndex = record.routingSnapshot.outputChannelIndex;
    compat.routingDescription = record.routingSnapshot.outputChannelLabel + " -> "
                                + record.routingSnapshot.inputChannelLabel;

    return compat;
}

calibration::CalibrationCaptureMetadata
NativeCalibrationPanel::buildCaptureMetadata(int requiredSamples, int capturedSamples)
{
    // The physical capture invariants (ADC verified, mock hardware isolated, plugin bypassed)
    // keep their contract defaults; only the sample counts vary per run.
    calibration::CalibrationCaptureMetadata captureMeta;
    captureMeta.sweepDurationMs = 1000;
    captureMeta.latencyMarginMs = 200;
    captureMeta.decayTailMs = 100;
    captureMeta.requiredSamples = requiredSamples;
    captureMeta.capturedSamples = capturedSamples;

    return captureMeta;
}

std::string NativeCalibrationPanel::buildProfileDisplayName(const calibration::CalibrationCompatibility& compat)
{
    const auto rateKhz = static_cast<int>(std::lround(compat.sampleRateHz / 1000.0));

    return compat.deviceStableId + " — " + compat.routingDescription + " — "
         + std::to_string(rateKhz) + " kHz";
}

//==============================================================================
void NativeCalibrationPanel::evaluateProfilesMatching()
{
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    const auto currentSnapshot = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
        device, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

    matchingProfile_ = calibration::CalibrationMatchEvaluator::findBestMatchingProfile(
        savedProfiles, currentSnapshot, &matchEvaluation_);

    if (currentState == State::ReadyToMeasure && matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
    {
        btnReuseCalibration.setVisible(true);
        btnReuseCalibration.setEnabled(true);
    }
    else
    {
        btnReuseCalibration.setVisible(false);
    }

    resized();
}

void NativeCalibrationPanel::refreshSavedProfiles()
{
    savedProfiles = profileStore.list();
    btnToggleSavedProfiles.setButtonText("Saved Calibrations ("
                                         + juce::String(static_cast<int>(savedProfiles.size())) + ")");

    selectedProfileIndex_ = savedProfiles.empty()
                                ? 0
                                : juce::jlimit(0, static_cast<int>(savedProfiles.size()) - 1,
                                               selectedProfileIndex_);

    // Per-action visibility and bounds live in applySavedProfileActionLayout (reached via
    // evaluateProfilesMatching -> resized) so there is a single place that owns this row.
    evaluateProfilesMatching();
}

void NativeCalibrationPanel::selectRelativeProfile(int delta)
{
    if (savedProfiles.empty() || delta == 0)
        return;

    const auto lastIndex = static_cast<int>(savedProfiles.size()) - 1;
    const auto nextIndex = juce::jlimit(0, lastIndex, selectedProfileIndex_ + delta);

    if (nextIndex == selectedProfileIndex_)
        return;

    selectedProfileIndex_ = nextIndex;
    resized();
    repaint();
}

void NativeCalibrationPanel::reuseMatchingProfile()
{
    if (!matchingProfile_.has_value() || !matchEvaluation_.isActionableMatch)
        return;

    stopTimer();

    calibrationData = matchingProfile_->calibrationResult;
    activeCalibrationRecord_ = *matchingProfile_;
    activeAlignment = calibration::ActiveCalibrationAlignment::AlignedAndActive;
    currentState = State::Success;

    audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);

    const auto compat = buildCompatibility(activeCalibrationRecord_);
    activeDraft_ = calibration::CalibrationDraft::fromMeasurement(
        buildProfileDisplayName(compat),
        activeCalibrationRecord_.profileId,
        compat,
        buildCaptureMetadata(0, 0),
        activeCalibrationRecord_.calibrationResult,
        0);
    activeSnapshot_ = activeDraft_->sealSnapshot();

    applyResolvedButtonLayout(false, true);

    if (activeSnapshot_.has_value())
        txtDisplayName.setText(juce::String(activeSnapshot_->displayName), false);

    saveFeedbackText_ = "Saved calibration profile reused successfully.";

    if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    startTimerHz(10);
    resized();
    repaint();
}

void NativeCalibrationPanel::saveCurrentCalibrationProfile()
{
    if (!calibrationData.isCalibrated || calibrationData.clippingDetected)
    {
        saveFeedbackText_ = "Cannot save an invalid calibration.";
        repaint();
        return;
    }

    sealActiveDraftFromEditor();

    if (activeDraft_.has_value())
    {
        // Snapshotted measurement path: the draft already carries the sealed snapshot.
        const auto result = profileStore.saveSnapshot(*activeSnapshot_, false);
        if (!result.success)
        {
            saveFeedbackText_ = "Error saving profile: " + juce::String(result.errorMessage);
            repaint();
            return;
        }
    }
    else
    {
        // Legacy path: no draft available, so the record is rebuilt from the live configuration.
        calibration::CalibrationRecord record;
        record.schemaVersion = 1;
        record.createdAt = calibration::CalibrationProfileStore::getCurrentUtcIsoTimestamp();

        captureActiveDeviceAndRoutingSnapshot(record);
        record.calibrationResult = calibrationData;
        record.provenance.applicationVersion = version::kAppVersion;
        record.provenance.calibrationAlgorithmVersion = 1;
        record.profileId = calibration::CalibrationProfileStore::generateDefaultProfileId(
            record.deviceSnapshot.deviceName, record.createdAt);

        const auto result = profileStore.save(record, false);
        if (!result.success)
        {
            saveFeedbackText_ = "Error saving profile: " + juce::String(result.errorMessage);
            repaint();
            return;
        }
    }

    saveFeedbackText_ = "Calibration profile saved successfully.";
    btnSaveCalibration.setEnabled(false);
    refreshSavedProfiles();
    repaint();
}

void NativeCalibrationPanel::deleteSelectedProfile()
{
    if (selectedProfileIndex_ < 0 || selectedProfileIndex_ >= static_cast<int>(savedProfiles.size()))
        return;

    const auto id = savedProfiles[static_cast<size_t>(selectedProfileIndex_)].profileId;
    profileStore.remove(id);

    refreshSavedProfiles();
    saveFeedbackText_ = "Profile deleted successfully.";
    resized();
    repaint();
}

} // namespace abdaudiolab::gui
