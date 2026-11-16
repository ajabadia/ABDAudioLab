#include <catch2/catch_test_macros.hpp>
#include "core/AutoTestPresetEngine.h"
#include "core/presets/FilterPresetCatalog.h"
#include "core/presets/ModulationPresetCatalog.h"
#include "core/presets/DynamicsPresetCatalog.h"
#include "core/presets/TimeAcousticPresetCatalog.h"
#include "core/presets/CasioCzPresetCatalog.h"
#include <unordered_set>

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("PresetEngineDecomposition: Sub-catalogs match master aggregated taxonomy", "[core][presets][refactor]")
{
    auto filters       = presets::FilterPresetCatalog::getPresets();
    auto modulations   = presets::ModulationPresetCatalog::getPresets();
    auto dynamics      = presets::DynamicsPresetCatalog::getPresets();
    auto timeAcoustics = presets::TimeAcousticPresetCatalog::getPresets();
    auto casioCz       = presets::CasioCzPresetCatalog::getPresets();

    SECTION("Validate sub-catalog counts and coverage")
    {
        // 5 classic filters + 6 AIRA filters (01..06) = 11
        REQUIRE(filters.size() == 11);

        // 2 classic modulators + 6 AIRA modulators (12..15, 24, 25) = 8
        REQUIRE(modulations.size() == 8);

        // 6 classic dynamics + 12 AIRA dynamics/shapers (07..11, 21, 22, 26..29, 31) = 18
        REQUIRE(dynamics.size() == 18);

        // 3 oscillators + 4 time effects + 7 AIRA spatial (16..20, 23, 30) + 2 acoustic = 16
        REQUIRE(timeAcoustics.size() == 16);

        // 3 Casio CZ profiling presets (DCW, ENV, DCO) = 3
        REQUIRE(casioCz.size() == 3);

        // Total count: 11 + 8 + 18 + 16 + 3 = 56 presets exactly
        size_t subCatalogTotal = filters.size() + modulations.size() + dynamics.size() + timeAcoustics.size() + casioCz.size();
        REQUIRE(subCatalogTotal == 56);
    }

    SECTION("Facade unifies all presets with zero duplicates")
    {
        auto masterList = AutoTestPresetEngine::getAllPresets();
        REQUIRE(masterList.size() == 56);

        std::unordered_set<std::string> uniqueIds;
        for (const auto& p : masterList)
        {
            REQUIRE(!p.typologyId.empty());
            REQUIRE(!p.displayName.empty());
            auto [iter, inserted] = uniqueIds.insert(p.typologyId);
            INFO("Duplicate typologyId detected: " << p.typologyId);
            REQUIRE(inserted);
        }
        REQUIRE(uniqueIds.size() == 56);
    }

    SECTION("Exact query validation for key Roland AIRA modules and new 19.7 presets")
    {
        // Roland AIRA 07: Vacuum Tube Warmth & Clipper (Torcido tube algorithm)
        auto tubeClipper = AutoTestPresetEngine::getPresetById("aira_07_tube_clipper");
        REQUIRE(tubeClipper.typologyId == "aira_07_tube_clipper");
        REQUIRE(tubeClipper.displayName == "Roland AIRA: 07. Vacuum Tube Warmth & Clipper");
        REQUIRE(tubeClipper.category == ComponentCategory::EurorackAiraSubmodules);
        REQUIRE(tubeClipper.badgeText == "AIRA");
        REQUIRE(tubeClipper.stimulusType == audio::StimulusType::AmplitudeRamp);

        // Classic VCF Ladder filter
        auto vcfLadder = AutoTestPresetEngine::getPresetById("vcf_ladder_ota_svf");
        REQUIRE(vcfLadder.typologyId == "vcf_ladder_ota_svf");
        REQUIRE(vcfLadder.stimulusType == audio::StimulusType::LogFarinaSweep);

        // New 19.7 Guitar Overdrive Pedal Preset
        auto pedal = AutoTestPresetEngine::getPresetById("sat_guitar_overdrive_pedal");
        REQUIRE(pedal.typologyId == "sat_guitar_overdrive_pedal");
        REQUIRE(pedal.badgeText == "PEDAL");
        REQUIRE(pedal.stimulusType == audio::StimulusType::LogFarinaSweep);
        REQUIRE(pedal.recommendedSteps == 8);

        // New 19.7 TPT State Variable Filter
        auto svf = AutoTestPresetEngine::getPresetById("filter_tpt_state_variable");
        REQUIRE(svf.typologyId == "filter_tpt_state_variable");
        REQUIRE(svf.badgeText == "SVF");
        REQUIRE(svf.stimulusType == audio::StimulusType::LogFarinaSweep);

        // New 19.7 Analog Tape Preamp
        auto tape = AutoTestPresetEngine::getPresetById("dyn_tape_saturation_preamp");
        REQUIRE(tape.typologyId == "dyn_tape_saturation_preamp");
        REQUIRE(tape.badgeText == "TAPE");
        REQUIRE(tape.stimulusType == audio::StimulusType::AmplitudeRamp);

        // New 19.7 Rotary Speaker / Tremolo
        auto rot = AutoTestPresetEngine::getPresetById("fx_rotary_tremolo_vibrato");
        REQUIRE(rot.typologyId == "fx_rotary_tremolo_vibrato");
        REQUIRE(rot.badgeText == "ROT");
        REQUIRE(rot.stimulusType == audio::StimulusType::SineWave1kHz);
    }
}
