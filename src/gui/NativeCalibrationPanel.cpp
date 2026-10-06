/**
 * @file NativeCalibrationPanel.cpp
 * @brief Step 2 panel: construction, paint orchestration, pointer routing and session sync.
 *        Painting itself lives in src/gui/calibration/CalibrationPanelPainter*.cpp.
 * @author ABDSynths
 * @date 2026
 */

#include "NativeCalibrationPanel.h"

#include "SoundIdTheme.h"
#include "calibration/CalibrationPanelMetrics.h"
#include "calibration/CalibrationPanelPainter.h"

#include <algorithm>
#include <utility>

namespace abdaudiolab::gui
{

namespace
{

// Card geometry lives in one place; import only the metrics, never the shared enum names
// (which would collide with the class aliases in NativeCalibrationPanel.h).
using calibrationpanel::kCardCornerRadius;
using calibrationpanel::kCardMarginX;
using calibrationpanel::kCardMarginY;
using calibrationpanel::kCardMaxHeight;
using calibrationpanel::kCardMaxWidth;
using calibrationpanel::kCardPaddingX;
using calibrationpanel::kCardPaddingY;
using calibrationpanel::kColumnGap;
using calibrationpanel::kColumnSplitRatio;
using calibrationpanel::kDividerHeight;
using calibrationpanel::kHeaderGap;
using calibrationpanel::kHeaderHeight;
using calibrationpanel::kStepperGapY;
using calibrationpanel::kStepperHeight;

struct ButtonSkin
{
    juce::String text;
    juce::String tooltip;
    juce::Colour background;
    juce::Colour foreground;
};

/** @brief Single place that applies text, colours, action and visibility to an action button. */
void addActionButton(juce::Component& parent,
                     juce::TextButton& button,
                     const ButtonSkin& skin,
                     std::function<void()> action)
{
    button.setButtonText(skin.text);
    button.setTooltip(skin.tooltip);
    button.setColour(juce::TextButton::buttonColourId, skin.background);
    button.setColour(juce::TextButton::textColourOffId, skin.foreground);
    button.onClick = std::move(action);
    parent.addChildComponent(button);
    button.setVisible(true);
}

} // namespace

//==============================================================================
NativeCalibrationPanel::NativeCalibrationPanel(audio::LabAudioEngine& engine)
    : audioEngine(engine), progressBar(progressValue)
{
    addActionButton(*this, btnStartMeasure,
                    { "Check Input Noise Baseline",
                      "Step 2A: Verifies that physical input is safe, feedback-free, and quiet with muted output",
                      SoundIdTheme::accentGreen, juce::Colours::white },
                    [this] { runActiveSubViewAction(); });

    addActionButton(*this, btnRecheckBaseline,
                    { "↺ Back to Baseline (2A)",
                      "Navigates between Step 2A (Noise Baseline) and Step 2B (Loopback Sweep)",
                      SoundIdTheme::bgCardHover, SoundIdTheme::accentBlue },
                    [this] { toggleActiveSubView(); });

    addActionButton(*this, btnReuseCalibration,
                    { "Reuse Saved Calibration",
                      "Applies the matching calibration found for the current configuration",
                      SoundIdTheme::accentGreen.withAlpha(0.25f), SoundIdTheme::accentGreen },
                    [this] { reuseMatchingProfile(); });

    addActionButton(*this, btnSkip,
                    { "Continue without calibration (Bypass)",
                      "Continues without audio interface latency or level compensation. Resets to unity gain.",
                      SoundIdTheme::bgCardHover, SoundIdTheme::accentAmber },
                    [this] { skipCalibration(); });

    addActionButton(*this, btnContinue,
                    { "Continue to Run Session (Step 3) ➔",
                      "Proceed to Step 3: session excitation and profiling",
                      SoundIdTheme::accentGreen, juce::Colours::white },
                    [this] {
                        sealActiveDraftFromEditor();
                        if (onContinueToSession)
                            onContinueToSession();
                    });

    addActionButton(*this, btnVerifyDigital,
                    { "Verify Digital Latency",
                      "Verifies digital bus readiness and plugin roundtrip latency",
                      SoundIdTheme::accentBlue.withAlpha(0.25f), SoundIdTheme::textPrimary },
                    [this] {
                        if (onVerifyDigitalRequested)
                            onVerifyDigitalRequested();
                    });

    addActionButton(*this, btnRetry,
                    { "Retry Calibration",
                      "Re-runs calibration after verifying connections and levels",
                      SoundIdTheme::accentAmber, juce::Colours::black },
                    [this] { runActiveSubViewAction(); });

    addActionButton(*this, btnSaveCalibration,
                    { "Save Calibration",
                      "Saves this calibration result to AppData for persistent reuse",
                      SoundIdTheme::bgCardHover, SoundIdTheme::accentGreen },
                    [this] { saveCurrentCalibrationProfile(); });

    addActionButton(*this, btnToggleSavedProfiles,
                    { "Saved Calibrations",
                      "Shows or collapses the list of saved calibrations on disk",
                      SoundIdTheme::bgCardHover, SoundIdTheme::textSecondary },
                    [this] {
                        showSavedProfilesSection_ = !showSavedProfilesSection_;
                        refreshSavedProfiles();
                        resized();
                        repaint();
                    });

    addActionButton(*this, btnDeleteProfile,
                    { "Delete",
                      "Deletes the selected saved calibration profile",
                      SoundIdTheme::bgCardHover, SoundIdTheme::accentRed },
                    [this] { deleteSelectedProfile(); });

    addActionButton(*this, btnViewProfileDetails,
                    { "View Details",
                      "Shows or hides technical details for the saved calibration",
                      SoundIdTheme::bgCardHover, SoundIdTheme::accentBlue },
                    [this] {
                        showProfileDetails_ = !showProfileDetails_;
                        repaint();
                    });

    addActionButton(*this, btnPrevProfile,
                    { "◀",
                      "Shows the previous saved calibration profile",
                      SoundIdTheme::bgCardHover, SoundIdTheme::textSecondary },
                    [this] { selectRelativeProfile(-1); });

    addActionButton(*this, btnNextProfile,
                    { "▶",
                      "Shows the next saved calibration profile",
                      SoundIdTheme::bgCardHover, SoundIdTheme::textSecondary },
                    [this] { selectRelativeProfile(+1); });

    // Retry only appears when the state machine asks for it (failure or stale calibration).
    btnRetry.setVisible(false);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::borderSubtle);
    addChildComponent(progressBar);

    lblDisplayName.setText("Profile Name:", juce::dontSendNotification);
    lblDisplayName.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
    lblDisplayName.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblDisplayName.setJustificationType(juce::Justification::centredRight);
    addChildComponent(lblDisplayName);

    txtDisplayName.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::plain));
    txtDisplayName.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::bgCardHover);
    txtDisplayName.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtDisplayName.setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderSubtle);
    txtDisplayName.setColour(juce::TextEditor::focusedOutlineColourId, SoundIdTheme::accentGreen);
    txtDisplayName.setTextToShowWhenEmpty("Enter profile name...", SoundIdTheme::textMuted);
    txtDisplayName.onTextChange = [this] { updateActiveDraftDisplayName(); };
    addChildComponent(txtDisplayName);

    refreshSavedProfiles();
    startTimerHz(30);
}

NativeCalibrationPanel::~NativeCalibrationPanel()
{
    stopTimer();
    scopedCapture_.reset();
}

//==============================================================================
void NativeCalibrationPanel::updateActiveDraftDisplayName()
{
    if (!activeDraft_.has_value())
        return;

    const auto editedName = txtDisplayName.getText().trim();
    if (editedName.isNotEmpty())
        activeDraft_->displayName = editedName.toStdString();
}

void NativeCalibrationPanel::sealActiveDraftFromEditor()
{
    if (!activeDraft_.has_value())
        return;

    updateActiveDraftDisplayName();
    activeSnapshot_ = activeDraft_->sealSnapshot();
}

//==============================================================================
juce::Rectangle<int> NativeCalibrationPanel::computeCardBounds(const juce::Rectangle<int>& area)
{
    const auto maxWidth = std::min(juce::roundToInt(kCardMaxWidth),
                                    area.getWidth() - juce::roundToInt(kCardMarginX));
    const auto maxHeight = std::min(juce::roundToInt(kCardMaxHeight),
                                     area.getHeight() - juce::roundToInt(kCardMarginY));

    // Integer division is intentional: the layout pass must stay on whole pixels.
    return { (area.getWidth() - maxWidth) / 2,
             (area.getHeight() - maxHeight) / 2,
             maxWidth, maxHeight };
}

juce::Rectangle<float> NativeCalibrationPanel::computeCardBounds(const juce::Rectangle<float>& area)
{
    const auto maxWidth = std::min(kCardMaxWidth, area.getWidth() - kCardMarginX);
    const auto maxHeight = std::min(kCardMaxHeight, area.getHeight() - kCardMarginY);

    return { (area.getWidth() - maxWidth) * 0.5f,
             (area.getHeight() - maxHeight) * 0.5f,
             maxWidth, maxHeight };
}

int NativeCalibrationPanel::computeStepperTop(const juce::Rectangle<int>& cardBounds)
{
    const auto headerBlock = juce::roundToInt(kCardPaddingY + kHeaderHeight + kHeaderGap
                                             + kDividerHeight + kHeaderGap);
    return cardBounds.getY() + headerBlock;
}

calibrationpanel::ViewState NativeCalibrationPanel::buildViewState() const
{
    calibrationpanel::ViewState view;

    view.state = currentState;
    view.subView = activeSubView_;
    view.noiseBaselineState = noiseBaselineState_;
    view.loopbackState = loopbackState_;
    view.noiseReport = noiseReport_;
    view.loopbackReport = loopbackReport_;
    view.diagnostics = lastDiagnostics_;

    view.outputChannelName = calibrationOutputChannelName;
    view.inputChannelName = calibrationInputChannelName;
    view.liveInputPeak = liveInputPeak;

    view.digitalMode = isDigitalMode_;
    view.digitalVerified = isDigitalVerified_;

    view.showSavedProfiles = showSavedProfilesSection_;
    view.showProfileDetails = showProfileDetails_;
    view.selectedProfileIndex = selectedProfileIndex_;
    view.saveFeedbackText = saveFeedbackText_;
    view.savedProfiles = &savedProfiles;

    return view;
}

//==============================================================================
void NativeCalibrationPanel::paint(juce::Graphics& g)
{
    namespace painter = calibrationpanel::painter;

    const auto view = buildViewState();

    g.fillAll(SoundIdTheme::bgLight);

    const auto cardBounds = computeCardBounds(getLocalBounds().toFloat());
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(cardBounds, kCardCornerRadius);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), kCardCornerRadius, 1.0f);


    auto content = cardBounds.reduced(kCardPaddingX, kCardPaddingY);

    // Header row: title + status badge.
    painter::paintHeaderRow(g, view, content.removeFromTop(kHeaderHeight));

    content.removeFromTop(kHeaderGap);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(kDividerHeight));
    content.removeFromTop(kHeaderGap);

    if (view.digitalMode)
    {
        painter::paintDigitalIntro(g, content);
    }
    else
    {
        // 2A / 2B navigation cards.
        painter::paintStepper(g, view, content.removeFromTop(kStepperHeight),
                              stepCard2ABounds_, stepCard2BBounds_);
        content.removeFromTop(kStepperGapY);
    }

    auto leftColumn = content.removeFromLeft(content.getWidth() * kColumnSplitRatio);
    content.removeFromLeft(kColumnGap);
    auto rightColumn = content;

    if (view.digitalMode)
    {
        painter::paintDigitalInstructions(g, view, leftColumn);
        painter::paintDigitalStatusCard(g, view, rightColumn);
        return;
    }

    painter::paintAnalogInstructions(g, view, leftColumn);

    juce::Rectangle<float> meterArea;
    painter::paintLevelMonitor(g, view, rightColumn, meterArea);

    if (view.subView == CalibrationSubView::NoiseBaseline_2A)
        painter::paintNoiseBaselineReport(g, view, meterArea);
    else
        painter::paintLoopbackReport(g, view, meterArea);

    painter::paintSavedProfiles(g, view, rightColumn);
}

//==============================================================================
void NativeCalibrationPanel::mouseDown(const juce::MouseEvent& e)
{
    if (isDigitalMode_)
        return;

    const auto pos = e.getPosition();

    // Clicking a navigation card ONLY switches sub-view. It NEVER starts audio or a sweep.
    if (stepCard2ABounds_.contains(pos))
        setActiveSubView(CalibrationSubView::NoiseBaseline_2A);
    else if (stepCard2BBounds_.contains(pos))
        setActiveSubView(CalibrationSubView::PhysicalLoopback_2B);
}

void NativeCalibrationPanel::mouseMove(const juce::MouseEvent& e)
{
    if (isDigitalMode_)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    const auto pos = e.getPosition();
    const auto onNavigationCard = stepCard2ABounds_.contains(pos) || stepCard2BBounds_.contains(pos);

    setMouseCursor(onNavigationCard ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
}

//==============================================================================
void NativeCalibrationPanel::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isDigitalMode_ = snapshot.calibration.audio.requirement == session::CalibrationRequirement::NotApplicable;
    isDigitalVerified_ = snapshot.calibration.digital.verified;

    if (isDigitalMode_)
    {
        applyDigitalButtonLayout();
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

} // namespace abdaudiolab::gui
