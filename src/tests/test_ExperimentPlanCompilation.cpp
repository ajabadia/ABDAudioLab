#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "core/LabResourcePaths.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-09A: Compilacion y resolucion determinista de ExperimentPlan", "[recipe][compilation]")
{
    MeasurementRecipeService service;

    SECTION("Compilacion de las tres recetas de referencia normativas")
    {
        const auto presetsDir = abdaudiolab::core::profilingPresetsDir();

        // 1. quick_vcf_3pts.json
        const auto quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
        REQUIRE(quickFile.existsAsFile());
        const auto quickLoad = service.loadAndValidate(quickFile);
        REQUIRE(quickLoad.isSuccess());
        CHECK_FALSE(quickLoad.recipeDocumentHash.empty());

        const auto quickPlan = ExperimentPlanCompiler::compileToExperimentPlan(quickLoad.recipe);
        CHECK(quickPlan.recipeId == "org.abd.profiling.vcf.quick-3pts");
        CHECK_FALSE(quickPlan.planHash.empty());
        // 3 puntos * 1 nota * 3 reps = 9 ventanas de observacion
        CHECK(quickPlan.windows.size() == 9);
        // Cada punto genera 3 eventos (Param, NoteOn, NoteOff) = 27 eventos
        CHECK(quickPlan.events.size() == 27);

        // 2. standard_vcf_11pts.json
        const auto stdFile = presetsDir.getChildFile("standard_vcf_11pts.json");
        REQUIRE(stdFile.existsAsFile());
        const auto stdLoad = service.loadAndValidate(stdFile);
        REQUIRE(stdLoad.isSuccess());
        CHECK_FALSE(stdLoad.recipeDocumentHash.empty());

        const auto stdPlan = ExperimentPlanCompiler::compileToExperimentPlan(stdLoad.recipe);
        CHECK(stdPlan.recipeId == "org.abd.profiling.vcf.standard-11pts");
        // 11 puntos * 1 nota * 2 reps = 22 ventanas
        CHECK(stdPlan.windows.size() == 22);
        CHECK(stdPlan.events.size() == 66);

        // 3. exhaustive_synth_full.json
        const auto exhFile = presetsDir.getChildFile("exhaustive_synth_full.json");
        REQUIRE(exhFile.existsAsFile());
        const auto exhLoad = service.loadAndValidate(exhFile);
        REQUIRE(exhLoad.isSuccess());
        CHECK_FALSE(exhLoad.recipeDocumentHash.empty());

        const auto exhPlan = ExperimentPlanCompiler::compileToExperimentPlan(exhLoad.recipe);
        CHECK(exhPlan.recipeId == "org.abd.profiling.synth.exhaustive-full");
        // 8 puntos * 3 notas * 3 reps = 72 ventanas
        CHECK(exhPlan.windows.size() == 72);
        CHECK(exhPlan.events.size() == 216);
    }

    SECTION("Resolucion frente a entorno fisico compatible")
    {
        const auto presetsDir = abdaudiolab::core::profilingPresetsDir();
        const auto quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
        const auto quickLoad = service.loadAndValidate(quickFile);
        REQUIRE(quickLoad.isSuccess());

        const auto plan = ExperimentPlanCompiler::compileToExperimentPlan(quickLoad.recipe);

        ExecutionEnvironment env;
        env.driver = "MockAudioEngine";
        env.sampleRate = 48000.0;
        env.blockSize = 256;
        env.channels = 2;
        env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput" };

        const auto resolveRes = ExperimentPlanCompiler::resolveExecutionPlan(plan, env, &quickLoad.recipe);

        REQUIRE(resolveRes.succeeded());
        REQUIRE(resolveRes.resolvedPlan.has_value());
        CHECK(resolveRes.diagnostics.empty());
        CHECK(resolveRes.resolvedPlan->environment.sampleRate == 48000.0);
        CHECK_FALSE(resolveRes.resolvedPlan->resolvedExecutionPlanHash.empty());
        CHECK(resolveRes.resolvedPlan->totalSamples > 0);
    }

    SECTION("Resolucion frente a entorno con sample rate no permitido falla con diagnostico")
    {
        const auto presetsDir = abdaudiolab::core::profilingPresetsDir();
        const auto quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
        const auto quickLoad = service.loadAndValidate(quickFile);
        REQUIRE(quickLoad.isSuccess());

        const auto plan = ExperimentPlanCompiler::compileToExperimentPlan(quickLoad.recipe);

        ExecutionEnvironment env;
        env.sampleRate = 192000.0; // quick_vcf_3pts solo permite 48000
        env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput" };

        const auto resolveRes = ExperimentPlanCompiler::resolveExecutionPlan(plan, env, &quickLoad.recipe);

        REQUIRE_FALSE(resolveRes.succeeded());
        REQUIRE_FALSE(resolveRes.diagnostics.empty());
        CHECK(resolveRes.diagnostics[0].code == "ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED");
        CHECK(resolveRes.diagnostics[0].jsonPointer == "/environment/sampleRate");
    }

    SECTION("Resolucion frente a entorno con capacidad faltante falla con diagnostico")
    {
        const auto presetsDir = abdaudiolab::core::profilingPresetsDir();
        const auto quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
        const auto quickLoad = service.loadAndValidate(quickFile);
        REQUIRE(quickLoad.isSuccess());

        const auto plan = ExperimentPlanCompiler::compileToExperimentPlan(quickLoad.recipe);

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.discoveredCapabilities = { "MidiInput" }; // Falta "StereoAudioOutput"

        const auto resolveRes = ExperimentPlanCompiler::resolveExecutionPlan(plan, env, &quickLoad.recipe);

        REQUIRE_FALSE(resolveRes.succeeded());
        REQUIRE_FALSE(resolveRes.diagnostics.empty());
        CHECK(resolveRes.diagnostics[0].code == "ERR_CAPABILITY_MISSING");
        CHECK(resolveRes.diagnostics[0].jsonPointer == "/environment/discoveredCapabilities");
    }
}
