#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "math/PresetAudit5DEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::math;
using namespace abd::audio;

namespace {

AudioABSignal makeTestSignal(const juce::String& id, float freqHz, float amp, int numSamples = 24000, double sr = 48000.0)
{
    AudioABSignal sig;
    sig.sourceId = id;
    sig.sampleRate = sr;
    sig.numChannels = 1;
    sig.originalNumSamples = numSamples;
    sig.buffer.setSize(1, numSamples);
    sig.buffer.clear();

    float* w = sig.buffer.getWritePointer(0);
    const float omega = 2.0f * std::numbers::pi_v<float> * freqHz / static_cast<float>(sr);
    for (int i = 0; i < numSamples; ++i)
    {
        w[i] = amp * std::sin(omega * static_cast<float>(i));
    }
    return sig;
}

AudioABSignal makeSilentSignal(const juce::String& id, int numSamples = 24000, double sr = 48000.0)
{
    AudioABSignal sig;
    sig.sourceId = id;
    sig.sampleRate = sr;
    sig.numChannels = 1;
    sig.originalNumSamples = numSamples;
    sig.buffer.setSize(1, numSamples);
    sig.buffer.clear();
    return sig;
}

} // namespace

TEST_CASE("PresetAudit5DEngine: Identical signal passes audit", "[PresetAudit5D][AudioComparator]")
{
    auto ref = makeTestSignal("ref_lead", 440.0f, 0.25f); // ~ -12 dBFS
    auto cand = makeTestSignal("cand_lead", 440.0f, 0.25f);

    auto result = PresetAudit5DEngine::auditPreset(1, "A11_SawLead", ref, cand);

    REQUIRE(result.passed());
    REQUIRE(result.verdictLevel == "PASS");
    REQUIRE(result.diagnosticCode == Diagnostic5D::kOkIdentity);
    REQUIRE(result.correlationPeak == Catch::Approx(1.0).margin(0.02));
    REQUIRE(std::abs(result.rmsDeltaDb) == Catch::Approx(0.0).margin(0.05));
    REQUIRE(result.peakDbfs < -6.0);
}

TEST_CASE("PresetAudit5DEngine: Clipping and Silence Dropouts detect Failures", "[PresetAudit5D][AudioComparator]")
{
    SECTION("Clipping overload triggers DIAG_FAIL_CLIPPING_OVERLOAD")
    {
        auto ref = makeTestSignal("ref_pad", 440.0f, 0.5f);
        auto cand = makeTestSignal("cand_pad", 440.0f, 1.05f); // Clips above 0 dBFS

        auto result = PresetAudit5DEngine::auditPreset(2, "A12_PolyPad", ref, cand);

        REQUIRE_FALSE(result.passed());
        REQUIRE(result.verdictLevel == "FAIL");
        REQUIRE(result.diagnosticCode == Diagnostic5D::kFailClippingOverload);
        REQUIRE(result.peakDbfs >= -0.01);
    }

    SECTION("Silence dropout triggers DIAG_FAIL_SILENCE_DROPOUT")
    {
        auto ref = makeTestSignal("ref_bass", 110.0f, 0.35f); // Active bass
        auto cand = makeSilentSignal("cand_bass");            // Muted / dropout

        auto result = PresetAudit5DEngine::auditPreset(3, "A13_SubBass", ref, cand);

        REQUIRE_FALSE(result.passed());
        REQUIRE(result.verdictLevel == "FAIL");
        REQUIRE(result.diagnosticCode == Diagnostic5D::kFailSilenceDropout);
        REQUIRE(result.rmsDbfs < -60.0);
    }
}

TEST_CASE("PresetAudit5DEngine: Spectral and Dynamic Divergence", "[PresetAudit5D][AudioComparator]")
{
    SECTION("Moderate level discrepancy triggers DIAG_WARN_DYNAMIC_MISMATCH")
    {
        auto ref = makeTestSignal("ref_pluck", 440.0f, 0.25f);
        // Candidate with +1.8 dB gain (0.25 * 1.2302 = 0.3075f)
        auto cand = makeTestSignal("cand_pluck", 440.0f, 0.3075f);

        AudioABVerdictTolerances tolerances;
        tolerances.maxRmsDeltaDb = 1.5; // Strict 1.5 dB tolerance
        tolerances.maxSpectralDeltaDb = 2.5;

        auto result = PresetAudit5DEngine::auditPreset(4, "A14_PluckSynth", ref, cand, {}, tolerances);

        REQUIRE(result.verdictLevel == "WARN");
        REQUIRE(result.diagnosticCode == Diagnostic5D::kWarnDynamicMismatch);
        REQUIRE(std::abs(result.rmsDeltaDb) > 1.5);
    }

    SECTION("Severe spectral divergence triggers Fail or Warn diagnostic")
    {
        auto ref = makeTestSignal("ref_warm", 220.0f, 0.3f);
        auto cand = makeTestSignal("cand_bright", 4000.0f, 0.3f); // Huge spectral gap

        AudioABVerdictTolerances tolerances;
        tolerances.maxSpectralDeltaDb = 3.0;

        auto result = PresetAudit5DEngine::auditPreset(5, "A15_WarmString", ref, cand, {}, tolerances);

        REQUIRE_FALSE(result.passed());
        REQUIRE((result.verdictLevel == "FAIL" || result.verdictLevel == "WARN"));
        REQUIRE((result.diagnosticCode == Diagnostic5D::kFailSpectralDivergence ||
                 result.diagnosticCode == Diagnostic5D::kFailAlignmentDesync ||
                 result.diagnosticCode == Diagnostic5D::kWarnHarmonicSpread));
    }
}

TEST_CASE("PresetAudit5DEngine: Batch preset auditing and statistical aggregation", "[PresetAudit5D][AudioComparator]")
{
    std::vector<std::pair<AudioABSignal, AudioABSignal>> pairs;
    std::vector<std::string> names = {
        "P01_InitLead",
        "P02_BritePad",
        "P03_DeepBass",
        "P04_HotPluck",
        "P05_ClippedOrgan"
    };

    // 1. Identical Pass
    pairs.push_back({ makeTestSignal("r1", 440.0f, 0.2f), makeTestSignal("c1", 440.0f, 0.2f) });
    // 2. Identical Pass
    pairs.push_back({ makeTestSignal("r2", 880.0f, 0.2f), makeTestSignal("c2", 880.0f, 0.2f) });
    // 3. Identical Pass
    pairs.push_back({ makeTestSignal("r3", 220.0f, 0.2f), makeTestSignal("c3", 220.0f, 0.2f) });
    // 4. Warn: +1.8 dB difference (0.2 * 1.2302 = 0.246f) with strict 1.5 dB tolerance
    pairs.push_back({ makeTestSignal("r4", 330.0f, 0.2f), makeTestSignal("c4", 330.0f, 0.246f) });
    // 5. Fail: Clipping
    pairs.push_back({ makeTestSignal("r5", 440.0f, 0.2f), makeTestSignal("c5", 440.0f, 1.05f) });

    AudioABVerdictTolerances tolerances;
    tolerances.maxRmsDeltaDb = 1.5;

    auto summary = PresetAudit5DEngine::auditBatch(pairs, names, {}, tolerances);

    REQUIRE(summary.totalPresets == 5);
    REQUIRE(summary.passedCount == 3);
    REQUIRE(summary.warnedCount == 1);
    REQUIRE(summary.failedCount == 1);
    REQUIRE(summary.passRatePercent() == Catch::Approx(60.0).margin(0.1));
    REQUIRE(summary.presetResults.size() == 5);
    REQUIRE(summary.presetResults[0].passed());
    REQUIRE(summary.presetResults[4].diagnosticCode == Diagnostic5D::kFailClippingOverload);
}
