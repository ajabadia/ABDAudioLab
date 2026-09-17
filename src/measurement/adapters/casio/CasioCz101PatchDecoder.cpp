/**
 * @file CasioCz101PatchDecoder.cpp
 * @brief Implementation of decode-only native patch parser for Casio CZ-101 SysEx tone dumps.
 * @author ABDSynths
 * @date 2026
 */

#include "CasioCz101PatchDecoder.h"
#include "../../../synth/Sha256.h"
#include <algorithm>

namespace abdaudiolab::measurement::adapters::casio
{

uint8_t CasioCz101PatchDecoder::decodeNibblePair(const uint8_t* payload, size_t& offset, size_t maxSize) noexcept
{
    if (payload == nullptr || offset + 2 > maxSize)
    {
        offset = maxSize;
        return 0;
    }
    uint8_t lowNibble = payload[offset++] & 0x0F;
    uint8_t highNibble = payload[offset++] & 0x0F;
    return static_cast<uint8_t>((highNibble << 4) | lowNibble);
}

void CasioCz101PatchDecoder::decodeEnvelopeSection(
    const uint8_t* msg,
    size_t& offset,
    size_t maxSize,
    std::array<CzEnvelopeStage, 8>& stages)
{
    // Byte 0 of envelope block contains sustain point in MSN and end point in LSN
    uint8_t endByte = decodeNibblePair(msg, offset, maxSize);
    int sustainPoint = (endByte >> 4) & 0x0F; // 0..7
    int endPoint = endByte & 0x0F;            // 0..7

    for (int i = 0; i < 8; ++i)
    {
        uint8_t rawRate = decodeNibblePair(msg, offset, maxSize);
        uint8_t rawLevel = decodeNibblePair(msg, offset, maxSize);

        stages[static_cast<size_t>(i)].stageIndex = i + 1;
        stages[static_cast<size_t>(i)].rate = std::min(static_cast<int>(rawRate), 99);
        stages[static_cast<size_t>(i)].level = std::min(static_cast<int>(rawLevel), 99);
        stages[static_cast<size_t>(i)].rateEncoding = "cz_raw_0_99";
        stages[static_cast<size_t>(i)].levelEncoding = "cz_raw_0_99";
        stages[static_cast<size_t>(i)].isSustainPoint = (i == sustainPoint);
        stages[static_cast<size_t>(i)].isEndPoint = (i == endPoint);
    }
}

CasioCz101DecodeResult CasioCz101PatchDecoder::decodeSysEx(const uint8_t* data, size_t size)
{
    CasioCz101DecodeResult result;
    auto finish = [](CasioCz101DecodeResult r) {
        r.isSuccess = r.success;
        r.errorMessage = r.error;
        return r;
    };

    if (data == nullptr || size == 0)
    {
        result.success = false;
        result.error = "buffer_too_short_or_null";
        return finish(result);
    }

    // 1. Fixity hash of source raw SysEx
    result.sourceSysExSha256 = abdaudiolab::synth::Sha256::computeHex(data, size);

    // 2. Validate frame integrity against profile catalog
    auto valRes = CasioCz101SysExFrameValidator::validateFrame(data, size);
    if (!valRes.valid)
    {
        result.success = false;
        result.error = "validation_failed: " + valRes.status + " (" + valRes.error + ")";
        return finish(result);
    }

    if (valRes.detectedKind != SysExFrameKind::PatchResponse)
    {
        result.success = false;
        result.error = "unsupported_frame_kind_for_tone_decode: " + sysExFrameKindToString(valRes.detectedKind);
        return finish(result);
    }

    // Tone payload starts at index 7, ends before checksum (size - 2)
    size_t offset = 7;
    const size_t maxPayloadSize = (size >= 2) ? (size - 2) : 0;

    CasioCz101NativePatchState& state = result.patchState;
    state.sourceSysExSha256 = result.sourceSysExSha256;
    state.modelIdentifier = "CZ-101";
    state.decoderVersion = "1.0.0";

    // 1. PFLAG: Line Select & Octave
    uint8_t pflag = decodeNibblePair(data, offset, maxPayloadSize);
    state.lineSelect = pflag & 0x03;
    uint8_t octaveRaw = (pflag >> 2) & 0x03;
    if (octaveRaw == 1) state.octave = 1;
    else if (octaveRaw == 2) state.octave = -1;
    else state.octave = 0;

    // 2. Detune: PDS (sign), PDL (fine), PDH (semitones)
    uint8_t pds = decodeNibblePair(data, offset, maxPayloadSize);
    uint8_t pdl = decodeNibblePair(data, offset, maxPayloadSize);
    uint8_t pdh = decodeNibblePair(data, offset, maxPayloadSize);

    // Casio PDL fine-tune scale with 0x10 gaps: invert gaps
    int fineUnits = static_cast<int>(pdl) - (static_cast<int>(pdl) / 16);
    double fineCents = (static_cast<double>(fineUnits) / 60.0) * 100.0;
    double detune = (static_cast<double>(pdh) * 100.0) + fineCents;
    if (pds & 0x01) detune = -detune;
    state.detuneCents = detune;

    // 3. Vibrato Waveform, Delay, Rate, Depth
    uint8_t pvk = decodeNibblePair(data, offset, maxPayloadSize);
    if (pvk == 0x08) state.vibratoWave = 0;      // Triangle
    else if (pvk == 0x04) state.vibratoWave = 1; // Saw up
    else if (pvk == 0x20) state.vibratoWave = 2; // Saw down
    else if (pvk == 0x02) state.vibratoWave = 3; // Square
    else state.vibratoWave = 0;

    uint8_t pvd1 = decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    state.vibratoDelay = std::min(static_cast<int>(pvd1), 99);

    uint8_t rv1 = decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    state.vibratoRate = std::min(static_cast<int>(rv1), 99);

    uint8_t dv1 = decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize);
    state.vibratoDepth = std::min(static_cast<int>(dv1), 99);

    // 4. Line 1 Waveforms & Modulation
    uint8_t mfw1 = decodeNibblePair(data, offset, maxPayloadSize);
    uint8_t mfw1_2 = decodeNibblePair(data, offset, maxPayloadSize);
    uint8_t w1 = (mfw1 >> 5) & 0x07;
    state.dco1Wave1 = static_cast<CzWaveform>(w1);

    bool w2En = (mfw1 & 0x10) != 0;
    if (w2En)
        state.dco1Wave2 = static_cast<CzWaveform>((mfw1 >> 1) & 0x07);
    else
        state.dco1Wave2 = std::nullopt;

    uint8_t mod = (mfw1_2 >> 3) & 0x07;
    state.ringMod = (mod == 4 || mod == 5 || mod == 2 || mod == 6);
    state.noiseMod = (mod == 3 || mod == 7);

    // 5. Line 1 Key Follow
    state.dca1KeyFollow = static_cast<int>(decodeNibblePair(data, offset, maxPayloadSize) & 0x0F);
    decodeNibblePair(data, offset, maxPayloadSize); // skip MAMV lookup
    state.dcw1KeyFollow = static_cast<int>(decodeNibblePair(data, offset, maxPayloadSize) & 0x0F);
    decodeNibblePair(data, offset, maxPayloadSize); // skip MWMV lookup

    // 6. Line 1 Envelopes: DCA1, DCW1, DCO1 Pitch
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dca1Amplitude);
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dcw1Timbre);
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dco1Pitch);

    // 7. Line 2 Waveforms
    uint8_t mfw2 = decodeNibblePair(data, offset, maxPayloadSize);
    decodeNibblePair(data, offset, maxPayloadSize); // mfw2_2
    uint8_t w1_2 = (mfw2 >> 5) & 0x07;
    state.dco2Wave1 = static_cast<CzWaveform>(w1_2);

    bool w2En_2 = (mfw2 & 0x10) != 0;
    if (w2En_2)
        state.dco2Wave2 = static_cast<CzWaveform>((mfw2 >> 1) & 0x07);
    else
        state.dco2Wave2 = std::nullopt;

    // 8. Line 2 Key Follow
    state.dca2KeyFollow = static_cast<int>(decodeNibblePair(data, offset, maxPayloadSize) & 0x0F);
    decodeNibblePair(data, offset, maxPayloadSize); // skip SAMV lookup
    state.dcw2KeyFollow = static_cast<int>(decodeNibblePair(data, offset, maxPayloadSize) & 0x0F);
    decodeNibblePair(data, offset, maxPayloadSize); // skip SWMV lookup

    // 9. Line 2 Envelopes: DCA2, DCW2, DCO2 Pitch
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dca2Amplitude);
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dcw2Timbre);
    decodeEnvelopeSection(data, offset, maxPayloadSize, state.dco2Pitch);

    // 10. Optional Name field (if additional nibble pairs exist before checksum)
    if (offset + 16 <= maxPayloadSize)
    {
        std::string parsedName;
        for (int i = 0; i < 8 && offset + 2 <= maxPayloadSize; ++i)
        {
            uint8_t ch = decodeNibblePair(data, offset, maxPayloadSize);
            if (ch >= 32 && ch <= 126)
                parsedName += static_cast<char>(ch);
        }
        while (!parsedName.empty() && (parsedName.back() == ' ' || parsedName.back() == '\0'))
            parsedName.pop_back();
        if (!parsedName.empty())
            state.patchName = parsedName;
    }

    result.success = true;
    return finish(result);
}

} // namespace abdaudiolab::measurement::adapters::casio
