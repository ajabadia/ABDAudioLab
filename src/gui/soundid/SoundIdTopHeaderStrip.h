#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Banda superior persistente de contexto operativo estilo SoundID.
 * Muestra Target activo, etapa del flujo, estado de sesión, modelo recomendado y alertas activas.
 */
class SoundIdTopHeaderStrip : public juce::Component
{
public:
    explicit SoundIdTopHeaderStrip(session::IProfilingSessionCommands* commands = nullptr);
    ~SoundIdTopHeaderStrip() override = default;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);
    void setHardwareTelemetry(double sampleRate, int blockSize, double cpuPercent);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    session::IProfilingSessionCommands* commands_ { nullptr };

    juce::Label targetLabel_;
    
    // Stepper interactivo de etapas de trabajo
    juce::TextButton step1Button_;
    juce::TextButton step2Button_;
    juce::TextButton step3Button_;

    // Acceso directo a carga de evaluaciones JSON
    juce::TextButton loadJsonButton_;
    std::shared_ptr<juce::FileChooser> fileChooser_;

    juce::Label statusLabel_;
    juce::Label modelLabel_;
    juce::Label telemetryLabel_;
    juce::Label alertsBadge_;

    session::ProfilingSessionStatus currentStatus_ { session::ProfilingSessionStatus::Idle };
    session::ProfilingWorkflowStage currentStage_ { session::ProfilingWorkflowStage::TargetSelection };
    int alertCount_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdTopHeaderStrip)
};

} // namespace abdaudiolab::gui::soundid
