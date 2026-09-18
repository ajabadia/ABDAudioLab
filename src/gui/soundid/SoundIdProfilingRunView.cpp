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
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText("System excites the target instrument or effect, acquires acoustic responses, and synthesizes the model.", juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Mode Badge
    modeBadgeLabel_.setText("Measurement Session", juce::dontSendNotification);
    modeBadgeLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
    modeBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    modeBadgeLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(modeBadgeLabel_);

    // Pre-Flight Review Card
    preflightCard_.setText("Pre-Flight Review");
    preflightCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    preflightCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(preflightCard_);

    auto setupInfo = [this](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(preflightRecipeLabel_, "Selected Recipe: Standard Profiling & VCF Sweep");
    setupInfo(preflightTimeLabel_, "Estimated Duration: ~30 seconds");
    setupInfo(preflightWarningsLabel_, "Acoustic Condition: Target verified and armed for execution");

    // Primary Giant Start Button
    startButton_.setButtonText("▶  START MEASUREMENT");
    startButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    startButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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

    // Load Evaluation Button
    loadEvaluationButton_.setButtonText("📂 Load Evaluation... ▼");
    loadEvaluationButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    loadEvaluationButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
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

    // Botón de salto directo al Paso 3
    // Direct Navigation to Results
    viewResultsButton_.setButtonText("View Results ➔");
    viewResultsButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    viewResultsButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    viewResultsButton_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.navigateToStage(session::ProfilingWorkflowStage::ReviewResults);
    };
    addAndMakeVisible(viewResultsButton_);

    advancedSettingsLink_.setButtonText("Advanced Measurement Parameters...");
    advancedSettingsLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    advancedSettingsLink_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    advancedSettingsLink_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.setOpenedAdvancedMode(true);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Advanced Measurement Parameters",
            "• Metrological Protocol: Fast-Acoustic Profiler (RFC 8785)\n"
            "• Execution Domain: Real-Time Audio Engine / VST3 Event Pipeline\n"
            "• Sampling Matrix: Adaptive trials with Bayesian search\n"
            "• Acceptance Criteria: RMS > -60 dBFS, Clean SNR, Zero Underruns\n"
            "• Phase Alignment: Note-on synchronized stimulus\n"
            "• Headroom: -3.0 dBFS safety margin",
            "OK");
    };
    addAndMakeVisible(advancedSettingsLink_);

    // Active Monitor Card
    activeMonitorCard_.setText("Real-Time Telemetry & Progress");
    activeMonitorCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    activeMonitorCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(activeMonitorCard_);

    progressBar_.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar_.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::surfaceSubtle);
    addAndMakeVisible(progressBar_);

    setupInfo(trialCounterLabel_, "Point: 0 of 0 (0%)");
    trialCounterLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    trialCounterLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);

    setupInfo(timeRemainingLabel_, "Elapsed: 0 s | Remaining: 0 s");
    setupInfo(stimulusLabel_, "Stimulus: Awaiting start");
    setupInfo(signalHealthLabel_, "Signal Health: RMS -120.0 dBFS | Peak -120.0 dBFS [OK]");

    pauseButton_.setButtonText("PAUSE");
    pauseButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    pauseButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
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

    cancelButton_.setButtonText("CANCEL");
    cancelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed.withAlpha(0.2f));
    cancelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
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
}

SoundIdProfilingRunView::~SoundIdProfilingRunView() = default;

void SoundIdProfilingRunView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isProfilingActive_ = (snapshot.sessionStatus == session::ProfilingSessionStatus::Profiling ||
                          snapshot.sessionStatus == session::ProfilingSessionStatus::Paused);
    isPaused_ = (snapshot.sessionStatus == session::ProfilingSessionStatus::Paused);

    pauseButton_.setButtonText(isPaused_ ? juce::String::fromUTF8(u8"Reanudar") : juce::String::fromUTF8(u8"Pausar"));

    currentProgress_ = snapshot.progress.progressPercent / 100.0;
    progressBar_.repaint();

    // Update Preflight with warnings if any
    std::string warningText;
    if (snapshot.audit.requiresResetBeforeEachTrial)
        warningText += "[!] Phase reset required before each trial. ";
    if (snapshot.audit.recommendedSettlingTimeMs > 200.0)
        warningText += "[!] Prolonged settling (" + std::to_string(static_cast<int>(snapshot.audit.recommendedSettlingTimeMs)) + " ms). ";
    if (!snapshot.audit.operationalWarnings.empty())
        warningText += snapshot.audit.operationalWarnings.front();
    else if (!snapshot.audit.humanGuidance.empty())
        warningText += snapshot.audit.humanGuidance;

    if (!warningText.empty())
    {
        preflightWarningsLabel_.setText("Measurement Notice: " + juce::String(warningText), juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
    }
    else
    {
        preflightWarningsLabel_.setText("Acoustic Condition: Target verified and armed for execution", juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    }

    // Update Monitor with real-time telemetry
    std::ostringstream ssCounter;
    ssCounter << "Point: " << snapshot.progress.currentTrial << " of "
              << snapshot.progress.totalTrials << " ("
              << static_cast<int>(snapshot.progress.progressPercent) << "%)";
    trialCounterLabel_.setText(ssCounter.str(), juce::dontSendNotification);

    std::ostringstream ssTime;
    ssTime << "Elapsed: " << static_cast<int>(snapshot.progress.elapsedTimeSec)
           << " s | Remaining: " << static_cast<int>(snapshot.progress.estimatedRemainingSec) << " s";
    timeRemainingLabel_.setText(ssTime.str(), juce::dontSendNotification);

    if (isPaused_)
    {
        stimulusLabel_.setText("Stimulus: [PAUSED] Trial waiting", juce::dontSendNotification);
    }
    else if (!snapshot.progress.currentStimulusDescription.empty())
    {
        stimulusLabel_.setText("Stimulus: " + juce::String(snapshot.progress.currentStimulusDescription), juce::dontSendNotification);
    }
    else
    {
        stimulusLabel_.setText("Stimulus: Awaiting start", juce::dontSendNotification);
    }

    std::ostringstream ssHealth;
    ssHealth << "Signal Health: RMS " << std::fixed << std::setprecision(1) << snapshot.observation.lastRmsDb
             << " dBFS | Peak " << snapshot.observation.lastPeakDb << " dBFS"
             << (snapshot.observation.clippingDetected ? " [CLIPPING DETECTED]" : " [OK]");
    signalHealthLabel_.setText(ssHealth.str(), juce::dontSendNotification);
    if (snapshot.observation.clippingDetected)
        signalHealthLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
    else
        signalHealthLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    // El botón Iniciar se habilita si está listo o si se desea reiniciar medición desde un estado previo
    bool canStart = (snapshot.sessionStatus == session::ProfilingSessionStatus::ReadyToProfile ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::TargetSelected ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::EvaluationLoadedForReview ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::Completed ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::Exported ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::Cancelled ||
                     snapshot.sessionStatus == session::ProfilingSessionStatus::Failed);
    startButton_.setEnabled(canStart && !isProfilingActive_);
    pauseButton_.setEnabled(isProfilingActive_);
    cancelButton_.setEnabled(isProfilingActive_);

    repaint();
}

void SoundIdProfilingRunView::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
}

void SoundIdProfilingRunView::resized()
{
    auto area = getLocalBounds().reduced(24);

    auto headerArea = area.removeFromTop(28);
    modeBadgeLabel_.setBounds(headerArea.removeFromRight(240));
    headerTitle_.setBounds(headerArea);

    headerSubtitle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(16);

    // Fila superior: Preflight card (izq) y Acciones (der)
    auto topRow = area.removeFromTop(160);
    auto leftPreflight = topRow.removeFromLeft(topRow.getWidth() / 2 - 12);
    preflightCard_.setBounds(leftPreflight);

    auto pfInner = leftPreflight.reduced(16, 24);
    preflightRecipeLabel_.setBounds(pfInner.removeFromTop(24));
    preflightTimeLabel_.setBounds(pfInner.removeFromTop(24));
    preflightWarningsLabel_.setBounds(pfInner.removeFromTop(24));

    topRow.removeFromLeft(24);
    auto rightActions = topRow;
    startButton_.setBounds(rightActions.removeFromTop(48));
    rightActions.removeFromTop(8);

    auto btnRow = rightActions.removeFromTop(32);
    loadEvaluationButton_.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2 - 4));
    btnRow.removeFromLeft(8);
    viewResultsButton_.setBounds(btnRow);

    rightActions.removeFromTop(6);
    advancedSettingsLink_.setBounds(rightActions.removeFromTop(24));

    area.removeFromTop(20);

    // Fila inferior: Monitor de progreso
    activeMonitorCard_.setBounds(area);
    auto monInner = area.reduced(16, 24);

    progressBar_.setBounds(monInner.removeFromTop(24));
    monInner.removeFromTop(12);

    trialCounterLabel_.setBounds(monInner.removeFromTop(24));
    timeRemainingLabel_.setBounds(monInner.removeFromTop(20));
    stimulusLabel_.setBounds(monInner.removeFromTop(20));
    signalHealthLabel_.setBounds(monInner.removeFromTop(20));

    monInner.removeFromTop(16);
    auto controlBtnRow = monInner.removeFromTop(36);
    pauseButton_.setBounds(controlBtnRow.removeFromLeft(120));
    controlBtnRow.removeFromLeft(16);
    cancelButton_.setBounds(controlBtnRow.removeFromLeft(120));
}

} // namespace abdaudiolab::gui::soundid
