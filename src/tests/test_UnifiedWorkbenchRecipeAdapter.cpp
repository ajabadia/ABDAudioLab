/**
 * @file test_UnifiedWorkbenchRecipeAdapter.cpp
 * @brief HITO-09B: Verificación de Paridad, No-Bifurcación e Invarianza del Banco de Trabajo Unificado.
 *
 * Valida los 7 contratos fundamentales del adaptador visual de recetas:
 * 1. Paridad de Carga (RecipeCatalogModel vs MeasurementRecipeService)
 * 2. Paridad de Compilación (RecipeExecutionController::selectRecipe vs compileToExperimentPlan)
 * 3. Paridad de Resolución de Entorno (resolveAgainstCurrentEnvironment vs resolveExecutionPlan)
 * 4. Diagnóstico de Incompatibilidad de Target sin Bloqueo (RFC 6901)
 * 5. Resiliencia ante Recetas Corruptas en Catálogo
 * 6. Invarianza: Selección de Receta NO Crea Sesión Ejecutable ni Altera la Activa
 * 7. Preparación Confirmada desde el Flujo Normal del Stepper
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <filesystem>

#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "gui/recipes/RecipeCatalogModel.h"
#include "gui/recipes/RecipeExecutionController.h"
#include "gui/session/ProfilingSessionController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::gui::recipes;
using namespace abdaudiolab::gui::session;

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

struct TemporaryPresetsDirectory
{
    std::filesystem::path path;

    explicit TemporaryPresetsDirectory(const std::string& prefix)
    {
        auto tempRoot = std::filesystem::temp_directory_path();
        path = tempRoot / (prefix + "_" + std::to_string(juce::Random::getSystemRandom().nextInt()));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~TemporaryPresetsDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TemporaryPresetsDirectory(const TemporaryPresetsDirectory&) = delete;
    TemporaryPresetsDirectory& operator=(const TemporaryPresetsDirectory&) = delete;
};

} // namespace

// ==============================================================================
// 1. Paridad de Carga
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Paridad de Carga", "[workbench][recipe][adapter][load]")
{
    juce::File presetsDir = getPresetsDirectory();
    REQUIRE(presetsDir.isDirectory());

    MeasurementRecipeService service;
    RecipeCatalogModel catalog(service);
    catalog.loadFromDirectory(presetsDir);

    REQUIRE(catalog.getValidEntryCount() >= 3);

    // Archivo de referencia normativo
    juce::File quickFile = presetsDir.getChildFile("quick_vcf_3pts.json");
    REQUIRE(quickFile.existsAsFile());

    // Carga directa headless
    RecipeLoadResult headlessResult = service.loadAndValidate(quickFile);
    REQUIRE(headlessResult.isSuccess());

    // Carga a través del modelo del catálogo de la UI
    auto catalogEntry = catalog.findByRecipeId("org.abd.profiling.vcf.quick-3pts");
    REQUIRE(catalogEntry.has_value());
    REQUIRE(catalogEntry->isAvailable);

    // Paridad bit a bit del hash canónico del documento RFC 8785
    CHECK(catalogEntry->loadResult.recipeDocumentHash == headlessResult.recipeDocumentHash);
    CHECK(catalogEntry->loadResult.recipe.recipeId == headlessResult.recipe.recipeId);
    CHECK(catalogEntry->loadResult.recipe.displayName == headlessResult.recipe.displayName);
    CHECK(catalogEntry->loadResult.recipe.revision == headlessResult.recipe.revision);
    CHECK(catalogEntry->loadResult.recipe.excitation.notes.size() == headlessResult.recipe.excitation.notes.size());
    CHECK(catalogEntry->loadResult.recipe.measurement.points.size() == headlessResult.recipe.measurement.points.size());
}

// ==============================================================================
// 2. Paridad de Compilación
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Paridad de Compilación", "[workbench][recipe][adapter][compile]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    RecipeLoadResult loadResult = service.loadAndValidate(presetsDir.getChildFile("standard_vcf_11pts.json"));
    REQUIRE(loadResult.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    // Compilación coordinada por el controlador de UI
    RecipePreparationState state = controller.selectRecipe(loadResult.recipe, loadResult.recipeDocumentHash);
    REQUIRE(state.isRecipeSelected);
    REQUIRE(!state.experimentPlanHash.empty());

    // Compilación headless directa
    synth::ExperimentPlan headlessPlan = ExperimentPlanCompiler::compileToExperimentPlan(loadResult.recipe);
    std::string headlessPlanHash = ExperimentPlanCompiler::computeExperimentPlanHash(headlessPlan);

    // Paridad estricta de hash científico y eventos
    CHECK(state.experimentPlanHash == headlessPlanHash);
    CHECK(state.experimentPlan.events.size() == headlessPlan.events.size());
    CHECK(state.experimentPlan.windows.size() == headlessPlan.windows.size());
    CHECK(state.experimentPlan.randomization.randomSeed == headlessPlan.randomization.randomSeed);
}

// ==============================================================================
// 3. Paridad de Resolución de Entorno
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Paridad de Resolución de Entorno", "[workbench][recipe][adapter][resolve]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    RecipeLoadResult loadResult = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(loadResult.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(loadResult.recipe, loadResult.recipeDocumentHash);

    // Entorno físico mock compatible
    ExecutionEnvironment env;
    env.driver = "MockAudioEngine";
    env.sampleRate = 48000.0;
    env.blockSize = 256;
    env.channels = 2;
    env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput", "ParameterAutomation" };

    TargetSelectionState targetState;
    targetState.kind = TargetKind::SyntheticFixture;
    targetState.targetName = "Mock Synthetic Target";
    targetState.isConnected = true;

    // Resolución vía controlador de UI
    RecipePreparationState state = controller.resolveAgainstCurrentEnvironment(env, targetState);
    REQUIRE(state.isEnvironmentCompatible);
    REQUIRE(state.resolvedPlan.has_value());
    REQUIRE(state.resolvedExecutionPlanHash.has_value());

    // Resolución headless directa
    synth::ExperimentPlan headlessPlan = ExperimentPlanCompiler::compileToExperimentPlan(loadResult.recipe);
    ResolveExecutionPlanResult headlessResult = ExperimentPlanCompiler::resolveExecutionPlan(headlessPlan, env, &loadResult.recipe);
    REQUIRE(headlessResult.succeeded());
    REQUIRE(headlessResult.resolvedPlan.has_value());

    // Paridad de resolvedExecutionPlanHash
    CHECK(*state.resolvedExecutionPlanHash == headlessResult.resolvedPlan->resolvedExecutionPlanHash);
    CHECK(state.resolvedPlan->totalSamples == headlessResult.resolvedPlan->totalSamples);
    CHECK(state.resolvedPlan->totalDurationSec == Catch::Approx(headlessResult.resolvedPlan->totalDurationSec).margin(1e-6));
}

// ==============================================================================
// 4. Diagnóstico de Incompatibilidad de Target sin Bloqueo (RFC 6901)
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Diagnóstico de Incompatibilidad sin Bloqueo", "[workbench][recipe][adapter][diagnostic]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    RecipeCatalogModel catalog(service);
    catalog.loadFromDirectory(presetsDir);

    RecipeLoadResult loadResult = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(loadResult.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(loadResult.recipe, loadResult.recipeDocumentHash);

    // Entorno con sample rate incompatible (44100 Hz frente a [48000, 96000] exigido por la receta)
    ExecutionEnvironment incompatibleEnv;
    incompatibleEnv.driver = "MockAudioEngine";
    incompatibleEnv.sampleRate = 44100.0;
    incompatibleEnv.blockSize = 256;
    incompatibleEnv.channels = 2;
    incompatibleEnv.discoveredCapabilities = { "MidiInput", "StereoAudioOutput", "ParameterAutomation" };

    TargetSelectionState targetState;
    targetState.kind = TargetKind::SyntheticFixture;
    targetState.targetName = "Mock Synthetic Target";
    targetState.isConnected = true;

    RecipePreparationState state = controller.resolveAgainstCurrentEnvironment(incompatibleEnv, targetState);

    // 1. La receta sigue siendo válida y seleccionada
    CHECK(state.isRecipeSelected);
    // 2. Se detecta la incompatibilidad
    CHECK_FALSE(state.isEnvironmentCompatible);
    CHECK_FALSE(state.resolvedPlan.has_value());

    // 3. Emite diagnóstico RFC 6901 exacto
    bool foundSampleRateDiag = false;
    for (const auto& diag : state.resolutionResult.diagnostics)
    {
        if (diag.code == "ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED" &&
            diag.jsonPointer == "/environment/sampleRate")
        {
            foundSampleRateDiag = true;
            break;
        }
    }
    CHECK(foundSampleRateDiag);

    // 4. Intentar preparar sesión falla con mensaje descriptivo y NO crea sesión
    PreparationResult prepResult = controller.prepareSessionAfterConfirmation(targetState);
    CHECK_FALSE(prepResult.success);
    CHECK_FALSE(prepResult.preparedSession.has_value());
    CHECK_FALSE(prepResult.errorMessage.empty());

    // 5. El catálogo de recetas sigue totalmente operativo sin bloqueos
    CHECK(catalog.getValidEntryCount() >= 3);
}

// ==============================================================================
// 5. Resiliencia ante Recetas Corruptas en Catálogo
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Resiliencia ante Recetas Corruptas", "[workbench][recipe][adapter][resilience]")
{
    TemporaryPresetsDirectory tempDir("corrupt_presets");
    juce::File tempJuceDir(tempDir.path.string());

    // Crear 1 receta válida
    juce::File validFile = tempJuceDir.getChildFile("valid.json");
    juce::File originalQuick = getPresetsDirectory().getChildFile("quick_vcf_3pts.json");
    REQUIRE(originalQuick.copyFileTo(validFile));

    // Crear 1 archivo JSON con sintaxis rota
    juce::File brokenJsonFile = tempJuceDir.getChildFile("broken_syntax.json");
    brokenJsonFile.replaceWithText("{ \"schemaVersion\": \"1.0\", \"recipeId\": ");

    // Crear 1 archivo con schemaVersion incompatible
    juce::File invalidSchemaFile = tempJuceDir.getChildFile("invalid_schema.json");
    invalidSchemaFile.replaceWithText(R"({
        "schemaVersion": "99.0",
        "recipeId": "future-recipe"
    })");

    MeasurementRecipeService service;
    RecipeCatalogModel catalog(service);
    catalog.loadFromDirectory(tempJuceDir);

    // El catálogo debe contener 3 entradas encontradas
    CHECK(catalog.getEntryCount() == 3);

    // Solo 1 debe ser válida y disponible
    CHECK(catalog.getValidEntryCount() == 1);

    auto validEntry = catalog.findByRecipeId("org.abd.profiling.vcf.quick-3pts");
    REQUIRE(validEntry.has_value());
    CHECK(validEntry->isAvailable);

    // Las entradas corruptas están marcadas como no disponibles con sus respectivos errores
    for (const auto& entry : catalog.getEntries())
    {
        if (entry.file.getFileName() == "broken_syntax.json")
        {
            CHECK_FALSE(entry.isAvailable);
            CHECK_FALSE(entry.loadResult.isSuccess());
            CHECK_FALSE(entry.loadResult.diagnostics.empty());
        }
        else if (entry.file.getFileName() == "invalid_schema.json")
        {
            CHECK_FALSE(entry.isAvailable);
            CHECK_FALSE(entry.loadResult.isSuccess());
            CHECK_FALSE(entry.loadResult.diagnostics.empty());
        }
    }
}

// ==============================================================================
// 6. Invarianza: Selección de Receta NO Crea Sesión Ejecutable
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Invarianza de Selección", "[workbench][recipe][adapter][invariance]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    RecipeLoadResult loadResult = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(loadResult.isSuccess());

    ProfilingSessionController sessionController;
    auto initialSnapshot = sessionController.getCurrentSnapshot();
    CHECK(initialSnapshot.sessionStatus == ProfilingSessionStatus::Idle);

    RecipeExecutionController controller(sessionController);

    // Acción: El usuario selecciona la receta en el catálogo (Paso 2)
    RecipePreparationState state = controller.selectRecipe(loadResult.recipe, loadResult.recipeDocumentHash);

    // REGLA FUNDAMENTAL:
    // 1. Se marca la receta seleccionada en el estado de preparación
    CHECK(state.isRecipeSelected);
    // 2. No se marca la sesión como preparada
    CHECK_FALSE(state.isSessionPrepared);
    // 3. El estado de la sesión activa en el controlador sigue siendo exactamente Idle
    auto snapshotAfterSelect = sessionController.getCurrentSnapshot();
    CHECK(snapshotAfterSelect.sessionStatus == ProfilingSessionStatus::Idle);
    CHECK(snapshotAfterSelect.sessionId == initialSnapshot.sessionId);
    // 4. El secuenciador NO ha arrancado
    CHECK(sessionController.getCoordinator()->getState() == CoordinatorState::Idle);
    CHECK_FALSE(sessionController.getCoordinator()->isRunning());
}

// ==============================================================================
// 7. Preparación Confirmada desde el Flujo Normal del Stepper
// ==============================================================================
TEST_CASE("Workbench Recipe Adapter: Preparación Confirmada", "[workbench][recipe][adapter][preparation]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    RecipeLoadResult loadResult = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(loadResult.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    // Paso 2: Selección
    controller.selectRecipe(loadResult.recipe, loadResult.recipeDocumentHash);

    // Paso 3: Resolución frente a entorno compatible
    ExecutionEnvironment env;
    env.driver = "MockAudioEngine";
    env.sampleRate = 48000.0;
    env.blockSize = 256;
    env.channels = 2;
    env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput", "ParameterAutomation" };

    TargetSelectionState targetState;
    targetState.kind = TargetKind::SyntheticFixture;
    targetState.targetName = "Mock Synthetic Target";
    targetState.isConnected = true;

    RecipePreparationState resolvedState = controller.resolveAgainstCurrentEnvironment(env, targetState);
    REQUIRE(resolvedState.isEnvironmentCompatible);
    REQUIRE(resolvedState.resolvedPlan.has_value());

    // Paso 3/4: El usuario confirma "Preparar prueba" en el Stepper existente
    PreparationResult prepResult = controller.prepareSessionAfterConfirmation(targetState);

    // Verificaciones:
    // 1. Preparación exitosa
    REQUIRE(prepResult.success);
    REQUIRE(prepResult.preparedSession.has_value());
    CHECK(controller.getCurrentState().isSessionPrepared);

    // 2. La sesión creada tiene la semántica idéntica a la resolución directa
    const core::ProfilingSession& session = *prepResult.preparedSession;
    CHECK(session.getMetadata().hardwareName == targetState.targetName);
    CHECK(session.getTestCases().size() == resolvedState.resolvedPlan->experimentPlan.windows.size());

    // 3. Mismo resolvedExecutionPlanHash que la ruta headless
    synth::ExperimentPlan headlessPlan = ExperimentPlanCompiler::compileToExperimentPlan(loadResult.recipe);
    ResolveExecutionPlanResult headlessResolution = ExperimentPlanCompiler::resolveExecutionPlan(headlessPlan, env, &loadResult.recipe);
    REQUIRE(headlessResolution.succeeded());
    REQUIRE(headlessResolution.resolvedPlan.has_value());

    CHECK(resolvedState.resolvedPlan->resolvedExecutionPlanHash == headlessResolution.resolvedPlan->resolvedExecutionPlanHash);

    // 4. El controlador tiene el target configurado pero no ha disparado la ejecución automáticamente
    auto finalSnapshot = sessionController.getCurrentSnapshot();
    CHECK(finalSnapshot.target.targetName == targetState.targetName);
    CHECK(sessionController.getCoordinator()->getState() == CoordinatorState::Idle);
    CHECK_FALSE(sessionController.getCoordinator()->isRunning());
}
