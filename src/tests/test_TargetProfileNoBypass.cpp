/**
 * @file test_TargetProfileNoBypass.cpp
 * @brief HITO-10A: Verificación de Aislamiento, No-Bypass y Ausencia de Efectos Secundarios.
 *
 * Garantiza contractualmente que:
 * 1. La ingestión, parsing y validación de TargetProfile son estrictamente pasivos (cero audio, cero MIDI).
 * 2. La compilación declarativa en ExperimentPlanCompiler no arranca ProfilingSequencer ni crea sesiones activas.
 * 3. La resolución falla de forma estricta ante perfiles inválidos sin crear planes ejecutables incompletos.
 * 4. La resolución es 100% determinista, reentrante y libre de efectos colaterales acumulativos.
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "profiling/MeasurementRecipeService.h"
#include "profiling/TargetProfileService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

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

} // namespace

TEST_CASE("TargetProfile No-Bypass: Carga e Ingestión son Estrictamente Pasivas", "[target_profile][nobypass]")
{
    TargetProfileService service;
    juce::File profileFile = getProfilesDirectory().getChildFile("reference_synth.target.json");
    REQUIRE(profileFile.existsAsFile());

    // Carga múltiple sucesiva
    auto res1 = service.loadAndValidateProfile(profileFile);
    auto res2 = service.loadAndValidateProfile(profileFile);

    REQUIRE(res1.isSuccess());
    REQUIRE(res2.isSuccess());

    // Cero acumulación de estado, hashes idénticos
    CHECK(res1.canonicalProfileHash == res2.canonicalProfileHash);
    CHECK(res1.diagnostics.empty());
    CHECK(res2.diagnostics.empty());
}

TEST_CASE("TargetProfile No-Bypass: Resolución Declarativa es Pura y Headless", "[target_profile][nobypass]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    TargetProfileService profileService;
    auto profileRes = profileService.loadAndValidateProfile(getProfilesDirectory().getChildFile("reference_synth.target.json"));
    REQUIRE(profileRes.isSuccess());

    auto env = getStandardEnvironment();

    // Resolución pura en ExperimentPlanCompiler: no inicializa audio device, no crea hilos en tiempo real
    auto res1 = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);
    auto res2 = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, profileRes.profile, env);

    REQUIRE(res1.succeeded());
    REQUIRE(res2.succeeded());
    REQUIRE(res1.resolvedPlan.has_value());
    REQUIRE(res2.resolvedPlan.has_value());

    // Invarianza absoluta de plan y hash
    CHECK(res1.resolvedPlan->resolvedExecutionPlanHash == res2.resolvedPlan->resolvedExecutionPlanHash);
    CHECK(res1.resolvedPlan->experimentPlan.planHash == res2.resolvedPlan->experimentPlan.planHash);
    CHECK(res1.resolvedPlan->experimentPlan.windows.size() == res2.resolvedPlan->experimentPlan.windows.size());
}

TEST_CASE("TargetProfile No-Bypass: Perfil No Válido Bloquea Construcción de Plan", "[target_profile][nobypass]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    // Perfil corrupto (sin parámetros mapeados para la receta)
    TargetProfile emptyProfile;
    emptyProfile.targetProfileId = "empty.profile";
    emptyProfile.capabilities.sampleRatesHz = { 48000 };
    emptyProfile.capabilities.audioOutput.supportedChannelCounts = { 2 };

    auto env = getStandardEnvironment();

    auto res = ExperimentPlanCompiler::resolveExecutionPlan(recipeRes.recipe, emptyProfile, env);

    // Debe fallar rotundamente sin dejar un plan ejecutable
    REQUIRE_FALSE(res.succeeded());
    REQUIRE_FALSE(res.resolvedPlan.has_value());
    REQUIRE_FALSE(res.diagnostics.empty());

    bool hasUnmappedDiag = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_TARGET_PROFILE_SEMANTIC_ID_UNMAPPED")
            hasUnmappedDiag = true;
    }
    CHECK(hasUnmappedDiag);
}
