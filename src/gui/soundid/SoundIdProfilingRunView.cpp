#include "SoundIdProfilingRunView.h"
#include "../SoundIdTheme.h"
#include "../session/ProfilingSessionController.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdProfilingRunView::SoundIdProfilingRunView(session::IProfilingSessionCommands& commands)
    : commands_(commands),
      progressBar_(currentProgress_)
{
    headerTitle_.setText("Step 3: Automated Recipe Profiling", juce::dontSendNotification);
    headerTitle_.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText("System excites the target instrument or effect, acquires acoustic responses, and synthesizes the model.", juce::dontSendNotification);
    headerSubtitle_.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Mode Badge
    modeBadgeLabel_.setText("Measurement Session", juce::dontSendNotification);
    modeBadgeLabel_.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    modeBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    modeBadgeLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(modeBadgeLabel_);

    // Pre-Flight Review Card
    preflightCard_.setText("Session Workspace & Pre-Flight");
    preflightCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard.withAlpha(0.3f));
    preflightCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(preflightCard_);

    auto setupInfo = [this](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::FontOptions(14.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(preflightRecipeLabel_, "Selected Recipe: Standard Profiling & VCF Sweep");
    setupInfo(preflightTimeLabel_, "Estimated Duration: ~30 seconds");
    setupInfo(preflightWarningsLabel_, "Acoustic Condition: Target verified and armed for execution");

    // Primary Giant Start Button
    startButton_.setButtonText("START MEASUREMENT");
    const bool isDarkInit = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);
    startButton_.setColour(juce::TextButton::buttonColourId, isDarkInit ? juce::Colour(0xff252c33) : juce::Colour(0xffe2e8f0));
    startButton_.setColour(juce::TextButton::textColourOffId, isDarkInit ? juce::Colour(0xff64748b) : juce::Colour(0xff94a3b8));
    startButton_.setEnabled(false);
    startButton_.onClick = [this]() {
        if (onStartClicked)
        {
            onStartClicked();
            return;
        }
        commands_.recordUserClick();
        commands_.startProfiling();
    };
    addAndMakeVisible(startButton_);

    // Load Evaluation Button (Secondary)
    loadEvaluationButton_.setButtonText("Load Evaluation...");
    loadEvaluationButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDarkInit));
    loadEvaluationButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDarkInit));
    loadEvaluationButton_.onClick = [this]() {
        juce::PopupMenu m;
        m.addSectionHeader(juce::String::fromUTF8(u8"Evaluaciones de Ejemplo (Precargadas)"));
        m.addItem(1, juce::String::fromUTF8(u8"1. Fixture Aprobado (Limpio, Verificado)"));
        m.addItem(2, juce::String::fromUTF8(u8"2. Dexed FM (Aceptado con Advertencias)"));
        m.addItem(3, juce::String::fromUTF8(u8"3. Adulterado / Tampered (Fallo de Hash)"));
        m.addItem(4, juce::String::fromUTF8(u8"4. Inconcluso (Falta Holdout)"));
        m.addItem(5, juce::String::fromUTF8(u8"5. Rechazado (Falta de Fidelidad ESR)"));
        m.addSeparator();
        m.addItem(6, juce::String::fromUTF8(u8"Examinar archivo JSON en disco..."));

        juce::Component::SafePointer<SoundIdProfilingRunView> safeThis(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadEvaluationButton_),
            [safeThis](int result) {
                if (safeThis == nullptr)
                    return;

                safeThis->commands_.recordUserClick();
                if (result == 1)
                    safeThis->commands_.loadPredefinedFixture("fixture_approved.json");
                else if (result == 2)
                    safeThis->commands_.loadPredefinedFixture("dexed_warnings.json");
                else if (result == 3)
                    safeThis->commands_.loadPredefinedFixture("tampered_hash_mismatch.json");
                else if (result == 4)
                    safeThis->commands_.loadPredefinedFixture("inconclusive.json");
                else if (result == 5)
                    safeThis->commands_.loadPredefinedFixture("rejected.json");
                else if (result == 6)
                {
                    auto dir = session::ProfilingSessionController::getEvaluationsDirectory();
                    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                    safeThis->fileChooser_ = std::make_shared<juce::FileChooser>(
                        juce::String::fromUTF8(u8"Cargar Evaluación Acústica (JSON)"),
                        dir,
                        "*.json");

                    safeThis->fileChooser_->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& fc) {
                        if (safeThis == nullptr)
                            return;
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            safeThis->commands_.loadEvaluationFromFile(file.getFullPathName().toStdString());
                        }
                    });
                }
            });
    };
    addAndMakeVisible(loadEvaluationButton_);

    advancedSettingsLink_.setButtonText("Advanced Measurement Parameters...");
    advancedSettingsLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    advancedSettingsLink_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    advancedSettingsLink_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.setOpenedAdvancedMode(true);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Advanced Measurement Parameters",
            "- Metrological Protocol: Fast-Acoustic Profiler (RFC 8785)\n"
            "- Execution Domain: Real-Time Audio Engine / VST3 Event Pipeline\n"
            "- Sampling Matrix: Adaptive trials with Bayesian search\n"
            "- Acceptance Criteria: RMS > -60 dBFS, Clean SNR, Zero Underruns\n"
            "- Phase Alignment: Note-on synchronized stimulus\n"
            "- Headroom: -3.0 dBFS safety margin",
            "OK");
    };
    addAndMakeVisible(advancedSettingsLink_);

    // Active Monitor Card
    activeMonitorCard_.setText("Live Acoustic Monitor & Telemetry");
    activeMonitorCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard.withAlpha(0.3f));
    activeMonitorCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(activeMonitorCard_);

    progressBar_.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar_.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::surfaceSubtle);
    addAndMakeVisible(progressBar_);

    setupInfo(trialCounterLabel_, "Point: 0 of 0 (0%)");
    trialCounterLabel_.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    trialCounterLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);

    setupInfo(timeRemainingLabel_, "Elapsed: 0 s | Remaining: 0 s");
    setupInfo(stimulusLabel_, "Stimulus: Awaiting start");
    setupInfo(signalHealthLabel_, "Signal Health: RMS -120.0 dBFS | Peak -120.0 dBFS [OK]");

    // Trial Stage Badge & MIDI details
    trialStageBadge_.setText("Stage: Armed", juce::dontSendNotification);
    trialStageBadge_.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    trialStageBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    addAndMakeVisible(trialStageBadge_);

    setupInfo(midiTrialDetailsLabel_, "Nota: C4 (60) | Vel: 80 | Ch: 1 | Gate: 250 ms");
    addAndMakeVisible(midiTrialDetailsLabel_);

    // Tarjeta de interacción manual del operador
    operatorStepCard_.setText("Manual Operator Step Guidance");
    operatorStepCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::accentAmber);
    operatorStepCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::accentAmber);
    operatorStepCard_.setVisible(false);
    addChildComponent(operatorStepCard_);

    operatorPromptLabel_.setText("Ajuste los controles físicos del hardware y pulse Listo [Espacio]", juce::dontSendNotification);
    operatorPromptLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    operatorPromptLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    operatorPromptLabel_.setVisible(false);
    addChildComponent(operatorPromptLabel_);

    expectedSettingLabel_.setText("Ajuste esperado: Standard Calibration", juce::dontSendNotification);
    expectedSettingLabel_.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    expectedSettingLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    expectedSettingLabel_.setVisible(false);
    addChildComponent(expectedSettingLabel_);

    btnConfirmManual_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnConfirmManual_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnConfirmManual_.onClick = [this] {
        btnConfirmManual_.setEnabled(false);
        commands_.confirmOperatorStep();
    };
    btnConfirmManual_.setVisible(false);
    addChildComponent(btnConfirmManual_);

    btnRepeatStep_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnRepeatStep_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnRepeatStep_.setVisible(false);
    addChildComponent(btnRepeatStep_);

    btnStepBack_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnStepBack_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnStepBack_.setVisible(false);
    addChildComponent(btnStepBack_);

    pauseButton_.setButtonText("PAUSE");
    pauseButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDarkInit));
    pauseButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDarkInit));
    pauseButton_.onClick = [this]() {
        if (onPauseClicked)
        {
            onPauseClicked();
            return;
        }
        commands_.recordUserClick();
        if (isPaused_)
            commands_.resumeProfiling();
        else
            commands_.pauseProfiling();
    };
    addAndMakeVisible(pauseButton_);

    // Cancel Button (Danger semantics: neutral surface, restrained red text)
    cancelButton_.setButtonText("CANCEL");
    cancelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDarkInit));
    cancelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::dangerText(isDarkInit));
    cancelButton_.onClick = [this]() {
        if (onCancelClicked)
        {
            onCancelClicked();
            return;
        }
        commands_.recordUserClick();
        commands_.cancelProfiling();
    };
    addAndMakeVisible(cancelButton_);

    applyTheme(AppTheme::currentMode == AppTheme::ThemeMode::Dark);
}

SoundIdProfilingRunView::~SoundIdProfilingRunView() = default;

void SoundIdProfilingRunView::applyTheme(bool isDark)
{
    const auto colCard = isDark ? juce::Colour(0xff1e2329) : juce::Colour(0xfff1f3f5);
    const auto colBorder = isDark ? juce::Colour(0xff2e3640) : juce::Colour(0xffcbd5e1);
    const auto colTextPrim = isDark ? juce::Colour(0xfff1f3f5) : juce::Colour(0xff1a1d20);
    const auto colTextSec = isDark ? juce::Colour(0xff94a3b8) : juce::Colour(0xff475569);

    headerTitle_.setColour(juce::Label::textColourId, colTextPrim);
    headerSubtitle_.setColour(juce::Label::textColourId, colTextSec);

    preflightCard_.setColour(juce::GroupComponent::outlineColourId, colBorder.withAlpha(0.4f));
    preflightCard_.setColour(juce::GroupComponent::textColourId, colTextSec);
    preflightRecipeLabel_.setColour(juce::Label::textColourId, colTextPrim);
    preflightTimeLabel_.setColour(juce::Label::textColourId, colTextSec);

    activeMonitorCard_.setColour(juce::GroupComponent::outlineColourId, colBorder.withAlpha(0.4f));
    activeMonitorCard_.setColour(juce::GroupComponent::textColourId, colTextSec);

    trialCounterLabel_.setColour(juce::Label::textColourId, colTextPrim);
    timeRemainingLabel_.setColour(juce::Label::textColourId, colTextSec);
    stimulusLabel_.setColour(juce::Label::textColourId, colTextSec);
    midiTrialDetailsLabel_.setColour(juce::Label::textColourId, colTextSec);

    operatorPromptLabel_.setColour(juce::Label::textColourId, colTextPrim);
    expectedSettingLabel_.setColour(juce::Label::textColourId, colTextSec);

    pauseButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
    pauseButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

    cancelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
    cancelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::dangerText(isDark));

    loadEvaluationButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
    loadEvaluationButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

    advancedSettingsLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    advancedSettingsLink_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::tertiaryText(isDark));

    btnRepeatStep_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
    btnRepeatStep_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

    btnStepBack_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
    btnStepBack_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));
}

bool SoundIdProfilingRunView::keyPressed(const juce::KeyPress& key)
{
    if (key.isKeyCurrentlyDown(juce::KeyPress::spaceKey) && isWaitingForOperator_ && btnConfirmManual_.isEnabled())
    {
        btnConfirmManual_.setEnabled(false);
        commands_.confirmOperatorStep();
        return true;
    }
    return false;
}

void SoundIdProfilingRunView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    // =========================================================================
    // PERF: Build lightweight presentation state and compare with last known.
    // Only update labels, progress and repaint when something meaningful changed.
    // Audio capture / session progress / clipping are driven by the audio thread
    // and are independent of this visual refresh rate.
    // =========================================================================
    ProfilingRunPresentationState newState;
    newState.pointIndex         = snapshot.progress.currentTrial;
    newState.pointCount         = snapshot.progress.totalTrials;
    newState.sessionStatus      = snapshot.sessionStatus;
    newState.stage              = snapshot.progress.trialStage;
    newState.rmsDb              = static_cast<float>(snapshot.observation.lastRmsDb);
    newState.peakDb             = static_cast<float>(snapshot.observation.lastPeakDb);
    newState.clippingDetected   = snapshot.observation.clippingDetected;
    newState.stimulusDescription = snapshot.progress.currentStimulusDescription;
    newState.operatorPromptText  = snapshot.progress.operatorPromptText;
    newState.excitationMode      = snapshot.progress.activeExcitationMode;
    newState.activeNoteNumber    = snapshot.progress.activeNoteNumber;
    newState.activeVelocity      = snapshot.progress.activeVelocity;
    newState.isPaused            = (snapshot.sessionStatus == session::ProfilingSessionStatus::Paused);
    newState.targetId            = snapshot.target.targetId;
    newState.isDarkMode          = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);

    // Build warning string (same logic as before, but only for comparison)
    std::string warningText;
    if (snapshot.audit.requiresResetBeforeEachTrial)
        warningText += "[!] Phase reset required before each trial. ";
    if (snapshot.audit.recommendedSettlingTimeMs > 200.0)
        warningText += "[!] Prolonged settling (" + std::to_string(static_cast<int>(snapshot.audit.recommendedSettlingTimeMs)) + " ms). ";
    if (!snapshot.audit.operationalWarnings.empty())
        warningText += snapshot.audit.operationalWarnings.front();
    else if (!snapshot.audit.humanGuidance.empty())
        warningText += snapshot.audit.humanGuidance;
    newState.warning = warningText;

    // --- Dirty check ---
    // !hasPresentationState_ ensures the very first snapshot always triggers a full UI update,
    // preventing a zero-initialized lastPresentationState_ from falsely matching an initial snapshot
    // where many fields are also at their defaults (e.g. pointIndex=0, rmsDb=-120, stage=Armed).
    const float rmsDeltaThreshold = 0.5f;
    const bool changed =
        !hasPresentationState_                                                  ||
        newState.pointIndex       != lastPresentationState_.pointIndex         ||
        newState.pointCount       != lastPresentationState_.pointCount         ||
        newState.sessionStatus    != lastPresentationState_.sessionStatus      ||
        newState.stage            != lastPresentationState_.stage              ||
        std::abs(newState.rmsDb  -  lastPresentationState_.rmsDb)  > rmsDeltaThreshold ||
        std::abs(newState.peakDb -  lastPresentationState_.peakDb) > rmsDeltaThreshold ||
        newState.clippingDetected != lastPresentationState_.clippingDetected   ||
        newState.warning          != lastPresentationState_.warning            ||
        newState.stimulusDescription != lastPresentationState_.stimulusDescription ||
        newState.operatorPromptText  != lastPresentationState_.operatorPromptText  ||
        newState.excitationMode   != lastPresentationState_.excitationMode     ||
        newState.activeNoteNumber != lastPresentationState_.activeNoteNumber   ||
        newState.activeVelocity   != lastPresentationState_.activeVelocity     ||
        newState.isPaused         != lastPresentationState_.isPaused           ||
        newState.targetId         != lastPresentationState_.targetId           ||
        newState.isDarkMode       != lastPresentationState_.isDarkMode;

    if (!changed)
        return;

    lastPresentationState_ = newState;
    hasPresentationState_  = true;

#ifdef ABD_TESTING
    ++testUpdateExecutedCount_;
#endif

    // =========================================================================
    // From here: actual UI update — only executed when state changed.
    // =========================================================================

    isProfilingActive_ = (snapshot.sessionStatus == session::ProfilingSessionStatus::Profiling ||
                          snapshot.sessionStatus == session::ProfilingSessionStatus::Paused);
    isPaused_ = newState.isPaused;

    pauseButton_.setButtonText(isPaused_ ? "RESUME" : "PAUSE");

    currentProgress_ = snapshot.progress.progressPercent / 100.0;
    progressBar_.repaint();
#ifdef ABD_TESTING
    ++testRepaintCount_;
#endif

    const bool isDark = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);
    applyTheme(isDark);

    const auto colTextPrim = isDark ? juce::Colour(0xfff1f3f5) : juce::Colour(0xff1a1d20);
    const auto colTextSec  = isDark ? juce::Colour(0xff94a3b8) : juce::Colour(0xff475569);
    const auto calGreen    = isDark ? juce::Colour(0xff00e676) : juce::Colour(0xff00875a);
    const auto calAmber    = isDark ? juce::Colour(0xffff9100) : juce::Colour(0xffb45309);
    const auto calRed      = isDark ? juce::Colour(0xffff5252) : juce::Colour(0xffb91c1c);

    const bool hasRealTarget = session::hasRealTargetInSnapshot(snapshot);
    const bool canStart = session::canStartProfilingFromSnapshot(snapshot, isProfilingActive_);

    if (!hasRealTarget)
    {
        preflightWarningsLabel_.setText("[ NO TARGET CONFIGURED ] Select a hardware target or VST3 plugin in Step 1: Target & Routing.", juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, calRed);

        preflightRecipeLabel_.setText("Recipe: [Requires Target Selection]", juce::dontSendNotification);
        preflightRecipeLabel_.setColour(juce::Label::textColourId, colTextSec);

        preflightTimeLabel_.setText("Estimated duration: --", juce::dontSendNotification);
        preflightTimeLabel_.setColour(juce::Label::textColourId, colTextSec);

        modeBadgeLabel_.setText("TARGET: NONE CONFIGURED", juce::dontSendNotification);
        modeBadgeLabel_.setColour(juce::Label::textColourId, calRed);

        trialStageBadge_.setText("Stage: No Target", juce::dontSendNotification);
        trialStageBadge_.setColour(juce::Label::textColourId, calRed);

        startButton_.setEnabled(false);
        startButton_.setColour(juce::TextButton::buttonColourId, isDark ? juce::Colour(0xff252c33) : juce::Colour(0xffe2e8f0));
        startButton_.setColour(juce::TextButton::textColourOffId, isDark ? juce::Colour(0xff64748b) : juce::Colour(0xff94a3b8));
        startButton_.setTooltip("Select a hardware target or VST3 plugin in Step 1: Target & Routing.");
    }
    else
    {
        juce::String recipeName = snapshot.progress.activeRecipeName.empty() ? "Standard VCF & Harmonic Sweep" : juce::String(snapshot.progress.activeRecipeName);
        juce::String recipeVersion = snapshot.excitation.recipeVersion.empty() ? "v1" : juce::String(snapshot.excitation.recipeVersion);
        int pointCount = snapshot.progress.totalTrials > 0 ? snapshot.progress.totalTrials : 32;
        int estSec = snapshot.progress.estimatedRemainingSec > 0.0 ? static_cast<int>(snapshot.progress.estimatedRemainingSec) : 45;

        preflightRecipeLabel_.setText("Recipe: " + recipeName + " " + recipeVersion + " | Points: " + juce::String(pointCount), juce::dontSendNotification);
        preflightRecipeLabel_.setColour(juce::Label::textColourId, colTextPrim);

        preflightTimeLabel_.setText("Estimated duration: ~" + juce::String(estSec) + " s", juce::dontSendNotification);
        preflightTimeLabel_.setColour(juce::Label::textColourId, colTextSec);

        // Update Preflight with warnings if any
        if (!warningText.empty())
        {
            preflightWarningsLabel_.setText("Measurement Notice: " + juce::String(warningText), juce::dontSendNotification);
            preflightWarningsLabel_.setColour(juce::Label::textColourId, calAmber);
            preflightWarningsLabel_.setFont(juce::FontOptions(14.5f, juce::Font::bold));
        }
        else
        {
            preflightWarningsLabel_.setText("Acoustic Condition: Target verified and armed for execution", juce::dontSendNotification);
            preflightWarningsLabel_.setColour(juce::Label::textColourId, colTextSec);
            preflightWarningsLabel_.setFont(juce::FontOptions(14.0f, juce::Font::plain));
        }

        if (snapshot.sessionStatus == session::ProfilingSessionStatus::EvaluationLoadedForReview)
        {
            modeBadgeLabel_.setText("EVALUATION: FIXTURE REVIEW", juce::dontSendNotification);
            modeBadgeLabel_.setColour(juce::Label::textColourId, isDark ? SoundIdTheme::accentBlue : juce::Colour(0xff0284c7));
        }
        else if (snapshot.calibration.audio.bypassed)
        {
            modeBadgeLabel_.setText("CALIBRATION: BYPASS", juce::dontSendNotification);
            modeBadgeLabel_.setColour(juce::Label::textColourId, calAmber);
        }
        else if (snapshot.calibration.audio.completed)
        {
            modeBadgeLabel_.setText("CALIBRATION: VALIDATED", juce::dontSendNotification);
            modeBadgeLabel_.setColour(juce::Label::textColourId, calGreen);
        }
        else
        {
            modeBadgeLabel_.setText("CALIBRATION: NOT PERFORMED", juce::dontSendNotification);
            modeBadgeLabel_.setColour(juce::Label::textColourId, colTextSec);
        }

        if (snapshot.progress.trialStage == session::TrialLifecycleStage::WaitingForOperator)
        {
            trialStageBadge_.setText("Stage: Operator Action", juce::dontSendNotification);
            trialStageBadge_.setColour(juce::Label::textColourId, calAmber);
        }
        else if (snapshot.progress.trialStage == session::TrialLifecycleStage::Capturing)
        {
            trialStageBadge_.setText("Stage: Capturing", juce::dontSendNotification);
            trialStageBadge_.setColour(juce::Label::textColourId, calGreen);
        }
        else if (isProfilingActive_)
        {
            trialStageBadge_.setText("Stage: Profiling", juce::dontSendNotification);
            trialStageBadge_.setColour(juce::Label::textColourId, isDark ? SoundIdTheme::accentPurple : juce::Colour(0xff6d28d9));
        }
        else
        {
            trialStageBadge_.setText("Stage: Ready", juce::dontSendNotification);
            trialStageBadge_.setColour(juce::Label::textColourId, isDark ? SoundIdTheme::accentBlue : juce::Colour(0xff0284c7));
        }

        startButton_.setEnabled(canStart);
        if (canStart)
        {
            startButton_.setColour(juce::TextButton::buttonColourId, calGreen);
            startButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            startButton_.setTooltip("Start profiling measurement for the configured target");
        }
        else
        {
            startButton_.setColour(juce::TextButton::buttonColourId, isDark ? juce::Colour(0xff252c33) : juce::Colour(0xffe2e8f0));
            startButton_.setColour(juce::TextButton::textColourOffId, isDark ? juce::Colour(0xff64748b) : juce::Colour(0xff94a3b8));
            startButton_.setTooltip("Measurement blocked: verify target, recipe and calibration.");
        }
    }
#ifdef ABD_TESTING
    ++testSetTextCount_;
#endif

    // Update Monitor with real-time telemetry
    {
        juce::String counterText = hasRealTarget
            ? ("Point: " + juce::String(newState.pointIndex) + " of " + juce::String(newState.pointCount) + " (" + juce::String(static_cast<int>(snapshot.progress.progressPercent)) + "%)")
            : "Point: 0 of 0 (0%)";
        trialCounterLabel_.setText(counterText, juce::dontSendNotification);
#ifdef ABD_TESTING
        ++testSetTextCount_;
#endif
    }

    {
        juce::String timeText = hasRealTarget
            ? ("Elapsed: " + juce::String(static_cast<int>(snapshot.progress.elapsedTimeSec)) + " s | Remaining: " + juce::String(static_cast<int>(snapshot.progress.estimatedRemainingSec)) + " s")
            : "Elapsed: 0 s | Remaining: --";
        timeRemainingLabel_.setText(timeText, juce::dontSendNotification);
#ifdef ABD_TESTING
        ++testSetTextCount_;
#endif
    }

    if (isPaused_)
        stimulusLabel_.setText("Stimulus: [PAUSED] Trial waiting", juce::dontSendNotification);
    else if (!newState.stimulusDescription.empty())
        stimulusLabel_.setText("Stimulus: " + juce::String(newState.stimulusDescription), juce::dontSendNotification);
    else
        stimulusLabel_.setText("Stimulus: Awaiting start", juce::dontSendNotification);
#ifdef ABD_TESTING
    ++testSetTextCount_;
#endif

    {
        juce::String healthText = "Signal Health: RMS " + juce::String(newState.rmsDb, 1)
                                + " dBFS | Peak " + juce::String(newState.peakDb, 1) + " dBFS"
                                + (newState.clippingDetected ? " [CLIPPING DETECTED]" : " [OK]");
        signalHealthLabel_.setText(healthText, juce::dontSendNotification);
#ifdef ABD_TESTING
        ++testSetTextCount_;
#endif
    }

    signalHealthLabel_.setColour(juce::Label::textColourId,
        newState.clippingDetected ? calRed : colTextSec);

    // Trial lifecycle stage badge and operator card
    if (snapshot.progress.trialStage == session::TrialLifecycleStage::WaitingForOperator)
    {
        isWaitingForOperator_ = true;
        operatorStepCard_.setVisible(true);
        operatorPromptLabel_.setVisible(true);
        expectedSettingLabel_.setVisible(true);
        btnConfirmManual_.setVisible(true);
        btnConfirmManual_.setEnabled(true);
        btnConfirmManual_.setColour(juce::TextButton::buttonColourId, calGreen);
        btnRepeatStep_.setVisible(true);
        btnStepBack_.setVisible(snapshot.progress.currentTrial > 1);

        juce::String prompt = snapshot.progress.operatorPromptText.empty()
            ? (snapshot.excitation.manual.has_value() ? snapshot.excitation.manual->instruction : juce::String("Ajustar controles del sintetizador y pulsar Listo [Espacio]"))
            : juce::String(snapshot.progress.operatorPromptText);
        operatorPromptLabel_.setText(prompt, juce::dontSendNotification);

        juce::String expected = snapshot.excitation.manual.has_value() ? snapshot.excitation.manual->expectedSetting : juce::String("Baseline");
        expectedSettingLabel_.setText("Ajuste esperado: " + expected, juce::dontSendNotification);
    }
    else
    {
        isWaitingForOperator_ = false;
        operatorStepCard_.setVisible(false);
        operatorPromptLabel_.setVisible(false);
        expectedSettingLabel_.setVisible(false);
        btnConfirmManual_.setVisible(false);
        btnRepeatStep_.setVisible(false);
        btnStepBack_.setVisible(false);
    }

    if (hasRealTarget && snapshot.progress.activeExcitationMode == session::ExcitationMode::AutomatedMidi)
    {
        midiTrialDetailsLabel_.setText("Nota: " + juce::String(snapshot.progress.activeNoteName)
                                       + " | Vel: " + juce::String(snapshot.progress.activeVelocity)
                                       + " | Ch: " + juce::String(snapshot.progress.activeMidiChannel)
                                       + " | Gate: " + juce::String(static_cast<int>(snapshot.progress.activeGateMs)) + " ms"
                                       + " | Latencia: " + juce::String(snapshot.progress.detectedLatencyMs, 1) + " ms",
                                       juce::dontSendNotification);
        midiTrialDetailsLabel_.setVisible(true);
    }
    else
    {
        midiTrialDetailsLabel_.setVisible(false);
    }

    if (isProfilingActive_)
        preflightCard_.setText(isPaused_ ? "Session Workspace [PAUSED]" : "Active Profiling Workspace");
    else
        preflightCard_.setText("Session Workspace & Pre-Flight");

    pauseButton_.setEnabled(isProfilingActive_);
    cancelButton_.setEnabled(isProfilingActive_);

    // Only call resized()+repaint() when structural layout may change
    // (operator card visibility changed) or on genuine state transitions.
    resized();
    repaint();
}

void SoundIdProfilingRunView::paint(juce::Graphics& g)
{
    constexpr int outerPadding = 20;
    constexpr int headerHeight = 60;
    constexpr int footerHeight = 54;
    constexpr float cornerRadius = 6.0f;

    const bool isDark = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);

    const auto colBg           = isDark ? juce::Colour(0xff101214) : juce::Colour(0xfff8f9fa);
    const auto colPanel        = isDark ? juce::Colour(0xff171a1e) : juce::Colour(0xffffffff);
    const auto colCard         = isDark ? juce::Colour(0xff1e2329) : juce::Colour(0xfff1f3f5);
    const auto colCardAct      = isDark ? juce::Colour(0xff252c33) : juce::Colour(0xffe2e8f0);
    const auto colBorder       = isDark ? juce::Colour(0xff2e3640) : juce::Colour(0xffcbd5e1);
    const auto colBorderSubtle = isDark ? juce::Colour(0xff23282f) : juce::Colour(0xffe2e8f0);
    const auto colFooterBg     = isDark ? juce::Colour(0xff14171a) : juce::Colour(0xffffffff);
    const auto colTextSec      = isDark ? juce::Colour(0xff94a3b8) : juce::Colour(0xff475569);

    const auto calGreen = isDark ? juce::Colour(0xff00e676) : juce::Colour(0xff00875a);
    const auto calAmber = isDark ? juce::Colour(0xffff9100) : juce::Colour(0xffb45309);
    const auto calRed   = isDark ? juce::Colour(0xffff5252) : juce::Colour(0xffb91c1c);

    // L0: Canvas base profundo (#101214 en dark / #f8f9fa en light)
    g.fillAll(colBg);

    // Separador sutil bajo la cabecera
    g.setColour(colBorderSubtle);
    g.drawHorizontalLine(outerPadding + headerHeight - 4,
                         static_cast<float>(outerPadding),
                         static_cast<float>(getWidth() - outerPadding));

    // Fondo del Footer anclado inferiormente (L1: #14171a en dark / #ffffff en light)
    auto footerBounds = juce::Rectangle<float>(
        static_cast<float>(outerPadding),
        static_cast<float>(getHeight() - outerPadding - footerHeight),
        static_cast<float>(getWidth() - 2 * outerPadding),
        static_cast<float>(footerHeight));
    g.setColour(colFooterBg);
    g.fillRoundedRectangle(footerBounds, cornerRadius);
    g.setColour(colBorderSubtle);
    g.drawRoundedRectangle(footerBounds, cornerRadius, 1.0f);

    // Fondo del Workspace de Sesión (preflightCard_)
    auto pfBounds = preflightCard_.getBounds().toFloat();
    if (!pfBounds.isEmpty())
    {
        juce::Colour wsBg = isProfilingActive_ ? colCardAct : colCard;
        g.setColour(wsBg);
        g.fillRoundedRectangle(pfBounds, cornerRadius);

        if (isProfilingActive_)
        {
            juce::Colour borderAccent = isPaused_ ? calAmber : juce::Colour(0xff38bdf8);
            g.setColour(borderAccent.withAlpha(0.6f));
            g.drawRoundedRectangle(pfBounds, cornerRadius, 1.2f);
        }
        else
        {
            g.setColour(colBorder);
            g.drawRoundedRectangle(pfBounds, cornerRadius, 1.0f);
        }
    }

    // Fondo de la Zona Reservada de Guidance (operatorStepCard_)
    auto opBounds = operatorStepCard_.getBounds().toFloat();
    if (!opBounds.isEmpty())
    {
        if (isWaitingForOperator_)
        {
            // Ámbar cálido para acción requerida de operador
            g.setColour(isDark ? juce::Colour(0xff2a2215) : juce::Colour(0xfffef3c7));
            g.fillRoundedRectangle(opBounds, cornerRadius);
            g.setColour(calAmber);
            g.drawRoundedRectangle(opBounds, cornerRadius, 2.0f);
        }
        else
        {
            // En reposo: neutro de soporte
            g.setColour(colPanel);
            g.fillRoundedRectangle(opBounds, cornerRadius);
            g.setColour(colBorderSubtle);
            g.drawRoundedRectangle(opBounds, cornerRadius, 1.0f);
        }
    }

    // Fondo del Monitor en vivo (activeMonitorCard_)
    auto monBounds = activeMonitorCard_.getBounds().toFloat();
    if (!monBounds.isEmpty())
    {
        g.setColour(colPanel);
        g.fillRoundedRectangle(monBounds, cornerRadius);
        g.setColour(colBorder);
        g.drawRoundedRectangle(monBounds, cornerRadius, 1.0f);

        // Renderizado del Vúmetro Gráfico dentro del monitor
        auto meterArea = monBounds.reduced(16.0f, 20.0f);
        meterArea.removeFromTop(38.0f); // Espacio bajo signalHealthLabel_

        float rms = lastPresentationState_.rmsDb;
        float peak = lastPresentationState_.peakDb;
        if (rms < -90.0f) rms = -90.0f;
        if (peak < -90.0f) peak = -90.0f;

        // Barra del vúmetro (Track) usando tokens compartidos TelemetryTokens
        auto meterBar = meterArea.removeFromTop(SoundIdTheme::TelemetryTokens::trackHeight);
        g.setColour(SoundIdTheme::TelemetryTokens::trackBg(isDark));
        g.fillRoundedRectangle(meterBar, SoundIdTheme::TelemetryTokens::cornerRadius);
        g.setColour(SoundIdTheme::TelemetryTokens::trackBorder(isDark));
        g.drawRoundedRectangle(meterBar, SoundIdTheme::TelemetryTokens::cornerRadius, 1.0f);

        // Barra de nivel RMS
        float normRms = juce::jlimit(0.0f, 1.0f, (rms + 60.0f) / 60.0f);
        if (normRms > 0.001f)
        {
            auto fillBar = meterBar.reduced(1.0f);
            fillBar.setWidth(fillBar.getWidth() * normRms);
            juce::Colour rmsCol = (rms > SoundIdTheme::TelemetryTokens::clippingThresholdDb) ? SoundIdTheme::TelemetryTokens::clippingColour()
                                : (rms > SoundIdTheme::TelemetryTokens::warningThresholdDb) ? SoundIdTheme::TelemetryTokens::warningColour()
                                : SoundIdTheme::TelemetryTokens::safeColour();
            g.setColour(rmsCol);
            g.fillRoundedRectangle(fillBar, SoundIdTheme::TelemetryTokens::cornerRadius - 1.0f);
        }

        // Línea de aguja Peak
        float normPeak = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 60.0f);
        if (normPeak > 0.001f)
        {
            float peakX = meterBar.getX() + meterBar.getWidth() * normPeak;
            g.setColour(lastPresentationState_.clippingDetected ? SoundIdTheme::TelemetryTokens::clippingColour()
                                                                : SoundIdTheme::TelemetryTokens::peakNeedleColour(isDark));
            g.fillRect(juce::Rectangle<float>(peakX - 1.0f, meterBar.getY(), 2.0f, meterBar.getHeight()));
        }

        // Ticks de escala dB (-60, -18, 0 dB)
        auto scaleArea = meterArea.removeFromTop(16.0f);
        g.setColour(colTextSec);
        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
        g.drawText("-60", juce::Rectangle<float>(scaleArea.getX(), scaleArea.getY(), 30.0f, scaleArea.getHeight()), juce::Justification::left);
        g.drawText("-18", juce::Rectangle<float>(scaleArea.getCentreX() - 15.0f, scaleArea.getY(), 30.0f, scaleArea.getHeight()), juce::Justification::centred);
        g.drawText("0 dB", juce::Rectangle<float>(scaleArea.getRight() - 35.0f, scaleArea.getY(), 35.0f, scaleArea.getHeight()), juce::Justification::right);
    }
}

void SoundIdProfilingRunView::resized()
{
    constexpr int outerPadding = 20;
    constexpr int columnGap = 16;
    constexpr int cardGap = 12;
    constexpr int headerHeight = 60;
    constexpr int footerHeight = 54;
    constexpr int guidanceMinHeight = 94;
    constexpr int minMonitorWidth = 260;

    auto area = getLocalBounds().reduced(outerPadding);

    // 1. Cabecera superior
    auto headerArea = area.removeFromTop(headerHeight);
    modeBadgeLabel_.setBounds(headerArea.removeFromRight(260));
    headerTitle_.setBounds(headerArea.removeFromTop(30));
    headerSubtitle_.setBounds(headerArea);

    area.removeFromTop(cardGap);

    // 2. Footer anclado inferiormente (siempre a la misma altura fija)
    auto footerArea = area.removeFromBottom(footerHeight);
    area.removeFromBottom(cardGap);

    // Layout de botones en el footer
    {
        auto footerRow = footerArea.reduced(10, 8);
        // Acciones primarias a la izquierda
        startButton_.setBounds(footerRow.removeFromLeft(180));
        footerRow.removeFromLeft(10);
        pauseButton_.setBounds(footerRow.removeFromLeft(90));
        footerRow.removeFromLeft(10);
        cancelButton_.setBounds(footerRow.removeFromLeft(90));

        // Acciones de soporte a la derecha
        loadEvaluationButton_.setBounds(footerRow.removeFromRight(140));
        footerRow.removeFromRight(12);
        advancedSettingsLink_.setBounds(footerRow.removeFromRight(200));
    }

    // 3. Grid central de dos columnas: Workspace (izq) y Monitor (der)
    const int contentWidth = area.getWidth();
    int rightWidth = juce::roundToInt(contentWidth * 0.32f);
    if (rightWidth < minMonitorWidth && contentWidth >= (minMonitorWidth + 240))
    {
        rightWidth = minMonitorWidth;
    }
    else if (contentWidth < (minMonitorWidth + 240))
    {
        rightWidth = juce::jmax(120, (contentWidth - columnGap) / 2);
    }
    int leftWidth = juce::jmax(120, contentWidth - rightWidth - columnGap);

    auto leftCol = area.removeFromLeft(leftWidth);
    area.removeFromLeft(columnGap);
    auto rightCol = area;

    // -------------------------------------------------------------------------
    // Columna Izquierda: Workspace de sesion + Zona reservada de guidance
    // -------------------------------------------------------------------------
    auto guidanceArea = leftCol.removeFromBottom(guidanceMinHeight);
    leftCol.removeFromBottom(cardGap);
    auto workspaceArea = leftCol;

    preflightCard_.setBounds(workspaceArea);
    auto wsInner = workspaceArea.reduced(16, 20);

    auto topInfoRow = wsInner.removeFromTop(24);
    trialStageBadge_.setBounds(topInfoRow.removeFromLeft(170));
    preflightRecipeLabel_.setBounds(topInfoRow);
    wsInner.removeFromTop(6);

    if (midiTrialDetailsLabel_.isVisible())
    {
        midiTrialDetailsLabel_.setBounds(wsInner.removeFromTop(20));
        wsInner.removeFromTop(4);
    }

    stimulusLabel_.setBounds(wsInner.removeFromTop(20));
    wsInner.removeFromTop(8);

    progressBar_.setBounds(wsInner.removeFromTop(22));
    wsInner.removeFromTop(8);

    auto counterRow = wsInner.removeFromTop(22);
    trialCounterLabel_.setBounds(counterRow.removeFromLeft(counterRow.getWidth() / 2));
    timeRemainingLabel_.setBounds(counterRow);
    wsInner.removeFromTop(6);

    preflightTimeLabel_.setBounds(wsInner.removeFromTop(20));

    // Zona reservada de instrucciones (Guidance)
    operatorStepCard_.setBounds(guidanceArea);
    if (isWaitingForOperator_)
    {
        operatorStepCard_.setVisible(true);
        preflightWarningsLabel_.setVisible(false);

        auto opInner = guidanceArea.reduced(14, 10);
        operatorPromptLabel_.setBounds(opInner.removeFromTop(20));
        expectedSettingLabel_.setBounds(opInner.removeFromTop(18));
        opInner.removeFromTop(4);

        auto opBtnRow = opInner.removeFromTop(SoundIdTheme::ButtonTokens::height);
        btnConfirmManual_.setBounds(opBtnRow.removeFromLeft(220));
        opBtnRow.removeFromLeft(10);
        btnRepeatStep_.setBounds(opBtnRow.removeFromLeft(100));
        opBtnRow.removeFromLeft(8);
        btnStepBack_.setBounds(opBtnRow.removeFromLeft(90));
    }
    else
    {
        operatorStepCard_.setVisible(false);
        preflightWarningsLabel_.setVisible(true);
        preflightWarningsLabel_.setBounds(guidanceArea.reduced(16, 14));
    }

    // -------------------------------------------------------------------------
    // Columna Derecha: Monitor en vivo y telemetria de salud
    // -------------------------------------------------------------------------
    activeMonitorCard_.setBounds(rightCol);
    auto monInner = rightCol.reduced(16, 22);
    signalHealthLabel_.setBounds(monInner.removeFromTop(24));
}

} // namespace abdaudiolab::gui::soundid
