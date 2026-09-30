/**
 * @file test_TargetProfileResolution.cpp
 * @brief HITO-10A: Verificación de Resolución Declarativa de MeasurementRecipe + TargetProfile + ExecutionEnvironment.
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "core/LabResourcePaths.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/TargetProfileService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getPresetsDirectory() { return abdaudiolab::core::profilingPresetsDir(); }

inline juce::File getProfilesDirectory() { return abdaudiolab::core::canonicalTargetsDir(); }

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

} // namespace

TEST_CASE("TargetProfile Resolution: Resolución Exitosa de Recipe + Profile + Environment", "[target_profile][resolution]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();

    // Resolución declarativa unificada en ExperimentPlanCompiler
    auto res = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);

    REQUIRE(res.succeeded());
    REQUIRE(res.resolvedPlan.has_value());
    CHECK(res.diagnostics.empty());

    const auto& resolved = *res.resolvedPlan;
    CHECK(resolved.environment.sampleRate == 48000.0);
    CHECK(resolved.environment.channels == 2);
    CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());
    // 3 puntos * 1 nota * 3 reps = 9 ventanas de observacion
    CHECK(resolved.experimentPlan.windows.size() == 9);
}

TEST_CASE("TargetProfile Resolution: Diagnóstico ante semanticId No Mapeado", "[target_profile][resolution]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    // Mutar la receta para solicitar un parámetro que ReferenceSynth no tiene mapeado
    MeasurementRecipe unmappedRecipe = recipeRes.recipe;
    unmappedRecipe.measurement.points[0].semanticId = "unmapped_distortion_drive";
    unmappedRecipe.measurement.points[0].parameter = "unmapped_distortion_drive";

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();

    auto res = ExperimentPlanCompiler::resolveExecutionPlan(unmappedRecipe, profileRes.profile, env);

    REQUIRE_FALSE(res.succeeded());
    REQUIRE_FALSE(res.resolvedPlan.has_value());
    REQUIRE_FALSE(res.diagnostics.empty());

    bool foundDiagnostic = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_TARGET_PROFILE_SEMANTIC_ID_UNMAPPED" && d.jsonPointer == "/measurement/points/0/semanticId")
            foundDiagnostic = true;
    }
    CHECK(foundDiagnostic);
}

TEST_CASE("TargetProfile Resolution: Diagnóstico ante Frecuencia de Muestreo Incompatible", "[target_profile][resolution]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    // Entorno físico a 192 kHz (no soportado por ReferenceSynth en su perfil: [44100, 48000, 96000])
    auto env = getStandardEnvironment();
    env.sampleRate = 192000.0;

    auto res = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);

    REQUIRE_FALSE(res.succeeded());
    REQUIRE_FALSE(res.resolvedPlan.has_value());

    bool foundSampleRateDiag = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED")
            foundSampleRateDiag = true;
    }
    CHECK(foundSampleRateDiag);
}

TEST_CASE("TargetProfile Resolution: Diagnóstico ante Canales Incompatibles", "[target_profile][resolution]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    // Entorno físico con 8 canales (ReferenceSynth solo soporta [2])
    auto env = getStandardEnvironment();
    env.channels = 8;

    auto res = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);

    REQUIRE_FALSE(res.succeeded());
    REQUIRE_FALSE(res.resolvedPlan.has_value());

    bool foundChannelsDiag = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_CAPABILITY_CHANNELS_UNSUPPORTED")
            foundChannelsDiag = true;
    }
    CHECK(foundChannelsDiag);
}
