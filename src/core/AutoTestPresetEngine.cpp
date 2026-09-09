/**
 * @file AutoTestPresetEngine.cpp
 * @brief Unified facade delegating to modular preset catalogs across synth, modular, and effects domains.
 * @author ABDSynths
 * @date 2026
 */

#include "AutoTestPresetEngine.h"
#include "presets/FilterPresetCatalog.h"
#include "presets/ModulationPresetCatalog.h"
#include "presets/DynamicsPresetCatalog.h"
#include "presets/TimeAcousticPresetCatalog.h"
#include "presets/CasioCzPresetCatalog.h"
#include <algorithm>

namespace abdaudiolab::core
{

std::vector<ComponentPresetRecommendation> AutoTestPresetEngine::getAllPresets()
{
    std::vector<ComponentPresetRecommendation> masterList;
    masterList.reserve(52);

    auto filters        = presets::FilterPresetCatalog::getPresets();
    auto modulations    = presets::ModulationPresetCatalog::getPresets();
    auto dynamics       = presets::DynamicsPresetCatalog::getPresets();
    auto timeAcoustics  = presets::TimeAcousticPresetCatalog::getPresets();
    auto casioCz        = presets::CasioCzPresetCatalog::getPresets();

    masterList.insert(masterList.end(), filters.begin(), filters.end());
    masterList.insert(masterList.end(), modulations.begin(), modulations.end());
    masterList.insert(masterList.end(), dynamics.begin(), dynamics.end());
    masterList.insert(masterList.end(), timeAcoustics.begin(), timeAcoustics.end());
    masterList.insert(masterList.end(), casioCz.begin(), casioCz.end());

    return masterList;
}

std::vector<ComponentPresetRecommendation> AutoTestPresetEngine::getPresetsForCategory(ComponentCategory cat)
{
    auto all = getAllPresets();
    std::vector<ComponentPresetRecommendation> filtered;
    for (const auto& p : all)
    {
        if (p.category == cat)
            filtered.push_back(p);
    }
    return filtered;
}

ComponentPresetRecommendation AutoTestPresetEngine::getPresetById(const std::string& typologyId)
{
    auto all = getAllPresets();
    for (const auto& p : all)
    {
        if (p.typologyId == typologyId)
            return p;
    }
    return getDefaultFilterPreset();
}

ComponentPresetRecommendation AutoTestPresetEngine::getDefaultFilterPreset()
{
    return getPresetById("vcf_ladder_ota_svf");
}

} // namespace abdaudiolab::core
