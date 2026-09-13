#include "SoundIdTopHeaderStrip.h"
#include "../SoundIdTheme.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdTopHeaderStrip::SoundIdTopHeaderStrip()
{
    auto setupPill = [this](juce::Label& lbl) {
        lbl.setFont(juce::Font(12.0f, juce::Font::bold));
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        addAndMakeVisible(lbl);
    };

    setupPill(targetLabel_);
    targetLabel_.setText("Target: Ninguno", juce::dontSendNotification);

    setupPill(stageLabel_);
    stageLabel_.setText("Paso 1: Selección", juce::dontSendNotification);

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
    alertCount_ = static_cast<int>(snapshot.activeAlerts.size());

    // Target
    if (snapshot.target.targetName.empty())
        targetLabel_.setText("Target: Ninguno", juce::dontSendNotification);
    else
        targetLabel_.setText("Target: " + snapshot.target.targetName, juce::dontSendNotification);

    // Etapa
    switch (snapshot.workflowStage)
    {
        case session::ProfilingWorkflowStage::TargetSelection:
            stageLabel_.setText("1. Target", juce::dontSendNotification);
            break;
        case session::ProfilingWorkflowStage::ConfigureAndStart:
            stageLabel_.setText("2. Preparado", juce::dontSendNotification);
            break;
        case session::ProfilingWorkflowStage::ProfilingActive:
            stageLabel_.setText("2. Perfilando...", juce::dontSendNotification);
            break;
        case session::ProfilingWorkflowStage::ReviewResults:
            stageLabel_.setText("3. Resultados", juce::dontSendNotification);
            break;
        case session::ProfilingWorkflowStage::AdvancedSettings:
            stageLabel_.setText("Avanzado", juce::dontSendNotification);
            break;
    }

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
    const int pillWidth = 140;
    const int gap = 8;

    targetLabel_.setBounds(area.removeFromLeft(pillWidth + 20));
    area.removeFromLeft(gap);

    stageLabel_.setBounds(area.removeFromLeft(pillWidth));
    area.removeFromLeft(gap);

    statusLabel_.setBounds(area.removeFromLeft(pillWidth));
    area.removeFromLeft(gap);

    modelLabel_.setBounds(area.removeFromLeft(pillWidth + 40));
    area.removeFromLeft(gap);

    alertsBadge_.setBounds(area.removeFromRight(100));
    area.removeFromRight(gap);

    telemetryLabel_.setBounds(area.removeFromRight(160));
}

} // namespace abdaudiolab::gui::soundid
