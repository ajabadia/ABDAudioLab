/**
 * @file test_OfflineReanalysis.cpp
 * @brief Unit tests for Offline Session Re-Analysis (1.7.1) using Catch2.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../core/SessionManager.h"
#include "../math/LabAnalyticEngine.h"
#include "../math/FarinaDeconvolver.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace abdaudiolab;

TEST_CASE("[OfflineReanalysis] Recompute Filter response from in-memory and disk audio", "[OfflineReanalysis]")
{
    core::SessionManager sm;
    sm.resetSession();

    core::SessionManifest manifest;
    manifest.sessionTitle = "Filter_Reanalysis_Test";
    manifest.sampleRate = 48000.0;
    manifest.hardwareId = "TEST_SYNTH";

    gui::TestConfiguration tc;
    tc.testName = "VCF_Cutoff_Sweep";
    tc.stimulusType = audio::StimulusType::LogFarinaSweep;
    tc.burstDurationSec = 0.5f;
    manifest.tests.push_back(tc);

    sm.setManifest(manifest);

    // Synthesize a dummy audio sweep pass and deliberate initial incorrect metrics
    constexpr double sampleRate = 48000.0;
    constexpr double durationSec = 0.5;
    auto sweep = math::FarinaDeconvolver::generateLogFarinaSweep(sampleRate, durationSec, 20.0f, 20000.0f);

    exporting::MeasuredPoint pt;
    pt.pointId = "P_001";
    pt.testId = "VCF_Cutoff_Sweep";
    pt.blockType = "SpectrumFilter";
    pt.stimulusType = "LogFarinaSweep";
    pt.param1Normalized = 0.5f;
    pt.irSamples = sweep;
    pt.muSigmaValue = { 0.0f, 0.0f };       // Deliberately empty
    pt.secondaryValue = { 0.0f, 0.0f };
    pt.thdPercent = 0.0f;
    pt.snrDb = 0.0f;

    sm.addMeasuredPoint(pt);
    REQUIRE(sm.getPointCount() == 1);

    int progressReports = 0;
    int reanalyzed = sm.reanalyzeSessionOffline([&progressReports](float prog, const core::SessionManager::ReanalysisProgress& info) {
        juce::ignoreUnused(prog, info);
        progressReports++;
    });

    REQUIRE(reanalyzed == 1);
    REQUIRE(progressReports >= 1);

    const auto* updatedPt = sm.getPoint(0);
    REQUIRE(updatedPt != nullptr);
    // After reanalysis of a raw log sweep through inverse filter, cutoff and resonance must be non-zero
    CHECK(updatedPt->muSigmaValue.mean > 20.0f);
    CHECK(updatedPt->snrDb > 10.0f);
    CHECK(sm.isDirty() == true);
}

TEST_CASE("[OfflineReanalysis] Recompute TimeDynamic ADSR Envelope", "[OfflineReanalysis]")
{
    core::SessionManager sm;
    sm.resetSession();

    core::SessionManifest manifest;
    manifest.sessionTitle = "ADSR_Reanalysis_Test";
    manifest.sampleRate = 48000.0;
    manifest.hardwareId = "TEST_ENVELOPE";

    gui::TestConfiguration tc;
    tc.testName = "VCA_ADSR_Attack";
    tc.stimulusType = audio::StimulusType::SyncPulses3;
    tc.burstDurationSec = 0.5f;
    manifest.tests.push_back(tc);

    sm.setManifest(manifest);

    // Synthesize a clean exponential attack envelope
    constexpr int numSamples = 24000; // 0.5s at 48kHz
    std::vector<float> envAudio(numSamples, 0.0f);
    for (int i = 0; i < 4800; ++i) // 100ms attack
    {
        envAudio[i] = static_cast<float>(i) / 4800.0f;
    }
    for (int i = 4800; i < numSamples; ++i)
    {
        envAudio[i] = 1.0f; // Sustain
    }

    exporting::MeasuredPoint pt;
    pt.pointId = "P_ENV_01";
    pt.testId = "VCA_ADSR_Attack";
    pt.blockType = "TimeDynamic";
    pt.stimulusType = "SyncPulses3";
    pt.param1Normalized = 0.1f;
    pt.irSamples = envAudio;
    pt.muSigmaValue = { 0.0f, 0.0f }; // Uncomputed
    pt.secondaryValue = { 0.0f, 0.0f };

    sm.addMeasuredPoint(pt);

    int reanalyzed = sm.reanalyzeSessionOffline();
    REQUIRE(reanalyzed == 1);

    const auto* updatedPt = sm.getPoint(0);
    REQUIRE(updatedPt != nullptr);
    // Attack time should be around 100 ms
    CHECK(updatedPt->muSigmaValue.mean > 50.0f);
    CHECK(updatedPt->secondaryValue.mean > 0.5f); // Sustain around 1.0
}

TEST_CASE("[OfflineReanalysis] Empty session safety", "[OfflineReanalysis]")
{
    core::SessionManager sm;
    sm.resetSession();
    REQUIRE(sm.getPointCount() == 0);

    int reanalyzed = sm.reanalyzeSessionOffline();
    CHECK(reanalyzed == 0);
}
