#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../math/LoopbackCalibrator.h"
#include "../audio/LabAudioEngine.h"
#include "session/ProfilingSessionContracts.h"

#include "../calibration/CalibrationProfileStore.h"
#include "../calibration/CalibrationMatchEvaluator.h"

namespace abdaudiolab::gui
{

class NativeCalibrationPanel : public juce::Component,
                               public juce::Timer
{
public:
    enum class State
    {
        ReadyToMeasure,
        Measuring,
        Success,
        Failed,
        Skipped
    };

    explicit NativeCalibrationPanel(audio::LabAudioEngine& engine);
    ~NativeCalibrationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void resetToInitialState();
    void startCalibrationSweep();
    void skipCalibration();

    void refreshSavedProfiles();
    void evaluateProfilesMatching();
    void reuseMatchingProfile();
    void neutralizeActiveTrim();
    void saveCurrentCalibrationProfile();
    void deleteSelectedProfile();

    [[nodiscard]] const math::LoopbackCalibrationData& getCalibrationData() const noexcept { return calibrationData; }
    [[nodiscard]] State getState() const noexcept { return currentState; }
    [[nodiscard]] calibration::ActiveCalibrationAlignment getActiveAlignment() const noexcept { return activeAlignment; }
    [[nodiscard]] const calibration::CalibrationMatchEvaluation& getMatchEvaluation() const noexcept { return matchEvaluation_; }
    [[nodiscard]] const std::optional<calibration::CalibrationRecord>& getMatchingProfile() const noexcept { return matchingProfile_; }

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);
    void setCalibrationChannels(const juce::String& outCh, const juce::String& inCh)
    {
        calibrationOutputChannelName = outCh;
        calibrationInputChannelName = inCh;
        repaint();
    }
    [[nodiscard]] juce::String getCalibrationOutputChannel() const noexcept { return calibrationOutputChannelName; }
    [[nodiscard]] juce::String getCalibrationInputChannel() const noexcept { return calibrationInputChannelName; }

    std::function<void(const math::LoopbackCalibrationData&)> onCalibrationApplied;
    std::function<void()> onCalibrationSkipped;
    std::function<void()> onContinueToSession;
    std::function<void()> onVerifyDigitalRequested;

private:
    void processCalibrationResult();

    audio::LabAudioEngine& audioEngine;
    State currentState { State::ReadyToMeasure };
    math::LoopbackCalibrationData calibrationData;

    calibration::CalibrationProfileStore profileStore;
    std::vector<calibration::CalibrationRecord> savedProfiles;
    bool showSavedProfilesSection_ { false };
    bool showProfileDetails_ { false };
    int selectedProfileIndex_ { 0 };
    juce::String saveFeedbackText_;

    juce::String calibrationOutputChannelName { "Output 1" };
    juce::String calibrationInputChannelName { "Input 1" };

    juce::TextButton btnStartMeasure { "Start Loopback Calibration" };
    juce::TextButton btnSkip { "Continue without calibration (Bypass)" };
    juce::TextButton btnContinue { "Continue to Run Session (Step 3) ➔" };
    juce::TextButton btnRetry { "Retry Calibration" };
    juce::TextButton btnVerifyDigital { "Verify Digital Latency" };

    juce::TextButton btnReuseCalibration { "Reuse Saved Calibration" };
    juce::TextButton btnSaveCalibration { "Save Calibration" };
    juce::TextButton btnToggleSavedProfiles { "Saved Calibrations" };
    juce::TextButton btnDeleteProfile { "Delete" };
    juce::TextButton btnViewProfileDetails { "View Details" };

    calibration::ActiveCalibrationAlignment activeAlignment { calibration::ActiveCalibrationAlignment::None };
    std::optional<calibration::CalibrationRecord> matchingProfile_;
    calibration::CalibrationMatchEvaluation matchEvaluation_;
    calibration::CalibrationRecord activeCalibrationRecord_;

    bool isDigitalMode_ { false };
    bool isDigitalVerified_ { false };
    juce::String digitalStatusText_;

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    float liveInputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeCalibrationPanel)
};

} // namespace abdaudiolab::gui
