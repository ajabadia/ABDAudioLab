#include <catch2/catch_test_macros.hpp>
#include "core/AutoTestPresetEngine.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("AutoTestPresetEngine - Repository and Taxonomy Coverage", "[core][presets][assistant]")
{
    auto allPresets = AutoTestPresetEngine::getAllPresets();

    // Verify comprehensive taxonomy count:
    // 3 Oscillators + 4 Filters + 2 Modulators + 4 Dynamics + 3 Time/Spatial + 31 AIRA + 2 Acoustics + 3 Casio CZ = 52 total
    REQUIRE(allPresets.size() == 52);

    SECTION("Coverage of Core Categories")
    {
        auto oscs = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::OscillatorsAndGenerators);
        auto flts = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::FiltersAndToneShapers);
        auto mods = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::ModulatorsAndEnvelopes);
        auto dyns = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::DynamicsAndNonLinear);
        auto time = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::TimeAndSpatialEffects);
        auto aira = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::EurorackAiraSubmodules);
        auto acst = AutoTestPresetEngine::getPresetsForCategory(ComponentCategory::AcousticsAndTransducers);

        REQUIRE(oscs.size() == 4); // 3 classic + 1 CZ DCO
        REQUIRE(flts.size() == 5); // 4 classic + 1 CZ DCW
        REQUIRE(mods.size() == 3); // 2 classic + 1 CZ ENV
        REQUIRE(dyns.size() >= 4);
        REQUIRE(time.size() >= 3);
        REQUIRE(aira.size() == 31);
        REQUIRE(acst.size() >= 2);
    }

    SECTION("Parameter Bounds and Timing Validity")
    {
        for (const auto& p : allPresets)
        {
            INFO("Testing preset: " << p.typologyId);
            REQUIRE(!p.typologyId.empty());
            REQUIRE(!p.displayName.empty());
            REQUIRE(!p.badgeText.empty());
            REQUIRE(!p.algorithmName.empty());

            // Check sample and buffer windows
            REQUIRE(p.recommendedSamples >= 4096);
            REQUIRE(p.recommendedFftSize >= 1024);

            // Check timing
            REQUIRE(p.burstDurationSec > 0.05f);
            REQUIRE(p.burstDurationSec <= 10.0f);
            REQUIRE(p.preRollMs >= 0.0f);
            REQUIRE(p.settlingWaitMs >= 0.0f);
            REQUIRE(p.recommendedSteps >= 1);
        }
    }

    SECTION("Query by ID and Fallback Handling")
    {
        auto ladder = AutoTestPresetEngine::getPresetById("vcf_ladder_ota_svf");
        REQUIRE(ladder.typologyId == "vcf_ladder_ota_svf");
        REQUIRE(ladder.stimulusType == audio::StimulusType::LogFarinaSweep);
        REQUIRE(ladder.burstDurationSec == 2.0f);

        auto aira21 = AutoTestPresetEngine::getPresetById("aira_21_ring_mod");
        REQUIRE(aira21.typologyId == "aira_21_ring_mod");
        REQUIRE(aira21.badgeText == "AIRA");

        auto czDcw = AutoTestPresetEngine::getPresetById("casio_cz_dcw_waveshaper");
        REQUIRE(czDcw.typologyId == "casio_cz_dcw_waveshaper");
        REQUIRE(czDcw.badgeText == "CZ-DCW");

        auto czEnv = AutoTestPresetEngine::getPresetById("casio_cz_multistep_envelope");
        REQUIRE(czEnv.typologyId == "casio_cz_multistep_envelope");
        REQUIRE(czEnv.badgeText == "CZ-ENV");

        auto fallback = AutoTestPresetEngine::getPresetById("non_existent_id");
        REQUIRE(fallback.typologyId == "vcf_ladder_ota_svf");
    }
}
