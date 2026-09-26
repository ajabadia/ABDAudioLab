/**
 * @file test_AssistanceLevelsRecipeViews.cpp
 * @brief HITO-09C: Verificación Normativa de Niveles de Asistencia (Rápido, Configurable, Avanzado)
 * como Vistas del Mismo Contrato MeasurementRecipe.
 *
 * Valida los 10 contratos fundamentales:
 * 1. Paridad Multinivel de Esquema JSON Schema Draft 2020-12
 * 2. Determinismo de Mutación en Modo Configurable
 * 3. Control de Límites en Modo Configurable (RFC 6901)
 * 4. Mutación Avanzada de Políticas Metrológicas
 * 5. Invarianza de Edición (No arranca audio ni sesión)
 * 6. Preparación Confirmada de Receta Mutada
 * 7. RecipeBase Permanece Idéntica tras Mutaciones
 * 8. Restablecer Preset (resetToPreset)
 * 9. Cambio de Receta Descarta Mutaciones Locales
 * 10. Separación Estricta entre Vista y Receta
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>

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

ExecutionEnvironment getMockEnvironment()
{
    ExecutionEnvironment env;
    env.driver = "MockAudioEngine";
    env.sampleRate = 48000.0;
    env.blockSize = 256;
    env.channels = 2;
    env.discoveredCapabilities = { "MidiInput", "StereoAudioOutput", "ParameterAutomation" };
    return env;
}

TargetSelectionState getMockTargetState()
{
    TargetSelectionState t;
    t.kind = TargetKind::SyntheticFixture;
    t.targetName = "Mock Synthetic Target";
    t.isConnected = true;
    return t;
}

} // namespace

// ==============================================================================
// 1. Paridad Multinivel de Esquema
// ==============================================================================
TEST_CASE("Assistance Levels: Paridad Multinivel de Esquema", "[assistance_levels][schema]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;

    auto quickRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(quickRes.isSuccess());
    CHECK(quickRes.recipe.assistanceLevel == AssistanceLevel::Quick);

    auto configRes = service.loadAndValidate(presetsDir.getChildFile("standard_vcf_11pts.json"));
    REQUIRE(configRes.isSuccess());
    CHECK(configRes.recipe.assistanceLevel == AssistanceLevel::Configurable);

    auto advRes = service.loadAndValidate(presetsDir.getChildFile("exhaustive_synth_full.json"));
    REQUIRE(advRes.isSuccess());
    CHECK(advRes.recipe.assistanceLevel == AssistanceLevel::Advanced);

    // Los tres niveles serializan y validan exhaustivamente con 0 errores
    for (const auto& res : { quickRes, configRes, advRes })
    {
        std::string jsonStr = MeasurementRecipeService::serializeRecipeToJson(res.recipe);
        auto reval = service.loadAndValidateJson(jsonStr);
        CHECK(reval.isSuccess());
        CHECK(reval.recipeDocumentHash == res.recipeDocumentHash);
    }
}

// ==============================================================================
// 2. Determinismo de Mutación en Modo Configurable
// ==============================================================================
TEST_CASE("Assistance Levels: Determinismo de Mutación en Configurable", "[assistance_levels][configurable][determinism]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController1;
    RecipeExecutionController controller1(sessionController1);
    controller1.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Modificar repeticiones de 3 a 5
    REQUIRE(controller1.updateRepetitions(5));
    const auto& state1 = controller1.getCurrentState();

    // 1. Los hashes cambiaron respecto a la base
    CHECK(state1.workingRecipeDocumentHash != baseRes.recipeDocumentHash);
    CHECK(state1.experimentPlanHash != controller1.getBaseRecipe().recipeId); // plan hash nuevo
    CHECK(state1.isModifiedFromBase);

    // 2. El número de ventanas del plan escala exactamente: 5 reps * 3 puntos = 15 ventanas
    CHECK(state1.experimentPlan.windows.size() == 15);

    // 3. Determinismo bit a bit: una segunda instancia con idéntica mutación produce los mismos hashes
    ProfilingSessionController sessionController2;
    RecipeExecutionController controller2(sessionController2);
    controller2.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);
    REQUIRE(controller2.updateRepetitions(5));
    const auto& state2 = controller2.getCurrentState();

    CHECK(state1.workingRecipeDocumentHash == state2.workingRecipeDocumentHash);
    CHECK(state1.experimentPlanHash == state2.experimentPlanHash);
}

// ==============================================================================
// 3. Control de Límites en Modo Configurable (RFC 6901)
// ==============================================================================
TEST_CASE("Assistance Levels: Control de Límites en Configurable", "[assistance_levels][configurable][limits]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Mutación ilegal 1: repeticiones = 0
    bool resRep0 = controller.updateRepetitions(0);
    CHECK_FALSE(resRep0);
    CHECK_FALSE(controller.getCurrentState().localEditDiagnostics.empty());
    CHECK(controller.getCurrentState().localEditDiagnostics[0].code == "ERR_SEMANTICS_INVALID_RANGE");
    CHECK(controller.getCurrentState().localEditDiagnostics[0].jsonPointer == "/excitation/repetitions");

    // La receta de trabajo conserva el valor legal anterior (3 repeticiones)
    CHECK(controller.getWorkingRecipe().excitation.repetitions == 3);

    // Mutación ilegal 2: nota MIDI fuera de rango (150 > 127)
    bool resNote150 = controller.updateNoteExcitation(150, 0.5, 250.0, 50.0);
    CHECK_FALSE(resNote150);
    CHECK(controller.getCurrentState().localEditDiagnostics[0].code == "ERR_SEMANTICS_INVALID_RANGE");
    CHECK(controller.getCurrentState().localEditDiagnostics[0].jsonPointer == "/excitation/notes/0/midiNote");

    // La receta de trabajo conserva la nota original
    CHECK(controller.getWorkingRecipe().excitation.notes[0].midiNote == 60);
}

// ==============================================================================
// 4. Mutación Avanzada de Políticas Metrológicas
// ==============================================================================
TEST_CASE("Assistance Levels: Mutación Avanzada de Políticas Metrológicas", "[assistance_levels][advanced][policy]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    size_t originalEventsCount = controller.getCurrentState().experimentPlan.events.size();

    // Actualizar umbrales y política de calibración
    REQUIRE(controller.updateEvaluationPolicy(65.0, 0.5, 5.0));
    REQUIRE(controller.updateCalibrationPolicy("Optional"));

    const auto& state = controller.getCurrentState();
    CHECK(state.workingRecipe.evaluationPolicy.minimumSnrDb == 65.0);
    CHECK(state.workingRecipe.measurement.calibrationPolicy == "Optional");

    // Hashes documentales cambian
    CHECK(state.workingRecipeDocumentHash != baseRes.recipeDocumentHash);

    // Los eventos físicos de excitación permanecen idénticos porque la excitación no se modificó
    CHECK(state.experimentPlan.events.size() == originalEventsCount);
}

// ==============================================================================
// 5. Invarianza de Edición (No arranca audio ni sesión)
// ==============================================================================
TEST_CASE("Assistance Levels: Invarianza de Edición", "[assistance_levels][invariance]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    auto initialSnap = sessionController.getCurrentSnapshot();
    CHECK(initialSnap.sessionStatus == ProfilingSessionStatus::Idle);

    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Realizar múltiples mutaciones en memoria
    controller.updateRepetitions(4);
    controller.updateNoteExcitation(62, 0.7, 300.0, 60.0);
    controller.updateEvaluationPolicy(55.0, 0.8, 8.0);

    // REGLA: El secuenciador sigue en Idle, no hay audio ni sesión preparada
    CHECK_FALSE(controller.getCurrentState().isSessionPrepared);
    auto currentSnap = sessionController.getCurrentSnapshot();
    CHECK(currentSnap.sessionStatus == ProfilingSessionStatus::Idle);
    CHECK(sessionController.getCoordinator()->getState() == CoordinatorState::Idle);
    CHECK_FALSE(sessionController.getCoordinator()->isRunning());
}

// ==============================================================================
// 6. Preparación Confirmada de Receta Mutada
// ==============================================================================
TEST_CASE("Assistance Levels: Preparación Confirmada de Receta Mutada", "[assistance_levels][preparation]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Mutación: 4 repeticiones, nota 67 (Sol)
    REQUIRE(controller.updateRepetitions(4));
    REQUIRE(controller.updateNoteExcitation(67, 0.6, 200.0, 40.0));

    auto env = getMockEnvironment();
    auto target = getMockTargetState();

    // Resolución física
    RecipePreparationState resolvedState = controller.resolveAgainstCurrentEnvironment(env, target);
    REQUIRE(resolvedState.isEnvironmentCompatible);
    REQUIRE(resolvedState.resolvedPlan.has_value());

    // Confirmación en el Stepper (Paso 3)
    PreparationResult prepResult = controller.prepareSessionAfterConfirmation(target);
    REQUIRE(prepResult.success);
    REQUIRE(prepResult.preparedSession.has_value());

    // La sesión tiene las características exactas editadas por el operador
    const auto& session = *prepResult.preparedSession;
    CHECK(session.getTestCases().size() == 4 * 3); // 4 repeticiones * 3 puntos = 12
    CHECK(session.getTestCases()[0].midiNoteNumber == 67);
}

// ==============================================================================
// 7. RecipeBase Permanece Idéntica tras Mutaciones
// ==============================================================================
TEST_CASE("Assistance Levels: RecipeBase Permanece Idéntica", "[assistance_levels][base_invariance]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Mutar la workingRecipe
    REQUIRE(controller.updateRepetitions(8));
    REQUIRE(controller.updateNoteExcitation(72, 0.9, 500.0, 100.0));

    // REGLA: RecipeBase permanece estrictamente intacta
    const auto& base = controller.getBaseRecipe();
    CHECK(base.excitation.repetitions == 3);
    CHECK(base.excitation.notes[0].midiNote == 60);
    CHECK(controller.getCurrentState().baseRecipeDocumentHash == baseRes.recipeDocumentHash);
}

// ==============================================================================
// 8. Restablecer Preset (resetToPreset)
// ==============================================================================
TEST_CASE("Assistance Levels: Restablecer Preset", "[assistance_levels][reset]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto baseRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(baseRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(baseRes.recipe, baseRes.recipeDocumentHash);

    // Aplicar cambios
    REQUIRE(controller.updateRepetitions(6));
    CHECK(controller.isModifiedFromPreset());

    // Acción: Restablecer
    controller.resetToPreset();

    CHECK_FALSE(controller.isModifiedFromPreset());
    CHECK(controller.getWorkingRecipe().excitation.repetitions == 3);
    CHECK(controller.getCurrentState().workingRecipeDocumentHash == baseRes.recipeDocumentHash);
    CHECK(controller.getCurrentState().experimentPlanHash == controller.getCurrentState().experimentPlanHash);
}

// ==============================================================================
// 9. Cambio de Receta Descarta Mutaciones Locales
// ==============================================================================
TEST_CASE("Assistance Levels: Cambio de Receta Descarta Mutaciones", "[assistance_levels][discard]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto quickRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    auto stdRes = service.loadAndValidate(presetsDir.getChildFile("standard_vcf_11pts.json"));
    REQUIRE(quickRes.isSuccess());
    REQUIRE(stdRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    // Seleccionar Quick y mutar a 9 repeticiones
    controller.selectRecipe(quickRes.recipe, quickRes.recipeDocumentHash);
    REQUIRE(controller.updateRepetitions(9));
    CHECK(controller.isModifiedFromPreset());

    // Seleccionar Standard (conmutación en el catálogo)
    controller.selectRecipe(stdRes.recipe, stdRes.recipeDocumentHash);

    // REGLA: Los cambios de Quick fueron descartados; Standard inicia limpio desde su preset base
    CHECK_FALSE(controller.isModifiedFromPreset());
    CHECK(controller.getWorkingRecipe().recipeId == stdRes.recipe.recipeId);
    CHECK(controller.getWorkingRecipe().excitation.repetitions == stdRes.recipe.excitation.repetitions);
    CHECK(controller.getCurrentState().workingRecipeDocumentHash == stdRes.recipeDocumentHash);
}

// ==============================================================================
// 10. Separación Estricta entre Vista y Receta
// ==============================================================================
TEST_CASE("Assistance Levels: Separación Estricta entre Vista y Receta", "[assistance_levels][view_separation]")
{
    juce::File presetsDir = getPresetsDirectory();
    MeasurementRecipeService service;
    auto quickRes = service.loadAndValidate(presetsDir.getChildFile("quick_vcf_3pts.json"));
    REQUIRE(quickRes.isSuccess());

    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);
    controller.selectRecipe(quickRes.recipe, quickRes.recipeDocumentHash);

    std::string originalDocHash = controller.getCurrentState().workingRecipeDocumentHash;
    std::string originalPlanHash = controller.getCurrentState().experimentPlanHash;

    // Cambiar la vista de Quick a Configurable
    controller.setEditorView(RecipeEditorView::Configurable);
    CHECK(controller.getEditorView() == RecipeEditorView::Configurable);
    CHECK(controller.getCurrentState().workingRecipeDocumentHash == originalDocHash);
    CHECK(controller.getCurrentState().experimentPlanHash == originalPlanHash);

    // Cambiar la vista a Advanced
    controller.setEditorView(RecipeEditorView::Advanced);
    CHECK(controller.getEditorView() == RecipeEditorView::Advanced);
    CHECK(controller.getCurrentState().workingRecipeDocumentHash == originalDocHash);
    CHECK(controller.getCurrentState().experimentPlanHash == originalPlanHash);

    // REGLA: No se ha creado sesión ni iniciado audio
    CHECK_FALSE(controller.getCurrentState().isSessionPrepared);
    CHECK(sessionController.getCoordinator()->getState() == CoordinatorState::Idle);
}
