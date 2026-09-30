/**
 * @file test_MeasurementRecipeRegression.cpp
 * @brief HITO-09A: Auditoría de Regresión y Ejecución E2E de Recetas Metrológicas Compiladas.
 *
 * Verifica que una MeasurementRecipe formal (quick_vcf_3pts.json):
 * 1. Se valida y compila de forma determinista.
 * 2. Se resuelve a un ResolvedExecutionPlan frente al entorno MockAudioEngine.
 * 3. Se ejecuta de principio a fin a través de ProfilingSequencer sin alterar la UI ni el motor DSP.
 * 4. Dos ejecuciones independientes producen exactamente el mismo audio canónico bit a bit (SHA-256 idéntico).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <filesystem>

#include "core/LabResourcePaths.h"
#include "support/MockAudioEngine.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::test::support;

namespace
{

struct TemporaryTestDirectory
{
    std::filesystem::path path;

    explicit TemporaryTestDirectory(const std::string& prefix)
    {
        auto tempRoot = std::filesystem::temp_directory_path();
        path = tempRoot / (prefix + "_" + std::to_string(juce::Random::getSystemRandom().nextInt()));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~TemporaryTestDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TemporaryTestDirectory(const TemporaryTestDirectory&) = delete;
    TemporaryTestDirectory& operator=(const TemporaryTestDirectory&) = delete;
};

struct ExecutionRunResult
{
    bool success { false };
    int pumpedBlocks { 0 };
    std::string canonicalAudioSha256;
    std::vector<float> audioL;
};

ExecutionRunResult runCompiledRecipe(const core::ProfilingSession& session, const std::string& runName)
{
    TemporaryTestDirectory tempDir("recipe_reg_" + runName);
    juce::File exportDirectory(tempDir.path.string());

    SyntheticAudioFixture syntheticFixture;
    syntheticFixture.prepareToPlay(48000.0, 256);

    audio::LabAudioEngine audioEngine;
    audioEngine.setActivePluginInstance(&syntheticFixture, 48000.0, 256);

    hardware::MockHardwareController mockHw;
    ProfilingSequencer sequencer(audioEngine, mockHw);

    MockAudioEngine mockEngine(audioEngine);
    mockEngine.prepare(48000.0, 256, 2);

    bool started = sequencer.startSession(session, exportDirectory, "rec_run_" + juce::String(runName));
    REQUIRE(started);

    auto pumpRes = mockEngine.pumpUntil([&]() {
        return sequencer.getCurrentState() == SequencerState::Finished ||
               sequencer.getCurrentState() == SequencerState::ErrorState ||
               !sequencer.isRunningSession();
    }, 6000);

    sequencer.waitForThreadToExit(2000);
    mockEngine.pumpOneBlock();

    ExecutionRunResult res;
    res.success = pumpRes.success && (sequencer.getCurrentState() == SequencerState::Finished);
    res.pumpedBlocks = pumpRes.pumpedBlocks + 1;
    res.audioL = mockEngine.getCapturedOutputL();
    res.canonicalAudioSha256 = MockAudioEngine::computeCanonicalBufferSha256(res.audioL);

    audioEngine.setActivePluginInstance(nullptr);
    return res;
}

} // namespace

TEST_CASE("HITO-09A: Regresion y ejecucion determinista de receta compilada", "[recipe][regression]")
{
    MeasurementRecipeService service;
    const auto presetsDir = abdaudiolab::core::profilingPresetsDir();
    const auto quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
    REQUIRE(quickFile.existsAsFile());

    // 1. Cargar y validar receta
    const auto loadRes = service.loadAndValidate(quickFile);
    REQUIRE(loadRes.isSuccess());

    // 2. Compilar a ExperimentPlan puro (experimentPlanHash agnóstico a hardware)
    const auto plan = ExperimentPlanCompiler::compileToExperimentPlan(loadRes.recipe);
    REQUIRE_FALSE(plan.planHash.empty());

    // 3. Resolver frente al entorno físico de MockAudioEngine (48 kHz, 256)
    ExecutionEnvironment env;
    env.driver = "MockAudioEngine";
    env.sampleRate = 48000.0;
    env.blockSize = 256;
    env.channels = 2;
    env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput" };

    const auto resolveRes = ExperimentPlanCompiler::resolveExecutionPlan(plan, env, &loadRes.recipe);
    REQUIRE(resolveRes.succeeded());
    REQUIRE(resolveRes.resolvedPlan.has_value());
    REQUIRE_FALSE(resolveRes.resolvedPlan->resolvedExecutionPlanHash.empty());

    // 4. Crear ProfilingSession mediante puente compatible
    TargetSelectionState targetState;
    targetState.targetId = "synthetic_fixture_integration01";
    targetState.targetName = "SyntheticAudioFixture";
    targetState.kind = TargetKind::SyntheticFixture;
    targetState.isConnected = true;

    const core::ProfilingSession session = ExperimentPlanCompiler::createProfilingSession(*resolveRes.resolvedPlan, targetState);
    REQUIRE(session.getTestCases().size() == resolveRes.resolvedPlan->experimentPlan.windows.size());

    // 5. Ejecución A
    ExecutionRunResult runA = runCompiledRecipe(session, "RunA");
    REQUIRE(runA.success);
    CHECK(runA.pumpedBlocks > 0);
    CHECK_FALSE(runA.audioL.empty());
    CHECK_FALSE(runA.canonicalAudioSha256.empty());

    // 6. Ejecución B
    ExecutionRunResult runB = runCompiledRecipe(session, "RunB");
    REQUIRE(runB.success);
    CHECK(runB.pumpedBlocks == runA.pumpedBlocks);

    // 7. Paridad determinista estricta bit a bit
    CHECK(runA.canonicalAudioSha256 == runB.canonicalAudioSha256);
    REQUIRE(runA.audioL.size() == runB.audioL.size());

    float maxDiff = 0.0f;
    for (size_t i = 0; i < runA.audioL.size(); ++i)
    {
        const float diff = std::abs(runA.audioL[i] - runB.audioL[i]);
        if (diff > maxDiff)
            maxDiff = diff;
    }
    CHECK(maxDiff == 0.0f);
}
