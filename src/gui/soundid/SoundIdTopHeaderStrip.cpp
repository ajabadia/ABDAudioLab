#include "SoundIdTopHeaderStrip.h"
#include "../SoundIdTheme.h"
#include "../session/ProfilingSessionController.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdTopHeaderStrip::SoundIdTopHeaderStrip(session::IProfilingSessionCommands* commands)
    : commands_(commands)
{
    auto setupPill = [this](juce::Label& lbl) {
        lbl.setFont(juce::Font(12.0f, juce::Font::bold));
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        addAndMakeVisible(lbl);
    };

    setupPill(targetLabel_);
    targetLabel_.setText("Target: Ninguno", juce::dontSendNotification);

    // Stepper interactivo de 3 etapas
    auto setupStepButton = [this](juce::TextButton& btn, const juce::String& text, session::ProfilingWorkflowStage targetStage) {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        btn.onClick = [this, targetStage]() {
            if (commands_ != nullptr)
            {
                commands_->recordUserClick();
                commands_->navigateToStage(targetStage);
            }
        };
        addAndMakeVisible(btn);
    };

    setupStepButton(step1Button_, juce::String::fromUTF8(u8"1. Target"), session::ProfilingWorkflowStage::TargetSelection);
    setupStepButton(step2Button_, juce::String::fromUTF8(u8"2. Medir"), session::ProfilingWorkflowStage::ConfigureAndStart);
    setupStepButton(step3Button_, juce::String::fromUTF8(u8"3. Resultados"), session::ProfilingWorkflowStage::ReviewResults);

    // Botón directo para cargar evaluaciones JSON con menú de ejemplos
    loadJsonButton_.setButtonText(juce::String::fromUTF8(u8"📂 Cargar JSON... ▼"));
    loadJsonButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    loadJsonButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    loadJsonButton_.onClick = [this]() {
        if (commands_ == nullptr)
            return;

        juce::PopupMenu m;
        m.addSectionHeader(juce::String::fromUTF8(u8"Evaluaciones de Ejemplo (Precargadas)"));
        m.addItem(1, juce::String::fromUTF8(u8"1. Fixture Aprobado (Limpio, Verificado)"));
        m.addItem(2, juce::String::fromUTF8(u8"2. Dexed FM (Aceptado con Advertencias)"));
        m.addItem(3, juce::String::fromUTF8(u8"3. Adulterado / Tampered (Fallo de Hash)"));
        m.addItem(4, juce::String::fromUTF8(u8"4. Inconcluso (Falta Holdout)"));
        m.addItem(5, juce::String::fromUTF8(u8"5. Rechazado (Falta de Fidelidad ESR)"));
        m.addSeparator();
        m.addItem(6, juce::String::fromUTF8(u8"Examinar archivo JSON en disco..."));

        juce::Component::SafePointer<SoundIdTopHeaderStrip> safeThis(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadJsonButton_),
            [safeThis](int result) {
                if (safeThis == nullptr || safeThis->commands_ == nullptr)
                    return;

                safeThis->commands_->recordUserClick();
                if (result == 1)
                    safeThis->commands_->loadPredefinedFixture("fixture_approved.json");
                else if (result == 2)
                    safeThis->commands_->loadPredefinedFixture("dexed_warnings.json");
                else if (result == 3)
                    safeThis->commands_->loadPredefinedFixture("tampered_hash_mismatch.json");
                else if (result == 4)
                    safeThis->commands_->loadPredefinedFixture("inconclusive.json");
                else if (result == 5)
                    safeThis->commands_->loadPredefinedFixture("rejected.json");
                else if (result == 6)
                {
                    auto dir = session::ProfilingSessionController::getEvaluationsDirectory();
                    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                    safeThis->fileChooser_ = std::make_shared<juce::FileChooser>(
                        juce::String::fromUTF8(u8"Cargar Evaluación Acústica (JSON)"),
                        dir,
                        "*.json");

                    safeThis->fileChooser_->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& fc) {
                        if (safeThis == nullptr || safeThis->commands_ == nullptr)
                            return;
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            safeThis->commands_->loadEvaluationFromFile(file.getFullPathName().toStdString());
                        }
                    });
                }
            });
    };
    addAndMakeVisible(loadJsonButton_);

    setupPill(statusLabel_);
    statusLabel_.setText("Reposo", juce::dontSendNotification);

    setupPill(modelLabel_);
    modelLabel_.setText("Modelo: Pendiente", juce::dontSendNotification);

    setupPill(telemetryLabel_);
    telemetryLabel_.setFont(juce::Font(11.0f, juce::Font::plain));
    telemetryLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    telemetryLabel_.setText("96 kHz | 256 smp", juce::dontSendNotification);

    setupPill(alertsBadge_);
    alertsBadge_.setText("Alertas: 0", juce::dontSendNotification);
}

void SoundIdTopHeaderStrip::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    currentStatus_ = snapshot.sessionStatus;
    currentStage_ = snapshot.workflowStage;
    alertCount_ = static_cast<int>(snapshot.activeAlerts.size());

    // Target
    if (snapshot.target.targetName.empty())
        targetLabel_.setText("Target: Ninguno", juce::dontSendNotification);
    else
        targetLabel_.setText("Target: " + snapshot.target.targetName, juce::dontSendNotification);

    // Resaltar el botón del stepper según la etapa activa
    auto setStepStyle = [](juce::TextButton& btn, bool isActive) {
        if (isActive)
        {
            btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
            btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        }
    };

    juce::String step1Text = juce::String::fromUTF8(u8"1. Target");
    if (!snapshot.target.targetId.empty())
        step1Text += " [OK]";
    step1Button_.setButtonText(step1Text);

    juce::String step2Text = juce::String::fromUTF8(u8"2. Medir");
    if (snapshot.audit.isAudited)
        step2Text += juce::String::fromUTF8(u8" [Listo]");
    step2Button_.setButtonText(step2Text);

    juce::String step3Text = juce::String::fromUTF8(u8"3. Resultados");
    if (snapshot.evaluation.hasEvaluation)
        step3Text += juce::String::fromUTF8(u8" [Disp.]");
    step3Button_.setButtonText(step3Text);

    setStepStyle(step1Button_, currentStage_ == session::ProfilingWorkflowStage::TargetSelection);
    setStepStyle(step2Button_, currentStage_ == session::ProfilingWorkflowStage::ConfigureAndStart ||
                               currentStage_ == session::ProfilingWorkflowStage::ProfilingActive);
    setStepStyle(step3Button_, currentStage_ == session::ProfilingWorkflowStage::ReviewResults);

    // Estado
    statusLabel_.setText(session::sessionStatusToString(snapshot.sessionStatus), juce::dontSendNotification);

    // Modelo
    if (snapshot.evaluation.hasEvaluation && !snapshot.evaluation.recommendedModelType.empty())
    {
        modelLabel_.setText("Modelo: " + snapshot.evaluation.recommendedModelType, juce::dontSendNotification);
    }
    else
    {
        modelLabel_.setText("Modelo: En espera", juce::dontSendNotification);
    }

    // Alertas
    alertsBadge_.setText("Alertas: " + std::to_string(alertCount_), juce::dontSendNotification);

    repaint();
}

void SoundIdTopHeaderStrip::setHardwareTelemetry(double sampleRate, int blockSize, double cpuPercent)
{
    std::ostringstream ss;
    ss << static_cast<int>(sampleRate / 1000.0) << " kHz | "
       << blockSize << " spl | CPU: "
       << std::fixed << std::setprecision(1) << cpuPercent << "%";
    telemetryLabel_.setText(ss.str(), juce::dontSendNotification);
}

void SoundIdTopHeaderStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fondo oscuro sutil de la barra
    g.setColour(SoundIdTheme::bgCard);
    g.fillRect(bounds);

    // Borde inferior separador
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    // Colorear badge de estado según situación
    auto pillBounds = statusLabel_.getBounds().toFloat().reduced(2.0f);
    juce::Colour statusCol = SoundIdTheme::surfaceSubtle;
    if (currentStatus_ == session::ProfilingSessionStatus::Profiling)
        statusCol = SoundIdTheme::accentGreen.withAlpha(0.2f);
    else if (currentStatus_ == session::ProfilingSessionStatus::ReadyToProfile)
        statusCol = SoundIdTheme::accentBlue.withAlpha(0.2f);
    else if (currentStatus_ == session::ProfilingSessionStatus::Completed ||
             currentStatus_ == session::ProfilingSessionStatus::EvaluationLoadedForReview ||
             currentStatus_ == session::ProfilingSessionStatus::Exported)
        statusCol = SoundIdTheme::accentGreen.withAlpha(0.3f);
    else if (currentStatus_ == session::ProfilingSessionStatus::AuditRejected ||
             currentStatus_ == session::ProfilingSessionStatus::Failed ||
             currentStatus_ == session::ProfilingSessionStatus::MeasurementInvalid)
        statusCol = SoundIdTheme::accentRed.withAlpha(0.25f);

    g.setColour(statusCol);
    g.fillRoundedRectangle(pillBounds, 4.0f);

    // Colorear badge de alertas
    if (alertCount_ > 0)
    {
        auto alertB = alertsBadge_.getBounds().toFloat().reduced(2.0f);
        g.setColour(SoundIdTheme::accentAmber.withAlpha(0.25f));
        g.fillRoundedRectangle(alertB, 4.0f);
    }
}

void SoundIdTopHeaderStrip::resized()
{
    auto area = getLocalBounds().reduced(8, 4);
    const int gap = 6;

    targetLabel_.setBounds(area.removeFromLeft(160));
    area.removeFromLeft(gap);

    // Stepper interactivo de 3 pasos
    step1Button_.setBounds(area.removeFromLeft(80));
    area.removeFromLeft(3);
    step2Button_.setBounds(area.removeFromLeft(80));
    area.removeFromLeft(3);
    step3Button_.setBounds(area.removeFromLeft(95));
    area.removeFromLeft(gap + 4);

    // Botón de carga rápida JSON
    loadJsonButton_.setBounds(area.removeFromLeft(120));
    area.removeFromLeft(gap + 4);

    statusLabel_.setBounds(area.removeFromLeft(130));
    area.removeFromLeft(gap);

    modelLabel_.setBounds(area.removeFromLeft(150));
    area.removeFromLeft(gap);

    alertsBadge_.setBounds(area.removeFromRight(80));
    area.removeFromRight(gap);

    telemetryLabel_.setBounds(area.removeFromRight(150));
}

} // namespace abdaudiolab::gui::soundid
