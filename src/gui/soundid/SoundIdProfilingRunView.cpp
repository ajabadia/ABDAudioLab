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
    headerTitle_.setText(juce::String::fromUTF8(u8"Paso 2: Ejecutar Perfilado Adaptativo"), juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText(juce::String::fromUTF8(u8"El sistema excita el target, mide respuestas acústicas y sintetiza el modelo óptimo."), juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Etiqueta de Modo Demo/Sintético claramente visible
    modeBadgeLabel_.setText(juce::String::fromUTF8(u8"SyntheticFixture / DemoMode"), juce::dontSendNotification);
    modeBadgeLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
    modeBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    modeBadgeLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(modeBadgeLabel_);

    // Tarjeta Pre-Vuelo
    preflightCard_.setText(juce::String::fromUTF8(u8"Revisión Pre-Vuelo"));
    preflightCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    preflightCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(preflightCard_);

    auto setupInfo = [this](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(preflightRecipeLabel_, juce::String::fromUTF8(u8"Receta seleccionada: Excitación de Parámetros y VCF Sweep"));
    setupInfo(preflightTimeLabel_, juce::String::fromUTF8(u8"Duración estimada: ~45 segundos (20 ensayos adaptativos)"));
    setupInfo(preflightWarningsLabel_, juce::String::fromUTF8(u8"Instrucciones: Requiere reset de fase entre notas"));

    // Botón gigante de inicio
    startButton_.setButtonText(juce::String::fromUTF8(u8"INICIAR PERFILADO"));
    startButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    startButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    startButton_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.startProfiling();
    };
    addAndMakeVisible(startButton_);

    // Botón directo para cargar o elegir evaluaciones de ejemplo
    loadEvaluationButton_.setButtonText(juce::String::fromUTF8(u8"📂 Cargar Evaluación... ▼"));
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
    viewResultsButton_.setButtonText(juce::String::fromUTF8(u8"Ver Resultados (Paso 3) ➔"));
    viewResultsButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    viewResultsButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    viewResultsButton_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.navigateToStage(session::ProfilingWorkflowStage::ReviewResults);
    };
    addAndMakeVisible(viewResultsButton_);

    advancedSettingsLink_.setButtonText(juce::String::fromUTF8(u8"Opciones avanzadas de medición..."));
    advancedSettingsLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    advancedSettingsLink_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    advancedSettingsLink_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.setOpenedAdvancedMode(true);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            juce::String::fromUTF8(u8"Opciones Avanzadas de Medición"),
            juce::String::fromUTF8(u8"• Protocolo Metrológico: Fast-Acoustic Profiler (RFC 8785)\n"
                                  u8"• Modo de Ejecución: SyntheticFixture / DemoMode\n"
                                  u8"• Malla de Muestreo: 20 ensayos adaptativos con optimización bayesiana\n"
                                  u8"• Criterio de Aceptación: ESR < -30 dB, Correlación rho > 0.95\n"
                                  u8"• Partición de Datos: 80% Entrenamiento, 20% Holdout reservado\n"
                                  u8"• Calibración: Headroom de -3.0 dBFS con protección anticliping\n"
                                  u8"• Estimulación Acústica: Sweeps logarítmicos y modulación de parámetros"),
            juce::String::fromUTF8(u8"Aceptar"));
    };
    addAndMakeVisible(advancedSettingsLink_);

    // Monitor activo
    activeMonitorCard_.setText(juce::String::fromUTF8(u8"Telemetría en Tiempo Real (Coordinador Desacoplado)"));
    activeMonitorCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    activeMonitorCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(activeMonitorCard_);

    progressBar_.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar_.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::surfaceSubtle);
    addAndMakeVisible(progressBar_);

    setupInfo(trialCounterLabel_, juce::String::fromUTF8(u8"Punto: 0 de 0 (0%)"));
    trialCounterLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    trialCounterLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);

    setupInfo(timeRemainingLabel_, juce::String::fromUTF8(u8"Tiempo transcurrido: 0 s | Restante: 0 s"));
    setupInfo(stimulusLabel_, juce::String::fromUTF8(u8"Estímulo: En espera de inicio"));
    setupInfo(signalHealthLabel_, juce::String::fromUTF8(u8"Salud acústica: RMS -120.0 dB | Peak -120.0 dB [OK]"));

    pauseButton_.setButtonText(juce::String::fromUTF8(u8"Pausar"));
    pauseButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    pauseButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    pauseButton_.onClick = [this]() {
        commands_.recordUserClick();
        if (isPaused_)
        {
            commands_.resumeProfiling();
        }
        else
        {
            commands_.pauseProfiling();
        }
    };
    addAndMakeVisible(pauseButton_);

    cancelButton_.setButtonText(juce::String::fromUTF8(u8"Cancelar"));
    cancelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed.withAlpha(0.2f));
    cancelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    cancelButton_.onClick = [this]() {
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

    // Actualizar Preflight con advertencias críticas visibles
    std::string warningText;
    if (snapshot.audit.requiresResetBeforeEachTrial)
    {
        warningText += "[!] Requiere reset de fase antes de cada ensayo. ";
    }
    if (snapshot.audit.recommendedSettlingTimeMs > 200.0)
    {
        warningText += "[!] Settling prolongado (" + std::to_string(static_cast<int>(snapshot.audit.recommendedSettlingTimeMs)) + " ms). ";
    }
    if (!snapshot.audit.operationalWarnings.empty())
    {
        warningText += snapshot.audit.operationalWarnings.front();
    }
    else if (!snapshot.audit.humanGuidance.empty())
    {
        warningText += snapshot.audit.humanGuidance;
    }

    if (!warningText.empty())
    {
        preflightWarningsLabel_.setText(juce::String::fromUTF8(u8"Advertencias de medición: ") + juce::String::fromUTF8(warningText.c_str()), juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
    }
    else
    {
        preflightWarningsLabel_.setText(juce::String::fromUTF8(u8"Condición acústica: Target verificado y óptimo para perfilado"), juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    }

    // Actualizar Monitor con telemetría real del coordinador
    std::ostringstream ssCounter;
    ssCounter << "Punto: " << snapshot.progress.currentTrial << " de "
              << snapshot.progress.totalTrials << " ("
              << static_cast<int>(snapshot.progress.progressPercent) << "%)";
    trialCounterLabel_.setText(juce::String::fromUTF8(ssCounter.str().c_str()), juce::dontSendNotification);

    std::ostringstream ssTime;
    ssTime << "Tiempo transcurrido: " << static_cast<int>(snapshot.progress.elapsedTimeSec)
           << " s | Restante: " << static_cast<int>(snapshot.progress.estimatedRemainingSec) << " s";
    timeRemainingLabel_.setText(juce::String::fromUTF8(ssTime.str().c_str()), juce::dontSendNotification);

    if (isPaused_)
    {
        stimulusLabel_.setText(juce::String::fromUTF8(u8"Estímulo: [PAUSADO] Ensayo en espera"), juce::dontSendNotification);
    }
    else if (!snapshot.progress.currentStimulusDescription.empty())
    {
        stimulusLabel_.setText(juce::String::fromUTF8(u8"Estímulo: ") + juce::String::fromUTF8(snapshot.progress.currentStimulusDescription.c_str()), juce::dontSendNotification);
    }
    else
    {
        stimulusLabel_.setText(juce::String::fromUTF8(u8"Estímulo: En espera de inicio"), juce::dontSendNotification);
    }

    std::ostringstream ssHealth;
    ssHealth << "Salud acústica: RMS " << std::fixed << std::setprecision(1) << snapshot.observation.lastRmsDb
             << " dB | Peak " << snapshot.observation.lastPeakDb << " dB"
             << (snapshot.observation.clippingDetected ? " [CLIPPING DETECTADO]" : " [OK]");
    signalHealthLabel_.setText(juce::String::fromUTF8(ssHealth.str().c_str()), juce::dontSendNotification);
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
