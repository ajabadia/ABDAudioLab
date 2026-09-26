/**
 * @file test_TargetProfileLegacyParity.cpp
 * @brief HITO-10B: Demostración de Paridad Exacta entre Ruta C++ Histórica y Ruta Declarativa TargetProfile.
 *
 * Verifica la ecuación de equivalencia estricta:
 *   Legacy ReferenceSynth == Declarative TargetProfile ReferenceSynth
 *
 * Contratos validados:
 * 1. Paridad en quick_vcf_3pts.json (semántica, offsets, eventos, ventanas y hashes canónicos idénticos).
 * 2. Paridad en standard_vcf_11pts.json (22 ventanas, 66 eventos, igualdad estricta de plan y hashes).
 * 3. Paridad en exhaustive_synth_full.json (72 ventanas, 216 eventos, múltiples notas y parámetros).
 * 4. Disponibilidad y Preservación del Fallback Legacy (la ruta legacy funciona sin TargetProfile).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <string>
#include <vector>

#include "profiling/MeasurementRecipeService.h"
#include "profiling/TargetProfileService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "support/ResolvedExecutionPlanParity.h"

using namespace abdaudiolab::profiling;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::test::support;

namespace
{

juce::File getPresetsDirectory()
{
    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile("presets/profiling");
    if (!dir.isDirectory())
    {
        dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory()
                    .getChildFile("presets/profiling");
    }
    return dir;
}

juce::File getProfilesDirectory()
{
    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile("profiles/targets");
    if (!dir.isDirectory())
    {
        dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory()
                    .getChildFile("profiles/targets");
    }
    return dir;
}

ExecutionEnvironment getStandardEnvironment()
{
    ExecutionEnvironment env;
    env.driver = "MockAudioEngine";
    env.sampleRate = 48000.0;
    env.blockSize = 256;
    env.channels = 2;
    env.deviceName = "VirtualLoopback";
    env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput", "ParameterAutomation" };
    return env;
}

TargetSelectionState getReferenceSynthTargetState()
{
    TargetSelectionState t;
    t.targetId = "reference-synth";
    t.targetName = "ReferenceSynth";
    t.manufacturer = "ABD AudioLab";
    t.version = "1.0.0";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    return t;
}

} // namespace

TEST_CASE("TargetProfile Parity: quick_vcf_3pts (Legacy == Declarative)", "[target_profile][legacy_parity]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();
    auto targetState = getReferenceSynthTargetState();

    // 1. Ruta Legacy C++
    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(*legacyResolve.resolvedPlan, targetState);

    // 2. Ruta Declarativa TargetProfile
    auto declResolve = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);
    REQUIRE(declResolve.succeeded());
    REQUIRE(declResolve.resolvedPlan.has_value());
    auto declSession = ExperimentPlanCompiler::createProfilingSession(*declResolve.resolvedPlan, targetState);

    // 3. Verificación de paridad estricta campo a campo mediante ResolvedExecutionPlanParity
    requireEquivalentExperimentPlans(legacyResolve.resolvedPlan->experimentPlan, declResolve.resolvedPlan->experimentPlan);
    requireEquivalentResolvedExecutionPlans(*legacyResolve.resolvedPlan, *declResolve.resolvedPlan);
    requireEquivalentProfilingSessions(legacySession, declSession);

    // 4. Paridad de hashes canónicos criptográficos
    CHECK(legacyResolve.resolvedPlan->resolvedExecutionPlanHash == declResolve.resolvedPlan->resolvedExecutionPlanHash);
    CHECK(legacyResolve.resolvedPlan->experimentPlan.planHash == declResolve.resolvedPlan->experimentPlan.planHash);
}

TEST_CASE("TargetProfile Parity: standard_vcf_11pts (Legacy == Declarative)", "[target_profile][legacy_parity]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("standard_vcf_11pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();
    auto targetState = getReferenceSynthTargetState();

    // 1. Ruta Legacy
    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(*legacyResolve.resolvedPlan, targetState);

    // 2. Ruta Declarativa
    auto declResolve = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);
    REQUIRE(declResolve.succeeded());
    REQUIRE(declResolve.resolvedPlan.has_value());
    auto declSession = ExperimentPlanCompiler::createProfilingSession(*declResolve.resolvedPlan, targetState);

    // 3. Paridad estricta
    requireEquivalentExperimentPlans(legacyResolve.resolvedPlan->experimentPlan, declResolve.resolvedPlan->experimentPlan);
    requireEquivalentResolvedExecutionPlans(*legacyResolve.resolvedPlan, *declResolve.resolvedPlan);
    requireEquivalentProfilingSessions(legacySession, declSession);

    CHECK(legacyResolve.resolvedPlan->resolvedExecutionPlanHash == declResolve.resolvedPlan->resolvedExecutionPlanHash);
}

TEST_CASE("TargetProfile Parity: exhaustive_synth_full (Legacy == Declarative)", "[target_profile][legacy_parity]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("exhaustive_synth_full.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();
    auto targetState = getReferenceSynthTargetState();

    // 1. Ruta Legacy
    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(*legacyResolve.resolvedPlan, targetState);

    // 2. Ruta Declarativa
    auto declResolve = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);
    REQUIRE(declResolve.succeeded());
    REQUIRE(declResolve.resolvedPlan.has_value());
    auto declSession = ExperimentPlanCompiler::createProfilingSession(*declResolve.resolvedPlan, targetState);

    // 3. Paridad estricta
    requireEquivalentExperimentPlans(legacyResolve.resolvedPlan->experimentPlan, declResolve.resolvedPlan->experimentPlan);
    requireEquivalentResolvedExecutionPlans(*legacyResolve.resolvedPlan, *declResolve.resolvedPlan);
    requireEquivalentProfilingSessions(legacySession, declSession);

    CHECK(legacyResolve.resolvedPlan->resolvedExecutionPlanHash == declResolve.resolvedPlan->resolvedExecutionPlanHash);
}

TEST_CASE("TargetProfile Parity: Legacy Fallback Remains Available", "[target_profile][legacy_parity][fallback]")
{
    // Verificación explícita de que la ruta legacy permanece completamente disponible y no depende de TargetProfile
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    auto env = getStandardEnvironment();
    auto targetState = getReferenceSynthTargetState();

    // Ejecutar ruta legacy pura sin invocar TargetProfileService ni cargar ningún .target.json
    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);

    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());
    CHECK(legacyResolve.diagnostics.empty());

    const auto& resolved = *legacyResolve.resolvedPlan;
    CHECK(resolved.experimentPlan.windows.size() == 9);
    CHECK(resolved.experimentPlan.events.size() == 27);
    CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());

    auto session = ExperimentPlanCompiler::createProfilingSession(resolved, targetState);
    CHECK(session.getTestCases().size() == 9);
}
