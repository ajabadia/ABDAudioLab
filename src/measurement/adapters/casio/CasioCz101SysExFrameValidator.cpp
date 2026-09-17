/**
 * @file CasioCz101SysExFrameValidator.cpp
 * @brief Implementation of profile-driven Casio CZ SysEx frame integrity validator.
 * @author ABDSynths
 * @date 2026
 */

#include "CasioCz101SysExFrameValidator.h"
#include <numeric>

namespace abdaudiolab::measurement::adapters::casio
{

FrameProfile CasioCz101SysExFrameValidator::getStandardPatchResponseProfile()
{
    FrameProfile p;
    p.modelIdentifier = "CZ-101";
    p.kind = SysExFrameKind::PatchResponse;
    p.minLength = 260;
    p.exactLength = 265;
    p.checksumPolicy = ChecksumPolicy::CasioComplement7Bit;
    p.description = "Casio CZ-101 256-nibble patch dump response with 7-bit checksum";
    return p;
}

FrameProfile CasioCz101SysExFrameValidator::getStandardPatchRequestProfile()
{
    FrameProfile p;
    p.modelIdentifier = "CZ-101";
    p.kind = SysExFrameKind::PatchRequest;
    p.minLength = 8;
    p.exactLength = 8;
    p.checksumPolicy = ChecksumPolicy::None;
    p.description = "Casio CZ-101 single tone dump request";
    return p;
}

FrameProfile CasioCz101SysExFrameValidator::getStandardParameterMessageProfile()
{
    FrameProfile p;
    p.modelIdentifier = "CZ-101";
    p.kind = SysExFrameKind::ParameterMessage;
    p.minLength = 9;
    p.exactLength = 9;
    p.checksumPolicy = ChecksumPolicy::None;
    p.description = "Casio CZ parameter change message";
    return p;
}

uint8_t CasioCz101SysExFrameValidator::computeCasioChecksum(const uint8_t* data, size_t count) noexcept
{
    if (data == nullptr || count == 0)
        return 0;

    uint32_t sum = 0;
    for (size_t i = 0; i < count; ++i)
        sum += static_cast<uint32_t>(data[i] & 0x7F);

    return static_cast<uint8_t>((128 - (sum % 128)) % 128);
}

SysExFrameValidationResult CasioCz101SysExFrameValidator::validateFrame(
    const uint8_t* data,
    size_t size,
    const std::vector<FrameProfile>& customProfiles)
{
    SysExFrameValidationResult res;

    auto finish = [](SysExFrameValidationResult r) {
        r.isValid = r.valid;
        r.validationStatus = r.status;
        r.detectedProfile = r.matchedProfile;
        return r;
    };

    if (data == nullptr || size < 4)
    {
        res.valid = false;
        res.status = "invalid_header";
        res.error = "buffer_null_or_too_small";
        return finish(res);
    }

    // 1. Initial delimiter
    if (data[0] != 0xF0)
    {
        res.valid = false;
        res.status = "missing_start_delimiter";
        res.error = "missing_f0_start_delimiter";
        return finish(res);
    }

    // 2. Final delimiter
    if (data[size - 1] != 0xF7)
    {
        res.valid = false;
        res.status = "missing_end_delimiter";
        res.error = "missing_or_corrupt_f7_terminator";
        return finish(res);
    }

    // 3. MIDI-Safe range check (< 0x80 inside payload)
    for (size_t i = 1; i < size - 1; ++i)
    {
        if (data[i] >= 0x80)
        {
            res.valid = false;
            res.status = "invalid_byte_value";
            res.error = "payload_contains_byte_with_msb_set_at_index_" + std::to_string(i);
            return finish(res);
        }
    }

    // 4. Manufacturer ID check (Casio = 0x44)
    if (data[1] != 0x44)
    {
        res.valid = false;
        res.status = "unsupported_profile";
        res.error = "unsupported_manufacturer_id_not_casio";
        return finish(res);
    }

    // 5. Build candidate profile list
    std::vector<FrameProfile> candidateProfiles = customProfiles;
    if (candidateProfiles.empty())
    {
        candidateProfiles.push_back(getStandardPatchResponseProfile());
        candidateProfiles.push_back(getStandardPatchRequestProfile());
        candidateProfiles.push_back(getStandardParameterMessageProfile());
    }

    // 6. Discriminate frame kind & model from bytes
    SysExFrameKind detectedKind = SysExFrameKind::Unknown;
    std::string detectedModel;
    int detectedChannel = 0;

    if (size >= 8 && data[1] == 0x44 && data[2] == 0x00 && data[3] == 0x00)
    {
        uint8_t modelByte = data[4];
        uint8_t cmdByte = data[5];
        detectedChannel = data[6] & 0x0F;

        if (modelByte == 0x70 || modelByte == 0x12)
            detectedModel = "CZ-101";

        if (cmdByte == 0x00 || cmdByte == 0x10)
        {
            detectedKind = SysExFrameKind::PatchRequest;
        }
        else if (cmdByte == 0x20 || (cmdByte == 0x00 && size > 200))
        {
            detectedKind = SysExFrameKind::PatchResponse;
        }
    }
    else if (size == 9 && data[1] == 0x44 && data[2] == 0x00)
    {
        detectedChannel = data[3] & 0x0F;
        detectedKind = SysExFrameKind::ParameterMessage;
        detectedModel = "CZ-101";
    }

    res.detectedKind = detectedKind;
    res.midiChannel = detectedChannel;

    if (detectedKind == SysExFrameKind::Unknown || detectedModel.empty())
    {
        res.valid = false;
        res.status = "unsupported_profile";
        res.error = "no_matching_profile_for_frame_header_or_model";
        return finish(res);
    }

    // 7. Find profile match
    const FrameProfile* matched = nullptr;
    for (const auto& prof : candidateProfiles)
    {
        if (prof.kind == detectedKind && prof.modelIdentifier == detectedModel)
        {
            matched = &prof;
            break;
        }
    }

    if (matched == nullptr)
    {
        res.valid = false;
        res.status = "unsupported_profile";
        res.error = "frame_kind_recognized_but_unsupported_by_registered_profiles";
        return finish(res);
    }

    res.matchedProfile = *matched;

    // 8. Length validation against matched profile
    if (matched->exactLength.has_value() && size != *matched->exactLength)
    {
        res.valid = false;
        res.status = "length_mismatch";
        res.error = "expected_length_" + std::to_string(*matched->exactLength) + "_but_got_" + std::to_string(size);
        return finish(res);
    }

    if (size < matched->minLength)
    {
        res.valid = false;
        res.status = "length_mismatch";
        res.error = "frame_shorter_than_minimum_profile_length_" + std::to_string(matched->minLength);
        return finish(res);
    }

    // 9. Checksum validation according to policy
    if (matched->checksumPolicy == ChecksumPolicy::CasioComplement7Bit)
    {
        if (size < 8)
        {
            res.valid = false;
            res.status = "length_mismatch";
            res.error = "frame_too_short_for_checksum_calculation";
            return finish(res);
        }

        const uint8_t receivedCk = data[size - 2];
        const size_t ckPayloadStart = 6;
        const size_t ckPayloadLen = (size - 2) - ckPayloadStart;

        const uint8_t computedCk = computeCasioChecksum(data + ckPayloadStart, ckPayloadLen);

        res.receivedChecksum = receivedCk;
        res.computedChecksum = computedCk;

        if (receivedCk == computedCk)
        {
            res.checksumStatus = ChecksumStatus::ChecksumValid;
        }
        else
        {
            res.checksumStatus = ChecksumStatus::ChecksumInvalid;
            res.valid = false;
            res.status = "checksum_error";
            res.error = "checksum_error: received " + std::to_string(receivedCk) + ", computed " + std::to_string(computedCk);
            return finish(res);
        }
    }
    else if (matched->checksumPolicy == ChecksumPolicy::None)
    {
        res.checksumStatus = ChecksumStatus::ChecksumNotPresent;
    }
    else
    {
        res.checksumStatus = ChecksumStatus::ChecksumNotVerified;
    }

    res.valid = true;
    res.status = "valid";
    return finish(res);
}

} // namespace abdaudiolab::measurement::adapters::casio
