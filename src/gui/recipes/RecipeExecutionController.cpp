#include "RecipeExecutionController.h"

namespace abdaudiolab::gui::recipes
{

RecipeExecutionController::RecipeExecutionController(session::ProfilingSessionController& sessionController)
    : sessionController_(sessionController)
{
}

RecipePreparationState RecipeExecutionController::selectRecipe(const profiling::MeasurementRecipe& recipe,
                                                               const std::string& recipeDocumentHash)
{
    currentState_ = RecipePreparationState{};

    // Base inmutable
    currentState_.baseRecipe = recipe;
    currentState_.baseRecipeDocumentHash = recipeDocumentHash;

    // Copia temporal de trabajo limpia
    currentState_.workingRecipe = recipe;
    currentState_.workingRecipeDocumentHash = recipeDocumentHash;

    // Aliases para retrocompatibilidad
    currentState_.recipe = recipe;
    currentState_.recipeDocumentHash = recipeDocumentHash;

    // Compilación determinista a ExperimentPlan puro
    currentState_.experimentPlan = profiling::ExperimentPlanCompiler::compileToExperimentPlan(currentState_.workingRecipe);
    currentState_.experimentPlanHash = profiling::ExperimentPlanCompiler::computeExperimentPlanHash(currentState_.experimentPlan);

    currentState_.isRecipeSelected = true;
    currentState_.isModifiedFromBase = false;
    currentState_.isEnvironmentCompatible = false;
    currentState_.isSessionPrepared = false;
    currentState_.localEditDiagnostics.clear();

    // La vista por defecto se alinea inicialmente con el perfil recomendado de la receta
    if (recipe.assistanceLevel == profiling::AssistanceLevel::Configurable)
        currentState_.activeView = RecipeEditorView::Configurable;
    else if (recipe.assistanceLevel == profiling::AssistanceLevel::Advanced)
        currentState_.activeView = RecipeEditorView::Advanced;
    else
        currentState_.activeView = RecipeEditorView::Quick;

    // REGLA FUNDAMENTAL: La selección NO altera la sesión activa en el controlador ni inicia audio.
    return currentState_;
}

RecipePreparationState RecipeExecutionController::promoteExploration(const profiling::MeasurementRecipe& promotedRecipe,
                                                                    const std::string& recipeDocumentHash)
{
    currentState_ = RecipePreparationState{};

    // Base inmutable con procedencia "promoted_from_exploration"
    currentState_.baseRecipe = promotedRecipe;
    currentState_.baseRecipeDocumentHash = recipeDocumentHash;

    // Copia temporal de trabajo en memoria
    currentState_.workingRecipe = promotedRecipe;
    currentState_.workingRecipeDocumentHash = recipeDocumentHash;

    // Aliases para retrocompatibilidad
    currentState_.recipe = promotedRecipe;
    currentState_.recipeDocumentHash = recipeDocumentHash;

    // Compilación determinista a ExperimentPlan puro
    currentState_.experimentPlan = profiling::ExperimentPlanCompiler::compileToExperimentPlan(currentState_.workingRecipe);
    currentState_.experimentPlanHash = profiling::ExperimentPlanCompiler::computeExperimentPlanHash(currentState_.experimentPlan);

    currentState_.isRecipeSelected = true;
    currentState_.isModifiedFromBase = false;
    currentState_.isEnvironmentCompatible = false;
    currentState_.isSessionPrepared = false;
    currentState_.localEditDiagnostics.clear();

    // Las recetas promovidas se abren por defecto en vista Configurable para revisión del operador
    currentState_.activeView = RecipeEditorView::Configurable;

    // REGLA FUNDAMENTAL: La promoción NO crea sesión ejecutable, no altera la sesión activa en el controlador ni inicia audio.
    return currentState_;
}

void RecipeExecutionController::setEditorView(RecipeEditorView view) noexcept
{
    currentState_.activeView = view;
    // Conmutar el nivel visual no cambia la receta, ni los hashes, ni crea sesiones ni audio.
}

bool RecipeExecutionController::commitWorkingRecipe(const profiling::MeasurementRecipe& candidate)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto mutated = candidate;
    // Marcar procedencia como modificada por el operador
    mutated.provenance.authoringSource = "operator_manual";

    // Serializar a JSON para validación estricta por capas mediante MeasurementRecipeService
    std::string jsonStr = profiling::MeasurementRecipeService::serializeRecipeToJson(mutated);
    auto loadResult = recipeService_.loadAndValidateJson(jsonStr);

    if (!loadResult.isSuccess())
    {
        // Retener la receta válida previa y publicar diagnósticos de error
        currentState_.localEditDiagnostics = loadResult.diagnostics;
        return false;
    }

    // Éxito: aplicar WorkingRecipe y recompilar plan
    currentState_.localEditDiagnostics.clear();
    currentState_.workingRecipe = loadResult.recipe;
    currentState_.workingRecipeDocumentHash = loadResult.recipeDocumentHash;

    // Actualizar aliases
    currentState_.recipe = loadResult.recipe;
    currentState_.recipeDocumentHash = loadResult.recipeDocumentHash;

    // Recompilar ExperimentPlan determinista con hashes nuevos
    currentState_.experimentPlan = profiling::ExperimentPlanCompiler::compileToExperimentPlan(currentState_.workingRecipe);
    currentState_.experimentPlanHash = profiling::ExperimentPlanCompiler::computeExperimentPlanHash(currentState_.experimentPlan);

    // Invalidar resolución de entorno previa (deberá re-evaluarse frente al target)
    currentState_.resolvedPlan = std::nullopt;
    currentState_.resolvedExecutionPlanHash = std::nullopt;
    currentState_.isEnvironmentCompatible = false;
    currentState_.isSessionPrepared = false;

    currentState_.isModifiedFromBase = (currentState_.workingRecipeDocumentHash != currentState_.baseRecipeDocumentHash);

    return true;
}

bool RecipeExecutionController::updateRepetitions(int reps)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    candidate.excitation.repetitions = reps;
    return commitWorkingRecipe(candidate);
}

bool RecipeExecutionController::updateNoteExcitation(int note, double vel, double gateMs, double settlingMs)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    if (candidate.excitation.notes.empty())
    {
        profiling::NoteExcitationConfig n;
        n.midiNote = note;
        n.velocity = vel;
        n.gateMs = gateMs;
        n.settlingMs = settlingMs;
        candidate.excitation.notes.push_back(n);
    }
    else
    {
        candidate.excitation.notes[0].midiNote = note;
        candidate.excitation.notes[0].velocity = vel;
        candidate.excitation.notes[0].gateMs = gateMs;
        candidate.excitation.notes[0].settlingMs = settlingMs;
    }

    return commitWorkingRecipe(candidate);
}

bool RecipeExecutionController::updatePointSet(const std::vector<profiling::MeasurementPointConfig>& points)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    candidate.measurement.points = points;
    return commitWorkingRecipe(candidate);
}

bool RecipeExecutionController::updateEvaluationPolicy(double snrDb, double thdPct, double f0TolCents)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    candidate.evaluationPolicy.minimumSnrDb = snrDb;
    candidate.evaluationPolicy.maximumThdPercent = thdPct;
    candidate.evaluationPolicy.f0ToleranceCents = f0TolCents;
    return commitWorkingRecipe(candidate);
}

bool RecipeExecutionController::updateCalibrationPolicy(const std::string& policy)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    candidate.measurement.calibrationPolicy = policy;
    return commitWorkingRecipe(candidate);
}

bool RecipeExecutionController::updateRandomSeed(std::optional<int> seed)
{
    if (!currentState_.isRecipeSelected)
        return false;

    auto candidate = currentState_.workingRecipe;
    candidate.excitation.seed = seed;
    return commitWorkingRecipe(candidate);
}

void RecipeExecutionController::resetToPreset()
{
    if (!currentState_.isRecipeSelected)
        return;

    currentState_.workingRecipe = currentState_.baseRecipe;
    currentState_.workingRecipeDocumentHash = currentState_.baseRecipeDocumentHash;

    currentState_.recipe = currentState_.baseRecipe;
    currentState_.recipeDocumentHash = currentState_.baseRecipeDocumentHash;

    currentState_.experimentPlan = profiling::ExperimentPlanCompiler::compileToExperimentPlan(currentState_.workingRecipe);
    currentState_.experimentPlanHash = profiling::ExperimentPlanCompiler::computeExperimentPlanHash(currentState_.experimentPlan);

    currentState_.resolvedPlan = std::nullopt;
    currentState_.resolvedExecutionPlanHash = std::nullopt;
    currentState_.isEnvironmentCompatible = false;
    currentState_.isSessionPrepared = false;
    currentState_.isModifiedFromBase = false;
    currentState_.localEditDiagnostics.clear();
}

RecipePreparationState RecipeExecutionController::resolveAgainstCurrentEnvironment(
    const profiling::ExecutionEnvironment& env,
    const gui::session::TargetSelectionState& /*targetState*/)
{
    if (!currentState_.isRecipeSelected)
    {
        return currentState_;
    }

    auto result = profiling::ExperimentPlanCompiler::resolveExecutionPlan(
        currentState_.experimentPlan,
        env,
        &currentState_.workingRecipe);

    currentState_.resolutionResult = result;

    if (result.succeeded() && result.resolvedPlan.has_value())
    {
        currentState_.resolvedPlan = result.resolvedPlan;
        currentState_.resolvedExecutionPlanHash = result.resolvedPlan->resolvedExecutionPlanHash;
        currentState_.isEnvironmentCompatible = true;
    }
    else
    {
        currentState_.resolvedPlan = std::nullopt;
        currentState_.resolvedExecutionPlanHash = std::nullopt;
        currentState_.isEnvironmentCompatible = false;
    }

    return currentState_;
}

PreparationResult RecipeExecutionController::prepareSessionAfterConfirmation(
    const gui::session::TargetSelectionState& targetState)
{
    if (!currentState_.isRecipeSelected)
    {
        return PreparationResult{ false, "No hay ninguna receta seleccionada para preparar.", std::nullopt };
    }

    if (!currentState_.isEnvironmentCompatible || !currentState_.resolvedPlan.has_value())
    {
        std::string err = "El target actual o el entorno fisico no cumplen los requisitos de la receta:";
        for (const auto& diag : currentState_.resolutionResult.diagnostics)
        {
            if (diag.severity == profiling::DiagnosticSeverity::Error)
            {
                err += " [" + diag.code + "] " + diag.message;
            }
        }
        return PreparationResult{ false, err, std::nullopt };
    }

    // Crea la sesión canónica a partir del plan resuelto de la WorkingRecipe y el targetState
    core::ProfilingSession session = profiling::ExperimentPlanCompiler::createProfilingSession(
        *currentState_.resolvedPlan,
        targetState);

    // Configura el target en el controlador existente sin saltar etapas ni ejecutar prematuramente
    sessionController_.selectTarget(targetState);

    currentState_.isSessionPrepared = true;

    return PreparationResult{ true, "", std::move(session) };
}

void RecipeExecutionController::clearPreparation() noexcept
{
    currentState_ = RecipePreparationState{};
}

} // namespace abdaudiolab::gui::recipes
