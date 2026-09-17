/**
 * @file CasioCz101PatchDecoder.h
 * @brief Decode-only native patch parser for Casio CZ-101 SysEx tone dumps.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "CasioCz101SysExContracts.h"
#include "CasioCz101SysExFrameValidator.h"
#include <vector>
#include <string>

namespace abdaudiolab::measurement::adapters::casio
{

/**
 * @brief Structured result of decoding a raw SysEx patch dump.
 */
struct CasioCz101DecodeResult
{
    bool success { false };
    bool isSuccess { false };                // ergonomic alias
    CasioCz101NativePatchState patchState;
    std::string error;
    std::string errorMessage;                // ergonomic alias
    std::string sourceSysExSha256;
};

/**
 * @brief Decode-only parser for Casio CZ-101 tone dumps.
 * Zero MIDI output or transmission capabilities.
 */
class CasioCz101PatchDecoder
{
public:
    /**
     * @brief Decodes a raw SysEx tone dump buffer into CasioCz101NativePatchState.
     * Automatically verifies frame integrity using CasioCz101SysExFrameValidator first.
     */
    [[nodiscard]] static CasioCz101DecodeResult decodeSysEx(const uint8_t* data, size_t size);

    /**
     * @brief Convenience overload for vector buffer.
     */
    [[nodiscard]] static CasioCz101DecodeResult decodeSysEx(const std::vector<uint8_t>& frame)
    {
        return decodeSysEx(frame.data(), frame.size());
    }

    /**
     * @brief Convenience aliases matching decodePatch semantics.
     */
    [[nodiscard]] static CasioCz101DecodeResult decodePatch(const uint8_t* data, size_t size)
    {
        return decodeSysEx(data, size);
    }

    [[nodiscard]] static CasioCz101DecodeResult decodePatch(const std::vector<uint8_t>& frame)
    {
        return decodeSysEx(frame.data(), frame.size());
    }

    /**
     * @brief Deserializes a nibble pair (Low nibble first, High nibble second) into a byte.
     */
    static uint8_t decodeNibblePair(const uint8_t* payload, size_t& offset, size_t maxSize) noexcept;

    /**
     * @brief Decodes an 8-stage Casio envelope section from the tone payload.
     */
    static void decodeEnvelopeSection(
        const uint8_t* msg,
        size_t& offset,
        size_t maxSize,
        std::array<CzEnvelopeStage, 8>& stages);
};

} // namespace abdaudiolab::measurement::adapters::casio
