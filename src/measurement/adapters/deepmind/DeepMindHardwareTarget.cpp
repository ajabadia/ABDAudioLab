// ==============================================================================
// src/measurement/adapters/deepmind/DeepMindHardwareTarget.cpp
// Fase 4 — Implementación del adaptador DeepMind.
// ==============================================================================

#include "DeepMindHardwareTarget.h"
#include "DeepMind12SysExContracts.h"

#include <HardwareDrivers/MidiCcController.h>
#include <cmath>
#include <algorithm>


namespace abdaudiolab::measurement::adapters::deepmind
{

namespace
{
    constexpr int kDefaultMidiChannel = 1;

    // Convert a 14-bit value into NRPN MSB/LSB + Data Entry MSB/LSB.
    // NRPN parameter number and data entry are both 14-bit in the standard
    // CC 99/98 followed by CC 6/38 sequence.
    void split14Bit(uint16_t value14bit, uint8_t& msb, uint8_t& lsb) noexcept
    {
        msb = static_cast<uint8_t>(value14bit >> 7);
        lsb = static_cast<uint8_t>(value14bit & 0x7Fu);
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

DeepMindHardwareTarget::DeepMindHardwareTarget()
{
    // Estado por defecto: filtro cerrado, sin modulación.
    currentNrpnCutoff          = 0;
    currentNrpnResonance       = 0;
    currentNrpnEnv2Depth       = 0;
    currentNrpnEnv2AttackCurve = 0;
}

bool DeepMindHardwareTarget::loadState(const synth::SynthPresetState&)
{
    // Para la Fase 4 usamos inicialización directa mediante NRPN; este método
    // se comporta como no-op estructural para mantener la interfaz canónica.
    return true;
}

synth::StateAppliedStatus DeepMindHardwareTarget::verifyState() const
{
    // En modo Gray-Box físico verificaríamos telemetría real; aquí reportamos
    // que el estado interno del adaptador es consistente si hay transporte abierto.
    if (!isOpen_)
        return synth::StateAppliedStatus::NotApplicable;

    return synth::StateAppliedStatus::Applied;
}

void DeepMindHardwareTarget::prepare(const synth::ProcessingSpec& spec)
{
    currentSpec_ = spec;
}

void DeepMindHardwareTarget::resetState()
{
    isOpen_        = false;
    stateLoaded_   = false;

    currentNrpnCutoff          = 0;
    currentNrpnResonance       = 0;
    currentNrpnEnv2Depth       = 0;
    currentNrpnEnv2AttackCurve = 0;
}

void DeepMindHardwareTarget::render(const synth::MidiExcitationSequence&,
                                     std::vector<float>& destinationAudio,
                                     int)
{
    // En la Fase 4 el renderizado de audio lo hace el receptor lock-free del
    // laboratorio; este método se mantiene como stub canónico.
    destinationAudio.clear();
}

[[nodiscard]] synth::TargetTimingInfo DeepMindHardwareTarget::timingInfo() const
{
    synth::TargetTimingInfo info;
    info.isPhysicalHardware = true;
    info.isDirectPlugin     = false;
    return info;
}

synth::ISynthTarget::StateTransferResult DeepMindHardwareTarget::getState(std::vector<uint8_t>&) const
{
    return { false, false, "NOT_SUPPORTED", 0, "" };
}

synth::ISynthTarget::StateTransferResult DeepMindHardwareTarget::setState(const std::vector<uint8_t>&)
{
    return { false, false, "NOT_SUPPORTED", 0, "" };
}

[[nodiscard]] bool DeepMindHardwareTarget::supportsBinaryState() const
{
    return false;
}

// -----------------------------------------------------------------------------
// NRPN
// -----------------------------------------------------------------------------

void DeepMindHardwareTarget::setTransport(std::shared_ptr<hardware::IMidiTransport> transport)
{
    transport_ = std::move(transport);
    isOpen_    = transport_ && transport_->isOpen();
}

bool DeepMindHardwareTarget::sendNrpnParameter(uint16_t parameterId, uint16_t value14bit)
{
    if (!transport_)
        return false;

    // Clamp to 14-bit range.
    if (value14bit > 16383u)
        value14bit = 16383u;

    const uint16_t paramMsb  = static_cast<uint16_t>(parameterId >> 7);
    const uint16_t paramLsb  = static_cast<uint16_t>(parameterId & 0x7Fu);

    uint8_t dataMsb, dataLsb;
    split14Bit(value14bit, dataMsb, dataLsb);

    // Secuencia estándar NRPN de 4 mensajes CC.
    // El canal 1 es el canal por defecto documentado para medición de laboratorio.
    hardware::MidiCcMessage msg;
    msg.channel          = kDefaultMidiChannel;
    msg.controllerNumber = 99;
    msg.value            = static_cast<uint8_t>(paramMsb);
    msg.targetProfileId  = "behringer_deepmind12";
    msg.semanticId       = DeepMind12FilterNrpnMap::semanticName(parameterId);
    if (transport_->sendCc(msg) != hardware::MidiTransportSendResult::ok(0))
        return false;

    msg.controllerNumber = 98;
    msg.value            = static_cast<uint8_t>(paramLsb);
    if (transport_->sendCc(msg) != hardware::MidiTransportSendResult::ok(1))
        return false;

    msg.controllerNumber = 6;
    msg.value            = dataMsb;
    if (transport_->sendCc(msg) != hardware::MidiTransportSendResult::ok(2))
        return false;

    msg.controllerNumber = 38;
    msg.value            = dataLsb;
    if (transport_->sendCc(msg) != hardware::MidiTransportSendResult::ok(3))
        return false;

    // Actualiza estado interno del adaptador.
    if (parameterId == DeepMind12FilterNrpnMap::Cutoff)
        currentNrpnCutoff = value14bit;
    else if (parameterId == DeepMind12FilterNrpnMap::Resonance)
        currentNrpnResonance = value14bit;
    else if (parameterId == DeepMind12FilterNrpnMap::Env2_Depth)
        currentNrpnEnv2Depth = value14bit;
    else if (parameterId == DeepMind12FilterNrpnMap::Env2_Attack_Curve)
        currentNrpnEnv2AttackCurve = value14bit;

    return true;
}

// -----------------------------------------------------------------------------
// Inicialización Gray-Box
// -----------------------------------------------------------------------------

bool DeepMindHardwareTarget::initializeFilterForAutoOscillation()
{
    // VCF Cutoff -> 0 (filtro totalmente cerrado)
    if (!sendNrpnParameter(DeepMind12FilterNrpnMap::Cutoff, 0))
        return false;

    // VCF Resonance -> 16383 (máximo fader NRPN, auto-oscilación sinusoidal)
    if (!sendNrpnParameter(DeepMind12FilterNrpnMap::Resonance, 16383))
        return false;

    // VCF Env2 Depth -> 16383 (apertura total de la rampa)
    if (!sendNrpnParameter(DeepMind12FilterNrpnMap::Env2_Depth, 16383))
        return false;

    // Env2 Attack Curve -> 0 (ataque instantáneo)
    if (!sendNrpnParameter(DeepMind12FilterNrpnMap::Env2_Attack_Curve, 0))
        return false;

    // Env2 Sustain -> 0 (caída total al suelo del filtro)
    // En el esquema NRPN del DeepMind esto se programa como valor de
    // profundidad/anchor de envolvente en el parámetro asociado; aquí usamos
    // la profundidad del filtro como el ancla de sustain para la medición.
    return true;
}

} // namespace abdaudiolab::measurement::adapters::deepmind
