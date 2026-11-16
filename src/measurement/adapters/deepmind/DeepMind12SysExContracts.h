// ==============================================================================
// src/measurement/adapters/deepmind/DeepMind12SysExContracts.h
// Fase 4 — Parámetros NRPN oficiales del filtro VCF del Behringer DeepMind 12 / 12D.
// ==============================================================================

#pragma once

#include <cstdint>
#include <string>

namespace abdaudiolab::measurement::adapters::deepmind
{

/**
 * @brief Mapa de parámetros NRPN estándar usados en la medición de curvas VCF.
 *
 * Los valores coinciden con el mapeo de control continuo documentado para el
 * DeepMind 12/12D y se usan como fuente única de verdad semántica para la
 * inyección de estímulos de la Fase 4.
 */
struct DeepMind12FilterNrpnMap
{
    /// Frecuencia de corte del filtro (VCF Cutoff)
    static constexpr uint16_t Cutoff = 24;

    /// Resonancia / Q del filtro (VCF Resonance)
    static constexpr uint16_t Resonance = 27;

    /// Profundidad de modulación de la envolvente 2 sobre el filtro (Env2 -> VCF Depth)
    static constexpr uint16_t Env2_Depth = 26;

    /// Curva de ataque de la envolvente 2 (Env2 Attack Curve)
    static constexpr uint16_t Env2_Attack_Curve = 58;

    /**
     * @brief Texto semántico ligero asociado a cada identificador de parámetro.
     */
    [[nodiscard]] static std::string semanticName(uint16_t parameterId) noexcept
    {
        switch (parameterId)
        {
            case Cutoff:            return "VCF Cutoff Frequency";
            case Resonance:         return "VCF Resonance (Q)";
            case Env2_Depth:        return "VCF Env2 Modulation Depth";
            case Env2_Attack_Curve: return "Env2 Attack Curve";
            default:                return "Unknown DeepMind12 NRPN Parameter";
        }
    }
};

} // namespace abdaudiolab::measurement::adapters::deepmind
