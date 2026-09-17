/**
 * @file test_MidiAndSysExContracts_T3.cpp
 * @brief Fase 20.11 T3.1 - Contratos de Estímulo MIDI y Validación SysEx DX7
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <string>
#include <numeric>

#include "synth/MidiExcitationSequence.h"
#include "synth/SysExContracts.h"

using namespace abdaudiolab::synth;
using Catch::Matchers::WithinAbs;

namespace
{

std::vector<uint8_t> createSyntheticDx7Bank(uint8_t fillByte = 0x3F)
{
    std::vector<uint8_t> msg;
    msg.reserve(4104);
    // Header: F0 43 00 09 20 00
    msg.push_back(0xF0);
    msg.push_back(0x43);
    msg.push_back(0x00);
    msg.push_back(0x09);
    msg.push_back(0x20);
    msg.push_back(0x00);

    // 4096 bytes de datos (7 bits)
    for (size_t i = 0; i < 4096; ++i)
    {
        msg.push_back(static_cast<uint8_t>((fillByte + (i % 31)) & 0x7F));
    }

    // Checksum
    uint8_t chk = Dx7SysExValidator::computeDx7Checksum(msg.data() + 6, 4096);
    msg.push_back(chk);
    msg.push_back(0xF7);

    return msg;
}

std::vector<uint8_t> createSyntheticDx7SingleVoice(uint8_t fillByte = 0x4A)
{
    std::vector<uint8_t> msg;
    msg.reserve(163);
    // Header: F0 43 00 00 01 1B
    msg.push_back(0xF0);
    msg.push_back(0x43);
    msg.push_back(0x00);
    msg.push_back(0x00);
    msg.push_back(0x01);
    msg.push_back(0x1B);

    // 155 bytes de datos (7 bits)
    for (size_t i = 0; i < 155; ++i)
    {
        msg.push_back(static_cast<uint8_t>((fillByte + (i % 17)) & 0x7F));
    }

    // Checksum
    uint8_t chk = Dx7SysExValidator::computeDx7Checksum(msg.data() + 6, 155);
    msg.push_back(chk);
    msg.push_back(0xF7);

    return msg;
}

} // namespace

// ==============================================================================
// T3.1: Secuencia de Excitación MIDI Canónica
// ==============================================================================

TEST_CASE("Fase 20.11 T3.1 - MidiExcitationSequence: Round-trip canónico y estabilidad de hash", "[midi][contracts][canonical]")
{
    auto seq1 = MidiExcitationSequence::createCanonicalNoteTrial(
        48000.0, // sampleRate
        1,       // channel
        60,      // note C4
        100,     // velocity
        0.25,    // noteDurationSec (12000 samples)
        0.50,    // releaseTailSec (24000 samples)
        100      // noteOnSampleOffset
    );

    REQUIRE(!seq1.canonicalSha256.empty());
    REQUIRE(seq1.canonicalEvents.size() == 2);
    CHECK(seq1.canonicalEvents[0].sampleOffset == 100);
    CHECK(seq1.canonicalEvents[0].bytes == std::vector<uint8_t>{ 0x90, 60, 100 });
    CHECK(seq1.canonicalEvents[1].sampleOffset == 100 + 12000);
    CHECK(seq1.canonicalEvents[1].bytes == std::vector<uint8_t>{ 0x80, 60, 0 });

    // 1. Serialización a JSON canónico
    std::string jsonStr = seq1.serializeCanonicalJson();
    REQUIRE(!jsonStr.empty());

    // 2. Deserialización
    MidiExcitationSequence seq2;
    std::string err;
    REQUIRE(MidiExcitationSequence::deserializeCanonicalJson(jsonStr, seq2, err));
    REQUIRE(err.empty());

    // 3. Verificación de identidad y estabilidad criptográfica
    CHECK(seq2.sampleRateHz == seq1.sampleRateHz);
    CHECK(seq2.midiChannel == seq1.midiChannel);
    CHECK(seq2.note == seq1.note);
    CHECK(seq2.velocity == seq1.velocity);
    CHECK(seq2.noteDurationSec == seq1.noteDurationSec);
    CHECK(seq2.releaseTailSec == seq1.releaseTailSec);
    CHECK(seq2.canonicalEvents == seq1.canonicalEvents);
    CHECK(seq2.canonicalSha256 == seq1.canonicalSha256);
}

TEST_CASE("Fase 20.11 T3.1 - MidiExcitationSequence: Sensibilidad a un solo byte o muestra", "[midi][contracts][canonical]")
{
    auto baseSeq = MidiExcitationSequence::createCanonicalNoteTrial(
        48000.0, 1, 60, 100, 0.25, 0.50, 0
    );
    std::string baseHash = baseSeq.canonicalSha256;

    // 1. Cambio de velocidad en un byte: 100 -> 101
    auto modifiedByteSeq = baseSeq;
    modifiedByteSeq.canonicalEvents[0].bytes[2] = 101;
    std::string newByteHash = modifiedByteSeq.computeCanonicalSha256();
    CHECK(newByteHash != baseHash);

    // 2. Cambio temporal de una muestra en sampleOffset: 0 -> 1
    auto modifiedOffsetSeq = baseSeq;
    modifiedOffsetSeq.canonicalEvents[0].sampleOffset = 1;
    std::string newOffsetHash = modifiedOffsetSeq.computeCanonicalSha256();
    CHECK(newOffsetHash != baseHash);
    CHECK(newOffsetHash != newByteHash);

    // 3. Cambio de canal: 1 -> 2
    auto modifiedChanSeq = baseSeq;
    modifiedChanSeq.midiChannel = 2;
    std::string newChanHash = modifiedChanSeq.computeCanonicalSha256();
    CHECK(newChanHash != baseHash);
}

// ==============================================================================
// T3.1: SysExArtifact y Validación Estructural DX7
// ==============================================================================

TEST_CASE("Fase 20.11 T3.1 - Dx7SysExValidator: Validación de Banco de 32 Voces (4104 B)", "[sysex][dx7][contracts]")
{
    auto bank = createSyntheticDx7Bank();
    REQUIRE(bank.size() == 4104);

    // 1. Banco nominalmente válido
    auto resValid = Dx7SysExValidator::validateBank32VoiceDump(bank);
    CHECK(resValid.isValid);
    CHECK(resValid.status == SysExValidationStatus::Valid);
    CHECK(resValid.byteCount == 4104);

    // 2. Checksum erróneo
    auto bankBadChecksum = bank;
    bankBadChecksum[4102] ^= 0x01; // Alterar checksum
    auto resBadChk = Dx7SysExValidator::validateBank32VoiceDump(bankBadChecksum);
    CHECK_FALSE(resBadChk.isValid);
    CHECK(resBadChk.status == SysExValidationStatus::ChecksumMismatch);

    // 3. Mensaje truncado
    auto bankTruncated = bank;
    bankTruncated.pop_back(); // 4103 bytes
    auto resTrunc = Dx7SysExValidator::validateBank32VoiceDump(bankTruncated);
    CHECK_FALSE(resTrunc.isValid);
    CHECK(resTrunc.status == SysExValidationStatus::LengthMismatch);

    // 4. Byte de datos con MSB activo (>= 128)
    auto bankBadBit = bank;
    bankBadBit[100] = 0x80; // Invalido en SysEx 7-bit
    auto resBadBit = Dx7SysExValidator::validateBank32VoiceDump(bankBadBit);
    CHECK_FALSE(resBadBit.isValid);
    CHECK(resBadBit.status == SysExValidationStatus::DataByteHighBitSet);

    // 5. Framing inicial o final faltante
    auto bankNoF0 = bank;
    bankNoF0[0] = 0x00;
    auto resNoF0 = Dx7SysExValidator::validateBank32VoiceDump(bankNoF0);
    CHECK_FALSE(resNoF0.isValid);
    CHECK(resNoF0.status == SysExValidationStatus::MissingSysExFraming);

    // 6. Manufacturer ID no es Yamaha
    auto bankNotYamaha = bank;
    bankNotYamaha[1] = 0x41; // Roland
    auto resNotYamaha = Dx7SysExValidator::validateBank32VoiceDump(bankNotYamaha);
    CHECK_FALSE(resNotYamaha.isValid);
    CHECK(resNotYamaha.status == SysExValidationStatus::InvalidManufacturerId);
}

TEST_CASE("Fase 20.11 T3.1 - Dx7SysExValidator: Validación de Single Voice Dump (163 B)", "[sysex][dx7][contracts]")
{
    auto voice = createSyntheticDx7SingleVoice();
    REQUIRE(voice.size() == 163);

    // 1. Voz nominalmente válida
    auto resValid = Dx7SysExValidator::validateSingleVoiceDump(voice);
    CHECK(resValid.isValid);
    CHECK(resValid.status == SysExValidationStatus::Valid);
    CHECK(resValid.byteCount == 163);

    // 2. Fallo de longitud para single voice
    auto voiceBadLen = voice;
    voiceBadLen.push_back(0x00);
    auto resBadLen = Dx7SysExValidator::validateSingleVoiceDump(voiceBadLen);
    CHECK_FALSE(resBadLen.isValid);
    CHECK(resBadLen.status == SysExValidationStatus::LengthMismatch);
}

TEST_CASE("Fase 20.11 T3.1 - SysExArtifact: Fixity SHA-256 e integridad", "[sysex][artifact][contracts]")
{
    auto voice = createSyntheticDx7SingleVoice();
    auto artifact = SysExArtifact::create("dx7_single_voice", voice, "valid");

    CHECK(artifact.format == "dx7_single_voice");
    CHECK(artifact.bytes.size() == 163);
    CHECK(!artifact.sha256.empty());
    CHECK(artifact.verifyFixity());

    // Tampering
    artifact.bytes[10] ^= 0x05;
    CHECK_FALSE(artifact.verifyFixity());
}
