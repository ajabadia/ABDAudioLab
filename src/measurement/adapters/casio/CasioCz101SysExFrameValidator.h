/**
 * @file CasioCz101SysExFrameValidator.h
 * @brief Profile-driven frame integrity validator for Casio CZ SysEx streams.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "CasioCz101SysExContracts.h"
#include <vector>
#include <string>
#include <optional>

namespace abdaudiolab::measurement::adapters::casio
{

/**
 * @brief Detailed validation report for an incoming SysEx byte buffer.
 */
struct SysExFrameValidationResult
{
    bool valid { false };
    bool isValid { false };                  // ergonomic alias
    std::string status { "unknown" };        // "valid", "unsupported_profile", "invalid_header", "missing_f7_delimiter", "non_midi_safe_byte", "length_mismatch", "checksum_mismatch"
    std::string validationStatus { "unknown" }; // ergonomic alias
    SysExFrameKind detectedKind { SysExFrameKind::Unknown };
    int midiChannel { 0 };                   // 0..15 (0-indexed)
    std::optional<FrameProfile> matchedProfile;
    std::optional<FrameProfile> detectedProfile; // ergonomic alias
    ChecksumStatus checksumStatus { ChecksumStatus::ChecksumNotVerified };
    std::optional<uint8_t> computedChecksum;
    std::optional<uint8_t> receivedChecksum;
    std::string error;
};

/**
 * @brief Validator selecting profile-driven constraints for Casio CZ SysEx messages.
 */
class CasioCz101SysExFrameValidator
{
public:
    /**
     * @brief Gets standard profile for CZ-101 tone dump response.
     */
    [[nodiscard]] static FrameProfile getStandardPatchResponseProfile();

    /**
     * @brief Gets standard profile for CZ-101 tone dump request.
     */
    [[nodiscard]] static FrameProfile getStandardPatchRequestProfile();

    /**
     * @brief Gets standard profile for CZ-101 parameter change.
     */
    [[nodiscard]] static FrameProfile getStandardParameterMessageProfile();

    /**
     * @brief Validates a raw SysEx buffer against registered profiles.
     */
    [[nodiscard]] static SysExFrameValidationResult validateFrame(
        const uint8_t* data,
        size_t size,
        const std::vector<FrameProfile>& customProfiles = {});

    /**
     * @brief Convenience overload accepting std::vector<uint8_t>.
     */
    [[nodiscard]] static SysExFrameValidationResult validateFrame(
        const std::vector<uint8_t>& frame,
        const std::vector<FrameProfile>& customProfiles = {})
    {
        return validateFrame(frame.data(), frame.size(), customProfiles);
    }

    /**
     * @brief Computes standard Casio 7-bit complement checksum over a byte range:
     * (128 - (sum % 128)) % 128
     */
    [[nodiscard]] static uint8_t computeCasioChecksum(const uint8_t* data, size_t count) noexcept;
};

} // namespace abdaudiolab::measurement::adapters::casio
