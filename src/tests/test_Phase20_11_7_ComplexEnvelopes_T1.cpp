/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T1.cpp
 * @brief Unit tests for Phase 20.11.7-T1: Canonical contracts, strict domain/unit validation,
 *        RFC 8785 determinism, and multi-stage envelope descriptors.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "measurement/ComplexEnvelopeContracts.h"

using namespace abdaudiolab::measurement;

TEST_CASE("Phase 20.11.7-T1: Strict domain and unit compatibility matrix", "[complex_envelopes][contracts]")
{
    SECTION("Pitch domain permits only pitch-related units")
    {
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "Hz"));
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "cents"));
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "semitones"));

        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "dBFS"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "normalized"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "spectralCentroidHz"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "rolloffHz"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, "phaseDistortionProxy"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Pitch, ""));
    }

    SECTION("Timbre domain permits only harmonic and phase distortion metrics")
    {
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "spectralCentroidHz"));
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "rolloffHz"));
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "phaseDistortionProxy"));

        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "Hz"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "cents"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "semitones"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "dBFS"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Timbre, "normalized"));
    }

    SECTION("Amplitude domain permits only energy and normalized metrics")
    {
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Amplitude, "dBFS"));
        REQUIRE(isUnitCompatibleWithDomain(EnvelopeDomain::Amplitude, "normalized"));

        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Amplitude, "Hz"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Amplitude, "cents"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Amplitude, "spectralCentroidHz"));
    }

    SECTION("Unknown domain rejects all units")
    {
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Unknown, "Hz"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Unknown, "dBFS"));
        REQUIRE_FALSE(isUnitCompatibleWithDomain(EnvelopeDomain::Unknown, "spectralCentroidHz"));
    }
}

TEST_CASE("Phase 20.11.7-T1: Envelope trajectory validation and constraints", "[complex_envelopes][validation]")
{
    EnvelopeTrajectory traj;
    traj.domain = EnvelopeDomain::Amplitude;
    traj.trajectoryLabel = "DCA1_Amplitude";
    traj.temporalGridId = "grid-001";
    traj.extractionMethod = "rms_windowed";
    traj.spectralMetadata.sampleRateHz = 48000.0;
    traj.spectralMetadata.hopSizeSamples = 256;
    traj.spectralMetadata.windowLengthSamples = 1024;
    traj.spectralMetadata.fftSize = 2048;

    SECTION("Valid trajectory passes strict validation")
    {
        for (int i = 0; i < 5; ++i)
        {
            EnvelopeObservationPoint pt;
            pt.frameIndex = i;
            pt.timeMs = i * 5.333;
            pt.value = -30.0 + i * 2.0;
            pt.unit = "dBFS";
            pt.status = "valid";
            pt.resolution = 0.01;
            pt.uncertaintyStatus = "not_estimated";
            traj.points.push_back(pt);
        }

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE(res.valid);
        REQUIRE(res.errors.empty());
    }

    SECTION("Rejects Unknown domain and empty trajectory label")
    {
        traj.domain = EnvelopeDomain::Unknown;
        traj.trajectoryLabel = "";
        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        REQUIRE(res.errors.size() >= 2);
    }

    SECTION("Rejects duplicate or non-monotonic frame indices and timestamps")
    {
        EnvelopeObservationPoint p1;
        p1.frameIndex = 0;
        p1.timeMs = 10.0;
        p1.value = -20.0;
        p1.unit = "dBFS";
        traj.points.push_back(p1);

        EnvelopeObservationPoint p2;
        p2.frameIndex = 0; // Duplicate frame
        p2.timeMs = 5.0;  // Decreasing time
        p2.value = -18.0;
        p2.unit = "dBFS";
        traj.points.push_back(p2);

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundTimeErr = false;
        bool foundFrameErr = false;
        for (const auto& err : res.errors)
        {
            if (err.find("time_not_strictly_increasing") != std::string::npos) foundTimeErr = true;
            if (err.find("duplicate_or_decreasing_frame") != std::string::npos) foundFrameErr = true;
        }
        REQUIRE(foundTimeErr);
        REQUIRE(foundFrameErr);
    }

    SECTION("Rejects null value when status is 'valid'")
    {
        EnvelopeObservationPoint pt;
        pt.frameIndex = 0;
        pt.timeMs = 0.0;
        pt.value = std::nullopt; // Null value
        pt.unit = "dBFS";
        pt.status = "valid"; // Conflict!
        traj.points.push_back(pt);

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundNullErr = false;
        for (const auto& err : res.errors)
            if (err.find("null_value_in_valid_status") != std::string::npos) foundNullErr = true;
        REQUIRE(foundNullErr);
    }

    SECTION("Allows null value when status is 'not_observable' or 'silence'")
    {
        EnvelopeObservationPoint pt;
        pt.frameIndex = 0;
        pt.timeMs = 0.0;
        pt.value = std::nullopt;
        pt.unit = "dBFS";
        pt.status = "not_observable";
        traj.points.push_back(pt);

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE(res.valid);
    }

    SECTION("Rejects incompatible unit for trajectory domain")
    {
        EnvelopeObservationPoint pt;
        pt.frameIndex = 0;
        pt.timeMs = 0.0;
        pt.value = 440.0;
        pt.unit = "Hz"; // Incompatible with Amplitude domain!
        pt.status = "valid";
        traj.points.push_back(pt);

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundUnitErr = false;
        for (const auto& err : res.errors)
            if (err.find("incompatible_unit") != std::string::npos) foundUnitErr = true;
        REQUIRE(foundUnitErr);
    }

    SECTION("Spectral metadata requirements: fftSize >= windowLengthSamples")
    {
        traj.spectralMetadata.fftSize = 512;
        traj.spectralMetadata.windowLengthSamples = 1024; // Conflict!

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundFftErr = false;
        for (const auto& err : res.errors)
            if (err.find("fft_smaller_than_window") != std::string::npos) foundFftErr = true;
        REQUIRE(foundFftErr);
    }
}

TEST_CASE("Phase 20.11.7-T1: Envelope stage descriptors (8-stage Casio CZ model)", "[complex_envelopes][stages]")
{
    EnvelopeTrajectory traj;
    traj.domain = EnvelopeDomain::Timbre;
    traj.trajectoryLabel = "DCW1_PhaseDistortion";
    traj.extractionMethod = "spectral_centroid_tracking";
    traj.spectralMetadata.sampleRateHz = 48000.0;
    traj.spectralMetadata.hopSizeSamples = 256;
    traj.spectralMetadata.windowLengthSamples = 1024;
    traj.spectralMetadata.fftSize = 2048;

    SECTION("Valid 8-stage Casio CZ parameterization")
    {
        for (int i = 1; i <= 8; ++i)
        {
            EnvelopeStageDescriptor st;
            st.stageIndex = i;
            st.parameterization = "rate_level";
            st.durationMs = 50.0 * i;
            st.rateOrSlope = 30.0 + i * 5.0;      // Rate 0..99
            st.targetLevel = (i == 8) ? 0.0 : (20.0 + i * 8.0); // Level 0..99
            st.levelUnit = "normalized_0_99";
            st.isSustainPoint = (i == 4);
            st.isEndKeyOnPoint = (i == 6);
            traj.inferredStages.push_back(st);
        }

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE(res.valid);
        REQUIRE(traj.inferredStages.size() == 8);
        REQUIRE(traj.inferredStages[3].isSustainPoint);
        REQUIRE(traj.inferredStages[5].isEndKeyOnPoint);
    }

    SECTION("Rejects non-consecutive stage indices")
    {
        EnvelopeStageDescriptor s1; s1.stageIndex = 1; traj.inferredStages.push_back(s1);
        EnvelopeStageDescriptor s2; s2.stageIndex = 3; traj.inferredStages.push_back(s2); // Missing stage 2!

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundConsecutiveErr = false;
        for (const auto& err : res.errors)
            if (err.find("non_consecutive_stage_index") != std::string::npos) foundConsecutiveErr = true;
        REQUIRE(foundConsecutiveErr);
    }

    SECTION("Rejects more than 8 stages")
    {
        for (int i = 1; i <= 9; ++i)
        {
            EnvelopeStageDescriptor st;
            st.stageIndex = i;
            traj.inferredStages.push_back(st);
        }

        auto res = validateEnvelopeTrajectory(traj);
        REQUIRE_FALSE(res.valid);
        bool foundTooManyErr = false;
        for (const auto& err : res.errors)
            if (err.find("too_many_stages") != std::string::npos) foundTooManyErr = true;
        REQUIRE(foundTooManyErr);
    }
}

TEST_CASE("Phase 20.11.7-T1: MultiDomainEnvelopeCaptureRecord and RFC 8785 fixity determinism", "[complex_envelopes][rfc8785]")
{
    MultiDomainEnvelopeCaptureRecord rec;
    rec.captureId = "cz101-exp-001";
    rec.dut.name = "Casio CZ-101";
    rec.dut.model = "CZ-101";
    rec.dut.vendor = "Casio";
    rec.dut.dutType = "hardware_synth";
    rec.dut.stateSha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    rec.temporalGrid.gridId = "grid-48k-256";
    rec.temporalGrid.originMs = 0.0;
    rec.temporalGrid.hopMs = 5.333333333;
    rec.temporalGrid.frameCount = 100;
    rec.temporalGrid.alignmentMethod = "stft_hop_synchronous";

    rec.pitchTrajectory.domain = EnvelopeDomain::Pitch;
    rec.pitchTrajectory.trajectoryLabel = "DCO1_Pitch";
    rec.pitchTrajectory.temporalGridId = rec.temporalGrid.gridId;
    rec.pitchTrajectory.extractionMethod = "stft_instantaneous_freq";

    rec.timbreTrajectory.domain = EnvelopeDomain::Timbre;
    rec.timbreTrajectory.trajectoryLabel = "DCW1_PhaseDistortion";
    rec.timbreTrajectory.temporalGridId = rec.temporalGrid.gridId;
    rec.timbreTrajectory.extractionMethod = "spectral_centroid_tracking";

    rec.amplitudeTrajectory.domain = EnvelopeDomain::Amplitude;
    rec.amplitudeTrajectory.trajectoryLabel = "DCA1_Amplitude";
    rec.amplitudeTrajectory.temporalGridId = rec.temporalGrid.gridId;
    rec.amplitudeTrajectory.extractionMethod = "rms_windowed";

    rec.audioSha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    rec.stimulusSha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    rec.nativePatchStateSha256 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

    SECTION("Validation of consistent multi-domain capture record")
    {
        auto res = validateMultiDomainEnvelopeCaptureRecord(rec);
        REQUIRE(res.valid);
        REQUIRE(res.errors.empty());
    }

    SECTION("Default metrological honesty requires nativeEnvelopeReconstruction == 'not_claimed'")
    {
        REQUIRE(rec.pitchTrajectory.nativeEnvelopeReconstruction == "not_claimed");
        REQUIRE(rec.timbreTrajectory.nativeEnvelopeReconstruction == "not_claimed");
        REQUIRE(rec.amplitudeTrajectory.nativeEnvelopeReconstruction == "not_claimed");
    }

    SECTION("RFC 8785 Determinism: exact canonical byte equality")
    {
        std::string jsonA = rec.serializeCanonicalJson();
        std::string jsonB = rec.serializeCanonicalJson();
        REQUIRE(jsonA == jsonB);

        std::string shaA = rec.computeCanonicalSha256();
        std::string shaB = rec.computeCanonicalSha256();
        REQUIRE(shaA == shaB);
        REQUIRE(shaA.size() == 64);
    }

    SECTION("RFC 8785 Determinism: modification of any property produces distinct canonical hash")
    {
        std::string origSha = rec.computeCanonicalSha256();

        // 1. Changing midi velocity
        auto mod1 = rec;
        mod1.midiVelocity = 99;
        REQUIRE(mod1.computeCanonicalSha256() != origSha);

        // 2. Changing unit in a point
        auto mod2 = rec;
        EnvelopeObservationPoint p;
        p.frameIndex = 0;
        p.timeMs = 0.0;
        p.value = 261.63;
        p.unit = "Hz";
        p.status = "valid";
        mod2.pitchTrajectory.points.push_back(p);
        std::string shaWithHz = mod2.computeCanonicalSha256();

        mod2.pitchTrajectory.points[0].unit = "cents";
        std::string shaWithCents = mod2.computeCanonicalSha256();
        REQUIRE(shaWithHz != origSha);
        REQUIRE(shaWithCents != shaWithHz);

        // 3. Null value vs 0.0 value produces distinct canonical hash
        auto mod3A = rec;
        EnvelopeObservationPoint pNull;
        pNull.frameIndex = 0; pNull.timeMs = 0.0; pNull.value = std::nullopt; pNull.unit = "dBFS"; pNull.status = "not_observable";
        mod3A.amplitudeTrajectory.points.push_back(pNull);

        auto mod3B = rec;
        EnvelopeObservationPoint pZero;
        pZero.frameIndex = 0; pZero.timeMs = 0.0; pZero.value = 0.0; pZero.unit = "dBFS"; pZero.status = "valid";
        mod3B.amplitudeTrajectory.points.push_back(pZero);

        REQUIRE(mod3A.computeCanonicalSha256() != mod3B.computeCanonicalSha256());
    }

    SECTION("Round-trip serialization and deserialization is bit-exact")
    {
        EnvelopeObservationPoint ptPitch;
        ptPitch.frameIndex = 0; ptPitch.timeMs = 0.0; ptPitch.value = 261.6256; ptPitch.unit = "Hz"; ptPitch.status = "valid";
        rec.pitchTrajectory.points.push_back(ptPitch);

        EnvelopeObservationPoint ptTimbre;
        ptTimbre.frameIndex = 0; ptTimbre.timeMs = 0.0; ptTimbre.value = 1450.0; ptTimbre.unit = "spectralCentroidHz"; ptTimbre.status = "valid";
        rec.timbreTrajectory.points.push_back(ptTimbre);

        EnvelopeObservationPoint ptAmp;
        ptAmp.frameIndex = 0; ptAmp.timeMs = 0.0; ptAmp.value = -12.4; ptAmp.unit = "dBFS"; ptAmp.status = "valid";
        rec.amplitudeTrajectory.points.push_back(ptAmp);

        std::string canonicalJson = rec.serializeCanonicalJson();
        auto parsed = nlohmann::json::parse(canonicalJson);
        auto restored = MultiDomainEnvelopeCaptureRecord::fromJson(parsed);

        REQUIRE(restored.captureId == rec.captureId);
        REQUIRE(restored.dut.model == rec.dut.model);
        REQUIRE(restored.pitchTrajectory.points.size() == 1);
        REQUIRE(restored.timbreTrajectory.points.size() == 1);
        REQUIRE(restored.amplitudeTrajectory.points.size() == 1);
        REQUIRE(restored.computeCanonicalSha256() == rec.computeCanonicalSha256());
    }

    SECTION("Grid alignment: rejects mismatched grid when status is 'aligned', permits when 'not_aligned'")
    {
        // 1. Mismatch with aligned status -> fails
        rec.timbreTrajectory.temporalGridId = "different-grid-999";
        rec.timbreTrajectory.alignmentStatus = "aligned";
        auto res1 = validateMultiDomainEnvelopeCaptureRecord(rec);
        REQUIRE_FALSE(res1.valid);
        bool foundMismatchErr = false;
        for (const auto& err : res1.errors)
            if (err.find("grid_id_mismatch") != std::string::npos) foundMismatchErr = true;
        REQUIRE(foundMismatchErr);

        // 2. Mismatch with declared 'not_aligned' status -> valid (no silent interpolation)
        rec.timbreTrajectory.alignmentStatus = "not_aligned";
        auto res2 = validateMultiDomainEnvelopeCaptureRecord(rec);
        REQUIRE(res2.valid);
    }

    SECTION("Round-trip preserves nullopt, optionals, inferredStages, and nativeBinding")
    {
        // Add nativeBinding
        TargetParameterBinding binding;
        binding.logicalName = "DCW_Waveform_Shape";
        binding.nativeId = "CZ_SYS_PARAM_0x2A";
        binding.unit = "normalized_0_99";
        binding.mappingVersion = "1.0";
        rec.timbreTrajectory.nativeBinding = binding;

        // Add optional attack/release
        rec.timbreTrajectory.attackTimeMs = 45.2;
        rec.timbreTrajectory.releaseTimeMs = 120.8;
        rec.timbreTrajectory.spectralMetadata.noiseFloorDbfs = -88.5;

        // Add stages with optional targetLevel and rateOrSlope
        EnvelopeStageDescriptor st;
        st.stageIndex = 1;
        st.parameterization = "rate_level";
        st.durationMs = 60.0;
        st.rateOrSlope = 75.0;
        st.targetLevel = 90.0;
        st.levelUnit = "normalized_0_99";
        st.isSustainPoint = false;
        rec.timbreTrajectory.inferredStages.push_back(st);

        // Point with uncertaintyValue and evaluated status
        EnvelopeObservationPoint pt;
        pt.frameIndex = 0;
        pt.timeMs = 0.0;
        pt.value = 1250.0;
        pt.unit = "spectralCentroidHz";
        pt.status = "valid";
        pt.resolution = 0.01;
        pt.uncertaintyStatus = "evaluated";
        pt.uncertaintyValue = 0.35;
        pt.uncertaintyMethod = "type_a_stdev";
        rec.timbreTrajectory.points.push_back(pt);

        std::string jsonStr = rec.serializeCanonicalJson();
        auto parsed = nlohmann::json::parse(jsonStr);
        auto restored = MultiDomainEnvelopeCaptureRecord::fromJson(parsed);

        REQUIRE(restored.timbreTrajectory.nativeBinding.has_value());
        REQUIRE(restored.timbreTrajectory.nativeBinding->logicalName == "DCW_Waveform_Shape");
        REQUIRE(restored.timbreTrajectory.nativeBinding->nativeId == "CZ_SYS_PARAM_0x2A");
        REQUIRE(restored.timbreTrajectory.attackTimeMs.has_value());
        REQUIRE(*restored.timbreTrajectory.attackTimeMs == 45.2);
        REQUIRE(restored.timbreTrajectory.releaseTimeMs.has_value());
        REQUIRE(*restored.timbreTrajectory.releaseTimeMs == 120.8);
        REQUIRE(restored.timbreTrajectory.spectralMetadata.noiseFloorDbfs.has_value());
        REQUIRE(*restored.timbreTrajectory.spectralMetadata.noiseFloorDbfs == -88.5);

        REQUIRE(restored.timbreTrajectory.inferredStages.size() == 1);
        REQUIRE(restored.timbreTrajectory.inferredStages[0].rateOrSlope.has_value());
        REQUIRE(*restored.timbreTrajectory.inferredStages[0].rateOrSlope == 75.0);

        REQUIRE(restored.timbreTrajectory.points.size() == 1);
        REQUIRE(restored.timbreTrajectory.points[0].uncertaintyValue.has_value());
        REQUIRE(*restored.timbreTrajectory.points[0].uncertaintyValue == 0.35);
        REQUIRE(restored.timbreTrajectory.points[0].uncertaintyStatus == "evaluated");
    }
}
