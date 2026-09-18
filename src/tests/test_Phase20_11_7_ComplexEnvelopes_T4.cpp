/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T4.cpp
 * @brief Catch2 unit test suite for Phase 20.11.7 Increment 4:
 *        Complex Envelope Orchestrator, Immutable Dual-Layer Capture Session,
 *        Measured Timing Reference Resolver with Ambiguity Detection,
 *        and Common-Space Observable Comparison Engine.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "measurement/ComplexEnvelopeOrchestratorContracts.h"
#include "measurement/ComplexEnvelopeCaptureSession.h"
#include "measurement/TimingReferenceResolver.h"
#include "measurement/ObservableComparisonEngine.h"
#include "measurement/ComplexEnvelopeOrchestrator.h"
#include "measurement/ComplexEnvelopeAnalyzer.h"

#include "measurement/adapters/casio/CasioCz101NativeStateProvider.h"
#include "measurement/adapters/casio/CasioCz101SysExContracts.h"

#include <vector>
#include <cmath>
#include <string>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::measurement::adapters::casio;

namespace
{

constexpr double kPi = 3.14159265358979323846;

std::vector<float> generateDelayedPulse(size_t totalSamples, size_t delaySamples, float amplitude = 1.0f)
{
    std::vector<float> sig(totalSamples, 0.0f);
    for (size_t i = delaySamples; i < totalSamples && (i - delaySamples) < 64; ++i)
    {
        double t = static_cast<double>(i - delaySamples) / 64.0;
        sig[i] = amplitude * static_cast<float>(std::sin(kPi * t));
    }
    return sig;
}

} // anonymous namespace

TEST_CASE("Phase 20.11.7 T4: Immutable Raw Audio Capture and Latency Compensation",
          "[complex_envelopes][orchestrator][capture_session]")
{
    std::vector<float> testAudio = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f };
    RawAudioCapture raw(testAudio, 48000.0, 512, "TestDevice", "12345");

    SECTION("Enforces raw capture immutability via std::span and constant hash")
    {
        CHECK(raw.size() == 8);
        CHECK_FALSE(raw.empty());
        CHECK_FALSE(raw.getSha256().empty());
        CHECK(raw.getRawFormat() == "PCM_FLOAT");
        CHECK(raw.getSampleType() == "float32");

        const auto span = raw.getSamples();
        CHECK(span.size() == 8);
        CHECK(span[0] == 0.1f);
        CHECK(span[7] == 0.8f);
    }

    SECTION("Applies integer sample shift latency compensation without mutating raw capture")
    {
        std::string initialRawSha = raw.getSha256();
        ComplexEnvelopeCaptureSession session(raw);

        AudioChainCalibration calib;
        calib.roundTripLatencySamples = 2;
        calib.roundTripLatencyMs = (2.0 / 48000.0) * 1000.0;
        calib.measurementMethod = "loopback_correlation";

        auto transform = session.applyCalibration(calib);
        CHECK(transform == AlignmentTransform::IntegerSampleShift);
        CHECK(session.hasCompensatedAudio());

        // Verify raw audio remained 100% untouched
        CHECK(session.getRawCapture().getSha256() == initialRawSha);
        CHECK(session.getRawCapture().size() == 8);

        // Verify compensated audio is shifted by 2 samples
        auto compSpan = session.getCompensatedSamples();
        REQUIRE(compSpan.size() == 6);
        CHECK(compSpan[0] == 0.3f);
        CHECK(compSpan[5] == 0.8f);
        CHECK_FALSE(session.getCompensatedSha256().empty());
        CHECK(session.getCompensatedSha256() != initialRawSha);
    }

    SECTION("CompensationDomain::TimeAxisOnly preserves audio samples untouched")
    {
        std::string initialRawSha = raw.getSha256();
        ComplexEnvelopeCaptureSession session(raw);

        AudioChainCalibration calib;
        calib.roundTripLatencySamples = 4;
        calib.compensationDomain = CompensationDomain::TimeAxisOnly;

        auto transform = session.applyCalibration(calib);
        CHECK(transform == AlignmentTransform::None);
        CHECK_FALSE(session.hasCompensatedAudio());

        // Audio remains identical to raw
        auto compSpan = session.getCompensatedSamples();
        CHECK(compSpan.size() == 8);
        CHECK(session.getCompensatedSha256() == initialRawSha);
    }

    SECTION("Rejects out-of-bounds latency shifts gracefully")
    {
        ComplexEnvelopeCaptureSession session(raw);
        AudioChainCalibration calib;
        calib.roundTripLatencySamples = 20; // larger than 8 samples buffer
        calib.compensationDomain = CompensationDomain::AudioSamples;

        auto transform = session.applyCalibration(calib);
        CHECK(transform == AlignmentTransform::Rejected);
        CHECK_FALSE(session.hasCompensatedAudio());
    }
}

TEST_CASE("Phase 20.11.7 T4: Timing Reference Resolver - Multi-Mode Alignment & Ambiguity",
          "[complex_envelopes][orchestrator][timing_resolver]")
{
    SECTION("Mode 1: ProvidedEvent returns exact declared offsets with full confidence")
    {
        auto res = TimingReferenceResolver::resolveFromProvidedEvent(1200, 48000.0);
        CHECK(res.status == "resolved");
        CHECK(res.type == TimingReferenceType::ProvidedEvent);
        CHECK(res.offsetSamples == 1200);
        CHECK(res.confidence == 1.0);
        CHECK(res.ambiguityMargin == 1.0);
        CHECK(res.peakRatio == 0.0);
    }

    SECTION("Mode 2: AudioOnset detects energy threshold on synthetic burst")
    {
        auto burst = SyntheticEnvelopeControlGenerator::generateNoiseBurstSignal(48000.0, 300.0);
        auto res = TimingReferenceResolver::resolveFromAudioOnset(burst.audioBuffer, 48000.0);

        CHECK(res.status == "resolved");
        CHECK(res.type == TimingReferenceType::AudioOnset);
        CHECK(res.offsetSamples.has_value());
        CHECK(*res.offsetSamples > 0);
        CHECK(res.peakRatio.has_value());
        CHECK(res.ambiguityMargin.has_value());
    }

    SECTION("Mode 2: AudioOnset reports insufficient_signal on absolute silence")
    {
        std::vector<float> silence(2048, 0.0f);
        auto res = TimingReferenceResolver::resolveFromAudioOnset(silence, 48000.0);
        CHECK(res.status == "insufficient_signal");
    }

    SECTION("Mode 3: LoopbackCorrelation detects exact lag unambiguously")
    {
        const size_t expectedLag = 120;
        auto stim = generateDelayedPulse(512, 0, 1.0f);
        auto cap  = generateDelayedPulse(2048, expectedLag, 0.8f);

        auto res = TimingReferenceResolver::resolveFromLoopbackCorrelation(stim, cap, 48000.0, 500);
        CHECK(res.status == "resolved");
        CHECK(res.type == TimingReferenceType::LoopbackCorrelation);
        REQUIRE(res.offsetSamples.has_value());
        CHECK(*res.offsetSamples == static_cast<int>(expectedLag));
        CHECK(res.confidence.value_or(0.0) > 0.90);
        CHECK(res.ambiguityMargin.value_or(0.0) > 0.15);
        CHECK(res.peakRatio.value_or(1.0) < 0.85);
    }

    SECTION("Mode 3: LoopbackCorrelation flags ambiguous peaks when two reflections are near-identical")
    {
        auto stim = generateDelayedPulse(64, 0, 1.0f);
        // Create captured signal with two identical peaks at lag 50 and lag 120
        std::vector<float> multiPeak(1024, 0.0f);
        for (size_t i = 0; i < 64; ++i)
        {
            float v = static_cast<float>(std::sin(kPi * (static_cast<double>(i) / 64.0)));
            multiPeak[50 + i] = v;
            multiPeak[120 + i] = v * 0.98f; // nearly identical second peak (peakRatio ~0.98 >= 0.85)
        }

        TimingReferenceResolver::Config cfg;
        cfg.ambiguityThreshold = 0.15;
        cfg.maxPeakRatio = 0.85;
        auto res = TimingReferenceResolver::resolveFromLoopbackCorrelation(stim, multiPeak, 48000.0, 500, cfg);

        CHECK(res.status == "ambiguous");
        CHECK(res.ambiguityMargin.has_value());
        CHECK(res.peakRatio.has_value());
        CHECK(*res.ambiguityMargin <= 0.15);
        CHECK(*res.peakRatio >= 0.85);
    }
}

TEST_CASE("Phase 20.11.7 T4: Observable Comparison Engine - Common-Space Honest Metrics",
          "[complex_envelopes][orchestrator][comparison_engine]")
{
    SECTION("Computes high Pearson correlation and low RMSE on concordant trajectories")
    {
        EnvelopeTrajectory obs;
        obs.domain = EnvelopeDomain::Timbre;
        obs.temporalGridId = "grid_01";

        EnvelopeTrajectory ref;
        ref.domain = EnvelopeDomain::Timbre;
        ref.temporalGridId = "grid_01";

        for (int i = 0; i < 20; ++i)
        {
            double t = i * 10.0;
            double vObs = std::sin(t / 100.0);
            double vRef = std::sin(t / 100.0) * 1.02;

            EnvelopeObservationPoint ptObs; ptObs.frameIndex = i; ptObs.timeMs = t; ptObs.value = vObs;
            EnvelopeObservationPoint ptRef; ptRef.frameIndex = i; ptRef.timeMs = t; ptRef.value = vRef;
            obs.points.push_back(ptObs);
            ref.points.push_back(ptRef);
        }

        auto rep = ObservableComparisonEngine::compare(obs, ref, "line1.dcw.envelope");

        CHECK(rep.comparisonStatus == "compared");
        CHECK(rep.comparisonLabel == "observable_agreement");
        CHECK(rep.phaseDistortionProxy == "not_claimed");
        REQUIRE(rep.correlation.has_value());
        CHECK(*rep.correlation > 0.99);
        REQUIRE(rep.trajectoryRmse.has_value());
        CHECK(*rep.trajectoryRmse < 0.05);
        CHECK(rep.alignmentTransform == AlignmentTransform::None);
    }

    SECTION("Handles zero-variance constant trajectories: correlation is null and status is not_observable")
    {
        EnvelopeTrajectory constObs;
        constObs.domain = EnvelopeDomain::Timbre;
        EnvelopeTrajectory constRef;
        constRef.domain = EnvelopeDomain::Timbre;

        for (int i = 0; i < 10; ++i)
        {
            EnvelopeObservationPoint pO; pO.frameIndex = i; pO.timeMs = i * 10.0; pO.value = 1000.0; // flat
            EnvelopeObservationPoint pR; pR.frameIndex = i; pR.timeMs = i * 10.0; pR.value = 1000.0; // flat
            constObs.points.push_back(pO);
            constRef.points.push_back(pR);
        }

        auto rep = ObservableComparisonEngine::compare(constObs, constRef, "line1.dcw.envelope");
        CHECK(rep.comparisonStatus == "not_observable");
        CHECK_FALSE(rep.correlation.has_value()); // Strictly nullopt, NEVER 0.0
        CHECK(rep.limitations.find("Zero variance") != std::string::npos);
    }

    SECTION("Applies linear interpolation when observation grids differ")
    {
        EnvelopeTrajectory obs;
        obs.domain = EnvelopeDomain::Timbre;
        obs.temporalGridId = "grid_hop10";
        // Grid with hop 10 ms
        for (int i = 0; i < 10; ++i)
        {
            EnvelopeObservationPoint p; p.frameIndex = i; p.timeMs = i * 10.0; p.value = i * 0.1;
            obs.points.push_back(p);
        }

        EnvelopeTrajectory ref;
        ref.domain = EnvelopeDomain::Timbre;
        ref.temporalGridId = "grid_hop25";
        // Grid with hop 25 ms
        for (int i = 0; i < 5; ++i)
        {
            EnvelopeObservationPoint p; p.frameIndex = i; p.timeMs = i * 25.0; p.value = i * 0.25;
            ref.points.push_back(p);
        }

        auto rep = ObservableComparisonEngine::compare(obs, ref, "line1.dcw.envelope");
        CHECK(rep.alignmentTransform == AlignmentTransform::Interpolation);
        CHECK(rep.comparisonStatus == "compared");
        CHECK(rep.validPairCount.has_value());
        CHECK(*rep.validPairCount >= 3);
    }

    SECTION("Returns not_compared when reference is nullopt")
    {
        EnvelopeTrajectory obs;
        obs.domain = EnvelopeDomain::Timbre;
        EnvelopeObservationPoint p; p.timeMs = 0.0; p.value = 1.0;
        obs.points.push_back(p);

        auto rep = ObservableComparisonEngine::compare(obs, std::nullopt, "line1.dcw.envelope");
        CHECK(rep.comparisonStatus == "not_compared");
    }
}

TEST_CASE("Phase 20.11.7 T4: Central ComplexEnvelopeOrchestrator - End-to-End Execution",
          "[complex_envelopes][orchestrator][e2e]")
{
    // Generate synthetic 8-stage test signal
    auto synthSig = SyntheticEnvelopeControlGenerator::generateStandard8StageTestSignal();

    RawAudioCapture raw(synthSig.audioBuffer, synthSig.sampleRate, 512, "SyntheticSource", "start_0");
    ComplexEnvelopeCaptureSession session(raw);

    ComplexEnvelopeStimulus stim;
    stim.stimulusId = "stim_cz_test";
    stim.nominalFrequencyHz = 261.6256;
    stim.expectedNoteOnSample = synthSig.noteOnSample;
    stim.expectedNoteOffSample = synthSig.noteOffSample;

    SECTION("Orchestrates with CasioCz101NativeStateProvider without coupling core")
    {
        CasioCz101NativePatchState patchState;
        patchState.lineSelect = 0; // Line 1 only
        patchState.modelIdentifier = "CZ-101";
        patchState.sourceSysExSha256 = "cafebabedeadbeef";
        patchState.dcw1Timbre[0].stageIndex = 1;
        patchState.dcw1Timbre[0].rate = 90;
        patchState.dcw1Timbre[0].level = 90;
        patchState.dcw1Timbre[0].isEndPoint = true;

        CasioCz101NativeStateProvider provider(patchState);

        auto orchResult = ComplexEnvelopeOrchestrator::orchestrate(
            stim,
            session,
            std::nullopt,
            &provider);

        CHECK(orchResult.status == "success");
        CHECK_FALSE(orchResult.comparisons.empty());
        CHECK(orchResult.timingResolution.status == "resolved");

        // Verify that comparisons honor honest labels
        for (const auto& comp : orchResult.comparisons)
        {
            CHECK(comp.comparisonLabel == "observable_agreement");
            CHECK(comp.phaseDistortionProxy == "not_claimed");
        }
    }

    SECTION("Orchestrates without native state provider: status is success and comparison is not_compared")
    {
        auto orchResult = ComplexEnvelopeOrchestrator::orchestrate(
            stim,
            session,
            std::nullopt,
            nullptr); // nullptr provider

        CHECK(orchResult.status == "success");
        REQUIRE(orchResult.comparisons.size() == 1);
        CHECK(orchResult.comparisons[0].comparisonStatus == "not_compared");
    }

    SECTION("Repeatability: Identical capture and calibration produces identical hashes and metrics")
    {
        ComplexEnvelopeCaptureSession sessionA(raw);
        ComplexEnvelopeCaptureSession sessionB(raw);

        auto resA = ComplexEnvelopeOrchestrator::orchestrate(stim, sessionA);
        auto resB = ComplexEnvelopeOrchestrator::orchestrate(stim, sessionB);

        CHECK(resA.sourceRawAudioSha256 == resB.sourceRawAudioSha256);
        CHECK(resA.compensatedAudioSha256 == resB.compensatedAudioSha256);
        CHECK(resA.captureRecord.temporalGrid.frameCount == resB.captureRecord.temporalGrid.frameCount);
    }
}

TEST_CASE("Phase 20.11.7 T4: Architectural Asepsis - Pure Core Independence",
          "[complex_envelopes][orchestrator][architecture]")
{
    // Ensure that INativeStateProvider can be implemented by an entirely generic dummy class
    // with ZERO dependencies on Casio or SysEx headers
    class GenericMockProvider : public INativeStateProvider
    {
    public:
        std::string getModelIdentifier() const override { return "GenericSynth"; }
        std::string getStateSha256() const override { return "00112233"; }
        std::vector<NativeEnvelopeBinding> getBindings() const override
        {
            return { { "vcf.envelope", EnvelopeDomain::Timbre, "time_target", "GenericSynth", "00112233" } };
        }
        std::vector<EnvelopeStageDescriptor> getNativeStageDescriptors(const std::string&) const override
        {
            return {};
        }
        std::optional<EnvelopeTrajectory> getObservableReference(const std::string&) const override
        {
            EnvelopeTrajectory t;
            t.domain = EnvelopeDomain::Timbre;
            EnvelopeObservationPoint pt;
            pt.frameIndex = 0;
            pt.timeMs = 0.0;
            pt.value = 0.5;
            pt.unit = "normalized_0_1";
            t.points = { pt };
            return t;
        }
    };

    GenericMockProvider genericProvider;
    CHECK(genericProvider.getModelIdentifier() == "GenericSynth");
    CHECK(genericProvider.getBindings().size() == 1);

    SECTION("Canonical JSON representation is strictly deterministic")
    {
        TimingResolution tr;
        tr.type = TimingReferenceType::LoopbackCorrelation;
        tr.offsetSamples = 100;
        tr.offsetFractionalSamples = 100.25;
        tr.confidence = 0.95;
        tr.ambiguityMargin = 0.20;
        tr.peakRatio = 0.80;
        tr.status = "resolved";

        auto jsonA = tr.toCanonicalJson();
        auto jsonB = tr.toCanonicalJson();
        CHECK(jsonA.dump() == jsonB.dump());
        CHECK(jsonA["peakRatio"] == 0.80);
        CHECK(jsonA["ambiguityMargin"] == 0.20);
        CHECK(jsonA["status"] == "resolved");
    }
}
