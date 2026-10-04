#include "NativeCalibrationPanel.h"
#include "SoundIdTheme.h"
#include "../BuildVersion.h"
#include <cmath>

namespace abdaudiolab::gui
{

NativeCalibrationPanel::NativeCalibrationPanel(audio::LabAudioEngine& engine)
    : audioEngine(engine), progressBar(progressValue)
{
    btnStartMeasure.setButtonText("Start Loopback Calibration");
    btnStartMeasure.setTooltip("Plays a 1.0s Farina sweep to measure roundtrip latency, gain trim, and H(f) compensation");
    btnStartMeasure.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnStartMeasure.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnStartMeasure.onClick = [this] { startCalibrationSweep(); };
    addAndMakeVisible(btnStartMeasure);

    btnReuseCalibration.setButtonText("Reuse Saved Calibration");
    btnReuseCalibration.setTooltip("Applies the matching calibration found for the current configuration");
    btnReuseCalibration.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.25f));
    btnReuseCalibration.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnReuseCalibration.onClick = [this] { reuseMatchingProfile(); };
    addChildComponent(btnReuseCalibration);

    btnSkip.setButtonText("Continue without calibration (Bypass)");
    btnSkip.setTooltip("Continues without audio interface latency or level compensation. Resets to unity gain.");
    btnSkip.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnSkip.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    btnSkip.onClick = [this] { skipCalibration(); };
    addAndMakeVisible(btnSkip);

    btnContinue.setButtonText("Continue to Run Session (Step 3) ➔");
    btnContinue.setTooltip("Proceed to Step 3: session excitation and profiling");
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueToSession)
            onContinueToSession();
    };
    addChildComponent(btnContinue);

    btnVerifyDigital.setButtonText("Verify Digital Latency");
    btnVerifyDigital.setTooltip("Verifies digital bus readiness and plugin roundtrip latency");
    btnVerifyDigital.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.25f));
    btnVerifyDigital.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnVerifyDigital.onClick = [this] {
        if (onVerifyDigitalRequested)
            onVerifyDigitalRequested();
    };
    addChildComponent(btnVerifyDigital);

    btnRetry.setButtonText("Retry Calibration");
    btnRetry.setTooltip("Re-runs calibration after verifying connections and levels");
    btnRetry.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber);
    btnRetry.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    btnRetry.onClick = [this] { startCalibrationSweep(); };
    addChildComponent(btnRetry);

    btnSaveCalibration.setButtonText("Save Calibration");
    btnSaveCalibration.setTooltip("Saves this calibration result to AppData for persistent reuse");
    btnSaveCalibration.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnSaveCalibration.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnSaveCalibration.onClick = [this] { saveCurrentCalibrationProfile(); };
    addChildComponent(btnSaveCalibration);

    btnToggleSavedProfiles.setButtonText("Saved Calibrations");
    btnToggleSavedProfiles.setTooltip("Shows or collapses the list of saved calibrations on disk");
    btnToggleSavedProfiles.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnToggleSavedProfiles.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleSavedProfiles.onClick = [this] {
        showSavedProfilesSection_ = !showSavedProfilesSection_;
        refreshSavedProfiles();
        resized();
        repaint();
    };
    addAndMakeVisible(btnToggleSavedProfiles);

    btnDeleteProfile.setButtonText("Delete");
    btnDeleteProfile.setTooltip("Deletes the selected saved calibration profile");
    btnDeleteProfile.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnDeleteProfile.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    btnDeleteProfile.onClick = [this] { deleteSelectedProfile(); };
    addChildComponent(btnDeleteProfile);

    btnViewProfileDetails.setButtonText("View Details");
    btnViewProfileDetails.setTooltip("Shows or hides technical details for the saved calibration");
    btnViewProfileDetails.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnViewProfileDetails.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    btnViewProfileDetails.onClick = [this] {
        showProfileDetails_ = !showProfileDetails_;
        repaint();
    };
    addChildComponent(btnViewProfileDetails);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::borderSubtle);
    addChildComponent(progressBar);

    refreshSavedProfiles();
    startTimerHz(30);
}

NativeCalibrationPanel::~NativeCalibrationPanel()
{
    stopTimer();
}

void NativeCalibrationPanel::neutralizeActiveTrim()
{
    audioEngine.setInputAutoTrim(1.0f);
}

void NativeCalibrationPanel::evaluateProfilesMatching()
{
    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    auto currentSnap = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
        dev, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

    matchingProfile_ = calibration::CalibrationMatchEvaluator::findBestMatchingProfile(
        savedProfiles, currentSnap, &matchEvaluation_);

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

    btnStartMeasure.setVisible(false);
    btnReuseCalibration.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnSaveCalibration.setVisible(false);

    saveFeedbackText_ = "Saved calibration profile reused successfully.";

    if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    startTimerHz(10);
    resized();
    repaint();
}

void NativeCalibrationPanel::resetToInitialState()
{
    currentState = State::ReadyToMeasure;
    activeAlignment = calibration::ActiveCalibrationAlignment::None;
    activeCalibrationRecord_ = {};
    neutralizeActiveTrim();
    measurementStep = 0;
    progressValue = 0.0;
    liveInputPeak = 0.0f;
    saveFeedbackText_ = {};
    showSavedProfilesSection_ = false;
    showProfileDetails_ = false;
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    btnSaveCalibration.setVisible(false);
    btnDeleteProfile.setVisible(false);
    btnViewProfileDetails.setVisible(false);
    progressBar.setVisible(false);

    evaluateProfilesMatching();

    startTimerHz(30);
    repaint();
}

void NativeCalibrationPanel::refreshSavedProfiles()
{
    savedProfiles = profileStore.list();
    btnToggleSavedProfiles.setButtonText("Saved Calibrations (" +
                                         juce::String((int)savedProfiles.size()) + ")");

    if (savedProfiles.empty())
    {
        selectedProfileIndex_ = 0;
        btnDeleteProfile.setVisible(false);
        btnViewProfileDetails.setVisible(false);
    }
    else
    {
        if (selectedProfileIndex_ >= (int)savedProfiles.size())
            selectedProfileIndex_ = (int)savedProfiles.size() - 1;
        if (selectedProfileIndex_ < 0)
            selectedProfileIndex_ = 0;

        btnDeleteProfile.setVisible(showSavedProfilesSection_);
        btnViewProfileDetails.setVisible(showSavedProfilesSection_);
    }

    evaluateProfilesMatching();
}

void NativeCalibrationPanel::saveCurrentCalibrationProfile()
{
    if (!calibrationData.isCalibrated || calibrationData.clippingDetected)
    {
        saveFeedbackText_ = "Cannot save an invalid calibration.";
        repaint();
        return;
    }

    calibration::CalibrationRecord rec;
    rec.schemaVersion = 1;
    rec.createdAt = calibration::CalibrationProfileStore::getCurrentUtcIsoTimestamp();

    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    rec.deviceSnapshot.deviceName = dev != nullptr ? dev->getName().toStdString() : "Audio Device";
    rec.deviceSnapshot.driverType = dev != nullptr ? dev->getTypeName().toStdString() : "Unknown";
    rec.deviceSnapshot.sampleRate = dev != nullptr ? dev->getCurrentSampleRate() : audioEngine.getSampleRate();
    rec.deviceSnapshot.bufferSizeSamples = dev != nullptr ? dev->getCurrentBufferSizeSamples() : 0;

    rec.routingSnapshot.inputChannelIndex = 0;
    rec.routingSnapshot.inputChannelLabel = calibrationInputChannelName.toStdString();
    rec.routingSnapshot.outputChannelIndex = 0;
    rec.routingSnapshot.outputChannelLabel = calibrationOutputChannelName.toStdString();

    rec.calibrationResult = calibrationData;
    rec.provenance.applicationVersion = version::kAppVersion;
    rec.provenance.calibrationAlgorithmVersion = 1;

    rec.profileId = calibration::CalibrationProfileStore::generateDefaultProfileId(
        rec.deviceSnapshot.deviceName, rec.createdAt);

    auto saveRes = profileStore.save(rec, false);
    if (saveRes.success)
    {
        saveFeedbackText_ = "Calibration profile saved successfully.";
        btnSaveCalibration.setEnabled(false);
        refreshSavedProfiles();
    }
    else
    {
        saveFeedbackText_ = "Error saving profile: " + juce::String(saveRes.errorMessage);
    }
    repaint();
}

void NativeCalibrationPanel::deleteSelectedProfile()
{
    if (selectedProfileIndex_ >= 0 && selectedProfileIndex_ < (int)savedProfiles.size())
    {
        std::string id = savedProfiles[static_cast<size_t>(selectedProfileIndex_)].profileId;
        profileStore.remove(id);
        refreshSavedProfiles();
        saveFeedbackText_ = "Profile deleted successfully.";
        resized();
        repaint();
    }
}

void NativeCalibrationPanel::startCalibrationSweep()
{
    currentState = State::Measuring;
    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setEnabled(false);
    btnSkip.setEnabled(false);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    progressBar.setVisible(true);
    repaint();

    // 1. Arm receiver for 1.25s capture
    double sr = audioEngine.getSampleRate();
    int captureSamples = static_cast<int>(sr * 1.25);
    audioEngine.getResponseReceiver().armCapture(captureSamples, 0.005f);

    // 2. Play 1.0s Farina sweep at full band
    audioEngine.getStimulusGenerator().setStimulus(audio::StimulusType::LogFarinaSweep, 1.0, 20.0f, 40000.0f);

    startTimer(50); // 50ms tick during sweep
}

void NativeCalibrationPanel::timerCallback()
{
    if (currentState == State::Measuring)
    {
        measurementStep++;
        progressValue = std::min(1.0, measurementStep * 0.05 / 1.25);

        if (measurementStep > 28) // ~1.4s
        {
            stopTimer();
            processCalibrationResult();
        }
    }
    else
    {
        // Live loopback signal detection
        float inL = audioEngine.getInputPeakL();
        float inR = audioEngine.getInputPeakR();
        liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));

        // Periodic runtime alignment check
        if (activeAlignment == calibration::ActiveCalibrationAlignment::AlignedAndActive)
        {
            auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
            auto currentSnap = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
                dev, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

            if (!calibration::CalibrationMatchEvaluator::isStillAligned(activeCalibrationRecord_, currentSnap))
            {
                activeAlignment = calibration::ActiveCalibrationAlignment::Misaligned;
                neutralizeActiveTrim();
                saveFeedbackText_ = "Audio configuration changed since last calibration. Previous calibration deactivated.";
                btnContinue.setEnabled(false);
                btnRetry.setVisible(true);
                btnSkip.setVisible(true);
                btnSkip.setEnabled(true);
                resized();
            }
        }
    }
    repaint();
}

void NativeCalibrationPanel::processCalibrationResult()
{
    progressBar.setVisible(false);
    double sr = audioEngine.getSampleRate();

    std::vector<float> captured;
    audioEngine.getResponseReceiver().retrieveRecordedData(captured);
    calibrationData = math::LoopbackCalibrator::analyzeLoopback(captured, sr, 1.0, 20.0f, 40000.0f, -3.0f);

    if (calibrationData.isCalibrated && !calibrationData.clippingDetected)
    {
        currentState = State::Success;
        activeAlignment = calibration::ActiveCalibrationAlignment::AlignedAndActive;

        activeCalibrationRecord_.schemaVersion = 1;
        activeCalibrationRecord_.calibrationResult = calibrationData;
        auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
        activeCalibrationRecord_.deviceSnapshot.deviceName = dev != nullptr ? dev->getName().toStdString() : "Audio Device";
        activeCalibrationRecord_.deviceSnapshot.driverType = dev != nullptr ? dev->getTypeName().toStdString() : "Unknown";
        activeCalibrationRecord_.deviceSnapshot.sampleRate = dev != nullptr ? dev->getCurrentSampleRate() : audioEngine.getSampleRate();
        activeCalibrationRecord_.deviceSnapshot.bufferSizeSamples = dev != nullptr ? dev->getCurrentBufferSizeSamples() : 0;
        activeCalibrationRecord_.routingSnapshot.inputChannelIndex = 0;
        activeCalibrationRecord_.routingSnapshot.inputChannelLabel = calibrationInputChannelName.toStdString();
        activeCalibrationRecord_.routingSnapshot.outputChannelIndex = 0;
        activeCalibrationRecord_.routingSnapshot.outputChannelLabel = calibrationOutputChannelName.toStdString();

        audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);
        btnStartMeasure.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnSkip.setVisible(false);
        btnRetry.setVisible(true);
        btnContinue.setVisible(true);
        btnContinue.setEnabled(true);
        btnSaveCalibration.setVisible(true);
        btnSaveCalibration.setEnabled(true);
        saveFeedbackText_ = {};
        refreshSavedProfiles();

        if (onCalibrationApplied)
            onCalibrationApplied(calibrationData);
    }
    else
    {
        currentState = State::Failed;
        btnStartMeasure.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnRetry.setVisible(true);
        btnSkip.setVisible(true);
        btnSkip.setEnabled(true);
        btnContinue.setVisible(false);
        btnSaveCalibration.setVisible(false);
    }
    startTimerHz(30);
    resized();
    repaint();
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

    btnStartMeasure.setVisible(false);
    btnReuseCalibration.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnSaveCalibration.setVisible(false);
    saveFeedbackText_ = {};

    if (onCalibrationSkipped)
        onCalibrationSkipped();
    else if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    repaint();
}

void NativeCalibrationPanel::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isDigitalMode_ = (snapshot.calibration.audio.requirement == session::CalibrationRequirement::NotApplicable);
    isDigitalVerified_ = snapshot.calibration.digital.verified;

    if (isDigitalMode_)
    {
        btnStartMeasure.setVisible(false);
        btnSkip.setVisible(false);
        btnRetry.setVisible(false);
        progressBar.setVisible(false);
        btnVerifyDigital.setVisible(true);
        btnContinue.setVisible(true);
        btnContinue.setEnabled(isDigitalVerified_);

        if (isDigitalVerified_)
        {
            digitalStatusText_ = "Digital path verified: 0 dBFS buffer, 0 ms physical latency.\nReady to proceed to profiling.";
            btnVerifyDigital.setButtonText("✓ Verification Complete");
            btnVerifyDigital.setEnabled(false);
        }
        else
        {
            digitalStatusText_ = "Digital mode active (VST3 Plugin / Virtual Synth)\n"
                                 "No loopback cable required.\n"
                                 "Analog interface calibration does not apply to plugins or virtual synthesizers.\n"
                                 "The application will use the digital path without DAC/ADC conversion compensation.";
            btnVerifyDigital.setButtonText("Verify Digital Latency");
            btnVerifyDigital.setEnabled(true);
        }
    }
    else
    {
        btnVerifyDigital.setVisible(false);
        if (currentState == State::ReadyToMeasure)
        {
            btnStartMeasure.setVisible(true);
            btnSkip.setVisible(true);
        }
    }
    repaint();
}

void NativeCalibrationPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Fondo del panel central
    g.fillAll(SoundIdTheme::bgLight);

    // Tarjeta central principal
    float maxCardW = juce::jmin(840.0f, area.getWidth() - 40.0f);
    float maxCardH = juce::jmin(540.0f, area.getHeight() - 30.0f);
    auto cardBounds = juce::Rectangle<float>((area.getWidth() - maxCardW) * 0.5f,
                                            (area.getHeight() - maxCardH) * 0.5f,
                                            maxCardW, maxCardH);

    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(cardBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = cardBounds.reduced(28.0f, 24.0f);

    // 1. Header Row
    auto headerRow = content.removeFromTop(32.0f);
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("2. Audio Interface Calibration", headerRow.removeFromLeft(460.0f), juce::Justification::centredLeft, true);

    // Estado Badge
    auto badgeRect = headerRow.removeFromRight(220.0f).reduced(0.0f, 3.0f);
    if (isDigitalMode_)
    {
        if (isDigitalVerified_)
        {
            g.setColour(juce::Colour(0xffd1fae5));
            g.fillRoundedRectangle(badgeRect, 6.0f);
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xff065f46));
            g.drawText("[ DIGITAL MODE ACTIVE ]", badgeRect, juce::Justification::centred, true);
        }
        else
        {
            g.setColour(juce::Colour(0xffe0e7ff));
            g.fillRoundedRectangle(badgeRect, 6.0f);
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xff3730a3));
            g.drawText("[ VERIFICATION PENDING ]", badgeRect, juce::Justification::centred, true);
        }
    }
    else if (currentState == State::Success)
    {
        g.setColour(juce::Colour(0xffd1fae5));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff065f46));
        g.drawText("[ CALIBRATION COMPLETED ]", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Skipped)
    {
        g.setColour(juce::Colour(0xfffef3c7));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff92400e));
        g.drawText("[ CALIBRATION BYPASSED ]", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setColour(juce::Colour(0xffe0e7ff));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff3730a3));
        g.drawText("[ MEASURING... ]", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Failed)
    {
        g.setColour(juce::Colour(0xfffee2e2));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("[ CHECK RETURN SIGNAL ]", badgeRect, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("[ READY TO CALIBRATE ]", badgeRect, juce::Justification::centred, true);
    }

    content.removeFromTop(10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(10.0f);

    // Subtítulo explicativo
    g.setFont(juce::FontOptions("Inter", 12.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Verify roundtrip latency and level calibration for your audio interface before profiling the instrument.",
               content.removeFromTop(18.0f), juce::Justification::topLeft, true);

    content.removeFromTop(2.0f);
    g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("This calibration measures the audio path of your audio interface, not the instrument being profiled.\n"
               "Connect the designated calibration output to the return input using a direct patch cable.\n"
               "The application will send a test signal to measure capture latency and system level.",
               content.removeFromTop(44.0f), juce::Justification::topLeft, true);

    content.removeFromTop(10.0f);

    // 2 Columnas de contenido
    float leftW = content.getWidth() * 0.52f;
    auto leftCol = content.removeFromLeft(leftW);
    content.removeFromLeft(20.0f);
    auto rightCol = content;

    if (isDigitalMode_)
    {
        auto drawDigitalItem = [&](int num, const juce::String& title, const juce::String& desc) {
            auto stepRow = leftCol.removeFromTop(62.0f);
            auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

            g.setColour(SoundIdTheme::accentBlue.withAlpha(0.15f));
            g.fillEllipse(circleBounds);
            g.setColour(SoundIdTheme::accentBlue);
            g.drawEllipse(circleBounds, 1.5f);

            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.drawText(juce::String(num), circleBounds, juce::Justification::centred, false);

            stepRow.removeFromLeft(10.0f);
            g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(title, stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(desc, stepRow, juce::Justification::topLeft, true);

            leftCol.removeFromTop(6.0f);
        };

        drawDigitalItem(1, "1. Digital Mode Active",
                        "No loopback cable required. Analog interface calibration does not apply to plugins or virtual synthesizers.");
        drawDigitalItem(2, "2. Direct Digital Path",
                        "The application will use the digital path without DAC/ADC conversion compensation.");
        drawDigitalItem(3, "3. Buffer Verification",
                        "Verifies that the host and plugin respond at the selected sample rate and block size.");

        // Columna derecha Digital
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(rightCol.withHeight(220.0f), 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(rightCol.withHeight(220.0f).reduced(0.5f), 8.0f, 1.0f);

        auto meterArea = rightCol.withHeight(220.0f).reduced(14.0f, 12.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("DIGITAL PATH STATUS", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        meterArea.removeFromTop(8.0f);

        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(isDigitalVerified_ ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber);
        g.drawText(isDigitalVerified_ ? "✓ Digital path verified" : "⟳ Digital verification pending",
                   meterArea.removeFromTop(20.0f), juce::Justification::centredLeft, true);

        meterArea.removeFromTop(8.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Mode: Digital (VST3 Plugin / Virtual Synth)\n"
                   "Analog Latency: 0 ms\n"
                   "Nominal Level: 0 dBFS\n"
                   "DAC/ADC Conversion: Not required",
                   meterArea, juce::Justification::topLeft, true);
        return;
    }

    // --- Columna Izquierda: 3 Pasos del protocolo ---
    // Paso 1: Conecta el cable de loopback
    {
        auto stepRow = leftCol.removeFromTop(74.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("1", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("1. Connect loopback cable", stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        auto channelBox = stepRow.removeFromTop(18.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText("Calibration Output: " + calibrationOutputChannelName +
                   "  ➔  Return Input: " + calibrationInputChannelName,
                   channelBox, juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Use a direct line-level cable. Do not connect the synthesizer being profiled yet.",
                   stepRow, juce::Justification::topLeft, true);

        leftCol.removeFromTop(6.0f);
    }

    // Paso 2: Comprueba el nivel de retorno
    {
        auto stepRow = leftCol.removeFromTop(60.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("2", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("2. Check return level", stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Ensure the signal is neither muted nor clipping. The engine will check level and apply recommended input trim.",
                   stepRow, juce::Justification::topLeft, true);

        leftCol.removeFromTop(6.0f);
    }

    // Paso 3: Ejecuta la calibración
    {
        auto stepRow = leftCol.removeFromTop(60.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("3", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("3. Run calibration", stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("The application will play a short sweep and calculate roundtrip latency and level trim for this audio path.",
                   stepRow, juce::Justification::topLeft, true);
    }

    // --- Columna Derecha: Monitor Balístico de Nivel & Métricas ---
    float monitorHeight = 240.0f;
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(rightCol.withHeight(monitorHeight), 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(rightCol.withHeight(monitorHeight).reduced(0.5f), 8.0f, 1.0f);

    auto meterArea = rightCol.withHeight(monitorHeight).reduced(14.0f, 12.0f);

    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("REAL-TIME SIGNAL MONITOR", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    meterArea.removeFromTop(6.0f);

    // Vúmetro horizontal con marcas de referencia
    auto vumeterBar = meterArea.removeFromTop(14.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.fillRoundedRectangle(vumeterBar, 4.0f);

    // Optimal range indicator background (-24 dBFS to -3 dBFS corresponds approx to 0.15 to 0.75 width)
    float optStart = vumeterBar.getX() + vumeterBar.getWidth() * 0.15f;
    float optEnd = vumeterBar.getX() + vumeterBar.getWidth() * 0.75f;
    g.setColour(SoundIdTheme::accentGreen.withAlpha(0.12f));
    g.fillRect(juce::Rectangle<float>(optStart, vumeterBar.getY(), optEnd - optStart, vumeterBar.getHeight()));

    float peakNorm = juce::jlimit(0.0f, 1.0f, liveInputPeak);
    auto fillBar = vumeterBar.withWidth(vumeterBar.getWidth() * peakNorm);

    if (peakNorm > 0.95f)
        g.setColour(SoundIdTheme::accentRed);
    else if (peakNorm >= 0.15f)
        g.setColour(SoundIdTheme::accentGreen);
    else if (peakNorm > 0.02f)
        g.setColour(SoundIdTheme::accentAmber);
    else
        g.setColour(SoundIdTheme::textMuted.withAlpha(0.4f));

    g.fillRoundedRectangle(fillBar, 4.0f);

    meterArea.removeFromTop(4.0f);
    float liveDb = 20.0f * std::log10(std::max(liveInputPeak, 1e-4f));
    juce::String dbText = (liveDb < -70.0f) ? "-inf dBFS" : juce::String(liveDb, 1) + " dBFS";

    juce::String statusRange;
    juce::Colour statusColour;
    if (liveDb > -0.5f)
    {
        statusRange = " [Clipping / Overload]";
        statusColour = SoundIdTheme::accentRed;
    }
    else if (liveDb >= -24.0f)
    {
        statusRange = " [Optimal Level]";
        statusColour = SoundIdTheme::accentGreen;
    }
    else if (liveDb >= -40.0f)
    {
        statusRange = " [Low Level - Turn Up]";
        statusColour = SoundIdTheme::accentAmber;
    }
    else
    {
        statusRange = " [Idle / Silent]";
        statusColour = SoundIdTheme::textMuted;
    }

    auto levelRow = meterArea.removeFromTop(16.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Input: " + dbText, levelRow.removeFromLeft(110.0f), juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
    g.setColour(statusColour);
    g.drawText(statusRange, levelRow, juce::Justification::centredLeft, true);

    meterArea.removeFromTop(4.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(meterArea.removeFromTop(1.0f));
    meterArea.removeFromTop(6.0f);

    // Métricas y mensajes de calibración
    if (currentState == State::Success)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("Calibration Completed", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("The audio path is verified and ready for profiling.", meterArea.removeFromTop(14.0f), juce::Justification::centredLeft, true);

        float trimDb = 20.0f * std::log10(std::max(calibrationData.recommendedTrimGain, 1e-4f));
        juce::String sign = (trimDb >= 0.0f) ? "+" : "";

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("Roundtrip Latency: " + juce::String(calibrationData.roundTripLatencyMs, 2) + " ms (" +
                   juce::String(calibrationData.latencySamples) + " samples)",
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        g.drawText("Level Trim: " + sign + juce::String(trimDb, 2) + " dB",
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        juce::String polarityStr = calibrationData.phaseInversionDetected ? juce::String("Inverted (180 deg)") : juce::String("Normal");
        g.drawText("Polarity: " + polarityStr,
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        if (calibrationData.clippingDetected)
        {
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentAmber);
            g.drawText("Warning: Clipping detected during calibration sweep.",
                       meterArea.removeFromTop(14.0f), juce::Justification::centredLeft, true);
        }

        if (!saveFeedbackText_.isEmpty())
        {
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(saveFeedbackText_, meterArea.removeFromTop(20.0f), juce::Justification::topLeft, true);
        }
        else
        {
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText("Save this calibration profile to disk by clicking [Save Calibration].",
                       meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);
        }
    }
    else if (currentState == State::Failed)
    {
        if (calibrationData.clippingDetected)
        {
            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText("Return signal is clipping / overloading.", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText("Reduce input gain or output volume on your interface and re-run calibration.",
                       meterArea, juce::Justification::topLeft, true);
        }
        else
        {
            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText("Insufficient or invalid return signal detected.", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText("Verify patch cable connection between designated output and input, and ensure input is not muted.",
                       meterArea, juce::Justification::topLeft, true);
        }
    }
    else if (currentState == State::Skipped)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("Calibration Bypassed", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Mode: Bypassed (no latency compensation)\n"
                   "Gain Trim: 0.0 dB nominal\n"
                   "Assumed Latency: 0 samples (0.0 ms)",
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText("Measuring interface response...", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Playing logarithmic Farina sine sweep.\n"
                   "Computing latency, auto-trim, and phase polarity...",
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (activeAlignment == calibration::ActiveCalibrationAlignment::Misaligned)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("Configuration changed since last calibration", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Previous latency and level compensation is inactive.\n"
                   "Please recalibrate or continue without calibration (Bypass).",
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("Matching Calibration Found", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textPrimary);
        juce::String devStr = juce::String(matchingProfile_->deviceSnapshot.deviceName) + " \u00B7 " +
                              juce::String(matchingProfile_->deviceSnapshot.sampleRate / 1000.0, 1) + " kHz \u00B7 Buffer " +
                              juce::String(matchingProfile_->deviceSnapshot.bufferSizeSamples);
        g.drawText(devStr, meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("Notice: Current audio device matches saved profile.\n"
                   "Physical cable changes or analog preamp gain adjustments cannot be detected automatically.",
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (!savedProfiles.empty() && matchEvaluation_.status == calibration::CalibrationMatchStatus::ConfigurationMismatch)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("Configuration Differs from Saved Profile", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        juce::String diffMsg = "Saved calibration does not match current device settings.\n";
        for (const auto& d : matchEvaluation_.differences)
        {
            diffMsg += juce::String(d.fieldName) + ": " + juce::String(d.profileValue) + " \u2192 " + juce::String(d.currentValue) + "\n";
        }
        g.drawText(diffMsg, meterArea, juce::Justification::topLeft, true);
    }
    else
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("Awaiting Calibration", meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Verify patch cable and click 'Start Loopback Calibration'.\n"
                   "The engine will measure levels and compute required compensation.",
                   meterArea.removeFromTop(32.0f), juce::Justification::topLeft, true);

        meterArea.removeFromTop(6.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("Save your calibration once completed successfully.",
                   meterArea, juce::Justification::topLeft, true);
    }

    // Sección compacta de perfiles guardados
    if (showSavedProfilesSection_)
    {
        auto savedArea = rightCol;
        savedArea.removeFromTop(226.0f);
        auto savedBox = savedArea.removeFromTop(juce::jmin(140.0f, savedArea.getHeight() - 65.0f));

        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(savedBox, 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(savedBox.reduced(0.5f), 8.0f, 1.0f);

        auto inner = savedBox.reduced(10.0f, 8.0f);
        auto titleRow = inner.removeFromTop(16.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("SAVED PROFILES IN APPDATA", titleRow, juce::Justification::centredLeft, true);

        inner.removeFromTop(4.0f);

        if (savedProfiles.empty())
        {
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText("No saved calibration profiles found on disk.", inner, juce::Justification::centredLeft, true);
        }
        else
        {
            const auto& p = savedProfiles[static_cast<size_t>(selectedProfileIndex_)];

            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(juce::String(p.deviceSnapshot.deviceName) + " (" + juce::String(p.deviceSnapshot.driverType) + ")",
                       inner.removeFromTop(15.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            juce::String srKhz = juce::String(p.deviceSnapshot.sampleRate / 1000.0, 1) + " kHz";
            juce::String bufSpl = "Buffer " + juce::String(p.deviceSnapshot.bufferSizeSamples);
            juce::String routeStr = juce::String(p.routingSnapshot.outputChannelLabel) + " \u2794 " + juce::String(p.routingSnapshot.inputChannelLabel);
            g.drawText(srKhz + " \u00B7 " + bufSpl + " \u00B7 " + routeStr,
                       inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textMuted);
            g.drawText("Date: " + juce::String(p.createdAt),
                       inner.removeFromTop(13.0f), juce::Justification::centredLeft, true);

            if (showProfileDetails_)
            {
                g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
                g.setColour(SoundIdTheme::accentGreen);
                g.drawText("Lat: " + juce::String(p.calibrationResult.roundTripLatencyMs, 1) + " ms (" +
                           juce::String(p.calibrationResult.latencySamples) + " spls) \u00B7 SNR: " +
                           juce::String(p.calibrationResult.snrDb, 1) + " dB \u00B7 Flat: " +
                           juce::String(p.calibrationResult.frequencyFlatnessDb, 1) + " dB",
                           inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
            }
        }
    }
}

void NativeCalibrationPanel::resized()
{
    auto area = getLocalBounds();
    int maxCardW = juce::jmin(840, area.getWidth() - 40);
    int maxCardH = juce::jmin(540, area.getHeight() - 30);
    auto cardBounds = juce::Rectangle<int>((area.getWidth() - maxCardW) / 2,
                                          (area.getHeight() - maxCardH) / 2,
                                          maxCardW, maxCardH);

    int bottomY = cardBounds.getBottom() - 56;
    int leftX = cardBounds.getX() + 28;
    int cardRight = cardBounds.getRight() - 28;

    progressBar.setBounds(leftX, bottomY - 24, cardRight - leftX, 12);

    if (isDigitalMode_)
    {
        btnVerifyDigital.setVisible(true);
        btnContinue.setVisible(true);
        btnVerifyDigital.setBounds(leftX, bottomY, 240, 36);
        btnContinue.setBounds(cardRight - 280, bottomY, 280, 36);

        btnStartMeasure.setVisible(false);
        btnRetry.setVisible(false);
        btnSkip.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnToggleSavedProfiles.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnDeleteProfile.setVisible(false);
        btnViewProfileDetails.setVisible(false);
        return;
    }

    btnVerifyDigital.setVisible(false);
    btnToggleSavedProfiles.setVisible(true);

    int rightBoundForLeftButtons = cardRight;

    if (currentState == State::Success)
    {
        btnContinue.setVisible(true);
        btnContinue.setBounds(cardRight - 230, bottomY, 230, 36);

        btnSaveCalibration.setVisible(true);
        btnSaveCalibration.setBounds(cardRight - 400, bottomY, 160, 36);

        rightBoundForLeftButtons = cardRight - 412;

        btnRetry.setVisible(true);
        btnRetry.setBounds(leftX, bottomY, 140, 36);

        int availW = rightBoundForLeftButtons - (leftX + 148);
        btnToggleSavedProfiles.setBounds(leftX + 148, bottomY, std::min(190, std::max(120, availW)), 36);

        btnSkip.setVisible(false);
        btnStartMeasure.setVisible(false);
        btnReuseCalibration.setVisible(false);
    }
    else if (currentState == State::ReadyToMeasure)
    {
        btnContinue.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnRetry.setVisible(false);

        btnStartMeasure.setVisible(true);
        btnStartMeasure.setEnabled(true);

        if (matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
        {
            btnStartMeasure.setBounds(cardRight - 230, bottomY, 230, 36);
            btnReuseCalibration.setVisible(true);
            btnReuseCalibration.setBounds(cardRight - 460, bottomY, 220, 36);
            rightBoundForLeftButtons = cardRight - 472;
        }
        else
        {
            btnReuseCalibration.setVisible(false);
            btnStartMeasure.setBounds(cardRight - 250, bottomY, 250, 36);
            rightBoundForLeftButtons = cardRight - 262;
        }

        btnSkip.setVisible(true);
        btnSkip.setEnabled(true);

        int availW = rightBoundForLeftButtons - leftX - 10;
        int btnW = std::min(190, std::max(110, availW / 2));
        btnSkip.setBounds(leftX, bottomY, btnW, 36);
        btnToggleSavedProfiles.setBounds(leftX + btnW + 10, bottomY, btnW, 36);
    }
    else if (currentState == State::Failed)
    {
        btnContinue.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnStartMeasure.setVisible(false);
        btnReuseCalibration.setVisible(false);

        btnRetry.setVisible(true);
        btnRetry.setBounds(cardRight - 190, bottomY, 190, 36);
        rightBoundForLeftButtons = cardRight - 202;

        btnSkip.setVisible(true);
        btnSkip.setEnabled(true);

        int availW = rightBoundForLeftButtons - leftX - 10;
        int btnW = std::min(190, std::max(110, availW / 2));
        btnSkip.setBounds(leftX, bottomY, btnW, 36);
        btnToggleSavedProfiles.setBounds(leftX + btnW + 10, bottomY, btnW, 36);
    }
    else if (currentState == State::Measuring)
    {
        btnContinue.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnRetry.setVisible(false);
        btnReuseCalibration.setVisible(false);

        btnStartMeasure.setVisible(true);
        btnStartMeasure.setEnabled(false);
        btnStartMeasure.setBounds(cardRight - 250, bottomY, 250, 36);
        rightBoundForLeftButtons = cardRight - 262;

        btnSkip.setVisible(true);
        btnSkip.setEnabled(false);

        int availW = rightBoundForLeftButtons - leftX - 10;
        int btnW = std::min(190, std::max(110, availW / 2));
        btnSkip.setBounds(leftX, bottomY, btnW, 36);
        btnToggleSavedProfiles.setBounds(leftX + btnW + 10, bottomY, btnW, 36);
    }
    else // State::Skipped
    {
        btnStartMeasure.setVisible(true);
        btnStartMeasure.setEnabled(true);
        btnStartMeasure.setBounds(cardRight - 250, bottomY, 250, 36);
        rightBoundForLeftButtons = cardRight - 262;

        btnContinue.setVisible(true);
        btnContinue.setBounds(cardRight - 460, bottomY, 200, 36);
        rightBoundForLeftButtons = cardRight - 472;

        btnSaveCalibration.setVisible(false);
        btnRetry.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnSkip.setVisible(false);

        int availW = rightBoundForLeftButtons - leftX;
        btnToggleSavedProfiles.setBounds(leftX, bottomY, std::min(190, std::max(120, availW)), 36);
    }

    // Botones dentro de la sección de perfiles guardados
    if (showSavedProfilesSection_ && !savedProfiles.empty())
    {
        int sectionX = cardBounds.getX() + static_cast<int>(maxCardW * 0.52f) + 48;
        int sectionW = cardRight - sectionX;
        int sectionBottom = cardBounds.getY() + 24 + 32 + 44 + 10 + 226 + 140;

        btnViewProfileDetails.setVisible(true);
        btnDeleteProfile.setVisible(true);
        btnViewProfileDetails.setBounds(sectionX + sectionW - 180, sectionBottom - 30, 95, 24);
        btnDeleteProfile.setBounds(sectionX + sectionW - 80, sectionBottom - 30, 75, 24);
    }
    else
    {
        btnViewProfileDetails.setVisible(false);
        btnDeleteProfile.setVisible(false);
    }
}

} // namespace abdaudiolab::gui
