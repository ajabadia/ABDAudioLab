/**
 * @file test_ProfilingSessionBuilder.cpp
 * @brief Unit and characterization tests for pure ProfilingSessionBuilder.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../core/ProfilingSessionBuilder.h"
#include <string>
#include <vector>

using namespace abdaudiolab;
using namespace abdaudiolab::core;

namespace
{

SessionMetadataConfig createValidConfig()
{
    SessionMetadataConfig cfg;
    cfg.hardwareName = "ROLAND_AIRA_S1";
    cfg.targetModule = "FILTER_RESONANCE_SWEEP";
    cfg.operatorMode = "AUTOMATED_SYSEX";
    cfg.sampleRate = 48000.0;
    cfg.bitDepth = 24;
    cfg.timestampIso8601 = "2026-09-19T08:30:00Z";
    cfg.operatorNotes = "Test characterization run";
    cfg.ambientTemperatureC = 23.5f;
    cfg.warmupTimeMinutes = 20;
    return cfg;
}

HardwareContractSnapshot createTestHardwareSnapshot(bool autonomous = false)
{
    HardwareContractSnapshot hw;
    hw.selectedHardwareId = "AIRA_S1";
    hw.selectedFunctionId = "FILTER_RESONANCE_SWEEP";
    hw.isAutonomousSynth = autonomous;

    HardwareContract c;
    c.id = "AIRA_S1";
    c.displayName = "Roland AIRA S-1";
    c.deviceType = "AUTOMATED_SYSEX";

    HardwareFunction f;
    f.id = "FILTER_RESONANCE_SWEEP";
    f.name = "Resonance Sweep";
    f.blockType = "SpectrumFilter";
    f.measurementRecipe.recipeType = "LEGATO_PITCH_SWEEP";
    f.measurementRecipe.excitationMode = ExcitationMode::AudioSweep;

    c.functions.push_back(f);
    hw.contracts.push_back(c);

    return hw;
}

gui::QueueItem createNoiseFloorTest()
{
    gui::QueueItem item;
    item.id = "test_noise_floor";
    item.title = "Noise Floor Baseline";
    item.badgeText = "NOI";
    item.stimulusType = audio::StimulusType::Silence;
    item.burstDurationSec = 0.8f;
    item.totalPoints = 1;
    return item;
}

gui::QueueItem createFilterSweepTest()
{
    gui::QueueItem item;
    item.id = "test_filter_sweep";
    item.hwId = "AIRA_S1";
    item.funcId = "FILTER_RESONANCE_SWEEP";
    item.title = "Filter Resonance Matrix";
    item.badgeText = "FLT";
    item.stimulusType = audio::StimulusType::LogFarinaSweep;
    item.burstDurationSec = 1.2f;

    gui::ControlStepConfig c1;
    c1.name = "Cutoff";
    c1.type = "Knob";
    c1.steps = 3;
    c1.minPct = 10.0f;
    c1.maxPct = 90.0f;

    gui::ControlStepConfig c2;
    c2.name = "Resonance";
    c2.type = "Knob";
    c2.steps = 2;
    c2.minPct = 0.0f;
    c2.maxPct = 100.0f;

    item.controls.push_back(c1);
    item.controls.push_back(c2);
    item.totalPoints = 6; // 3 * 2

    return item;
}

} // namespace

TEST_CASE("ProfilingSessionBuilder: Input Validation & Error Handling", "[ProfilingSessionBuilder]")
{
    auto config = createValidConfig();
    auto hw = createTestHardwareSnapshot();

    SECTION("Invalid non-positive sample rate is rejected")
    {
        config.sampleRate = 0.0;
        std::vector<gui::QueueItem> q = { createNoiseFloorTest() };
        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == BuildStatus::InvalidSampleRate);
        REQUIRE(res.errorCode == "INVALID_SAMPLE_RATE");
    }

    SECTION("Empty queue produces EmptyQueue status")
    {
        std::vector<gui::QueueItem> emptyQueue;
        auto res = ProfilingSessionBuilder::buildFromQueue(emptyQueue, hw, config);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == BuildStatus::EmptyQueue);
        REQUIRE(res.errorCode == "EMPTY_QUEUE");
    }

    SECTION("Empty patch list produces InvalidPatch status")
    {
        std::vector<gui::QueueItem> q = { createNoiseFloorTest() };
        std::vector<PatchPoint> emptyPatch;
        auto res = ProfilingSessionBuilder::buildPatch(emptyPatch, q, hw, config);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == BuildStatus::InvalidPatch);
        REQUIRE(res.errorCode == "EMPTY_PATCH_POINTS");
    }

    SECTION("Out-of-bounds patch coordinates produce NO_VALID_PATCH_POINTS error")
    {
        std::vector<gui::QueueItem> q = { createNoiseFloorTest() };
        std::vector<PatchPoint> badPatch = { { 5, 0 }, { -1, 2 } };
        auto res = ProfilingSessionBuilder::buildPatch(badPatch, q, hw, config);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == BuildStatus::InvalidPatch);
        REQUIRE(res.errorCode == "NO_VALID_PATCH_POINTS");
    }
}

TEST_CASE("ProfilingSessionBuilder: Pure Determinism and Immutability", "[ProfilingSessionBuilder]")
{
    auto config = createValidConfig();
    auto hw = createTestHardwareSnapshot();
    std::vector<gui::QueueItem> q = { createNoiseFloorTest(), createFilterSweepTest() };

    SECTION("Queue input is strictly not modified")
    {
        const auto originalQueue = q;
        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE(res.succeeded());
        REQUIRE(q.size() == originalQueue.size());
        REQUIRE(q[0].id == originalQueue[0].id);
        REQUIRE(q[1].controls.size() == originalQueue[1].controls.size());
    }

    SECTION("Two consecutive builds with identical inputs yield identical JSON serialization")
    {
        auto res1 = ProfilingSessionBuilder::buildFromQueue(q, hw, config);
        auto res2 = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE(res1.succeeded());
        REQUIRE(res2.succeeded());
        REQUIRE(res1.session.saveProfileToJson() == res2.session.saveProfileToJson());
    }
}

TEST_CASE("ProfilingSessionBuilder: Test Cases and Cartesian Generation", "[ProfilingSessionBuilder]")
{
    auto config = createValidConfig();
    auto hw = createTestHardwareSnapshot(false);

    SECTION("Noise floor test produces 1 NoiseFloor test case")
    {
        std::vector<gui::QueueItem> q = { createNoiseFloorTest() };
        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE(res.succeeded());
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 1);
        REQUIRE(tcs[0].functionalBlockType == "NoiseFloor");
        REQUIRE(tcs[0].stimulusType == audio::StimulusType::Silence);
        REQUIRE(tcs[0].globalPointIndex == 0);
        REQUIRE(tcs[0].pointId == "P_001");
    }

    SECTION("Skipped tests are faithfully omitted")
    {
        auto noise = createNoiseFloorTest();
        noise.isSkipped = true;
        auto filter = createFilterSweepTest();
        std::vector<gui::QueueItem> q = { noise, filter };

        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);
        REQUIRE(res.succeeded());
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 6); // Only filter test cases
        REQUIRE(tcs[0].testId == filter.title.toStdString());
        REQUIRE(tcs[0].globalPointIndex == 0); // Index starts at 0 since skipped didn't increment
    }

    SECTION("Multiple tests preserve order and monotonic globalPointIndex")
    {
        std::vector<gui::QueueItem> q = { createNoiseFloorTest(), createFilterSweepTest() };
        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE(res.succeeded());
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 7); // 1 noise + 6 filter

        for (size_t i = 0; i < tcs.size(); ++i)
        {
            REQUIRE(tcs[i].globalPointIndex == static_cast<int>(i));
            std::string expectedId = std::string("P_") + (i < 9 ? "00" : "0") + std::to_string(i + 1);
            REQUIRE(tcs[i].pointId == expectedId);
        }
    }

    SECTION("Autonomous synth flag forces MidiNotes and Silence stimulus")
    {
        auto autoHw = createTestHardwareSnapshot(true);
        std::vector<gui::QueueItem> q = { createFilterSweepTest() };

        auto res = ProfilingSessionBuilder::buildFromQueue(q, autoHw, config);
        REQUIRE(res.succeeded());
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 6);
        for (const auto& tc : tcs)
        {
            REQUIRE(tc.isAutonomousSynth == true);
            REQUIRE(tc.excitationMode == ExcitationMode::MidiNotes);
            REQUIRE(tc.stimulusType == audio::StimulusType::Silence);
        }
    }

    SECTION("Cartesian product bounds and parameter step normalization")
    {
        std::vector<gui::QueueItem> q = { createFilterSweepTest() };
        auto res = ProfilingSessionBuilder::buildFromQueue(q, hw, config);

        REQUIRE(res.succeeded());
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 6);

        // First point: Cutoff at minNorm (0.10), Res at minNorm (0.00)
        REQUIRE(tcs[0].parameterSteps.size() == 2);
        REQUIRE(tcs[0].parameterSteps[0].paramName == "Cutoff");
        REQUIRE(tcs[0].parameterSteps[0].normalizedValue == Catch::Approx(0.10f));
        REQUIRE(tcs[0].parameterSteps[1].paramName == "Resonance");
        REQUIRE(tcs[0].parameterSteps[1].normalizedValue == Catch::Approx(0.00f));

        // Last point: Cutoff at maxNorm (0.90), Res at maxNorm (1.00)
        REQUIRE(tcs[5].parameterSteps[0].normalizedValue == Catch::Approx(0.90f));
        REQUIRE(tcs[5].parameterSteps[1].normalizedValue == Catch::Approx(1.00f));
    }
}

TEST_CASE("ProfilingSessionBuilder: Patch Profiling Precision", "[ProfilingSessionBuilder]")
{
    auto config = createValidConfig();
    auto hw = createTestHardwareSnapshot(false);
    std::vector<gui::QueueItem> q = { createNoiseFloorTest(), createFilterSweepTest() };

    SECTION("Patching specific point generates exactly requested target with globalPointIndex")
    {
        // Patch point in test 1 (Filter Sweep), sub-point 2 (0-based)
        std::vector<PatchPoint> patchList = { { 1, 2 } };
        auto res = ProfilingSessionBuilder::buildPatch(patchList, q, hw, config);

        REQUIRE(res.succeeded());
        REQUIRE(res.session.isPatchSession() == true);
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 1);
        REQUIRE(tcs[0].queueItemIndex == 1);
        REQUIRE(tcs[0].pointIndexInTest == 3); // 1-based
        // Session offset for test 1 is test 0 size (1) -> global index 1 + 2 = 3
        REQUIRE(tcs[0].globalPointIndex == 3);
        REQUIRE(tcs[0].pointId == "P_004");
    }

    SECTION("Multiple patch points preserve order and metadata")
    {
        std::vector<PatchPoint> patchList = { { 0, 0 }, { 1, 5 } };
        auto res = ProfilingSessionBuilder::buildPatch(patchList, q, hw, config);

        REQUIRE(res.succeeded());
        REQUIRE(res.session.isPatchSession() == true);
        const auto& tcs = res.session.getTestCases();
        REQUIRE(tcs.size() == 2);
        REQUIRE(tcs[0].pointId == "P_001");
        REQUIRE(tcs[1].pointId == "P_007"); // 1 + 6 = 7
    }
}
