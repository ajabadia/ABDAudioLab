/**
 * @file test_TargetProfileLegacyCharacterization.cpp
 * @brief HITO-10B: Caracterización y Congelación de la Ruta C++ Histórica como Oráculo Metrológico.
 *
 * Ejecuta la ruta legacy para ReferenceSynth:
 *   MeasurementRecipe -> compileToExperimentPlan -> resolveExecutionPlan(plan, env, &recipe) -> createProfilingSession
 *
 * Congela la huella canónica y el comportamiento de las 3 recetas normativas:
 *   1. quick_vcf_3pts.json: 3 puntos * 1 nota * 3 reps = 9 ventanas, 27 eventos
 *   2. standard_vcf_11pts.json: 11 puntos * 1 nota * 2 reps = 22 ventanas, 66 eventos
 *   3. exhaustive_synth_full.json: 8 puntos * 3 notas * 3 reps = 72 ventanas, 216 eventos
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <string>
#include <vector>

#include "core/LabResourcePaths.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "gui/session/ProfilingSessionContracts.h"

using namespace abdaudiolab::profiling;
using namespace abdaudiolab::gui::session;

namespace
{

inline juce::File getPresetsDirectory() { return abdaudiolab::core::profilingPresetsDir(); }

ExecutionEnvironment getLegacyEnvironment()
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

TEST_CASE("TargetProfile Legacy Characterization: quick_vcf_3pts (9 ventanas, 27 eventos)", "[target_profile][legacy_characterization]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("quick_vcf_3pts.json"));
    REQUIRE(recipeRes.isSuccess());

    auto env = getLegacyEnvironment();

    // 1. Compilación legacy a ExperimentPlan
    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    REQUIRE(legacyPlan.recipeId == "org.abd.profiling.vcf.quick-3pts");
    CHECK_FALSE(legacyPlan.planHash.empty());
    CHECK(legacyPlan.sampleRate == 96000.0); // Tasa maestra canónica pura (CompilationDefaults)

    // 3 puntos * 1 nota * 3 reps = 9 ventanas
    REQUIRE(legacyPlan.windows.size() == 9);
    // 3 eventos por ventana (Param, NoteOn, NoteOff) = 27 eventos
    REQUIRE(legacyPlan.events.size() == 27);

    // Verificar orden temporal estricto de eventos por ventana
    for (size_t w = 0; w < legacyPlan.windows.size(); ++w)
    {
        size_t evBase = w * 3;
        const auto& evParam   = legacyPlan.events[evBase];
        const auto& evNoteOn  = legacyPlan.events[evBase + 1];
        const auto& evNoteOff = legacyPlan.events[evBase + 2];

        CHECK(evParam.eventType == abdaudiolab::synth::TargetEventType::Parameter);
        CHECK(evNoteOn.eventType == abdaudiolab::synth::TargetEventType::Midi);
        CHECK(evNoteOn.midi.type == abdaudiolab::synth::TimedMidiType::NoteOn);
        CHECK(evNoteOff.eventType == abdaudiolab::synth::TargetEventType::Midi);
        CHECK(evNoteOff.midi.type == abdaudiolab::synth::TimedMidiType::NoteOff);

        CHECK(evParam.absoluteSample <= evNoteOn.absoluteSample);
        CHECK(evNoteOn.absoluteSample < evNoteOff.absoluteSample);
    }

    // 2. Resolución legacy frente al entorno
    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());

    const auto& resolved = *legacyResolve.resolvedPlan;
    CHECK(resolved.experimentPlan.sampleRate == 48000.0);
    CHECK(resolved.environment.sampleRate == 48000.0);
    CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());
    CHECK(resolved.totalSamples > 0);
    CHECK(resolved.totalDurationSec > 0.0);

    // 3. Generación de ProfilingSession mediante puente compatible
    auto targetState = getReferenceSynthTargetState();
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(resolved, targetState);
    REQUIRE(legacySession.getTestCases().size() == 9);

    for (const auto& tc : legacySession.getTestCases())
    {
        CHECK(tc.midiNoteNumber == 60); // C4
        CHECK(tc.midiVelocity == Catch::Approx(0.5f));
        CHECK(tc.noteGateDurationSec == Catch::Approx(0.25f)); // 250 ms
        CHECK(tc.stabilizationWaitMs == Catch::Approx(50.0));  // 50 ms
        REQUIRE(tc.parameterSteps.size() == 1);
        CHECK(tc.parameterSteps[0].paramName == "vcf.cutoff");
    }
}

TEST_CASE("TargetProfile Legacy Characterization: standard_vcf_11pts (22 ventanas, 66 eventos)", "[target_profile][legacy_characterization]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("standard_vcf_11pts.json"));
    REQUIRE(recipeRes.isSuccess());

    auto env = getLegacyEnvironment();

    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    REQUIRE(legacyPlan.recipeId == "org.abd.profiling.vcf.standard-11pts");

    // 11 puntos * 1 nota * 2 reps = 22 ventanas
    REQUIRE(legacyPlan.windows.size() == 22);
    // 22 * 3 = 66 eventos
    REQUIRE(legacyPlan.events.size() == 66);

    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());

    const auto& resolved = *legacyResolve.resolvedPlan;
    CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());

    auto targetState = getReferenceSynthTargetState();
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(resolved, targetState);
    REQUIRE(legacySession.getTestCases().size() == 22);

    // Primer punto: cutoff 0.0, último punto: cutoff 1.0
    CHECK(legacySession.getTestCases()[0].parameterSteps[0].normalizedValue == Catch::Approx(0.0f));
    CHECK(legacySession.getTestCases()[10].parameterSteps[0].normalizedValue == Catch::Approx(1.0f));
}

TEST_CASE("TargetProfile Legacy Characterization: exhaustive_synth_full (72 ventanas, 216 eventos)", "[target_profile][legacy_characterization]")
{
    MeasurementRecipeService recipeService;
    auto recipeRes = recipeService.loadAndValidate(getPresetsDirectory().getChildFile("exhaustive_synth_full.json"));
    REQUIRE(recipeRes.isSuccess());

    auto env = getLegacyEnvironment();

    auto legacyPlan = ExperimentPlanCompiler::compileToExperimentPlan(recipeRes.recipe);
    REQUIRE(legacyPlan.recipeId == "org.abd.profiling.synth.exhaustive-full");

    // 8 puntos * 3 notas * 3 reps = 72 ventanas
    REQUIRE(legacyPlan.windows.size() == 72);
    // 72 * 3 = 216 eventos
    REQUIRE(legacyPlan.events.size() == 216);

    auto legacyResolve = ExperimentPlanCompiler::resolveExecutionPlan(legacyPlan, env, &recipeRes.recipe);
    REQUIRE(legacyResolve.succeeded());
    REQUIRE(legacyResolve.resolvedPlan.has_value());

    const auto& resolved = *legacyResolve.resolvedPlan;
    CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());

    auto targetState = getReferenceSynthTargetState();
    auto legacySession = ExperimentPlanCompiler::createProfilingSession(resolved, targetState);
    REQUIRE(legacySession.getTestCases().size() == 72);

    // Notas de excitación declaradas en la receta: C2 (36), C4 (60), C6 (84)
    std::vector<int> expectedNotes = { 36, 60, 84 };
    bool foundAllNotes = true;
    for (int n : expectedNotes)
    {
        bool found = false;
        for (const auto& tc : legacySession.getTestCases())
        {
            if (tc.midiNoteNumber == n)
            {
                found = true;
                break;
            }
        }
        if (!found) foundAllNotes = false;
    }
    CHECK(foundAllNotes);
}
