/**
 * @file NativeCalibrationPanelLayout.cpp
 * @brief Bounds management for the Step 2 action bar and saved-profile controls.
 * @author ABDSynths
 * @date 2026
 */

#include "NativeCalibrationPanel.h"

#include "calibration/CalibrationPanelMetrics.h"

#include <algorithm>

namespace abdaudiolab::gui
{

namespace
{
using calibrationpanel::kBottomRowOffsetPx;
using calibrationpanel::kCardPaddingXPx;
using calibrationpanel::kSavedProfilesActionRowInsetPx;
using calibrationpanel::kSavedProfilesActionRowTopPx;
using calibrationpanel::kSavedProfilesSideInsetPx;
using calibrationpanel::kStepperGapXPx;
using calibrationpanel::kStepperHeightPx;
} // namespace

void NativeCalibrationPanel::resized()
{
    const auto cardBounds = computeCardBounds(getLocalBounds());

    const int bottomY = cardBounds.getBottom() - kBottomRowOffsetPx;
    const int leftX = cardBounds.getX() + kCardPaddingXPx;
    const int cardRight = cardBounds.getRight() - kCardPaddingXPx;

    const int stepperWidth = cardBounds.getWidth() - 2 * kCardPaddingXPx;
    const int stepperCardWidth = (stepperWidth - kStepperGapXPx) / 2;
    const int stepperTop = computeStepperTop(cardBounds);
    stepCard2ABounds_ = { leftX, stepperTop, stepperCardWidth, kStepperHeightPx };
    stepCard2BBounds_ = { leftX + stepperCardWidth + kStepperGapXPx, stepperTop,
                          stepperWidth - stepperCardWidth - kStepperGapXPx, kStepperHeightPx };

    progressBar.setBounds(leftX, bottomY - 24, cardRight - leftX, 12);

    if (isDigitalMode_)
    {
        btnVerifyDigital.setVisible(true);
        btnContinue.setVisible(true);
        btnVerifyDigital.setBounds(leftX, bottomY, 240, 36);
        btnContinue.setBounds(cardRight - 280, bottomY, 280, 36);

        btnStartMeasure.setVisible(false);
        btnRecheckBaseline.setVisible(false);
        btnRetry.setVisible(false);
        btnSkip.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnToggleSavedProfiles.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnDeleteProfile.setVisible(false);
        btnViewProfileDetails.setVisible(false);
        btnPrevProfile.setVisible(false);
        btnNextProfile.setVisible(false);
        lblDisplayName.setVisible(false);
        txtDisplayName.setVisible(false);
        return;
    }

    btnVerifyDigital.setVisible(false);
    btnToggleSavedProfiles.setVisible(true);

    const bool isMeasuringAny = currentState == State::MeasuringNoiseBaseline || currentState == State::Measuring;
    progressBar.setVisible(isMeasuringAny);

    int rightBoundForLeftButtons = cardRight;

    layoutResolvedRow(bottomY, cardRight, rightBoundForLeftButtons);

    if (activeSubView_ == CalibrationSubView::NoiseBaseline_2A)
        layoutNoiseBaselineRow(leftX, bottomY, isMeasuringAny, rightBoundForLeftButtons);
    else
        layoutLoopbackRow(leftX, bottomY, isMeasuringAny, rightBoundForLeftButtons);

    applySavedProfileActionLayout(cardBounds, cardRight);
}

/** @brief Bottom-right cluster: Continue to Step 3 plus the optional profile name editor. */
void NativeCalibrationPanel::layoutResolvedRow(int bottomY,
                                               int cardRight,
                                               int& rightBoundForLeftButtons)
{
    // Continue to Step 3 is ONLY enabled when a valid 2B calibration has passed.
    if (loopbackState_ != LoopbackState::Passed)
    {
        btnContinue.setVisible(false);
        btnContinue.setEnabled(false);
        btnSaveCalibration.setVisible(false);
        lblDisplayName.setVisible(false);
        txtDisplayName.setVisible(false);
        return;
    }

    // Once 2B has passed, re-running is what the main button already offers.
    btnRetry.setVisible(false);

    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnContinue.setBounds(cardRight - 230, bottomY, 230, 36);
    rightBoundForLeftButtons = cardRight - 242;

    const bool showProfileName = activeSubView_ == CalibrationSubView::PhysicalLoopback_2B;
    btnSaveCalibration.setVisible(showProfileName);
    lblDisplayName.setVisible(showProfileName);
    txtDisplayName.setVisible(showProfileName);

    if (showProfileName)
    {
        btnSaveCalibration.setBounds(cardRight - 410, bottomY, 160, 36);
        rightBoundForLeftButtons = cardRight - 422;

        lblDisplayName.setBounds(cardRight - 420, bottomY - 36, 100, 26);
        txtDisplayName.setBounds(cardRight - 320, bottomY - 36, 320, 26);
    }
}

/** @brief 2A row: measure/re-check baseline, bypass and the saved-profiles toggle. */
void NativeCalibrationPanel::layoutNoiseBaselineRow(int leftX, int bottomY, bool isMeasuringAny,
                                                   int& rightBoundForLeftButtons)
{
    btnReuseCalibration.setVisible(false);

    // An explicit Retry (failed sweep, stale calibration) takes over the action slot: both
    // buttons run the active sub-step, so showing both at once would just be noise.
    const bool useRetry = btnRetry.isVisible();

    constexpr int measureWidth = 240;
    juce::TextButton& action = useRetry ? btnRetry : btnStartMeasure;

    btnStartMeasure.setVisible(!useRetry);
    action.setVisible(true);
    action.setEnabled(!isMeasuringAny);

    if (useRetry)
        action.setButtonText(noiseBaselineState_ == NoiseBaselineState::Passed ? "Retry Baseline (2A)"
                                                                              : "Retry Calibration (2A)");
    else
        action.setButtonText(noiseBaselineState_ == NoiseBaselineState::Passed
                                 ? "Re-check Noise Baseline (2A)"
                                 : "Check Input Noise Baseline");

    action.setBounds(rightBoundForLeftButtons - measureWidth, bottomY, measureWidth, 36);
    rightBoundForLeftButtons -= (measureWidth + 10);

    if (noiseBaselineState_ == NoiseBaselineState::Passed)
    {
        constexpr int navWidth = 190;
        btnRecheckBaseline.setVisible(true);
        btnRecheckBaseline.setEnabled(true);
        btnRecheckBaseline.setButtonText("Go to Loopback (2B) ->");
        btnRecheckBaseline.setTooltip("Navigate to Step 2B: connect loopback cable and calibrate");
        btnRecheckBaseline.setBounds(rightBoundForLeftButtons - navWidth, bottomY, navWidth, 36);
        rightBoundForLeftButtons -= (navWidth + 10);
    }
    else
    {
        btnRecheckBaseline.setVisible(false);
    }

    layoutBypassAndProfilesRow(leftX, bottomY, isMeasuringAny, rightBoundForLeftButtons);
}

/** @brief 2B row: run/re-run sweep, navigate back to 2A, bypass and saved profiles. */
void NativeCalibrationPanel::layoutLoopbackRow(int leftX, int bottomY, bool isMeasuringAny,
                                               int& rightBoundForLeftButtons)
{
    // Retry owns the action slot whenever the state machine asked for it; otherwise the
    // main button keeps its context-sensitive label.
    const bool useRetry = btnRetry.isVisible();

    constexpr int sweepWidth = 260;
    juce::TextButton& action = useRetry ? btnRetry : btnStartMeasure;

    btnStartMeasure.setVisible(!useRetry);
    action.setVisible(true);

    if (useRetry)
    {
        // Retry is only actionable once 2A has unlocked the sweep, exactly like the main button.
        const bool reachable = loopbackState_ != LoopbackState::Locked;
        action.setEnabled(reachable && !isMeasuringAny);
        action.setButtonText(reachable ? "Retry Loopback (2B)" : "Retry Loopback (Locked)");
    }
    else if (loopbackState_ == LoopbackState::Locked)
    {
        action.setEnabled(false);
        action.setButtonText("Run Loopback Sweep (Locked)");
    }
    else
    {
        action.setEnabled(!isMeasuringAny);
        action.setButtonText(loopbackState_ == LoopbackState::Passed
                                 ? "Re-run Loopback Calibration"
                                 : "Run Physical Loopback Calibration");
    }

    action.setBounds(rightBoundForLeftButtons - sweepWidth, bottomY, sweepWidth, 36);
    rightBoundForLeftButtons -= (sweepWidth + 10);

    constexpr int navWidth = 180;
    btnRecheckBaseline.setVisible(true);
    btnRecheckBaseline.setEnabled(true);
    btnRecheckBaseline.setButtonText("<- Back to Baseline (2A)");
    btnRecheckBaseline.setTooltip("Navigate to Step 2A: inspect or re-check input noise floor");
    btnRecheckBaseline.setBounds(rightBoundForLeftButtons - navWidth, bottomY, navWidth, 36);
    rightBoundForLeftButtons -= (navWidth + 10);

    btnReuseCalibration.setVisible(false);

    layoutBypassAndProfilesRow(leftX, bottomY, isMeasuringAny, rightBoundForLeftButtons);
}

/** @brief Bypass button + saved-profiles toggle share the left half of the bottom row. */
void NativeCalibrationPanel::layoutBypassAndProfilesRow(int leftX, int bottomY, bool isMeasuringAny,
                                                        int& rightBoundForLeftButtons)
{
    btnSkip.setVisible(true);
    btnSkip.setEnabled(!isMeasuringAny);

    const int availableWidth = rightBoundForLeftButtons - leftX - 10;
    const int halfWidth = std::min(180, std::max(110, availableWidth / 2));

    btnSkip.setBounds(leftX, bottomY, halfWidth, 36);
    btnToggleSavedProfiles.setBounds(leftX + halfWidth + 10, bottomY, halfWidth, 36);
}

} // namespace abdaudiolab::gui
