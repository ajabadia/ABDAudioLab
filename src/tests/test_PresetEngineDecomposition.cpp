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
        // 4 classic filters + 6 AIRA filters (01..06) = 10
        REQUIRE(filters.size() == 10);

        // 2 classic modulators + 6 AIRA modulators (12..15, 24, 25) = 8
        REQUIRE(modulations.size() == 8);

        // 4 classic dynamics + 12 AIRA dynamics/shapers (07..11, 21, 22, 26..29, 31) = 16
        REQUIRE(dynamics.size() == 16);

        // 3 oscillators + 3 time effects + 7 AIRA spatial (16..20, 23, 30) + 2 acoustic = 15
        REQUIRE(timeAcoustics.size() == 15);

        // 3 Casio CZ profiling presets (DCW, ENV, DCO) = 3
        REQUIRE(casioCz.size() == 3);

        // Total count: 10 + 8 + 16 + 15 + 3 = 52 presets exactly
        size_t subCatalogTotal = filters.size() + modulations.size() + dynamics.size() + timeAcoustics.size() + casioCz.size();
        REQUIRE(subCatalogTotal == 52);
    }

    SECTION("Facade unifies all presets with zero duplicates")
    {
        auto masterList = AutoTestPresetEngine::getAllPresets();
        REQUIRE(masterList.size() == 52);

        std::unordered_set<std::string> uniqueIds;
        for (const auto& p : masterList)
        {
            REQUIRE(!p.typologyId.empty());
            REQUIRE(!p.displayName.empty());
            auto [iter, inserted] = uniqueIds.insert(p.typologyId);
            INFO("Duplicate typologyId detected: " << p.typologyId);
            REQUIRE(inserted);
        }
        REQUIRE(uniqueIds.size() == 52);
    }

    SECTION("Exact query validation for key Roland AIRA modules")
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
    }
}
