/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T3.cpp
 * @brief Catch2 unit test suite for Phase 20.11.7 Increment 3:
 *        Casio CZ-101 Target Adapter: Profile-based SysEx frame validation,
 *        decode-only parser, deterministic round-trip encoder, and honest observable binding.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "measurement/adapters/casio/CasioCz101SysExContracts.h"
#include "measurement/adapters/casio/CasioCz101SysExFrameValidator.h"
#include "measurement/adapters/casio/CasioCz101PatchDecoder.h"
#include "measurement/adapters/casio/CasioCz101PatchEncoder.h"
#include "measurement/adapters/casio/CasioCz101ObservableBinding.h"

#include "measurement/ComplexEnvelopeContracts.h"
#include "measurement/ComplexEnvelopeAnalyzer.h"

#include <vector>
#include <string>
#include <cstdint>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::measurement::adapters::casio;

namespace
{

/**
 * @brief Helper to build a synthetic, valid 265-byte Casio CZ-101 patch dump frame.
 */
std::vector<uint8_t> createSyntheticValidPatchDump(uint8_t midiChannel = 0)
{
    CasioCz101NativePatchState state;
    state.patchName = "CZ Reference";
    state.lineSelect = 3; // 1+2'
    state.octave = 0;
    state.dco1Pitch[0].stageIndex = 1;
    state.dco1Pitch[0].rate = 90;
    state.dco1Pitch[0].level = 50;
    state.dco1Pitch[2].isSustainPoint = true;
    state.dco1Pitch[5].isEndPoint = true;

    return CasioCz101PatchEncoder::encodePatch(state, midiChannel);
}

} // anonymous namespace

TEST_CASE("Phase 20.11.7 T3: SysEx Frame Validator - Profile Selection and Channel Handling",
          "[complex_envelopes][casio][validator]")
{
    CasioCz101SysExFrameValidator validator;

    SECTION("Detects PatchResponse profile correctly with channel 0")
    {
        auto frame = createSyntheticValidPatchDump(0);
        auto res = validator.validateFrame(frame);

        CHECK(res.isValid);
        CHECK(res.detectedProfile.has_value());
        CHECK(res.detectedProfile->kind == SysExFrameKind::PatchResponse);
        CHECK(res.detectedProfile->modelIdentifier == "CZ-101");
        CHECK(res.checksumStatus == ChecksumStatus::ChecksumValid);
        CHECK(res.midiChannel == 0);
        CHECK(res.validationStatus == "valid");
    }

    SECTION("Detects channel 5 correctly")
    {
        auto frame = createSyntheticValidPatchDump(5);
        auto res = validator.validateFrame(frame);

        CHECK(res.isValid);
        CHECK(res.midiChannel == 5);
        CHECK(res.checksumStatus == ChecksumStatus::ChecksumValid);
    }

    SECTION("Detects PatchRequest frame profile (8 bytes) with ChecksumPolicy::None")
    {
        // F0 44 00 00 70 00 02 F7 (Request dump on channel 2)
        std::vector<uint8_t> reqFrame = { 0xF0, 0x44, 0x00, 0x00, 0x70, 0x00, 0x02, 0xF7 };
        auto res = validator.validateFrame(reqFrame);

        CHECK(res.isValid);
        CHECK(res.detectedProfile.has_value());
        CHECK(res.detectedProfile->kind == SysExFrameKind::PatchRequest);
        CHECK(res.checksumStatus == ChecksumStatus::ChecksumNotPresent);
        CHECK(res.midiChannel == 2);
    }

    SECTION("Detects ParameterMessage frame profile (9 bytes)")
    {
        // F0 44 00 01 10 20 05 02 F7
        std::vector<uint8_t> paramFrame = { 0xF0, 0x44, 0x00, 0x01, 0x10, 0x20, 0x05, 0x02, 0xF7 };
        auto res = validator.validateFrame(paramFrame);

        CHECK(res.isValid);
        CHECK(res.detectedProfile.has_value());
        CHECK(res.detectedProfile->kind == SysExFrameKind::ParameterMessage);
        CHECK(res.checksumStatus == ChecksumStatus::ChecksumNotPresent);
        CHECK(res.midiChannel == 1);
    }

    SECTION("Rejects unknown manufacturer with unsupported_profile")
    {
        // Roland / Yamaha manufacturer ID 0x41 / 0x43
        std::vector<uint8_t> foreignFrame = { 0xF0, 0x41, 0x10, 0x00, 0x00, 0x12, 0x34, 0xF7 };
        auto res = validator.validateFrame(foreignFrame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "unsupported_profile");
    }

    SECTION("Rejects unknown Casio model ID with unsupported_profile")
    {
        // Model ID 0x55 (unknown Casio model, valid MIDI byte < 0x80)
        std::vector<uint8_t> foreignModelFrame = { 0xF0, 0x44, 0x00, 0x00, 0x55, 0x00, 0x00, 0xF7 };
        auto res = validator.validateFrame(foreignModelFrame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "unsupported_profile");
    }
}

TEST_CASE("Phase 20.11.7 T3: SysEx Frame Validator - Integrity and Error Trapping",
          "[complex_envelopes][casio][validator]")
{
    CasioCz101SysExFrameValidator validator;

    SECTION("Rejects frame lacking start delimiter 0xF0")
    {
        auto frame = createSyntheticValidPatchDump(0);
        frame[0] = 0x00; // Corrupt start
        auto res = validator.validateFrame(frame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "missing_start_delimiter");
    }

    SECTION("Rejects truncated frame lacking end delimiter 0xF7")
    {
        auto frame = createSyntheticValidPatchDump(0);
        frame.pop_back(); // Remove 0xF7
        auto res = validator.validateFrame(frame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "missing_end_delimiter");
    }

    SECTION("Rejects frame with non-MIDI-safe byte (>= 0x80) inside data payload")
    {
        auto frame = createSyntheticValidPatchDump(0);
        frame[50] = 0x80; // Illegal status byte in data stream
        auto res = validator.validateFrame(frame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "invalid_byte_value");
    }

    SECTION("Detects corrupted checksum for PatchResponse profile")
    {
        auto frame = createSyntheticValidPatchDump(0);
        frame[263] ^= 0x01; // Corrupt checksum bit
        auto res = validator.validateFrame(frame);

        CHECK_FALSE(res.isValid);
        CHECK(res.checksumStatus == ChecksumStatus::ChecksumInvalid);
        CHECK(res.validationStatus == "checksum_error");
    }

    SECTION("Detects length mismatch against known profile")
    {
        auto frame = createSyntheticValidPatchDump(0);
        frame.insert(frame.begin() + 100, 0x05); // Make it 266 bytes
        auto res = validator.validateFrame(frame);

        CHECK_FALSE(res.isValid);
        CHECK(res.validationStatus == "length_mismatch");
    }
}

TEST_CASE("Phase 20.11.7 T3: Patch Decoder - Decode-Only Extraction of Envelopes and Tone Parameters",
          "[complex_envelopes][casio][decoder]")
{
    CasioCz101SysExFrameValidator validator;
    CasioCz101PatchDecoder decoder;

    auto frame = createSyntheticValidPatchDump(0);
    auto validation = validator.validateFrame(frame);
    REQUIRE(validation.isValid);

    auto result = decoder.decodePatch(frame);
    REQUIRE(result.isSuccess);

    const auto& state = result.patchState;

    SECTION("Decodes envelope stages and sustain/end markers accurately")
    {
        // DCO1 was configured with sustain=3, end=6
        CHECK(state.dco1Pitch[2].isSustainPoint);
        CHECK(state.dco1Pitch[5].isEndPoint);

        // Stage 1 rate=90, level=50
        CHECK(state.dco1Pitch[0].stageIndex == 1);
        CHECK(state.dco1Pitch[0].rate == 90);
        CHECK(state.dco1Pitch[0].level == 50);

        // Line select was configured to 3 (1+2')
        CHECK(state.lineSelect == 3);
        CHECK(state.modelIdentifier == "CZ-101");
        CHECK_FALSE(state.sourceSysExSha256.empty());
    }

    SECTION("Verifies decode-only safety: rejects undersized buffer")
    {
        std::vector<uint8_t> shortFrame = { 0xF0, 0x44, 0x00, 0xF7 };
        auto failResult = decoder.decodePatch(shortFrame);
        CHECK_FALSE(failResult.isSuccess);
        CHECK_FALSE(failResult.errorMessage.empty());
    }
}

TEST_CASE("Phase 20.11.7 T3: Patch Encoder & Deterministic Round-Trip",
          "[complex_envelopes][casio][encoder][roundtrip]")
{
    CasioCz101SysExFrameValidator validator;
    CasioCz101PatchDecoder decoder;
    CasioCz101PatchEncoder encoder;

    // 1. Create native patch state
    CasioCz101NativePatchState originalState;
    originalState.patchName = "Cosmic Bell";
    originalState.lineSelect = 3;
    originalState.octave = -1;
    originalState.detuneCents = 12.5;
    originalState.vibratoWave = 2;
    originalState.vibratoRate = 45;
    originalState.vibratoDepth = 20;
    originalState.vibratoDelay = 15;
    originalState.ringMod = true;
    originalState.noiseMod = false;
    originalState.dcw1KeyFollow = 5;

    for (int i = 0; i < 8; ++i)
    {
        originalState.dco1Pitch[static_cast<size_t>(i)].stageIndex = i + 1;
        originalState.dco1Pitch[static_cast<size_t>(i)].rate = 20 + i * 5;
        originalState.dco1Pitch[static_cast<size_t>(i)].level = 10 + i * 10;

        originalState.dcw1Timbre[static_cast<size_t>(i)].stageIndex = i + 1;
        originalState.dcw1Timbre[static_cast<size_t>(i)].rate = 50 - i * 4;
        originalState.dcw1Timbre[static_cast<size_t>(i)].level = 90 - i * 8;

        originalState.dca1Amplitude[static_cast<size_t>(i)].stageIndex = i + 1;
        originalState.dca1Amplitude[static_cast<size_t>(i)].rate = 70;
        originalState.dca1Amplitude[static_cast<size_t>(i)].level = 0;
    }
    originalState.dcw1Timbre[3].isSustainPoint = true;
    originalState.dcw1Timbre[6].isEndPoint = true;

    SECTION("Encodes state into valid 265-byte SysEx frame with correct checksum")
    {
        auto encodedFrame = encoder.encodePatch(originalState, 1);
        REQUIRE(encodedFrame.size() == 265);

        auto validation = validator.validateFrame(encodedFrame);
        CHECK(validation.isValid);
        CHECK(validation.detectedProfile->kind == SysExFrameKind::PatchResponse);
        CHECK(validation.checksumStatus == ChecksumStatus::ChecksumValid);
        CHECK(validation.midiChannel == 1);
    }

    SECTION("Round-trip: decode(encode(state)) preserves canonical state")
    {
        auto encoded = encoder.encodePatch(originalState, 0);
        auto decodedRes = decoder.decodePatch(encoded);
        REQUIRE(decodedRes.isSuccess);

        const auto& decodedState = decodedRes.patchState;
        CHECK(decodedState.lineSelect == originalState.lineSelect);
        CHECK(decodedState.octave == originalState.octave);
        CHECK(decodedState.ringMod == originalState.ringMod);
        CHECK(decodedState.noiseMod == originalState.noiseMod);
        CHECK(decodedState.dcw1KeyFollow == originalState.dcw1KeyFollow);

        // Verify DCW envelope stages
        for (size_t i = 0; i < 8; ++i)
        {
            CHECK(decodedState.dcw1Timbre[i].rate == originalState.dcw1Timbre[i].rate);
            CHECK(decodedState.dcw1Timbre[i].level == originalState.dcw1Timbre[i].level);
            CHECK(decodedState.dcw1Timbre[i].isSustainPoint == originalState.dcw1Timbre[i].isSustainPoint);
            CHECK(decodedState.dcw1Timbre[i].isEndPoint == originalState.dcw1Timbre[i].isEndPoint);
        }
    }
}

TEST_CASE("Phase 20.11.7 T3: DCW to TimbreObservable Mapping and Honest Metrology",
          "[complex_envelopes][casio][observable_binding]")
{
    CasioCzDcwMappingInput input;
    input.lineIndex = 1;
    input.waveform = CzWaveform::Reso1;
    input.modelIdentifier = "CZ-101";
    input.sourceSysExSha256 = "d41d8cd98f00b204e9800998ecf8427e";
    input.sustainStage = 3;
    input.endStage = 5;
    input.dcwKeyFollow = 4;
    input.dcwInitialLevel = 99;
    input.envelopeAmount = 70;

    for (int i = 0; i < 8; ++i)
    {
        input.stages[static_cast<size_t>(i)].stageIndex = i + 1;
        input.stages[static_cast<size_t>(i)].rate = 40 + i * 3;
        input.stages[static_cast<size_t>(i)].level = 80 - i * 10;
        input.stages[static_cast<size_t>(i)].isSustainPoint = (i + 1 == 3);
        input.stages[static_cast<size_t>(i)].isEndPoint = (i + 1 == 5);
    }

    auto mappingResult = CasioCz101ObservableBinding::mapDcwToTimbreObservable(input);

    SECTION("Produces declarative TimbreObservable and respects endStage boundary")
    {
        CHECK(mappingResult.mappingStatus == "mapped");
        CHECK(mappingResult.nativeParameterPath == "line1.dcw.envelope");
        CHECK(mappingResult.observedDomain.domain == EnvelopeDomain::Timbre);
        CHECK(mappingResult.inferredStages[0].levelUnit == "native_cz_level");

        // Active stages up to endStage=5
        REQUIRE(mappingResult.inferredStages.size() == 5);
        CHECK(mappingResult.inferredStages[2].isSustainPoint);
        CHECK(mappingResult.inferredStages[4].isEndKeyOnPoint);
    }

    SECTION("Enforces honest metrological claim: phaseDistortionProxy is strictly not_claimed")
    {
        CHECK(mappingResult.phaseDistortionProxy == "not_claimed");
        CHECK(mappingResult.nativeEnvelopeReconstruction == "not_claimed");
    }

    SECTION("Rejects invalid line index gracefully")
    {
        input.lineIndex = 3; // Invalid for CZ-101
        auto badResult = CasioCz101ObservableBinding::mapDcwToTimbreObservable(input);
        CHECK(badResult.mappingStatus == "invalid");
    }
}

TEST_CASE("Phase 20.11.7 T3: Native Binding Association and Acoustic Comparison",
          "[complex_envelopes][casio][observable_comparison]")
{
    CasioCz101NativePatchState state;
    state.lineSelect = 3; // 1+2' (both lines active)
    state.modelIdentifier = "CZ-101";
    state.sourceSysExSha256 = "fedcba9876543210";

    auto bindings = CasioCz101ObservableBinding::bindNativePatchToObservables(state);

    SECTION("Binds Line 1 and Line 2 envelopes to canonical Pitch, Timbre, and Amplitude")
    {
        REQUIRE(bindings.size() == 6);
        CHECK(bindings[0].nativePath == "line1.dco.envelope");
        CHECK(bindings[0].observableDomain == EnvelopeDomain::Pitch);
        CHECK(bindings[1].nativePath == "line1.dcw.envelope");
        CHECK(bindings[1].observableDomain == EnvelopeDomain::Timbre);
        CHECK(bindings[2].nativePath == "line1.dca.envelope");
        CHECK(bindings[2].observableDomain == EnvelopeDomain::Amplitude);

        CHECK(bindings[3].nativePath == "line2.dco.envelope");
        CHECK(bindings[3].observableDomain == EnvelopeDomain::Pitch);
        CHECK(bindings[4].nativePath == "line2.dcw.envelope");
        CHECK(bindings[4].observableDomain == EnvelopeDomain::Timbre);
        CHECK(bindings[5].nativePath == "line2.dca.envelope");
        CHECK(bindings[5].observableDomain == EnvelopeDomain::Amplitude);
    }

    SECTION("Compares native binding with observed acoustic trajectory honestly")
    {
        EnvelopeTrajectory observed;
        observed.domain = EnvelopeDomain::Timbre;
        observed.trajectoryLabel = "Observed Centroid";

        EnvelopeObservationPoint p0; p0.frameIndex = 0; p0.timeMs = 0.0; p0.value = 1500.0; p0.unit = "spectralCentroidHz";
        EnvelopeObservationPoint p1; p1.frameIndex = 1; p1.timeMs = 10.0; p1.value = 2500.0; p1.unit = "spectralCentroidHz";
        EnvelopeObservationPoint p2; p2.frameIndex = 2; p2.timeMs = 20.0; p2.value = 2200.0; p2.unit = "spectralCentroidHz";
        observed.points = { p0, p1, p2 };

        auto comp = CasioCz101ObservableBinding::compareNativeWithObserved(bindings[1], observed);
        CHECK(comp.comparisonStatus == "compared");
        CHECK(comp.observableDomain == "Timbre");
        CHECK(comp.nativeParameterPath == "line1.dcw.envelope");
        CHECK(comp.timingErrorMs.has_value());
        CHECK_THAT(comp.timingErrorMs.value(), Catch::Matchers::WithinRel(20.0, 1e-4));
        CHECK(comp.limitations.find("observed proxy") != std::string::npos);
    }

    SECTION("Identifies domain mismatch as inconclusive")
    {
        EnvelopeTrajectory wrongDomainObserved;
        wrongDomainObserved.domain = EnvelopeDomain::Pitch;
        wrongDomainObserved.trajectoryLabel = "Observed Pitch";

        EnvelopeObservationPoint wp0; wp0.frameIndex = 0; wp0.timeMs = 0.0; wp0.value = 440.0; wp0.unit = "Hz";
        wrongDomainObserved.points = { wp0 };

        auto comp = CasioCz101ObservableBinding::compareNativeWithObserved(bindings[1], wrongDomainObserved);
        CHECK(comp.comparisonStatus == "inconclusive");
        CHECK(comp.limitations.find("mismatch") != std::string::npos);
    }
}

TEST_CASE("Phase 20.11.7 T3: Architectural Asepsis - Measurement Core Independence",
          "[complex_envelopes][casio][architecture]")
{
    // Verify that ComplexEnvelopeAnalyzer can be instantiated and used independently
    // without any knowledge of Casio CZ or SysEx adapters.
    ComplexEnvelopeAnalysisInput input;
    input.audioBuffer = std::vector<float>(512, 0.0f);
    input.sampleRate = 44100.0;
    input.nominalFrequencyHz = 440.0;
    auto analysis = ComplexEnvelopeAnalyzer::analyze(input);

    CHECK(analysis.temporalGrid.hopMs > 0.0);
    CHECK(analysis.pitchTrajectory.domain == EnvelopeDomain::Pitch);
    CHECK(analysis.timbreTrajectory.domain == EnvelopeDomain::Timbre);
    CHECK(analysis.amplitudeTrajectory.domain == EnvelopeDomain::Amplitude);
}
