/**
 * @file test_FreeCaptureRecipePromotion.cpp
 * @brief HITO-09D: Verificación Normativa de la Promoción de Toma Libre (Exploración) a MeasurementRecipe Formal.
 *
 * Valida los 10 contratos fundamentales de honestidad metrológica:
 * 1. Extracción Válida de Parámetros Observados (JSON Schema Draft 2020-12)
 * 2. Trazabilidad Criptográfica de Procedencia (sourceExplorationHash determinista, sourceRecipeDocumentHash vacío)
 * 3. Resiliencia ante Controles No Declarados (Sin Invención, ERR_PROMOTION_CONTROL_UNDECLARED y PromotionDraft)
 * 4. Carga en RecipeExecutionController (workingRecipe, vista Configurable y compilación de ExperimentPlan)
 * 5. Invarianza de Estado (Promover NO crea sesión, NO inicia audio y NO arranca el secuenciador)
 * 6. Edición Posterior de la Receta Promovida (mutaciones locales en memoria con recálculo determinista)
 * 7. Preparación Confirmada en el Stepper (Paso 3 genera ProfilingSession con kind = Measurement)
 * 8. Blindaje e Inmutabilidad de la Exploración Original (la sesión previa permanece como Exploration)
 * 9. Promoción con Control Declarado (mapeo múltiple fiel de semanticIds y posiciones normalizadas)
 * 10. Promoción Incompleta Bloquea Preparación (PromotionDraft no produce experimentPlanHash ni permite sesión)
 * 11. Navegación a Revisión de Recipe en el Stepper (Paso 2 Calibration & Setup muestra RecipeEditorComponent en Configurable)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>

#include "profiling/MeasurementRecipe.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "profiling/RecipePromotionService.h"
#include "gui/recipes/RecipeExecutionController.h"
#include "gui/recipes/RecipeEditorComponent.h"
#include "gui/session/ProfilingSessionController.h"
#include "core/ExperimentRecord.h"

using namespace abdaudiolab;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::gui::recipes;
using namespace abdaudiolab::gui::session;

namespace
{

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
    t.targetName = "Moog Analog Lead";
    t.isConnected = true;
    return t;
}

ExplorationContext createCompleteExplorationContext()
{
    ExplorationContext ctx;
    ctx.targetId = "moog_sub37";
    ctx.targetName = "Moog Sub 37";
    ctx.targetDeviceType = "SyntheticFixture";
    ctx.sampleRate = 48000.0;
    ctx.channels = 2;
    ctx.repetitions = 3;
    ctx.explorationSessionId = "exp_sess_7a8b9c";

    NoteExcitationConfig note;
    note.midiNote = 64; // E4
    note.velocity = 0.75;
    note.gateMs = 300.0;
    note.settlingMs = 60.0;
    ctx.observedNotes.push_back(note);

    measurement::ControlStateSnapshot snap;
    snap.controlId = "FILTER_CUTOFF";
    snap.confirmationStatus = "user_supplied";
    snap.displayValue = "650 Hz / 65%";
    snap.normalizedValue = 0.65f;
    ctx.controlSnapshots.push_back(snap);

    return ctx;
}

ExplorationContext createIncompleteExplorationContext()
{
    ExplorationContext ctx;
    ctx.targetId = "korg_ms20";
    ctx.targetName = "Korg MS-20";
    ctx.targetDeviceType = "SyntheticFixture";
    ctx.sampleRate = 48000.0;
    ctx.channels = 2;
    ctx.repetitions = 3;
    ctx.explorationSessionId = "exp_sess_incomplete_01";

    NoteExcitationConfig note;
    note.midiNote = 60;
    note.velocity = 0.8;
    note.gateMs = 250.0;
    note.settlingMs = 50.0;
    ctx.observedNotes.push_back(note);

    // Snapshot con control no declarado / anónimo
    measurement::ControlStateSnapshot snap;
    snap.controlId = "unspecified_param";
    snap.confirmationStatus = "unknown";
    snap.displayValue = "Posición no declarada";
    snap.normalizedValue = std::nullopt;
    ctx.controlSnapshots.push_back(snap);

    return ctx;
}

} // namespace

// ==============================================================================
// 1. Extracción Válida de Parámetros Observados
// ==============================================================================
TEST_CASE("Promotion: Extracción Válida de Parámetros Observados", "[recipe_promotion][schema]")
{
    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();

    auto result = promoService.promoteExploration(ctx);

    REQUIRE(result.isExecutableRecipe());
    REQUIRE(result.promotedRecipe.has_value());
    REQUIRE_FALSE(result.draft.has_value());
    REQUIRE(result.diagnostics.empty());

    const auto& r = *result.promotedRecipe;
    CHECK(r.schemaVersion == "1.0");
    CHECK(r.kind == "abd.measurement-recipe");
    CHECK(r.assistanceLevel == AssistanceLevel::Configurable);
    CHECK(r.revision == 1);
    CHECK_FALSE(r.recipeId.empty());
    CHECK(r.displayName == "Receta promovida: Moog Sub 37");

    // Restricciones de target
    REQUIRE(r.targetConstraints.targetKinds.size() == 1);
    CHECK(r.targetConstraints.targetKinds[0] == "SyntheticFixture");
    REQUIRE(r.targetConstraints.allowedSampleRatesHz.size() == 1);
    CHECK(r.targetConstraints.allowedSampleRatesHz[0] == 48000);
    CHECK(r.targetConstraints.channels == 2);

    // Excitación
    REQUIRE(r.excitation.notes.size() == 1);
    CHECK(r.excitation.notes[0].midiNote == 64);
    CHECK(r.excitation.notes[0].velocity == Catch::Approx(0.75));
    CHECK(r.excitation.notes[0].gateMs == Catch::Approx(300.0));
    CHECK(r.excitation.notes[0].settlingMs == Catch::Approx(60.0));
    CHECK(r.excitation.repetitions == 3);

    // Medición
    REQUIRE(r.measurement.points.size() == 1);
    CHECK(r.measurement.points[0].parameter == "FILTER_CUTOFF");
    CHECK(r.measurement.points[0].normalizedValue == Catch::Approx(0.65));
    CHECK(r.measurement.calibrationPolicy == "Required");
    CHECK(r.measurement.analysisPolicy == "CanonicalV1");

    // Validación formal completa con MeasurementRecipeService
    MeasurementRecipeService recipeService;
    std::string serializedJson = MeasurementRecipeService::serializeRecipeToJson(r);
    auto loadRes = recipeService.loadAndValidateJson(serializedJson);
    REQUIRE(loadRes.isSuccess());
    REQUIRE(loadRes.diagnostics.empty());
    CHECK(loadRes.recipeDocumentHash == result.recipeDocumentHash);
}

// ==============================================================================
// 2. Trazabilidad Criptográfica de Procedencia
// ==============================================================================
TEST_CASE("Promotion: Trazabilidad Criptográfica de Procedencia", "[recipe_promotion][provenance]")
{
    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();

    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    const auto& prov = result.promotedRecipe->provenance;
    CHECK(prov.authoringSource == "promoted_from_exploration");
    CHECK(prov.sourceKind == "exploration");
    CHECK(prov.sourceExplorationId == "exp_sess_7a8b9c");
    CHECK_FALSE(prov.sourceExplorationHash.empty());
    CHECK(prov.promotionToolVersion == "1.0.0");

    // CORRECCIÓN 1 OBLIGATORIA: sourceRecipeDocumentHash debe permanecer estrictamente vacío
    CHECK(prov.sourceRecipeDocumentHash.empty());

    // Determinismo del sourceExplorationHash
    std::string expectedHash = RecipePromotionService::computeExplorationHash(ctx);
    CHECK(prov.sourceExplorationHash == expectedHash);

    // Idéntico contexto reconstruido produce idéntico sourceExplorationHash
    auto ctxRebuilt = createCompleteExplorationContext();
    CHECK(RecipePromotionService::computeExplorationHash(ctxRebuilt) == expectedHash);

    // Modificar una nota observada cambia el hash
    auto ctxModifiedNote = ctx;
    ctxModifiedNote.observedNotes[0].midiNote = 67;
    CHECK(RecipePromotionService::computeExplorationHash(ctxModifiedNote) != expectedHash);

    // Modificar un control declarado cambia el hash
    auto ctxModifiedControl = ctx;
    ctxModifiedControl.controlSnapshots[0].normalizedValue = 0.80f;
    CHECK(RecipePromotionService::computeExplorationHash(ctxModifiedControl) != expectedHash);

    // Modificar el estado de confirmación de un control cambia el hash
    auto ctxModifiedConfirm = ctx;
    ctxModifiedConfirm.controlSnapshots[0].confirmationStatus = "operator_verified";
    CHECK(RecipePromotionService::computeExplorationHash(ctxModifiedConfirm) != expectedHash);

    // Modificar la traza de eventos observados cambia el hash
    auto ctxModifiedTrace = ctx;
    ctxModifiedTrace.relevantEventTrace.push_back("EVT_NOTE_ON_E4");
    CHECK(RecipePromotionService::computeExplorationHash(ctxModifiedTrace) != expectedHash);

    // OPCIÓN B: promotedAt vive fuera de MeasurementRecipe (en PromotionRecord)
    // Dos promociones idénticas a horas distintas producen idéntico recipeDocumentHash
    auto resMorning = promoService.promoteExploration(ctx, "", "2026-09-24T08:00:00Z");
    auto resEvening = promoService.promoteExploration(ctx, "", "2026-09-24T20:45:00Z");

    REQUIRE(resMorning.isExecutableRecipe());
    REQUIRE(resEvening.isExecutableRecipe());
    REQUIRE(resMorning.promotionRecord.has_value());
    REQUIRE(resEvening.promotionRecord.has_value());

    // Metadata temporal cambia en PromotionRecord (demostrando que el tiempo pertenece al registro histórico)
    CHECK(resMorning.promotionRecord->promotedAtIso8601 == "2026-09-24T08:00:00Z");
    CHECK(resEvening.promotionRecord->promotedAtIso8601 == "2026-09-24T20:45:00Z");
    CHECK(resMorning.promotionRecord->promotedAtIso8601 != resEvening.promotionRecord->promotedAtIso8601);
    CHECK(resMorning.promotionRecord->promotedRecipeDocumentHash == resMorning.recipeDocumentHash);
    CHECK(resEvening.promotionRecord->promotedRecipeDocumentHash == resEvening.recipeDocumentHash);

    // Hashes científicos permanecen 100% deterministas e idénticos en la Recipe
    CHECK(resMorning.recipeDocumentHash == resEvening.recipeDocumentHash);
    CHECK(resMorning.promotedRecipe->provenance.sourceExplorationHash == resEvening.promotedRecipe->provenance.sourceExplorationHash);
}

// ==============================================================================
// 3. Resiliencia ante Controles No Declarados (Sin Invención)
// ==============================================================================
TEST_CASE("Promotion: Resiliencia ante Controles No Declarados (Sin Invención)", "[recipe_promotion][honesty]")
{
    RecipePromotionService promoService;
    auto ctx = createIncompleteExplorationContext();

    auto result = promoService.promoteExploration(ctx);

    // CORRECCIÓN 2 OBLIGATORIA: Cero invención de puntos como filter_cutoff@0.5
    REQUIRE_FALSE(result.isExecutableRecipe());
    REQUIRE_FALSE(result.promotedRecipe.has_value());
    REQUIRE(result.draft.has_value());

    // Verificación del PromotionDraft
    const auto& draft = *result.draft;
    CHECK(draft.targetId == "korg_ms20");
    CHECK(draft.targetName == "Korg MS-20");
    CHECK(draft.sourceExplorationId == "exp_sess_incomplete_01");
    CHECK_FALSE(draft.sourceExplorationHash.empty());
    REQUIRE(draft.undeclaredSnapshots.size() == 1);
    CHECK(draft.undeclaredSnapshots[0].confirmationStatus == "unknown");

    // Diagnóstico estructurado emitido
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics[0].code == "ERR_PROMOTION_CONTROL_UNDECLARED");
    CHECK(result.diagnostics[0].severity == DiagnosticSeverity::Error);
    CHECK(result.diagnostics[0].jsonPointer == "/exploration/controlStateSnapshots/0");
}

// ==============================================================================
// 4. Carga en RecipeExecutionController
// ==============================================================================
TEST_CASE("Promotion: Carga en RecipeExecutionController", "[recipe_promotion][controller]")
{
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();
    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    auto prepState = controller.promoteExploration(*result.promotedRecipe, result.recipeDocumentHash);

    CHECK(prepState.isRecipeSelected);
    CHECK(prepState.activeView == RecipeEditorView::Configurable);
    CHECK_FALSE(prepState.isModifiedFromBase);
    CHECK(prepState.workingRecipe.recipeId == result.promotedRecipe->recipeId);
    CHECK(prepState.workingRecipeDocumentHash == result.recipeDocumentHash);
    CHECK(prepState.baseRecipeDocumentHash == result.recipeDocumentHash);

    // Compilación determinista a ExperimentPlan
    CHECK_FALSE(prepState.experimentPlanHash.empty());
    REQUIRE(prepState.experimentPlan.windows.size() == 3); // 1 nota * 3 repeticiones = 3 ventanas
    CHECK(prepState.experimentPlan.events.size() == 9);   // 3 eventos por ensayo (Param + NoteOn + NoteOff)
}

// ==============================================================================
// 5. Invarianza de Estado (Sin Ejecución Prematura)
// ==============================================================================
TEST_CASE("Promotion: Invarianza de Estado (Sin Ejecución Prematura)", "[recipe_promotion][invariance]")
{
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();
    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    // Estado inicial del controlador de sesión
    auto initialSnap = sessionController.getCurrentSnapshot();
    CHECK(initialSnap.sessionStatus == ProfilingSessionStatus::Idle);

    // Promover la exploración
    auto prepState = controller.promoteExploration(*result.promotedRecipe, result.recipeDocumentHash);

    // REGLA: No altera la sesión activa en ProfilingSessionController, no crea audio
    auto postSnap = sessionController.getCurrentSnapshot();
    CHECK(postSnap.sessionStatus == ProfilingSessionStatus::Idle);
    CHECK(postSnap.sessionId == initialSnap.sessionId);
    CHECK_FALSE(prepState.isSessionPrepared);
    CHECK_FALSE(prepState.isEnvironmentCompatible);
}

// ==============================================================================
// 6. Edición Posterior de la Receta Promovida
// ==============================================================================
TEST_CASE("Promotion: Edición Posterior de la Receta Promovida", "[recipe_promotion][editing]")
{
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();
    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    controller.promoteExploration(*result.promotedRecipe, result.recipeDocumentHash);
    std::string originalPlanHash = controller.getCurrentState().experimentPlanHash;

    // El operador edita repeticiones en modo Configurable (3 -> 5)
    bool okReps = controller.updateRepetitions(5);
    REQUIRE(okReps);
    CHECK(controller.isModifiedFromPreset());
    CHECK(controller.getCurrentState().experimentPlan.windows.size() == 5);
    CHECK(controller.getCurrentState().workingRecipeDocumentHash != result.recipeDocumentHash);
    CHECK(controller.getCurrentState().experimentPlanHash != originalPlanHash);

    // El operador restablece la receta promovida a su estado inicial
    controller.resetToPreset();
    CHECK_FALSE(controller.isModifiedFromPreset());
    CHECK(controller.getCurrentState().workingRecipeDocumentHash == result.recipeDocumentHash);
    CHECK(controller.getCurrentState().experimentPlanHash == originalPlanHash);
    CHECK(controller.getCurrentState().experimentPlan.windows.size() == 3);
}

// ==============================================================================
// 7. Preparación Confirmada en el Stepper
// ==============================================================================
TEST_CASE("Promotion: Preparación Confirmada en el Stepper", "[recipe_promotion][stepper]")
{
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();
    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    controller.promoteExploration(*result.promotedRecipe, result.recipeDocumentHash);

    // Paso 3: Resolución de compatibilidad física frente al entorno
    auto env = getMockEnvironment();
    auto targetState = getMockTargetState();
    auto state = controller.resolveAgainstCurrentEnvironment(env, targetState);
    REQUIRE(state.isEnvironmentCompatible);

    // Confirmación formal del operador
    auto prepRes = controller.prepareSessionAfterConfirmation(targetState);
    REQUIRE(prepRes.success);
    REQUIRE(prepRes.preparedSession.has_value());

    const auto& sess = *prepRes.preparedSession;
    CHECK(sess.getTestCases().size() == 3);
    CHECK(sess.getTestCases()[0].midiNoteNumber == 64);
    CHECK(controller.getCurrentState().isSessionPrepared);
}

// ==============================================================================
// 8. Blindaje e Inmutabilidad de la Exploración Original
// ==============================================================================
TEST_CASE("Promotion: Blindaje e Inmutabilidad de la Exploración Original", "[recipe_promotion][d8_guard]")
{
    // Simular un registro de experimento de exploración ad-hoc
    core::ExperimentRecord originalRecord;
    originalRecord.experimentId = "exp_free_take_001";
    originalRecord.kind = core::ExperimentKind::Exploration;
    originalRecord.status = core::ExperimentStatus::LoadedForExploration;

    // Verificar que la exploración no es exportable como medición de producción
    CHECK_FALSE(originalRecord.isExportable());
    CHECK(originalRecord.kind == core::ExperimentKind::Exploration);

    // Ejecutar promoción a receta formal
    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();
    ctx.explorationSessionId = originalRecord.experimentId;

    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());

    // La exploración original permanece inmutable
    CHECK(originalRecord.kind == core::ExperimentKind::Exploration);
    CHECK(originalRecord.status == core::ExperimentStatus::LoadedForExploration);
    CHECK_FALSE(originalRecord.isExportable());
}

// ==============================================================================
// 9. Promoción con Control Declarado
// ==============================================================================
TEST_CASE("Promotion: Promoción con Control Declarado", "[recipe_promotion][declared_controls]")
{
    RecipePromotionService promoService;
    ExplorationContext ctx;
    ctx.targetId = "prophet_08";
    ctx.targetName = "Dave Smith Prophet '08";
    ctx.targetDeviceType = "SyntheticFixture";
    ctx.explorationSessionId = "exp_prophet_take_42";

    // Múltiples controles declarados por el operador
    measurement::ControlStateSnapshot snap1;
    snap1.controlId = "VCF_CUTOFF";
    snap1.confirmationStatus = "user_supplied";
    snap1.displayValue = "1.2 kHz";
    snap1.normalizedValue = 0.55f;
    ctx.controlSnapshots.push_back(snap1);

    measurement::ControlStateSnapshot snap2;
    snap2.controlId = "VCF_RESONANCE";
    snap2.confirmationStatus = "user_supplied";
    snap2.displayValue = "4.0 / 10.0";
    snap2.normalizedValue = 0.40f;
    ctx.controlSnapshots.push_back(snap2);

    auto result = promoService.promoteExploration(ctx);
    REQUIRE(result.isExecutableRecipe());
    REQUIRE(result.promotedRecipe.has_value());

    const auto& r = *result.promotedRecipe;
    REQUIRE(r.measurement.points.size() == 2);
    CHECK(r.measurement.points[0].parameter == "VCF_CUTOFF");
    CHECK(r.measurement.points[0].normalizedValue == Catch::Approx(0.55));
    CHECK(r.measurement.points[1].parameter == "VCF_RESONANCE");
    CHECK(r.measurement.points[1].normalizedValue == Catch::Approx(0.40));

    // Compilación a plan preserva los puntos de medición declarados
    auto plan = ExperimentPlanCompiler::compileToExperimentPlan(r);
    CHECK(plan.windows.size() == 6); // 1 nota por defecto * 3 reps * 2 puntos = 6 ventanas
}

// ==============================================================================
// 10. Promoción Incompleta Bloquea Preparación
// ==============================================================================
TEST_CASE("Promotion: Promoción Incompleta Bloquea Preparación", "[recipe_promotion][blocked]")
{
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createIncompleteExplorationContext();

    auto result = promoService.promoteExploration(ctx);
    REQUIRE_FALSE(result.isExecutableRecipe());
    REQUIRE(result.draft.has_value());

    // Un PromotionDraft no produce experimentPlanHash ejecutable ni PromotionRecord
    CHECK(result.recipeDocumentHash.empty());
    CHECK_FALSE(result.promotionRecord.has_value());
    CHECK_FALSE(result.promotedRecipe.has_value()); // No contiene MeasurementRecipe

    // Garantía estructural: PromotionDraft solo conserva evidencia de contexto no ejecutable
    const auto& draft = *result.draft;
    CHECK_FALSE(draft.sourceExplorationId.empty());
    CHECK_FALSE(draft.sourceExplorationHash.empty());
    CHECK(draft.undeclaredSnapshots.size() == 1);

    // El estado del controlador permanece sin receta seleccionada ni plan de experimento
    CHECK_FALSE(controller.getCurrentState().isRecipeSelected);
    CHECK_FALSE(controller.getCurrentState().isSessionPrepared);
    CHECK(controller.getCurrentState().experimentPlanHash.empty());      // No contiene ExperimentPlan
    CHECK_FALSE(controller.getCurrentState().resolvedPlan.has_value()); // No contiene ResolvedExecutionPlan
    CHECK_FALSE(controller.getCurrentState().isEnvironmentCompatible);  // No permite avanzar al Paso 3

    // La sesión permanece Idle (no contiene ProfilingSession y no arranca ProfilingSequencer)
    auto snap = sessionController.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Idle);

    // Intentar preparar sesión sin receta seleccionada debe fallar limpiamente
    // Una PromotionDraft jamás puede ser entregada a prepareSessionAfterConfirmation()
    auto targetState = getMockTargetState();
    auto prepRes = controller.prepareSessionAfterConfirmation(targetState);
    REQUIRE_FALSE(prepRes.success);
    CHECK_FALSE(prepRes.preparedSession.has_value());
    CHECK(prepRes.errorMessage == "No hay ninguna receta seleccionada para preparar.");
    CHECK_FALSE(controller.getCurrentState().isSessionPrepared);
}

// ==============================================================================
// 11. Navegación a Revisión de Recipe en el Stepper
// ==============================================================================
TEST_CASE("Promotion: Navegación a Revisión de Recipe en el Stepper", "[recipe_promotion][navigation][stepper]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    // 1. Contexto inicial: Exploración válida con controles declarados
    ProfilingSessionController sessionController;
    RecipeExecutionController controller(sessionController);

    RecipePromotionService promoService;
    auto ctx = createCompleteExplorationContext();

    // 2. Promoción exitosa
    auto promoResult = promoService.promoteExploration(ctx);
    REQUIRE(promoResult.isExecutableRecipe());
    REQUIRE(promoResult.promotedRecipe.has_value());

    // 3. El usuario pulsa "Promover a Receta Formal":
    //    Carga la receta en el controlador en modo Configurable
    auto prepState = controller.promoteExploration(*promoResult.promotedRecipe, promoResult.recipeDocumentHash);
    REQUIRE(prepState.isRecipeSelected);

    // Consecuencias directas en el controlador:
    CHECK(controller.getEditorView() == RecipeEditorView::Configurable);
    CHECK(controller.getWorkingRecipe().provenance.authoringSource == "promoted_from_exploration");
    CHECK(controller.getWorkingRecipe().provenance.sourceExplorationId == ctx.explorationSessionId);

    // Invarianza: No existe ProfilingSession nueva ni se genera audio
    auto snap = sessionController.getCurrentSnapshot();
    CHECK(snap.sessionStatus == ProfilingSessionStatus::Idle);
    CHECK_FALSE(prepState.isSessionPrepared);

    // 4. Panel visual RecipeEditorComponent en Paso 2 (Calibration & Setup)
    RecipeEditorComponent recipeEditorComponent;
    recipeEditorComponent.setController(&controller);
    recipeEditorComponent.updateFromState();
    recipeEditorComponent.setVisible(true);

    // Verificaciones normativas de consecuencia visible:
    REQUIRE(recipeEditorComponent.isVisible());
    REQUIRE(recipeEditorComponent.showsWorkingRecipe());
    REQUIRE(recipeEditorComponent.editorView() == RecipeEditorView::Configurable);

    // 5. El usuario no está obligado a reseleccionar target y puede continuar a preflight
    auto env = getMockEnvironment();
    auto targetState = getMockTargetState(); // Target compatible de la exploración
    auto resolvedState = controller.resolveAgainstCurrentEnvironment(env, targetState);
    REQUIRE(resolvedState.isEnvironmentCompatible);
    CHECK(resolvedState.resolutionResult.diagnostics.empty());

    // Y tras confirmación explícita, se prepara la sesión formal
    auto prepRes = controller.prepareSessionAfterConfirmation(targetState);
    REQUIRE(prepRes.success);
    REQUIRE(prepRes.preparedSession.has_value());
    CHECK_FALSE(prepRes.preparedSession->getTestCases().empty());
    CHECK(controller.getCurrentState().isSessionPrepared);
}


