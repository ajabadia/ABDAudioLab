#include "SoundIdProfilingRunView.h"
#include "../SoundIdTheme.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdProfilingRunView::SoundIdProfilingRunView(session::IProfilingSessionCommands& commands)
    : commands_(commands),
      progressBar_(currentProgress_)
{
    headerTitle_.setText("Paso 2: Ejecutar Perfilado Adaptativo", juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText("El sistema excita el target, mide respuestas acústicas y sintetiza el modelo óptimo.", juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Tarjeta Pre-Vuelo
    preflightCard_.setText("Revisión Pre-Vuelo");
    preflightCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    preflightCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(preflightCard_);

    auto setupInfo = [this](juce::Label& lbl, const std::string& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(preflightRecipeLabel_, "Receta seleccionada: Excitación de Parámetros y VCF Sweep");
    setupInfo(preflightTimeLabel_, "Duración estimada: ~45 segundos (20 ensayos adaptativos)");
    setupInfo(preflightWarningsLabel_, "Instrucciones: Requiere reset de fase entre notas");

    // Botón gigante de inicio
    startButton_.setButtonText("INICIAR PERFILADO");
    startButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    startButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    startButton_.onClick = [this]() {
        commands_.startProfiling();
    };
    addAndMakeVisible(startButton_);

    advancedSettingsLink_.setButtonText("Opciones avanzadas de medición...");
    advancedSettingsLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    advancedSettingsLink_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    advancedSettingsLink_.onClick = [this]() {
        commands_.navigateToStage(session::ProfilingWorkflowStage::AdvancedSettings);
    };
    addAndMakeVisible(advancedSettingsLink_);

    // Monitor activo
    activeMonitorCard_.setText("Telemetría en Tiempo Real");
    activeMonitorCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    activeMonitorCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(activeMonitorCard_);

    progressBar_.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar_.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::surfaceSubtle);
    addAndMakeVisible(progressBar_);

    setupInfo(trialCounterLabel_, "Punto: 0 de 0 (0%)");
    trialCounterLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    trialCounterLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);

    setupInfo(timeRemainingLabel_, "Tiempo restante estimado: -- s");
    setupInfo(stimulusLabel_, "Estímulo activo: En espera");
    setupInfo(signalHealthLabel_, "Salud acústica: Nivel óptimo (SNR: >70 dB)");

    pauseButton_.setButtonText("Pausar");
    pauseButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    pauseButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    pauseButton_.onClick = [this]() {
        commands_.pauseProfiling();
    };
    addAndMakeVisible(pauseButton_);

    cancelButton_.setButtonText("Cancelar");
    cancelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentRed.withAlpha(0.2f));
    cancelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    cancelButton_.onClick = [this]() {
        commands_.cancelProfiling();
    };
    addAndMakeVisible(cancelButton_);
}

void SoundIdProfilingRunView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isProfilingActive_ = (snapshot.sessionStatus == session::ProfilingSessionStatus::Profiling ||
                          snapshot.sessionStatus == session::ProfilingSessionStatus::Paused);

    currentProgress_ = snapshot.progress.progressPercent / 100.0;

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
        preflightWarningsLabel_.setText("Advertencias de medición: " + warningText, juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
    }
    else
    {
        preflightWarningsLabel_.setText("Condición acústica: Target verificado y óptimo para perfilado", juce::dontSendNotification);
        preflightWarningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    }

    // Actualizar Monitor
    std::ostringstream ssCounter;
    ssCounter << "Punto: " << snapshot.progress.currentTrial << " de "
              << snapshot.progress.totalTrials << " ("
              << static_cast<int>(snapshot.progress.progressPercent) << "%)";
    trialCounterLabel_.setText(ssCounter.str(), juce::dontSendNotification);

    std::ostringstream ssTime;
    ssTime << "Tiempo transcurrido: " << static_cast<int>(snapshot.progress.elapsedTimeSec)
           << " s | Restante: " << static_cast<int>(snapshot.progress.estimatedRemainingSec) << " s";
    timeRemainingLabel_.setText(ssTime.str(), juce::dontSendNotification);

    stimulusLabel_.setText("Estímulo: " + snapshot.progress.currentStimulusDescription, juce::dontSendNotification);

    std::ostringstream ssHealth;
    ssHealth << "Salud acústica: RMS " << std::fixed << std::setprecision(1) << snapshot.observation.lastRmsDb
             << " dB | Peak " << snapshot.observation.lastPeakDb << " dB"
             << (snapshot.observation.clippingDetected ? " [CLIPPING DETECTADO]" : " [OK]");
    signalHealthLabel_.setText(ssHealth.str(), juce::dontSendNotification);
    if (snapshot.observation.clippingDetected)
        signalHealthLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
    else
        signalHealthLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    startButton_.setEnabled(snapshot.sessionStatus == session::ProfilingSessionStatus::ReadyToProfile);
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

    headerTitle_.setBounds(area.removeFromTop(28));
    headerSubtitle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(16);

    // Fila superior: Preflight card (izq) y Botón Start (der)
    auto topRow = area.removeFromTop(160);
    auto leftPreflight = topRow.removeFromLeft(topRow.getWidth() / 2 - 12);
    preflightCard_.setBounds(leftPreflight);

    auto pfInner = leftPreflight.reduced(16, 24);
    preflightRecipeLabel_.setBounds(pfInner.removeFromTop(24));
    preflightTimeLabel_.setBounds(pfInner.removeFromTop(24));
    preflightWarningsLabel_.setBounds(pfInner.removeFromTop(24));

    topRow.removeFromLeft(24);
    auto rightStart = topRow;
    startButton_.setBounds(rightStart.removeFromTop(60));
    rightStart.removeFromTop(8);
    advancedSettingsLink_.setBounds(rightStart.removeFromTop(24));

    area.removeFromTop(24);

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
    auto btnRow = monInner.removeFromTop(36);
    pauseButton_.setBounds(btnRow.removeFromLeft(120));
    btnRow.removeFromLeft(16);
    cancelButton_.setBounds(btnRow.removeFromLeft(120));
}

} // namespace abdaudiolab::gui::soundid
