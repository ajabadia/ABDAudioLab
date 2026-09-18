/**
 * @file CasioCz101SysExContracts.h
 * @brief Native SysEx data structures, frame profiles, and observable bindings for Casio CZ-101.
 * @author ABDSynths
 * @date 2026
 *
 * Implements aseptic, decode-only native contracts for the Casio CZ-101 target adapter,
 * cleanly separated from generic core measurement contracts.
 */

#pragma once

#include "../../ComplexEnvelopeContracts.h"
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cstdint>

namespace abdaudiolab::measurement::adapters::casio
{

/**
 * @brief Kind of SysEx frame within Casio CZ protocol.
 */
enum class SysExFrameKind
{
    PatchRequest,     /**< F0 44 00 00 modelId 0x00/0x10 channel F7 */
    PatchResponse,    /**< F0 44 00 00 modelId 0x20 channel [nibbles...] checksum F7 */
    ParameterMessage, /**< F0 44 00 channel opcodeMSB opcodeLSB msn lsn F7 */
    Unknown
};

[[nodiscard]] inline std::string sysExFrameKindToString(SysExFrameKind k) noexcept
{
    switch (k)
    {
        case SysExFrameKind::PatchRequest:     return "PatchRequest";
        case SysExFrameKind::PatchResponse:    return "PatchResponse";
        case SysExFrameKind::ParameterMessage: return "ParameterMessage";
        case SysExFrameKind::Unknown:          return "Unknown";
    }
    return "Unknown";
}

/**
 * @brief Policy used to compute or verify checksums on a frame profile.
 */
enum class ChecksumPolicy
{
    None,                /**< Frame has no checksum byte (e.g. parameter changes, requests) */
    CasioComplement7Bit, /**< Standard Casio 7-bit checksum over payload: (128 - (sum % 128)) % 128 */
    ProfileSpecific,     /**< Non-standard or vendor-extended checksum rule */
    Unknown
};

/**
 * @brief Result of checksum validation.
 */
enum class ChecksumStatus
{
    ChecksumValid,
    ChecksumInvalid,
    ChecksumNotPresent,
    ChecksumNotVerified
};

[[nodiscard]] inline std::string checksumStatusToString(ChecksumStatus s) noexcept
{
    switch (s)
    {
        case ChecksumStatus::ChecksumValid:       return "checksum_valid";
        case ChecksumStatus::ChecksumInvalid:     return "checksum_invalid";
        case ChecksumStatus::ChecksumNotPresent:   return "checksum_not_present";
        case ChecksumStatus::ChecksumNotVerified: return "checksum_not_verified";
    }
    return "checksum_not_verified";
}

/**
 * @brief Profile defining expected frame constraints for a specific Casio model and message kind.
 */
struct FrameProfile
{
    std::string modelIdentifier { "CZ-101" };
    SysExFrameKind kind { SysExFrameKind::Unknown };
    size_t minLength { 8 };
    std::optional<size_t> exactLength;
    ChecksumPolicy checksumPolicy { ChecksumPolicy::None };
    std::string description;
};

/**
 * @brief Native 8 waveforms of the Casio CZ series.
 */
enum class CzWaveform : uint8_t
{
    Sawtooth   = 0,
    Square     = 1,
    Pulse      = 2,
    DoubleSine = 3,
    SawPulse   = 4,
    Reso1      = 5,
    Reso2      = 6,
    Reso3      = 7
};

[[nodiscard]] inline std::string czWaveformToString(CzWaveform w) noexcept
{
    switch (w)
    {
        case CzWaveform::Sawtooth:   return "Sawtooth";
        case CzWaveform::Square:     return "Square";
        case CzWaveform::Pulse:      return "Pulse";
        case CzWaveform::DoubleSine: return "DoubleSine";
        case CzWaveform::SawPulse:   return "SawPulse";
        case CzWaveform::Reso1:      return "Reso1";
        case CzWaveform::Reso2:      return "Reso2";
        case CzWaveform::Reso3:      return "Reso3";
    }
    return "Unknown";
}

/**
 * @brief Single stage within an 8-stage Casio envelope (Rate 0..99, Level 0..99).
 */
struct CzEnvelopeStage
{
    int stageIndex { 1 };                    // 1..8
    int rate { 0 };                          // 0..99 native
    int level { 0 };                         // 0..99 native
    std::string rateEncoding { "cz_raw_0_99" };
    std::string levelEncoding { "cz_raw_0_99" };
    bool isSustainPoint { false };
    bool isEndPoint { false };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["isEndPoint"] = isEndPoint;
        j["isSustainPoint"] = isSustainPoint;
        j["level"] = level;
        j["levelEncoding"] = levelEncoding;
        j["rate"] = rate;
        j["rateEncoding"] = rateEncoding;
        j["stageIndex"] = stageIndex;
        return j;
    }
};

/**
 * @brief Complete decoded native tone state for Casio CZ-101.
 */
struct CasioCz101NativePatchState
{
    std::string patchName { "Init Voice" };
    int lineSelect { 0 };                    // 0: 1, 1: 2, 2: 1+1', 3: 1+2'
    int octave { 0 };                        // -1, 0, +1
    double detuneCents { 0.0 };              // Signed detune in cents

    // Vibrato
    int vibratoWave { 0 };                   // 0: Triangle, 1: Saw Up, 2: Saw Down, 3: Square
    int vibratoDelay { 0 };                  // 0..99
    int vibratoRate { 0 };                   // 0..99
    int vibratoDepth { 0 };                  // 0..99

    // Oscillators & Modulations
    CzWaveform dco1Wave1 { CzWaveform::Sawtooth };
    std::optional<CzWaveform> dco1Wave2;
    CzWaveform dco2Wave1 { CzWaveform::Sawtooth };
    std::optional<CzWaveform> dco2Wave2;

    bool ringMod { false };
    bool noiseMod { false };

    // Key Follow
    int dca1KeyFollow { 0 };                 // 0..9
    int dcw1KeyFollow { 0 };                 // 0..9
    int dca2KeyFollow { 0 };                 // 0..9
    int dcw2KeyFollow { 0 };                 // 0..9

    // 6 Envelopes of 8 stages each
    std::array<CzEnvelopeStage, 8> dco1Pitch;
    std::array<CzEnvelopeStage, 8> dcw1Timbre;
    std::array<CzEnvelopeStage, 8> dca1Amplitude;

    std::array<CzEnvelopeStage, 8> dco2Pitch;
    std::array<CzEnvelopeStage, 8> dcw2Timbre;
    std::array<CzEnvelopeStage, 8> dca2Amplitude;

    // Provenance & Integrity
    std::string sourceSysExSha256;
    std::string modelIdentifier { "CZ-101" };
    std::string decoderVersion { "1.0.0" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["dca1KeyFollow"] = dca1KeyFollow;
        j["dca2KeyFollow"] = dca2KeyFollow;
        j["dcw1KeyFollow"] = dcw1KeyFollow;
        j["dcw2KeyFollow"] = dcw2KeyFollow;
        j["decoderVersion"] = decoderVersion;
        j["detuneCents"] = detuneCents;
        j["lineSelect"] = lineSelect;
        j["modelIdentifier"] = modelIdentifier;
        j["noiseMod"] = noiseMod;
        j["octave"] = octave;
        j["patchName"] = patchName;
        j["ringMod"] = ringMod;
        j["sourceSysExSha256"] = sourceSysExSha256;
        j["vibratoDelay"] = vibratoDelay;
        j["vibratoDepth"] = vibratoDepth;
        j["vibratoRate"] = vibratoRate;
        j["vibratoWave"] = vibratoWave;

        auto stagesToJson = [](const std::array<CzEnvelopeStage, 8>& arr) {
            nlohmann::ordered_json a = nlohmann::ordered_json::array();
            for (const auto& s : arr)
                a.push_back(s.toCanonicalJson());
            return a;
        };

        j["dca1Amplitude"] = stagesToJson(dca1Amplitude);
        j["dca2Amplitude"] = stagesToJson(dca2Amplitude);
        j["dco1Pitch"] = stagesToJson(dco1Pitch);
        j["dco2Pitch"] = stagesToJson(dco2Pitch);
        j["dcw1Timbre"] = stagesToJson(dcw1Timbre);
        j["dcw2Timbre"] = stagesToJson(dcw2Timbre);

        return j;
    }
};

/**
 * @brief Input context for mapping native DCW parameters to TimbreObservable.
 */
struct CasioCzDcwMappingInput
{
    int lineIndex { 1 };                     // 1 or 2
    CzWaveform waveform { CzWaveform::Sawtooth };
    std::array<CzEnvelopeStage, 8> stages;

    std::optional<int> sustainStage;         // 1..8
    std::optional<int> endStage;             // 1..8

    std::optional<int> dcwKeyFollow;         // 0..9
    std::optional<int> dcwInitialLevel;
    std::optional<int> envelopeAmount;

    std::string modelIdentifier { "CZ-101" };
    std::string sourceSysExSha256;
};

/**
 * @brief Result of mapping native DCW parameters to TimbreObservable.
 */
struct CasioCzDcwMappingResult
{
    EnvelopeTrajectory observedDomain;
    std::vector<EnvelopeStageDescriptor> inferredStages;

    std::string nativeParameterPath { "line1.dcw.envelope" };
    std::string mappingStatus { "mapped" };  // "mapped", "partial", "unsupported", "invalid"

    std::string phaseDistortionProxy { "not_claimed" };
    std::string nativeEnvelopeReconstruction { "not_claimed" };

    std::string sourceSysExSha256;
};

using abdaudiolab::measurement::NativeEnvelopeBinding;

/**
 * @brief Comparison result between native intent and observed acoustic trajectory.
 */
struct NativeObservableComparison
{
    std::string observableDomain;            // "Pitch", "Timbre", "Amplitude"
    std::string nativeParameterPath;         // e.g. "line1.dcw.envelope"
    std::string comparisonStatus { "not_compared" }; // "not_compared", "compared", "inconclusive"
    std::optional<double> timingErrorMs;
    std::optional<double> levelError;
    std::string limitations;
};

} // namespace abdaudiolab::measurement::adapters::casio
