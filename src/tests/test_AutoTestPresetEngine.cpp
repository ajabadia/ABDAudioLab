#include <catch2/catch_test_macros.hpp>
#include "core/AutoTestPresetEngine.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("AutoTestPresetEngine - Repository and Taxonomy Coverage", "[core][presets][assistant]")
{
    auto allPresets = AutoTestPresetEngine::getAllPresets();

    // Verify comprehensive taxonomy count:
    // 3 Oscillators + 5 Filters + 2 Modulators + 6 Dynamics + 4 Time/Spatial + 31 AIRA + 2 Acoustics + 3 Casio CZ = 56 total
    REQUIRE(allPresets.size() == 56);

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
        REQUIRE(flts.size() == 6); // 5 classic + 1 CZ DCW
        REQUIRE(mods.size() == 3); // 2 classic + 1 CZ ENV
        REQUIRE(dyns.size() == 6); // 6 classic (VCA, Waveshaper, Overdrive Pedal, Tape, Compressor, Ring Mod)
        REQUIRE(time.size() == 4); // 4 classic (Delay, Modulation, Rotary/Tremolo, Reverb)
        REQUIRE(aira.size() == 31); // 31 Roland AIRA submodules
        REQUIRE(acst.size() == 2); // 2 acoustics (Cabinet, Mic Preamp)
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

        // Fase 19.7 New Presets
        auto pedal = AutoTestPresetEngine::getPresetById("sat_guitar_overdrive_pedal");
        REQUIRE(pedal.typologyId == "sat_guitar_overdrive_pedal");
        REQUIRE(pedal.badgeText == "PEDAL");
        REQUIRE(pedal.stimulusType == audio::StimulusType::LogFarinaSweep);
        REQUIRE(pedal.recommendedSteps == 8);
        REQUIRE(pedal.burstDurationSec == 2.0f);

        auto svf = AutoTestPresetEngine::getPresetById("filter_tpt_state_variable");
        REQUIRE(svf.typologyId == "filter_tpt_state_variable");
        REQUIRE(svf.badgeText == "SVF");
        REQUIRE(svf.stimulusType == audio::StimulusType::LogFarinaSweep);

        auto tape = AutoTestPresetEngine::getPresetById("dyn_tape_saturation_preamp");
        REQUIRE(tape.typologyId == "dyn_tape_saturation_preamp");
        REQUIRE(tape.badgeText == "TAPE");
        REQUIRE(tape.stimulusType == audio::StimulusType::AmplitudeRamp);

        auto rot = AutoTestPresetEngine::getPresetById("fx_rotary_tremolo_vibrato");
        REQUIRE(rot.typologyId == "fx_rotary_tremolo_vibrato");
        REQUIRE(rot.badgeText == "ROT");
        REQUIRE(rot.stimulusType == audio::StimulusType::SineWave1kHz);

        auto fallback = AutoTestPresetEngine::getPresetById("non_existent_id");
        REQUIRE(fallback.typologyId == "vcf_ladder_ota_svf");
    }

    SECTION("Query Helper Methods (hasPreset, getPresetsForBadge, getAllTypologyIds)")
    {
        REQUIRE(AutoTestPresetEngine::hasPreset("sat_guitar_overdrive_pedal"));
        REQUIRE(AutoTestPresetEngine::hasPreset("filter_tpt_state_variable"));
        REQUIRE(AutoTestPresetEngine::hasPreset("vcf_ladder_ota_svf"));
        REQUIRE(!AutoTestPresetEngine::hasPreset("non_existent_preset_xyz"));

        auto pedalPresets = AutoTestPresetEngine::getPresetsForBadge("PEDAL");
        REQUIRE(pedalPresets.size() == 1);
        REQUIRE(pedalPresets[0].typologyId == "sat_guitar_overdrive_pedal");

        auto airaPresets = AutoTestPresetEngine::getPresetsForBadge("AIRA");
        REQUIRE(airaPresets.size() == 31);

        auto allIds = AutoTestPresetEngine::getAllTypologyIds();
        REQUIRE(allIds.size() == 56);
    }
}
