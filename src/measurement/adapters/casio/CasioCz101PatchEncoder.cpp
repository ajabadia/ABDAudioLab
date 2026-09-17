/**
 * @file CasioCz101PatchEncoder.cpp
 * @brief Implementation of symmetric round-trip patch encoder for Casio CZ-101 SysEx data.
 * @author ABDSynths
 * @date 2026
 */

#include "CasioCz101PatchEncoder.h"
#include "CasioCz101SysExFrameValidator.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::measurement::adapters::casio
{

void CasioCz101PatchEncoder::encodeNibblePair(uint8_t byte, std::vector<uint8_t>& out)
{
    // Casio nibble order: Low nibble first, High nibble second
    out.push_back(byte & 0x0F);
    out.push_back((byte >> 4) & 0x0F);
}

void CasioCz101PatchEncoder::encodeEnvelopeSection(
    const std::array<CzEnvelopeStage, 8>& stages,
    std::vector<uint8_t>& out)
{
    int sustainPoint = 0;
    int endPoint = 7;

    for (int i = 0; i < 8; ++i)
    {
        if (stages[static_cast<size_t>(i)].isSustainPoint)
            sustainPoint = i;
        if (stages[static_cast<size_t>(i)].isEndPoint)
            endPoint = i;
    }

    uint8_t endByte = static_cast<uint8_t>(((sustainPoint & 0x0F) << 4) | (endPoint & 0x0F));
    encodeNibblePair(endByte, out);

    for (int i = 0; i < 8; ++i)
    {
        uint8_t r = static_cast<uint8_t>(std::clamp(stages[static_cast<size_t>(i)].rate, 0, 99));
        uint8_t l = static_cast<uint8_t>(std::clamp(stages[static_cast<size_t>(i)].level, 0, 99));
        encodeNibblePair(r, out);
        encodeNibblePair(l, out);
    }
}

std::vector<uint8_t> CasioCz101PatchEncoder::encodeSysEx(
    const CasioCz101NativePatchState& state,
    int channel)
{
    std::vector<uint8_t> out;
    out.reserve(265);

    // 1. Header: F0 44 00 00 70 20 channel
    out.push_back(0xF0);
    out.push_back(0x44);
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x70); // CZ-101 Model ID
    out.push_back(0x20); // Patch dump response
    out.push_back(static_cast<uint8_t>(channel & 0x0F));

    // 2. PFLAG (Line select & Octave)
    uint8_t octaveRaw = 0;
    if (state.octave == 1) octaveRaw = 1;
    else if (state.octave == -1) octaveRaw = 2;

    uint8_t pflag = static_cast<uint8_t>(((octaveRaw & 0x03) << 2) | (state.lineSelect & 0x03));
    encodeNibblePair(pflag, out);

    // 3. Detune (PDS, PDL, PDH)
    double detune = state.detuneCents;
    uint8_t pds = (detune < 0.0) ? 0x01 : 0x00;
    detune = std::abs(detune);

    uint8_t pdh = static_cast<uint8_t>(std::clamp(static_cast<int>(detune / 100.0), 0, 127));
    double remCents = detune - (pdh * 100.0);
    int fineUnits = static_cast<int>(std::round((remCents / 100.0) * 60.0));
    // Re-insert gaps for PDL scale
    uint8_t pdl = static_cast<uint8_t>(fineUnits + (fineUnits / 15));

    encodeNibblePair(pds, out);
    encodeNibblePair(pdl, out);
    encodeNibblePair(pdh, out);

    // 4. Vibrato (PVK, PVD, RV, DV)
    uint8_t pvk = 0x08; // Triangle default
    if (state.vibratoWave == 1) pvk = 0x04;      // Saw up
    else if (state.vibratoWave == 2) pvk = 0x20; // Saw down
    else if (state.vibratoWave == 3) pvk = 0x02; // Square
    encodeNibblePair(pvk, out);

    encodeNibblePair(static_cast<uint8_t>(std::clamp(state.vibratoDelay, 0, 99)), out);
    encodeNibblePair(0, out);
    encodeNibblePair(0, out);

    encodeNibblePair(static_cast<uint8_t>(std::clamp(state.vibratoRate, 0, 99)), out);
    encodeNibblePair(0, out);
    encodeNibblePair(0, out);

    encodeNibblePair(static_cast<uint8_t>(std::clamp(state.vibratoDepth, 0, 99)), out);
    encodeNibblePair(0, out);
    encodeNibblePair(0, out);

    // 5. Line 1 Waveforms & Modulations
    uint8_t w1 = static_cast<uint8_t>(state.dco1Wave1) & 0x07;
    uint8_t w2 = state.dco1Wave2.has_value() ? (static_cast<uint8_t>(*state.dco1Wave2) & 0x07) : 0;
    uint8_t w2En = state.dco1Wave2.has_value() ? 0x10 : 0x00;
    uint8_t mfw1 = static_cast<uint8_t>((w1 << 5) | w2En | (w2 << 1));

    uint8_t mod = 0;
    if (state.ringMod) mod = 4;
    else if (state.noiseMod) mod = 3;
    uint8_t mfw1_2 = static_cast<uint8_t>((mod << 3) & 0x38);

    encodeNibblePair(mfw1, out);
    encodeNibblePair(mfw1_2, out);

    // 6. Line 1 Key Follow
    encodeNibblePair(static_cast<uint8_t>(state.dca1KeyFollow & 0x0F), out);
    encodeNibblePair(0, out);
    encodeNibblePair(static_cast<uint8_t>(state.dcw1KeyFollow & 0x0F), out);
    encodeNibblePair(0, out);

    // 7. Line 1 Envelopes: DCA1, DCW1, DCO1 Pitch
    encodeEnvelopeSection(state.dca1Amplitude, out);
    encodeEnvelopeSection(state.dcw1Timbre, out);
    encodeEnvelopeSection(state.dco1Pitch, out);

    // 8. Line 2 Waveforms
    uint8_t w1_2 = static_cast<uint8_t>(state.dco2Wave1) & 0x07;
    uint8_t w2_2 = state.dco2Wave2.has_value() ? (static_cast<uint8_t>(*state.dco2Wave2) & 0x07) : 0;
    uint8_t w2En_2 = state.dco2Wave2.has_value() ? 0x10 : 0x00;
    uint8_t mfw2 = static_cast<uint8_t>((w1_2 << 5) | w2En_2 | (w2_2 << 1));

    encodeNibblePair(mfw2, out);
    encodeNibblePair(0, out);

    // 9. Line 2 Key Follow
    encodeNibblePair(static_cast<uint8_t>(state.dca2KeyFollow & 0x0F), out);
    encodeNibblePair(0, out);
    encodeNibblePair(static_cast<uint8_t>(state.dcw2KeyFollow & 0x0F), out);
    encodeNibblePair(0, out);

    // 10. Line 2 Envelopes: DCA2, DCW2, DCO2 Pitch
    encodeEnvelopeSection(state.dca2Amplitude, out);
    encodeEnvelopeSection(state.dcw2Timbre, out);
    encodeEnvelopeSection(state.dco2Pitch, out);

    // 11. Pad payload with name or zeroes up to exact 263 bytes (before checksum + F7 = 265 total)
    const size_t targetPayloadLength = 256; // 128 bytes in nibble pairs
    while ((out.size() - 7) < targetPayloadLength)
    {
        encodeNibblePair(0, out);
    }

    // 12. Checksum calculation over payload (from byte 6 to end of payload)
    const size_t ckStart = 6;
    const size_t ckLen = out.size() - ckStart;
    uint8_t ck = CasioCz101SysExFrameValidator::computeCasioChecksum(out.data() + ckStart, ckLen);

    out.push_back(ck);
    out.push_back(0xF7);

    return out;
}

} // namespace abdaudiolab::measurement::adapters::casio
