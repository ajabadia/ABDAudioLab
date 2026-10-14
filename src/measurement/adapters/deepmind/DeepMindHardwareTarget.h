// ==============================================================================
// src/measurement/adapters/deepmind/DeepMindHardwareTarget.h
// Fase 4 — Adaptador de hardware DeepMind basado en composición sobre
//          HardwareSynthTarget / ISynthTarget.
// ==============================================================================

#pragma once

#include "../../synth/ISynthTarget.h"
#include "../../hardware/transport/IMidiTransport.h"
#include "../deepmind/DeepMind12SysExContracts.h"

#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::measurement::adapters::deepmind
{

/**
 * @brief Adaptador de hardware para el Behringer DeepMind 12 / 12D orientado a
 *        la medición de curvas VCF + envolvente en la Fase 4.
 *
 * Diseñado como wrapper compuesto sobre la interfaz canónica ISynthTarget y el
 * transporte MIDI del laboratorio, sin hardcodear marcas ni bytes mágicos fuera
 * del contrato de parámetros NRPN.
 */
class DeepMindHardwareTarget : public synth::ISynthTarget
{
public:
    DeepMindHardwareTarget();
    ~DeepMindHardwareTarget() override = default;

    // Sunced from ISynthTarget -----------------------------------------
    bool loadState(const synth::SynthPresetState& state) override;
    synth::StateAppliedStatus verifyState() const override;
    void prepare(const synth::ProcessingSpec& spec) override;
    void resetState() override;
    void render(const synth::MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int repetitionIndex = 0) override;
    [[nodiscard]] synth::TargetTimingInfo timingInfo() const override;

    synth::ISynthTarget::StateTransferResult getState(std::vector<uint8_t>& stateData) const override;
    synth::ISynthTarget::StateTransferResult setState(const std::vector<uint8_t>& stateData) override;
    [[nodiscard]] bool supportsBinaryState() const override;

    // Fase 4 helpers ----------------------------------------------------
    /**
     * @brief Envía un parámetro NRPN de 14 bits usando la secuencia estándar
     *        CC 99 (MSB), CC 98 (LSB), CC 6 (Data MSB), CC 38 (Data LSB).
     */
    bool sendNrpnParameter(uint16_t parameterId, uint16_t value14bit);

    /**
     * @brief Fuerza el estado Gray-Box base para auto-oscilación controlada del filtro:
     *        - VCF Cutoff -> 0
     *        - VCF Resonance -> 16383
     *        - VCF Env2 Depth -> 16383
     *        - Env2 Attack -> 0
     *        - Env2 Sustain -> 0
     */
    bool initializeFilterForAutoOscillation();

    [[nodiscard]] uint16_t getNrpnCutoff() const noexcept { return currentNrpnCutoff; }
    [[nodiscard]] uint16_t getNrpnResonance() const noexcept { return currentNrpnResonance; }
    [[nodiscard]] uint16_t getNrpnEnv2Depth() const noexcept { return currentNrpnEnv2Depth; }
    [[nodiscard]] uint16_t getNrpnEnv2AttackCurve() const noexcept { return currentNrpnEnv2AttackCurve; }

    void setTransport(std::shared_ptr<hardware::IMidiTransport> transport);

private:
    std::shared_ptr<hardware::IMidiTransport> transport_;
    synth::ProcessingSpec currentSpec_;

    uint16_t currentNrpnCutoff            { 0 };
    uint16_t currentNrpnResonance         { 0 };
    uint16_t currentNrpnEnv2Depth         { 0 };
    uint16_t currentNrpnEnv2AttackCurve   { 0 };

    bool isOpen_                          { false };
    bool stateLoaded_                     { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepMindHardwareTarget)
};

} // namespace abdaudiolab::measurement::adapters::deepmind
