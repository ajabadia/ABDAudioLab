#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "MeasurementRecipe.h"
#include "MeasurementRecipeService.h"
#include "../measurement/MeasurementSessionContracts.h"

namespace abdaudiolab::profiling
{

/**
 * @brief Registro canónico inmutable de evidencia estable de una exploración ad-hoc.
 *
 * Contrato metrológico estricto:
 * INCLUYE exclusivamente evidencia física y de configuración determinista:
 *   - schemaVersion;
 *   - explorationSessionId (identificador único persistido y estable, ej. UUID; NUNCA punteros C++, handles de ventana o contadores temporales);
 *   - targetId estable, targetDeviceType (identidad canónica del target); targetName (descriptor legible);
 *   - sampleRate, channels;
 *   - observedNotes (midiNote, velocity, gateMs, settlingMs);
 *   - controlSnapshots (controlId, confirmationStatus, displayValue, normalizedValue);
 *   - relevantEventTrace (identificadores canónicos de eventos observados).
 *
 * EXCLUYE expresamente:
 *   - Rutas temporales de archivo o directorios del sistema;
 *   - Punteros o direcciones de memoria;
 *   - Estado de interfaz de usuario (ventanas, scroll, selecciones transitorias);
 *   - Timestamps volátiles de render o ejecución;
 *   - Cadenas de diagnóstico local dependientes del host.
 */
struct ExplorationRecord
{
    std::string schemaVersion { "1.0" };
    std::string explorationSessionId;
    std::string targetId;
    std::string targetName;
    std::string targetDeviceType;
    double sampleRate { 48000.0 };
    int channels { 2 };
    std::vector<NoteExcitationConfig> observedNotes;
    std::vector<measurement::ControlStateSnapshot> controlSnapshots;
    std::vector<std::string> relevantEventTrace;

    [[nodiscard]] std::string toCanonicalJson() const;
};

/**
 * @brief Contexto de observación capturado durante una sesión de Toma Libre.
 */
struct ExplorationContext
{
    std::string targetId;
    std::string targetName;
    std::string targetDeviceType { "SyntheticFixture" };
    double sampleRate { 48000.0 };
    int channels { 2 };
    int repetitions { 3 };

    std::vector<NoteExcitationConfig> observedNotes;
    std::vector<measurement::ControlStateSnapshot> controlSnapshots;
    std::vector<std::string> relevantEventTrace;

    std::string explorationSessionId;
    std::string explorationHash; // Si está vacío, se computa canónicamente vía ExplorationRecord
};

/**
 * @brief Registro histórico temporal de la acción de promoción (Opción B).
 * Mantiene la metadata temporal (promotedAtIso8601) fuera de MeasurementRecipe
 * para garantizar determinismo y reproducibilidad científica de la receta y sus hashes.
 */
struct PromotionRecord
{
    std::string kind { "abd.promotion-record" };
    std::string promotionRecordId;
    std::string promotedAtIso8601;

    std::string sourceExplorationId;
    std::string sourceExplorationHash;

    std::string promotedRecipeDocumentHash;
    std::string promotionToolVersion { "1.0.0" };
};

/**
 * @brief Borrador no ejecutable generado cuando una exploración carece de controles declarados.
 * Prohíbe la invención automática de puntos de control (ej. filter_cutoff@0.5) y preserva
 * la exploración intacta como evidencia no certificable.
 * Garantía: NO contiene MeasurementRecipe, ExperimentPlan, ResolvedExecutionPlan ni ProfilingSession.
 */
struct PromotionDraft
{
    std::string targetId;
    std::string targetName;
    std::string targetDeviceType;
    std::vector<NoteExcitationConfig> observedNotes;
    std::vector<measurement::ControlStateSnapshot> undeclaredSnapshots;
    std::string sourceExplorationId;
    std::string sourceExplorationHash;
    std::string diagnosticReason;
};

/**
 * @brief Resultado explícito del intento de promoción de una exploración.
 */
struct PromotionResult
{
    std::optional<MeasurementRecipe> promotedRecipe;
    std::string recipeDocumentHash;
    std::optional<PromotionRecord> promotionRecord; // Metadata temporal separada (Opción B)
    std::optional<PromotionDraft> draft;
    std::vector<ValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isExecutableRecipe() const noexcept
    {
        return promotedRecipe.has_value()
            && std::none_of(
                diagnostics.begin(),
                diagnostics.end(),
                [](const ValidationDiagnostic& diagnostic)
                {
                    return diagnostic.severity == DiagnosticSeverity::Error;
                });
    }
};

/**
 * @class RecipePromotionService
 * @brief Servicio de dominio puro para convertir honestamente una exploración de Toma Libre
 * en una MeasurementRecipe formal según JSON Schema Draft 2020-12 y RFC 8785.
 */
class RecipePromotionService
{
public:
    explicit RecipePromotionService(const MeasurementRecipeService& recipeService = MeasurementRecipeService{});
    ~RecipePromotionService() = default;

    /**
     * @brief Computa el hash criptográfico SHA-256 canónico del ExplorationRecord.
     */
    [[nodiscard]] static std::string computeExplorationHash(const ExplorationRecord& record);
    [[nodiscard]] static std::string computeExplorationHash(const ExplorationContext& ctx);

    /**
     * @brief Construye el ExplorationRecord determinista excluyendo datos volátiles.
     */
    [[nodiscard]] static ExplorationRecord buildExplorationRecord(const ExplorationContext& ctx);

    /**
     * @brief Evalúa y promueve un contexto exploratorio a una receta formal o borrador no ejecutable.
     * Caso A (Controles declarados): produce MeasurementRecipe ejecutable con sourceExplorationHash y PromotionRecord.
     * Caso B (Controles no declarados / incompletos): produce PromotionDraft y ERR_PROMOTION_CONTROL_UNDECLARED.
     */
    [[nodiscard]] PromotionResult promoteExploration(const ExplorationContext& ctx,
                                                     const std::string& customRecipeName = "",
                                                     const std::string& promotedAtIso8601 = "") const;

private:
    MeasurementRecipeService recipeService_;
};

} // namespace abdaudiolab::profiling
