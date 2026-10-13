/**
 * @file NativeCalibrationPanelUiState.cpp
 * @brief Button choreography for Step 2. Every outcome funnels through one of these
 *        layout helpers so the operator never sees an inconsistent action bar.
 * @author ABDSynths
 * @date 2026
 */

#include "NativeCalibrationPanel.h"

#include "calibration/CalibrationPanelMetrics.h"

namespace abdaudiolab::gui
{

//==============================================================================
void NativeCalibrationPanel::resetToInitialState()
{
    scopedCapture_.reset();

    currentState = State::ReadyToMeasure;
    activeSubView_ = CalibrationSubView::NoiseBaseline_2A;
    noiseBaselineState_ = NoiseBaselineState::NotChecked;
    loopbackState_ = LoopbackState::Locked;
    noiseReport_ = {};
    loopbackReport_ = {};
    preflightTicks_ = 0;

    activeAlignment = calibration::ActiveCalibrationAlignment::None;
    activeCalibrationRecord_ = {};
    activeSnapshot_ = std::nullopt;
    neutralizeActiveTrim();

    measurementStep = 0;
    progressValue = 0.0;
    liveInputPeak = 0.0f;
    saveFeedbackText_ = {};
    showSavedProfilesSection_ = false;
    showProfileDetails_ = false;

    btnStartMeasure.setButtonText("Check Input Noise Baseline");
    btnStartMeasure.setTooltip("Step 2A: Verifies that physical input is safe, feedback-free, and quiet with muted output");
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnRecheckBaseline.setVisible(false);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    btnSaveCalibration.setVisible(false);
    lblDisplayName.setVisible(false);
    txtDisplayName.setVisible(false);
    btnDeleteProfile.setVisible(false);
    btnViewProfileDetails.setVisible(false);
    progressBar.setVisible(false);

    evaluateProfilesMatching();

    startTimerHz(30);
    repaint();
}

//==============================================================================
// Measuring: only the progress bar is meaningful, everything else is disabled.
//==============================================================================
void NativeCalibrationPanel::applyMeasuringButtonLayout()
{
    btnStartMeasure.setEnabled(false);
    btnRecheckBaseline.setVisible(false);
    btnSkip.setEnabled(false);
    btnContinue.setVisible(false);
    btnContinue.setEnabled(false);
    btnRetry.setVisible(false);
    btnSaveCalibration.setVisible(false);
    lblDisplayName.setVisible(false);
    txtDisplayName.setVisible(false);
    progressBar.setVisible(true);
}

//==============================================================================
// Idle after 2A passed: loopback is unlocked but not measured yet.
//==============================================================================
void NativeCalibrationPanel::applyIdleButtonLayout()
{
    progressBar.setVisible(false);
    btnStartMeasure.setButtonText("Run Physical Loopback Calibration (2B)");
    btnStartMeasure.setTooltip("Step 2B: Plays a 1.0s Farina sweep over physical loopback to measure latency, gain trim, and flatness");
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnRecheckBaseline.setVisible(true);
    btnRecheckBaseline.setEnabled(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnRetry.setVisible(false);
    btnContinue.setVisible(false);
    btnSaveCalibration.setVisible(false);
}

//==============================================================================
// Any 2A/2B failure: offer retry, bypass and navigation, never "continue".
//==============================================================================
void NativeCalibrationPanel::applyFailureButtonLayout()
{
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnRecheckBaseline.setVisible(true);
    btnReuseCalibration.setVisible(false);
    btnRetry.setVisible(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnContinue.setVisible(false);
    btnSaveCalibration.setVisible(false);
    progressBar.setVisible(false);
}

//==============================================================================
// Step 2 resolved (success, reused profile or bypass): retry + continue are unlocked.
//==============================================================================
void NativeCalibrationPanel::applyResolvedButtonLayout(bool offerSaveAction, bool showProfileNameEditor)
{
    btnStartMeasure.setVisible(false);
    btnReuseCalibration.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnSaveCalibration.setVisible(offerSaveAction);
    if (offerSaveAction)
        btnSaveCalibration.setEnabled(true);
    lblDisplayName.setVisible(showProfileNameEditor);
    txtDisplayName.setVisible(showProfileNameEditor);
    progressBar.setVisible(false);
}

//==============================================================================
// Digital mode: analog actions disappear, verification drives the flow.
//==============================================================================
void NativeCalibrationPanel::applyDigitalButtonLayout()
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

//==============================================================================
void NativeCalibrationPanel::applySavedProfileActionLayout(const juce::Rectangle<int>& cardBounds, int cardRight)
{
    const bool showActions = showSavedProfilesSection_ && !savedProfiles.empty();
    btnViewProfileDetails.setVisible(showActions);
    btnDeleteProfile.setVisible(showActions);

    if (!showActions)
    {
        btnPrevProfile.setVisible(false);
        btnNextProfile.setVisible(false);
        return;
    }

    // Kept aligned with the saved-profiles box drawn by the painter: same top offset,
    // same box height, same action row inset.
    const int sectionX = cardBounds.getX()
                       + static_cast<int>(cardBounds.getWidth() * calibrationpanel::kColumnSplitRatio)
                       + calibrationpanel::kSavedProfilesSideInsetPx;
    const int sectionWidth = cardRight - sectionX;
    const int sectionBottom = cardBounds.getY() + calibrationpanel::kSavedProfilesActionRowTopPx;

    const int actionRowY = sectionBottom - calibrationpanel::kSavedProfilesActionRowInsetPx;
    const int sectionRight = sectionX + sectionWidth;

    // [◀][▶] [View Details] [Delete] — navigation on the left, destructive action on the right.
    constexpr int navWidth = 28;
    const int prevX = sectionRight - 249;
    const int nextX = sectionRight - 217;

    // On narrow cards the row would spill into the left column, so navigation is hidden
    // rather than drawn on top of the monitor card.
    const bool navFits = prevX >= sectionX;
    btnPrevProfile.setVisible(navFits);
    btnNextProfile.setVisible(navFits);

    if (navFits)
    {
        btnPrevProfile.setBounds(prevX, actionRowY, navWidth, 24);
        btnNextProfile.setBounds(nextX, actionRowY, navWidth, 24);
    }

    btnPrevProfile.setEnabled(navFits && selectedProfileIndex_ > 0);
    btnNextProfile.setEnabled(navFits && selectedProfileIndex_ < static_cast<int>(savedProfiles.size()) - 1);

    btnViewProfileDetails.setBounds(sectionRight - 180, actionRowY, 95, 24);
    btnDeleteProfile.setBounds(sectionRight - 80, actionRowY, 75, 24);
}

} // namespace abdaudiolab::gui
