#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../math/LoopbackCalibrator.h"
#include "../audio/LabAudioEngine.h"
#include "session/ProfilingSessionContracts.h"

#include "../calibration/CalibrationProfileStore.h"
#include "../calibration/CalibrationMatchEvaluator.h"
#include "../calibration/CalibrationSnapshot.h"

#include "../audio/ScopedPhysicalLoopbackCapture.h"

#include "calibration/CalibrationPanelTypes.h"
#include "calibration/CalibrationPanelViewState.h"

namespace abdaudiolab::gui
{

/**
 * @class NativeCalibrationPanel
 * @brief Step 2 of the setup workflow: 2A input noise floor + 2B physical loopback sweep.
 *
 * The class is intentionally thin: it owns state and orchestration only. Rendering lives in
 * src/gui/calibration/CalibrationPanelPainter*.cpp and button choreography lives in
 * NativeCalibrationPanelUiState.cpp, so every translation unit stays small and focused.
 */
class NativeCalibrationPanel : public juce::Component,
                               public juce::Timer
{
public:
    using State = calibrationpanel::State;
    using CalibrationSubView = calibrationpanel::SubView;
    using NoiseBaselineState = calibrationpanel::NoiseBaselineState;
    using LoopbackState = calibrationpanel::LoopbackState;
    using NoiseBaselineReport = calibrationpanel::NoiseBaselineReport;
    using LoopbackReport = calibrationpanel::LoopbackReport;
    using CalibrationDiagnostics = calibrationpanel::CalibrationDiagnostics;

    explicit NativeCalibrationPanel(audio::LabAudioEngine& engine);
    ~NativeCalibrationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void resetToInitialState();
    void startCalibrationSweep();
    void startNoiseBaselineCheck();
    void startPhysicalLoopbackSweep();
    void skipCalibration();

    void refreshSavedProfiles();
    void evaluateProfilesMatching();
    void reuseMatchingProfile();
    void neutralizeActiveTrim();
    void saveCurrentCalibrationProfile();
    void deleteSelectedProfile();

    [[nodiscard]] CalibrationSubView getActiveSubView() const noexcept { return activeSubView_; }
    void setActiveSubView(CalibrationSubView view) noexcept;

    [[nodiscard]] NoiseBaselineState getNoiseBaselineState() const noexcept { return noiseBaselineState_; }
    [[nodiscard]] LoopbackState getLoopbackState() const noexcept { return loopbackState_; }
    [[nodiscard]] const NoiseBaselineReport& getNoiseReport() const noexcept { return noiseReport_; }
    [[nodiscard]] const LoopbackReport& getLoopbackReport() const noexcept { return loopbackReport_; }

    [[nodiscard]] const math::LoopbackCalibrationData& getCalibrationData() const noexcept { return calibrationData; }
    [[nodiscard]] State getState() const noexcept { return currentState; }
    [[nodiscard]] calibration::ActiveCalibrationAlignment getActiveAlignment() const noexcept { return activeAlignment; }
    [[nodiscard]] const calibration::CalibrationMatchEvaluation& getMatchEvaluation() const noexcept { return matchEvaluation_; }
    [[nodiscard]] const std::optional<calibration::CalibrationRecord>& getMatchingProfile() const noexcept { return matchingProfile_; }
    [[nodiscard]] const std::optional<calibration::CalibrationSnapshot>& getActiveSnapshot() const noexcept { return activeSnapshot_; }
    [[nodiscard]] juce::Rectangle<int> getStepCard2ABounds() const noexcept { return stepCard2ABounds_; }
    [[nodiscard]] juce::Rectangle<int> getStepCard2BBounds() const noexcept { return stepCard2BBounds_; }

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
    //==========================================================================
    // Measurement lifecycle (NativeCalibrationPanelWorkflow.cpp)
    //==========================================================================
    void processCalibrationResult();
    void handleNoiseBaselineTick();
    void handleLoopbackTick();
    void handleIdleTick();
    void advanceToLoopbackSweep();
    void failNoiseBaselineStep(NoiseBaselineState failure,
                               const juce::String& statusText,
                               const juce::String& reason,
                               bool clippingDetected);
    void failLoopbackStep(LoopbackState failure, const juce::String& reason, bool clippingDetected);
    void handleAudioDeviceLost();
    void ensurePhysicalIsolation();
    void runActiveSubViewAction();
    void toggleActiveSubView();
    [[nodiscard]] double effectiveSampleRate() const;
    [[nodiscard]] int secondsToSamples(double seconds) const;
    [[nodiscard]] static double progressFromTicks(int ticks, double seconds);
    void captureActiveDeviceAndRoutingSnapshot(calibration::CalibrationRecord& record) const;
    void fillDiagnosticsFromResult(const math::LoopbackCalibrationData& result);
    void publishLoopbackReport();
    void completeSuccessfulCalibration();
    void completeFailedCalibration();

    //==========================================================================
    // Profile record shaping (NativeCalibrationPanelProfiles.cpp)
    //==========================================================================
    [[nodiscard]] static calibration::CalibrationCompatibility buildCompatibility(const calibration::CalibrationRecord& record);
    [[nodiscard]] static calibration::CalibrationCaptureMetadata buildCaptureMetadata(int requiredSamples, int capturedSamples);
    [[nodiscard]] static std::string buildProfileDisplayName(const calibration::CalibrationCompatibility& compat);
    void selectRelativeProfile(int delta);

    //==========================================================================
    // Button choreography (NativeCalibrationPanelUiState.cpp)
    //==========================================================================
    void applyMeasuringButtonLayout();
    void applyIdleButtonLayout();
    void applyFailureButtonLayout();
    void applyResolvedButtonLayout(bool offerSaveAction, bool showProfileNameEditor);
    void applyDigitalButtonLayout();
    void applySavedProfileActionLayout(const juce::Rectangle<int>& cardBounds, int cardRight);

    //==========================================================================
    // Bottom-row geometry (NativeCalibrationPanelLayout.cpp)
    //==========================================================================
    void layoutResolvedRow(int bottomY, int cardRight, int& rightBoundForLeftButtons);
    void layoutNoiseBaselineRow(int leftX, int bottomY, bool isMeasuringAny, int& rightBoundForLeftButtons);
    void layoutLoopbackRow(int leftX, int bottomY, bool isMeasuringAny, int& rightBoundForLeftButtons);
    void layoutBypassAndProfilesRow(int leftX, int bottomY, bool isMeasuringAny, int& rightBoundForLeftButtons);

    //==========================================================================
    // Rendering support (NativeCalibrationPanel.cpp)
    //==========================================================================
    [[nodiscard]] calibrationpanel::ViewState buildViewState() const;
    [[nodiscard]] static juce::Rectangle<int> computeCardBounds(const juce::Rectangle<int>& area);
    [[nodiscard]] static juce::Rectangle<float> computeCardBounds(const juce::Rectangle<float>& area);
    [[nodiscard]] static int computeStepperTop(const juce::Rectangle<int>& cardBounds);
    void updateActiveDraftDisplayName();
    void sealActiveDraftFromEditor();

    //==========================================================================
    // State
    //==========================================================================
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
    juce::TextButton btnRecheckBaseline { "↺ Re-check Baseline (2A)" };
    juce::TextButton btnSkip { "Bypass Calibration" };
    juce::TextButton btnContinue { "Continue to Step 3 ➔" };
    juce::TextButton btnRetry { "Retry Calibration" };
    juce::TextButton btnVerifyDigital { "Verify Digital Latency" };

    juce::TextButton btnReuseCalibration { "Reuse Saved Calibration" };
    juce::TextButton btnSaveCalibration { "Save Calibration" };
    juce::TextButton btnToggleSavedProfiles { "Saved Profiles" };
    juce::TextButton btnDeleteProfile { "Delete" };
    juce::TextButton btnViewProfileDetails { "View Details" };
    juce::TextButton btnPrevProfile { "◀" };
    juce::TextButton btnNextProfile { "▶" };

    calibration::ActiveCalibrationAlignment activeAlignment { calibration::ActiveCalibrationAlignment::None };
    std::optional<calibration::CalibrationRecord> matchingProfile_;
    calibration::CalibrationMatchEvaluation matchEvaluation_;
    calibration::CalibrationRecord activeCalibrationRecord_;
    std::optional<calibration::CalibrationSnapshot> activeSnapshot_;
    std::optional<calibration::CalibrationDraft> activeDraft_;
    calibration::CalibrationNoiseBaseline lastNoiseBaseline_;

    CalibrationSubView activeSubView_ { CalibrationSubView::NoiseBaseline_2A };
    NoiseBaselineState noiseBaselineState_ { NoiseBaselineState::NotChecked };
    LoopbackState loopbackState_ { LoopbackState::Locked };
    NoiseBaselineReport noiseReport_;
    LoopbackReport loopbackReport_;
    int preflightTicks_ { 0 };

    juce::Label lblDisplayName { "lblDisplayName", "Profile Name:" };
    juce::TextEditor txtDisplayName;

    bool isDigitalMode_ { false };
    bool isDigitalVerified_ { false };
    juce::String digitalStatusText_;

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    int measurementDeadlineTicks_ { 0 };
    bool finalizingCapture_ { false };
    bool baselineArmed_ { false };
    int baselineTicks_ { 0 };
    audio::CaptureRequirements activeCaptureRequirements_;
    audio::CaptureStatus lastCaptureStatus_;

    CalibrationDiagnostics lastDiagnostics_;

    float liveInputPeak { 0.0f };
    std::unique_ptr<audio::ScopedPhysicalLoopbackCapture> scopedCapture_;
    juce::Rectangle<int> stepCard2ABounds_;
    juce::Rectangle<int> stepCard2BBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeCalibrationPanel)
};

} // namespace abdaudiolab::gui
