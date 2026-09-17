/**
 * @file CasioCz101PatchEncoder.h
 * @brief Symmetric round-trip patch encoder for Casio CZ-101 SysEx data.
 * @author ABDSynths
 * @date 2026
 *
 * Dedicated strictly to offline round-trip verification, tests, and dry-run validation.
 * Does NOT contain any MIDI port transmission API.
 */

#pragma once

#include "CasioCz101SysExContracts.h"
#include <vector>
#include <cstdint>

namespace abdaudiolab::measurement::adapters::casio
{

/**
 * @brief Serializer encoding CasioCz101NativePatchState into SysEx byte buffer.
 */
class CasioCz101PatchEncoder
{
public:
    /**
     * @brief Encodes native tone state to standard Casio CZ-101 tone dump SysEx frame.
     */
    [[nodiscard]] static std::vector<uint8_t> encodeSysEx(
        const CasioCz101NativePatchState& state,
        int channel = 0);

    /**
     * @brief Alias matching CasioCz101PatchDecoder::decodePatch.
     */
    [[nodiscard]] static std::vector<uint8_t> encodePatch(
        const CasioCz101NativePatchState& state,
        int channel = 0)
    {
        return encodeSysEx(state, channel);
    }

    /**
     * @brief Appends low-nibble then high-nibble of a byte to output buffer.
     */
    static void encodeNibblePair(uint8_t byte, std::vector<uint8_t>& out);

    /**
     * @brief Encodes an 8-stage Casio envelope section to the nibble stream.
     */
    static void encodeEnvelopeSection(
        const std::array<CzEnvelopeStage, 8>& stages,
        std::vector<uint8_t>& out);
};

} // namespace abdaudiolab::measurement::adapters::casio
