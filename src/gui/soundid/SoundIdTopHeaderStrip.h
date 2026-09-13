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
    SoundIdTopHeaderStrip();
    ~SoundIdTopHeaderStrip() override = default;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);
    void setHardwareTelemetry(double sampleRate, int blockSize, double cpuPercent);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label targetLabel_;
    juce::Label stageLabel_;
    juce::Label statusLabel_;
    juce::Label modelLabel_;
    juce::Label telemetryLabel_;
    juce::Label alertsBadge_;

    session::ProfilingSessionStatus currentStatus_ { session::ProfilingSessionStatus::Idle };
    int alertCount_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdTopHeaderStrip)
};

} // namespace abdaudiolab::gui::soundid
