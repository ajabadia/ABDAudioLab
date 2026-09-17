/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T2.cpp
 * @brief Unit and integration tests for Phase 20.11.7-T2: Lockstep MultiDomainEnvelopeAnalyzer,
 *        acoustic observability, unguided mode detection, and synthetic control benchmarks.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/ComplexEnvelopeAnalyzer.h"

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

TEST_CASE("Phase 20.11.7-T2: Single lockstep TemporalGrid and silence frame preservation", "[complex_envelopes][analyzer]")
{
    // Generate synthetic test signal with 50 ms pre-onset silence and 200 ms post-decay silence
    SyntheticEnvelopeControlGenerator::GenerationParams params;
    params.sampleRate = 48000.0;
    params.noteOnMs = 50.0;
    params.noteOffMs = 500.0;
    params.totalDurationMs = 800.0;
    params.baseFrequencyHz = 261.6256;

    auto gen = SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal(params);

    ComplexEnvelopeAnalysisInput input;
    input.audioBuffer = gen.audioBuffer;
    input.sampleRate = gen.sampleRate;
    input.noteOnSample = gen.noteOnSample;
    input.noteOffSample = gen.noteOffSample;
    input.nominalFrequencyHz = params.baseFrequencyHz;

    ComplexEnvelopeAnalyzerConfig config;
    config.sampleRateHz = 48000.0;
    config.hopSizeSamples = 256;
    config.fftSize = 2048;

    auto record = ComplexEnvelopeAnalyzer::analyze(input, config);

    SECTION("Strict single TemporalGrid across all 3 trajectories")
    {
        const int expectedFrames = static_cast<int>(gen.audioBuffer.size() / 256);
        REQUIRE(record.temporalGrid.frameCount == expectedFrames);
        REQUIRE(record.temporalGrid.frameCount > 0);
        REQUIRE_THAT(record.temporalGrid.hopMs, WithinAbs((256.0 / 48000.0) * 1000.0, 1e-4));
        REQUIRE(record.temporalGrid.gridId == "grid_48000_hop256");

        // Trajectories share the exact same gridId and frame count
        REQUIRE(record.pitchTrajectory.temporalGridId == record.temporalGrid.gridId);
        REQUIRE(record.timbreTrajectory.temporalGridId == record.temporalGrid.gridId);
        REQUIRE(record.amplitudeTrajectory.temporalGridId == record.temporalGrid.gridId);

        REQUIRE(record.pitchTrajectory.points.size() == static_cast<size_t>(expectedFrames));
        REQUIRE(record.timbreTrajectory.points.size() == static_cast<size_t>(expectedFrames));
        REQUIRE(record.amplitudeTrajectory.points.size() == static_cast<size_t>(expectedFrames));
    }

    SECTION("Silent frames preserve frameIndex and timeMs without dropping frames")
    {
        // Frame 0 is in the 50ms pre-onset region: must be marked silence, null value, but keep frameIndex 0
        const auto& ptAmp0 = record.amplitudeTrajectory.points[0];
        const auto& ptTimbre0 = record.timbreTrajectory.points[0];
        const auto& ptPitch0 = record.pitchTrajectory.points[0];

        REQUIRE(ptAmp0.frameIndex == 0);
        REQUIRE_THAT(ptAmp0.timeMs, WithinAbs(0.0, 1e-4));
        REQUIRE(ptAmp0.status == "silence");
        REQUIRE_FALSE(ptAmp0.value.has_value());

        REQUIRE(ptTimbre0.frameIndex == 0);
        REQUIRE_THAT(ptTimbre0.timeMs, WithinAbs(0.0, 1e-4));
        REQUIRE(ptTimbre0.status == "silence");
        REQUIRE_FALSE(ptTimbre0.value.has_value());

        REQUIRE(ptPitch0.frameIndex == 0);
        REQUIRE_THAT(ptPitch0.timeMs, WithinAbs(0.0, 1e-4));
        REQUIRE(ptPitch0.status == "silence");
        REQUIRE_FALSE(ptPitch0.value.has_value());

        // Last frame is in post-release silence: must also retain position
        size_t lastIdx = record.amplitudeTrajectory.points.size() - 1;
        const auto& ptAmpLast = record.amplitudeTrajectory.points[lastIdx];
        REQUIRE(ptAmpLast.frameIndex == static_cast<int>(lastIdx));
        REQUIRE(ptAmpLast.status == "silence");
        REQUIRE_FALSE(ptAmpLast.value.has_value());
    }

    SECTION("MultiDomainEnvelopeCaptureRecord passes strict validator")
    {
        auto valRes = validateMultiDomainEnvelopeCaptureRecord(record);
        REQUIRE(valRes.valid);
        REQUIRE(valRes.errors.empty());
    }
}

TEST_CASE("Phase 20.11.7-T2: Acoustic observability segregation (Pitch ambiguity vs Timbre & Amplitude)", "[complex_envelopes][observability]")
{
    SECTION("Ambiguous pitch on noise burst yields nullopt with reason 'ambiguous_f0'")
    {
        auto noise = SyntheticEnvelopeControlGenerator::generateNoiseBurstSignal(48000.0, 600.0);

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = noise.audioBuffer;
        input.sampleRate = noise.sampleRate;
        input.nominalFrequencyHz = 261.6256;

        auto record = ComplexEnvelopeAnalyzer::analyze(input);

        // Find active frames in the middle of the noise burst (e.g. at 250ms)
        bool foundActiveFrame = false;
        for (size_t i = 0; i < record.amplitudeTrajectory.points.size(); ++i)
        {
            const auto& ptAmp = record.amplitudeTrajectory.points[i];
            const auto& ptTimbre = record.timbreTrajectory.points[i];
            const auto& ptPitch = record.pitchTrajectory.points[i];

            if (ptAmp.timeMs >= 200.0 && ptAmp.timeMs <= 400.0)
            {
                foundActiveFrame = true;
                // Amplitude is observable
                REQUIRE(ptAmp.status == "valid");
                REQUIRE(ptAmp.value.has_value());
                REQUIRE(*ptAmp.value > -60.0);

                // Timbre (spectral centroid) is observable
                REQUIRE(ptTimbre.status == "valid");
                REQUIRE(ptTimbre.value.has_value());
                REQUIRE(*ptTimbre.value > 1000.0); // Broadband noise has high centroid

                // Pitch MUST be unobservable / ambiguous on noise burst
                REQUIRE(ptPitch.status == "unreliable");
                REQUIRE_FALSE(ptPitch.value.has_value());
                REQUIRE(ptPitch.reason == "ambiguous_f0");
                REQUIRE(ptPitch.voicedStatus == "unvoiced");
            }
        }
        REQUIRE(foundActiveFrame);
    }

    SECTION("Dual equal sinusoidal tones trigger ambiguity detection")
    {
        const double sr = 48000.0;
        const size_t totalSamples = 48000 / 2; // 500 ms
        std::vector<float> dualTone(totalSamples, 0.0f);
        const double twoPi = 2.0 * 3.14159265358979323846;

        // Equal amplitude at 440 Hz and 880 Hz
        for (size_t n = 0; n < totalSamples; ++n)
        {
            double s1 = std::sin(twoPi * 440.0 * n / sr);
            double s2 = std::sin(twoPi * 880.0 * n / sr);
            dualTone[n] = static_cast<float>(0.4 * (s1 + s2));
        }

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = dualTone;
        input.sampleRate = sr;
        input.nominalFrequencyHz = 440.0;

        auto record = ComplexEnvelopeAnalyzer::analyze(input);

        // Check active frame: pitch ambiguity detected due to equal secondary peak
        const auto& ptPitch = record.pitchTrajectory.points[record.pitchTrajectory.points.size() / 2];
        REQUIRE(ptPitch.status == "unreliable");
        REQUIRE_FALSE(ptPitch.value.has_value());
        REQUIRE(ptPitch.reason == "ambiguous_f0");
    }
}

TEST_CASE("Phase 20.11.7-T2: Unguided mode autonomous onset, decay and release timing", "[complex_envelopes][unguided]")
{
    SECTION("Autonomous energy onset detection is reproducible and does not default to sample 0")
    {
        SyntheticEnvelopeControlGenerator::GenerationParams params;
        params.sampleRate = 48000.0;
        params.noteOnMs = 120.0; // Clear onset after 120 ms
        params.noteOffMs = 600.0;
        params.totalDurationMs = 900.0;

        auto gen = SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal(params);

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = gen.audioBuffer;
        input.sampleRate = gen.sampleRate;
        // Do NOT provide noteOnSample nor noteOffSample (simulate unguided / free mode)
        input.nominalFrequencyHz = params.baseFrequencyHz;

        auto record1 = ComplexEnvelopeAnalyzer::analyze(input);
        auto record2 = ComplexEnvelopeAnalyzer::analyze(input);

        REQUIRE(record1.pitchTrajectory.noteOnMethod == "energy_onset");
        REQUIRE(record1.pitchTrajectory.noteOffMethod == "energy_decay");

        // Attack time detected > 0 ms and not starting at 0.0 ms
        REQUIRE(record1.amplitudeTrajectory.attackTimeMs.has_value());
        REQUIRE(*record1.amplitudeTrajectory.attackTimeMs > 20.0);
        REQUIRE(*record1.amplitudeTrajectory.attackTimeMs < 120.0);

        // Idempotent and deterministic
        REQUIRE_THAT(*record1.amplitudeTrajectory.attackTimeMs,
                     WithinAbs(*record2.amplitudeTrajectory.attackTimeMs, 1e-6));
    }

    SECTION("Absent note-off on sustaining tone leaves releaseTimeMs null with method not_available")
    {
        const double sr = 48000.0;
        const size_t totalSamples = 48000 / 2; // 500 ms
        std::vector<float> sustained(totalSamples, 0.0f);
        const double twoPi = 2.0 * 3.14159265358979323846;

        for (size_t n = 0; n < totalSamples; ++n)
        {
            sustained[n] = static_cast<float>(0.7 * std::sin(twoPi * 440.0 * n / sr));
        }

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = sustained;
        input.sampleRate = sr;
        input.nominalFrequencyHz = 440.0;
        // noteOn and noteOff omitted; audio never decays

        auto record = ComplexEnvelopeAnalyzer::analyze(input);

        REQUIRE_FALSE(record.amplitudeTrajectory.releaseTimeMs.has_value());
        REQUIRE(record.amplitudeTrajectory.noteOffMethod == "not_available");
    }

    SECTION("Completely silent audio produces not_available onset and null attackTimeMs")
    {
        std::vector<float> silence(4800, 0.0f); // 100 ms pure silence
        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = silence;
        input.sampleRate = 48000.0;

        auto record = ComplexEnvelopeAnalyzer::analyze(input);

        REQUIRE_FALSE(record.amplitudeTrajectory.attackTimeMs.has_value());
        REQUIRE(record.amplitudeTrajectory.noteOnMethod == "not_available");
    }
}

TEST_CASE("Phase 20.11.7-T2: Metrological honesty and proxy declarations", "[complex_envelopes][honesty]")
{
    auto sweep = SyntheticEnvelopeControlGenerator::generatePhaseDistortionSweepSignal(48000.0, 261.6256, 1.2, 500.0);

    ComplexEnvelopeAnalysisInput input;
    input.audioBuffer = sweep.audioBuffer;
    input.sampleRate = sweep.sampleRate;
    input.nominalFrequencyHz = 261.6256;

    auto record = ComplexEnvelopeAnalyzer::analyze(input);

    SECTION("phaseDistortionProxy and nativeEnvelopeReconstruction must be 'not_claimed'")
    {
        REQUIRE(record.timbreTrajectory.phaseDistortionProxy == "not_claimed");
        REQUIRE(record.timbreTrajectory.nativeEnvelopeReconstruction == "not_claimed");
        REQUIRE(record.pitchTrajectory.nativeEnvelopeReconstruction == "not_claimed");
        REQUIRE(record.amplitudeTrajectory.nativeEnvelopeReconstruction == "not_claimed");
    }

    SECTION("Amplitude preserves both rmsDbfs and amplitudeNormalized with declared reference")
    {
        bool foundValid = false;
        for (const auto& pt : record.amplitudeTrajectory.points)
        {
            if (pt.status == "valid")
            {
                foundValid = true;
                REQUIRE(pt.rmsDbfs.has_value());
                REQUIRE(pt.amplitudeNormalized.has_value());
                REQUIRE(pt.normalizationReference == "peak_observed");
                REQUIRE(*pt.amplitudeNormalized >= 0.0);
                REQUIRE(*pt.amplitudeNormalized <= 1.0001);
            }
        }
        REQUIRE(foundValid);
    }
}

TEST_CASE("Phase 20.11.7-T2: Sample rate mismatch handling and channel downmix policy", "[complex_envelopes][channels_and_sr]")
{
    SECTION("Sample rate mismatch: adapts effective metadata to input rate")
    {
        auto gen = SyntheticEnvelopeControlGenerator::generatePitchSweepSignal(44100.0, 220.0, 440.0, 400.0);

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = gen.audioBuffer;
        input.sampleRate = 44100.0; // 44.1 kHz input
        input.nominalFrequencyHz = 220.0;

        ComplexEnvelopeAnalyzerConfig config;
        config.sampleRateHz = 48000.0; // Config has 48 kHz
        config.hopSizeSamples = 256;

        auto record = ComplexEnvelopeAnalyzer::analyze(input, config);

        // Effective metadata must reflect the true 44.1 kHz audio rate
        REQUIRE(record.temporalGrid.gridId == "grid_44100_hop256");
        REQUIRE(record.pitchTrajectory.spectralMetadata.sampleRateHz == 44100.0);
        REQUIRE_THAT(record.temporalGrid.hopMs, WithinAbs((256.0 / 44100.0) * 1000.0, 1e-4));
    }

    SECTION("Multichannel input downmixed to mono according to policy")
    {
        SyntheticEnvelopeControlGenerator::GenerationParams params;
        params.sampleRate = 48000.0;
        params.numChannels = 2; // Interleaved stereo
        params.totalDurationMs = 500.0;

        auto genStereo = SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal(params);

        ComplexEnvelopeAnalysisInput input;
        input.audioBuffer = genStereo.audioBuffer;
        input.numChannels = 2;
        input.downmixPolicy = ChannelDownmixPolicy::AverageToMono;
        input.sampleRate = 48000.0;
        input.nominalFrequencyHz = 261.6256;

        auto record = ComplexEnvelopeAnalyzer::analyze(input);
        REQUIRE(record.temporalGrid.frameCount > 0);
        REQUIRE(record.amplitudeTrajectory.points.size() == static_cast<size_t>(record.temporalGrid.frameCount));

        // MonoOnly policy rejects multichannel buffer cleanly
        input.downmixPolicy = ChannelDownmixPolicy::MonoOnly;
        auto recordRejected = ComplexEnvelopeAnalyzer::analyze(input);
        REQUIRE(recordRejected.temporalGrid.frameCount == 0);
    }
}

TEST_CASE("Phase 20.11.7-T2: Synthetic ground truth vs observed trajectory validation", "[complex_envelopes][ground_truth]")
{
    SyntheticEnvelopeControlGenerator::GenerationParams params;
    params.sampleRate = 48000.0;
    params.noteOnMs = 50.0;
    params.noteOffMs = 600.0;
    params.totalDurationMs = 900.0;
    params.baseFrequencyHz = 261.6256;

    auto gen = SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal(params);

    ComplexEnvelopeAnalysisInput input;
    input.audioBuffer = gen.audioBuffer;
    input.sampleRate = gen.sampleRate;
    input.noteOnSample = gen.noteOnSample;
    input.noteOffSample = gen.noteOffSample;
    input.nominalFrequencyHz = params.baseFrequencyHz;

    ComplexEnvelopeAnalyzerConfig config;
    config.pitchUnit = "Hz"; // Extract in Hz to compare with ground truth tolerance

    auto record = ComplexEnvelopeAnalyzer::analyze(input, config);

    // Verify Ground Truth tolerances on sustain region (e.g. frame around 300 ms)
    bool verifiedSustain = false;
    for (size_t i = 0; i < record.pitchTrajectory.points.size(); ++i)
    {
        const auto& ptPitch = record.pitchTrajectory.points[i];
        const auto& ptTimbre = record.timbreTrajectory.points[i];
        const auto& ptAmp = record.amplitudeTrajectory.points[i];

        if (ptPitch.timeMs >= 250.0 && ptPitch.timeMs <= 450.0)
        {
            verifiedSustain = true;
            // Pitch f0 must track 261.63 Hz within expectedF0ToleranceHz
            REQUIRE(ptPitch.status == "valid");
            REQUIRE(ptPitch.value.has_value());
            REQUIRE_THAT(*ptPitch.value, WithinAbs(params.baseFrequencyHz, gen.groundTruth.expectedF0ToleranceHz));

            // Timbre centroid is elevated due to phase distortion
            REQUIRE(ptTimbre.status == "valid");
            REQUIRE(ptTimbre.value.has_value());
            REQUIRE(*ptTimbre.value > params.baseFrequencyHz);

            // Amplitude during sustain: RMS is around -6.0 dBFS (peak 0.75 / sqrt(2) with PD) and normalized peak is around 0.75
            REQUIRE(ptAmp.status == "valid");
            REQUIRE(ptAmp.value.has_value());
            REQUIRE_THAT(*ptAmp.value, WithinAbs(-6.0, gen.groundTruth.expectedRmsToleranceDb));
            REQUIRE(ptAmp.amplitudeNormalized.has_value());
            REQUIRE_THAT(*ptAmp.amplitudeNormalized, WithinAbs(0.75, 0.08));
        }
    }
    REQUIRE(verifiedSustain);

    // Attack time in ground truth is ~60 ms
    REQUIRE(record.amplitudeTrajectory.attackTimeMs.has_value());
    REQUIRE_THAT(*record.amplitudeTrajectory.attackTimeMs, WithinAbs(60.0, 15.0));

    // Valid container record
    auto valRes = validateMultiDomainEnvelopeCaptureRecord(record);
    REQUIRE(valRes.valid);
}
