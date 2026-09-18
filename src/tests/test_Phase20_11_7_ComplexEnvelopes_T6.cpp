/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T6.cpp
 * @brief Catch2 unit tests for Phase 20.11.7 Increment 6:
 *        End-to-End Metrological Audit, Bit-Exact Repeatability,
 *        FAIR Provenance & Governance, and Phase Sealing.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "measurement/ComplexEnvelopeContracts.h"
#include "measurement/ComplexEnvelopeOrchestratorContracts.h"
#include "measurement/ComplexEnvelopeCaptureSession.h"
#include "measurement/TimingReferenceResolver.h"
#include "measurement/ObservableComparisonEngine.h"
#include "measurement/ComplexEnvelopeOrchestrator.h"
#include "measurement/ComplexEnvelopeExportContracts.h"
#include "measurement/ComplexEnvelopeSvgRenderer.h"
#include "measurement/ComplexEnvelopeHtmlReportGenerator.h"
#include "measurement/ComplexEnvelopeFairExporter.h"
#include "measurement/adapters/casio/CasioCz101NativeStateProvider.h"
#include "measurement/adapters/casio/CasioCz101SysExContracts.h"
#include "../synth/Sha256.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <string>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::measurement::adapters::casio;

namespace
{

constexpr double kPi = 3.14159265358979323846;

std::vector<float> generateDeterministicSyntheticPulse(size_t sampleCount, double freqHz, double sampleRateHz)
{
    std::vector<float> buf(sampleCount, 0.0f);
    for (size_t i = 0; i < sampleCount; ++i)
    {
        double t = static_cast<double>(i) / sampleRateHz;
        double env = std::exp(-t * 2.0); // Simple decay
        buf[i] = static_cast<float>(env * std::sin(2.0 * kPi * freqHz * t));
    }
    return buf;
}

std::string getFileSha256(const juce::File& f)
{
    juce::MemoryBlock mb;
    if (!f.loadFileAsData(mb)) return "";
    return abdaudiolab::synth::Sha256::computeHex(mb.getData(), mb.getSize());
}

} // anonymous namespace

TEST_CASE("Phase 20.11.7 T6: Unified End-to-End Pipeline Audit (T1 -> T5)",
          "[complex_envelopes][e2e_repeatability][audit]")
{
    const double sampleRate = 48000.0;
    const size_t totalSamples = 9600; // 200 ms

    // 1. Generate excitation signal & immutable raw capture
    auto rawSignal = generateDeterministicSyntheticPulse(totalSamples, 261.6256, sampleRate);
    RawAudioCapture rawCapture(rawSignal, sampleRate, 512, "SyntheticAcousticRig", "1000");
    std::string initialRawSha = rawCapture.getSha256();

    ComplexEnvelopeCaptureSession session(rawCapture);

    // 2. Configure calibration & stimulus
    AudioChainCalibration calib;
    calib.roundTripLatencySamples = 48; // 1 ms
    calib.roundTripLatencyMs = 1.0;
    calib.compensationDomain = CompensationDomain::TimeAxisOnly;

    ComplexEnvelopeStimulus stim;
    stim.stimulusId = "cz101_e2e_stimulus";
    stim.nominalFrequencyHz = 261.6256;
    stim.midiVelocity = 100;
    stim.noteDurationMs = 150.0;
    stim.totalDurationMs = 200.0;
    stim.expectedNoteOnSample = 480; // 10 ms
    stim.expectedNoteOffSample = 7680; // 160 ms

    // 3. Configure Casio CZ-101 native patch state (8 stages)
    CasioCz101NativePatchState patchState;
    patchState.modelIdentifier = "CZ-101";
    patchState.sourceSysExSha256 = "cafed00ddeadbeef1234567890abcdef";
    for (int i = 0; i < 8; ++i)
    {
        patchState.dcw1Timbre[i].stageIndex = i + 1;
        patchState.dcw1Timbre[i].rate = 60 + i * 4;
        patchState.dcw1Timbre[i].level = (i <= 3) ? (i * 25) : (100 - i * 10);
        patchState.dcw1Timbre[i].isSustainPoint = (i == 3);
        patchState.dcw1Timbre[i].isEndPoint = (i == 7);
    }

    CasioCz101NativeStateProvider provider(patchState);

    // 4. Run Orchestrator
    auto orchResult = ComplexEnvelopeOrchestrator::orchestrate(
        stim,
        session,
        calib,
        &provider);

    REQUIRE(orchResult.status == "success");
    CHECK(orchResult.timingResolution.status == "resolved");
    CHECK_FALSE(orchResult.comparisons.empty());

    // 5. Verify Raw Immutability
    CHECK(session.getRawCapture().getSha256() == initialRawSha);

    // 6. Export FAIR Container
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_T6_E2E_" + juce::String(juce::Random::getSystemRandom().nextInt()));

    auto cleanup = [&]() {
        if (tempDir.exists()) tempDir.deleteRecursively();
        juce::File stagingDir = tempDir.getParentDirectory().getChildFile(tempDir.getFileName() + "_staging");
        if (stagingDir.exists()) stagingDir.deleteRecursively();
    };

    cleanup();

    ComplexEnvelopeExportSpec spec;
    spec.experimentId = "e2e_cz101_audit";
    spec.timestampPolicy = TimestampPolicy::FixedForTest;

    auto nativeStages = provider.getNativeStageDescriptors("line1.dcw.envelope");

    juce::String exportErr;
    bool ok = ComplexEnvelopeFairExporter::exportContainer(
        tempDir, orchResult, spec, nativeStages, rawSignal, session.getCompensatedSamples(), exportErr);

    INFO("ExportContainer error: " << exportErr.toStdString());
    REQUIRE(ok);

    // 7. Validate Container Integrity
    juce::String auditErr;
    bool valid = ComplexEnvelopeFairExporter::validateContainerIntegrity(tempDir, auditErr);
    INFO("Integrity audit error: " << auditErr.toStdString());
    CHECK(valid);

    // 8. Verify FAIR Governance & Provenance Fields
    auto manifestJson = nlohmann::json::parse(tempDir.getChildFile("manifest.json").loadFileAsString().toStdString());
    CHECK(manifestJson.contains("license"));
    CHECK(manifestJson["license"]["dataLicense"] == "CC-BY-4.0");
    CHECK(manifestJson["license"]["softwareLicense"] == "Proprietary/Internal");
    CHECK(manifestJson["license"]["audioLicense"] == "not_specified");
    CHECK(manifestJson["license"].contains("thirdPartyNotice"));

    CHECK(manifestJson.contains("environment"));
    CHECK(manifestJson["environment"]["osFamily"] == "windows");
    CHECK(manifestJson["environment"]["architecture"] == "x86_64");

    CHECK(manifestJson.contains("measurementConditions"));
    CHECK(manifestJson["measurementConditions"]["sampleRateHz"] == 48000.0);

    CHECK(manifestJson.contains("limitations"));
    CHECK(manifestJson["limitations"].get<std::string>().find("perceptual proxies") != std::string::npos);

    cleanup();
}

TEST_CASE("Phase 20.11.7 T6: Strict Bit-Exact Repeatability Across Independent Runs",
          "[complex_envelopes][e2e_repeatability][determinism]")
{
    const double sampleRate = 48000.0;
    const size_t totalSamples = 4800; // 100 ms

    auto rawSignal = generateDeterministicSyntheticPulse(totalSamples, 440.0, sampleRate);

    ComplexEnvelopeStimulus stim;
    stim.stimulusId = "repeatability_stim";
    stim.nominalFrequencyHz = 440.0;
    stim.totalDurationMs = 100.0;

    AudioChainCalibration calib;
    calib.roundTripLatencySamples = 24;
    calib.compensationDomain = CompensationDomain::TimeAxisOnly;

    ComplexEnvelopeExportSpec spec;
    spec.experimentId = "repeatability_test";
    spec.timestampPolicy = TimestampPolicy::FixedForTest;

    juce::File tempDirA = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("ABDAudioLab_T6_PassA_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    juce::File tempDirB = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("ABDAudioLab_T6_PassB_" + juce::String(juce::Random::getSystemRandom().nextInt()));

    auto cleanup = [&]() {
        if (tempDirA.exists()) tempDirA.deleteRecursively();
        if (tempDirB.exists()) tempDirB.deleteRecursively();
    };

    cleanup();

    // Pass A
    {
        RawAudioCapture rawA(rawSignal, sampleRate);
        ComplexEnvelopeCaptureSession sessA(rawA);
        auto resA = ComplexEnvelopeOrchestrator::orchestrate(stim, sessA, calib);
        juce::String errA;
        REQUIRE(ComplexEnvelopeFairExporter::exportContainer(tempDirA, resA, spec, {}, rawSignal, {}, errA));
    }

    // Pass B
    {
        RawAudioCapture rawB(rawSignal, sampleRate);
        ComplexEnvelopeCaptureSession sessB(rawB);
        auto resB = ComplexEnvelopeOrchestrator::orchestrate(stim, sessB, calib);
        juce::String errB;
        REQUIRE(ComplexEnvelopeFairExporter::exportContainer(tempDirB, resB, spec, {}, rawSignal, {}, errB));
    }

    // Verify bit-exactness of all exported artifacts between Pass A and Pass B
    const std::vector<std::string> artifactPaths = {
        "manifest.json",
        "spec.json",
        "data/envelope_record.json",
        "data/comparison_report.json",
        "reports/complex_envelope_overlay.svg",
        "reports/envelope_report.html",
        "audio/raw_capture.wav"
    };

    for (const auto& relPath : artifactPaths)
    {
        juce::File fileA = tempDirA.getChildFile(relPath);
        juce::File fileB = tempDirB.getChildFile(relPath);

        REQUIRE(fileA.existsAsFile());
        REQUIRE(fileB.existsAsFile());

        std::string shaA = getFileSha256(fileA);
        std::string shaB = getFileSha256(fileB);

        INFO("Checking bit-exact match for: " << relPath);
        CHECK_FALSE(shaA.empty());
        CHECK(shaA == shaB);
    }

    cleanup();
}

TEST_CASE("Phase 20.11.7 T6: Architectural Asepsis Across Entire Pipeline",
          "[complex_envelopes][e2e_repeatability][asepsis]")
{
    // Ensure the entire end-to-end pipeline executes cleanly with a pure generic mock
    // having ZERO Casio or SysEx dependencies
    class GenericPureSynthProvider : public INativeStateProvider
    {
    public:
        std::string getModelIdentifier() const override { return "GenericAnalogDco"; }
        std::string getStateSha256() const override { return "aabbccddeeff00112233445566778899"; }
        std::vector<NativeEnvelopeBinding> getBindings() const override
        {
            return {
                { "vcf.envelope", EnvelopeDomain::Timbre, "time_target", "GenericAnalogDco", "aabbccddeeff00112233445566778899" },
                { "vca.envelope", EnvelopeDomain::Amplitude, "time_target", "GenericAnalogDco", "aabbccddeeff00112233445566778899" }
            };
        }
        std::vector<EnvelopeStageDescriptor> getNativeStageDescriptors(const std::string&) const override
        {
            EnvelopeStageDescriptor s1; s1.stageIndex = 1; s1.durationMs = 20.0; s1.targetLevel = 1.0;
            EnvelopeStageDescriptor s2; s2.stageIndex = 2; s2.durationMs = 80.0; s2.targetLevel = 0.4; s2.isSustainPoint = true;
            EnvelopeStageDescriptor s3; s3.stageIndex = 3; s3.durationMs = 50.0; s3.targetLevel = 0.0; s3.isEndKeyOnPoint = true;
            return { s1, s2, s3 };
        }
        std::optional<EnvelopeTrajectory> getObservableReference(const std::string&) const override
        {
            EnvelopeTrajectory traj;
            traj.domain = EnvelopeDomain::Timbre;
            EnvelopeObservationPoint p1; p1.frameIndex = 0; p1.timeMs = 0.0; p1.value = 0.0;
            EnvelopeObservationPoint p2; p2.frameIndex = 1; p2.timeMs = 50.0; p2.value = 0.8;
            traj.points = { p1, p2 };
            return traj;
        }
    };

    GenericPureSynthProvider genericProvider;
    auto rawSignal = generateDeterministicSyntheticPulse(2400, 440.0, 48000.0);
    RawAudioCapture raw(rawSignal, 48000.0);
    ComplexEnvelopeCaptureSession session(raw);

    ComplexEnvelopeStimulus stim;
    stim.stimulusId = "generic_asepsis_test";
    stim.totalDurationMs = 50.0;

    auto res = ComplexEnvelopeOrchestrator::orchestrate(stim, session, std::nullopt, &genericProvider);

    CHECK(res.status == "success");
    CHECK(res.comparisons.size() == 2);
    for (const auto& comp : res.comparisons)
    {
        CHECK(comp.comparisonLabel == "observable_agreement");
        CHECK(comp.phaseDistortionProxy == "not_claimed");
    }
}
