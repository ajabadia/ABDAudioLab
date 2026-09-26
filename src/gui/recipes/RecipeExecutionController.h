#pragma once

#include <string>
#include <vector>
#include <optional>
#include "../../profiling/MeasurementRecipe.h"
#include "../../profiling/MeasurementRecipeService.h"
#include "../../profiling/ExperimentPlanCompiler.h"
#include "../session/ProfilingSessionController.h"
#include "../session/ProfilingSessionContracts.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @brief Nivel de detalle y controles expuestos por la interfaz de usuario en el editor de recetas.
 * Desacoplado del perfil intrínseco de la receta (recipe.assistanceLevel).
 */
enum class RecipeEditorView
{
    Quick,
    Configurable,
    Advanced
};

[[nodiscard]] inline std::string recipeEditorViewToString(RecipeEditorView v) noexcept
{
    switch (v)
    {
        case RecipeEditorView::Quick:        return "Quick";
        case RecipeEditorView::Configurable: return "Configurable";
        case RecipeEditorView::Advanced:     return "Advanced";
    }
    return "Quick";
}

/**
 * @brief Estado de preparación inmutable de la receta para el Stepper.
 * La selección o edición NO crea ni altera la sesión activa en el controlador.
 */
struct RecipePreparationState
{
    // Preset original inmutable cargado desde disco
    profiling::MeasurementRecipe baseRecipe;
    std::string baseRecipeDocumentHash;

    // Copia temporal de trabajo en memoria donde se aplican ajustes locales
    profiling::MeasurementRecipe workingRecipe;
    std::string workingRecipeDocumentHash;

    // Aliases para retrocompatibilidad con código y tests de HITO-09B
    profiling::MeasurementRecipe recipe;
    std::string recipeDocumentHash;

    synth::ExperimentPlan experimentPlan;
    std::string experimentPlanHash;

    std::optional<profiling::ResolvedExecutionPlan> resolvedPlan;
    std::optional<std::string> resolvedExecutionPlanHash;

    profiling::ResolveExecutionPlanResult resolutionResult;
    std::vector<profiling::ValidationDiagnostic> localEditDiagnostics;

    RecipeEditorView activeView { RecipeEditorView::Quick };
    bool isModifiedFromBase { false };
    bool isRecipeSelected { false };
    bool isEnvironmentCompatible { false };
    bool isSessionPrepared { false };
};

/**
 * @brief Resultado de la preparación confirmada de la sesión.
 */
struct PreparationResult
{
    bool success { false };
    std::string errorMessage;
    std::optional<core::ProfilingSession> preparedSession;
};

/**
 * @class RecipeExecutionController
 * @brief Coordinador de preparación y edición de recetas declarativas dentro del flujo de pasos existente.
 * Respeta la separación estricta:
 *   selectRecipe()                  -> Carga RecipeBase y clona WorkingRecipe limpia.
 *   update*()                       -> Aplica ajustes sobre WorkingRecipe con revalidación y recálculo determinista.
 *   resetToPreset()                 -> Descarta ajustes temporales y restablece WorkingRecipe a RecipeBase.
 *   setEditorView()                 -> Conmuta el nivel visual de detalle sin alterar la receta ni los hashes.
 *   resolveAgainstCurrentEnvironment() -> Comprueba compatibilidad física frente al target actual.
 *   prepareSessionAfterConfirmation()  -> Inyecta la ProfilingSession limpia sólo tras confirmación en el Stepper.
 */
class RecipeExecutionController
{
public:
    explicit RecipeExecutionController(session::ProfilingSessionController& sessionController);
    ~RecipeExecutionController() = default;

    /**
     * @brief Registra la selección de una receta, inicializa RecipeBase y clona WorkingRecipe limpia.
     * REGLA: No crea ProfilingSession, no inicia audio y no altera la sesión activa.
     */
    RecipePreparationState selectRecipe(const profiling::MeasurementRecipe& recipe,
                                        const std::string& recipeDocumentHash);

    /**
     * @brief Registra una receta formal derivada de una exploración ad-hoc (HITO-09D).
     * Inicializa RecipeBase y WorkingRecipe con la receta promovida, y fija la vista en Configurable.
     * REGLA: No crea ProfilingSession, no inicia audio y no altera la sesión activa.
     */
    RecipePreparationState promoteExploration(const profiling::MeasurementRecipe& promotedRecipe,
                                              const std::string& recipeDocumentHash);

    /**
     * @brief Cambia el nivel visual de inspección/edición (Quick, Configurable, Advanced).
     * REGLA: No altera ningún hash, no crea sesión y no inicia audio.
     */
    void setEditorView(RecipeEditorView view) noexcept;
    [[nodiscard]] RecipeEditorView getEditorView() const noexcept { return currentState_.activeView; }

    // --- Métodos de Edición Segura sobre WorkingRecipe ---
    bool updateRepetitions(int reps);
    bool updateNoteExcitation(int note, double vel, double gateMs, double settlingMs);
    bool updatePointSet(const std::vector<profiling::MeasurementPointConfig>& points);
    bool updateEvaluationPolicy(double snrDb, double thdPct, double f0TolCents);
    bool updateCalibrationPolicy(const std::string& policy);
    bool updateRandomSeed(std::optional<int> seed);

    /**
     * @brief Descarta los ajustes temporales y restablece WorkingRecipe al estado exacto de RecipeBase.
     */
    void resetToPreset();

    [[nodiscard]] bool isModifiedFromPreset() const noexcept { return currentState_.isModifiedFromBase; }
    [[nodiscard]] bool isRecipeSelected() const noexcept { return currentState_.isRecipeSelected; }
    [[nodiscard]] const profiling::MeasurementRecipe& getBaseRecipe() const noexcept { return currentState_.baseRecipe; }
    [[nodiscard]] const profiling::MeasurementRecipe& getWorkingRecipe() const noexcept { return currentState_.workingRecipe; }

    /**
     * @brief Resuelve la receta seleccionada frente al entorno físico y capacidades del target activo.
     */
    RecipePreparationState resolveAgainstCurrentEnvironment(const profiling::ExecutionEnvironment& env,
                                                            const gui::session::TargetSelectionState& targetState);

    /**
     * @brief Prepara formalmente la sesión sólo tras confirmación del usuario en el punto adecuado del Stepper.
     */
    PreparationResult prepareSessionAfterConfirmation(const gui::session::TargetSelectionState& targetState);

    [[nodiscard]] const RecipePreparationState& getCurrentState() const noexcept { return currentState_; }
    void clearPreparation() noexcept;

private:
    bool commitWorkingRecipe(const profiling::MeasurementRecipe& candidate);

    session::ProfilingSessionController& sessionController_;
    profiling::MeasurementRecipeService recipeService_;
    RecipePreparationState currentState_;
};

} // namespace abdaudiolab::gui::recipes
