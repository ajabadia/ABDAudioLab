#include "RecipePromotionService.h"
#include <nlohmann/json.hpp>
#include <unordered_set>
#include "../synth/Sha256.h"

namespace abdaudiolab::profiling
{

namespace
{

using json = nlohmann::json;

std::string sanitizeId(const std::string& input)
{
    std::string out;
    for (char c : input)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            out.push_back(c);
        else
            out.push_back('_');
    }
    return out.empty() ? "unnamed" : out;
}

} // namespace

std::string ExplorationRecord::toCanonicalJson() const
{
    json j = json::object();
    j["channels"] = channels;

    json ctrlArr = json::array();
    for (const auto& c : controlSnapshots)
    {
        json cObj = json::object();
        cObj["confirmationStatus"] = c.confirmationStatus;
        cObj["controlId"] = c.controlId;
        cObj["displayValue"] = c.displayValue;
        if (c.normalizedValue.has_value())
            cObj["normalizedValue"] = *c.normalizedValue;
        else
            cObj["normalizedValue"] = nullptr;
        ctrlArr.push_back(cObj);
    }
    j["controlSnapshots"] = ctrlArr;

    j["explorationSessionId"] = explorationSessionId;

    json noteArr = json::array();
    for (const auto& n : observedNotes)
    {
        json nObj = json::object();
        nObj["gateMs"] = n.gateMs;
        nObj["midiNote"] = n.midiNote;
        nObj["settlingMs"] = n.settlingMs;
        nObj["velocity"] = n.velocity;
        noteArr.push_back(nObj);
    }
    j["observedNotes"] = noteArr;

    json eventArr = json::array();
    for (const auto& ev : relevantEventTrace)
        eventArr.push_back(ev);
    j["relevantEventTrace"] = eventArr;

    j["sampleRate"] = sampleRate;
    j["schemaVersion"] = schemaVersion;
    j["targetDeviceType"] = targetDeviceType;
    j["targetId"] = targetId;
    j["targetName"] = targetName;

    return MeasurementRecipeService::canonicalizeJsonRfc8785(j.dump());
}

RecipePromotionService::RecipePromotionService(const MeasurementRecipeService& recipeService)
    : recipeService_(recipeService)
{
}

ExplorationRecord RecipePromotionService::buildExplorationRecord(const ExplorationContext& ctx)
{
    ExplorationRecord rec;
    rec.schemaVersion = "1.0";
    rec.explorationSessionId = ctx.explorationSessionId;
    rec.targetId = ctx.targetId;
    rec.targetName = ctx.targetName;
    rec.targetDeviceType = ctx.targetDeviceType.empty() ? "SyntheticFixture" : ctx.targetDeviceType;
    rec.sampleRate = ctx.sampleRate > 0.0 ? ctx.sampleRate : 48000.0;
    rec.channels = ctx.channels > 0 ? ctx.channels : 2;
    rec.observedNotes = ctx.observedNotes;
    rec.controlSnapshots = ctx.controlSnapshots;
    rec.relevantEventTrace = ctx.relevantEventTrace;
    return rec;
}

std::string RecipePromotionService::computeExplorationHash(const ExplorationRecord& record)
{
    std::string canonicalJson = record.toCanonicalJson();
    return synth::Sha256::computeHex(canonicalJson);
}

std::string RecipePromotionService::computeExplorationHash(const ExplorationContext& ctx)
{
    ExplorationRecord rec = buildExplorationRecord(ctx);
    return computeExplorationHash(rec);
}

PromotionResult RecipePromotionService::promoteExploration(const ExplorationContext& ctx,
                                                           const std::string& customRecipeName,
                                                           const std::string& promotedAtIso8601) const
{
    PromotionResult res;
    std::string explorationHash = ctx.explorationHash.empty() ? computeExplorationHash(ctx) : ctx.explorationHash;

    // -------------------------------------------------------------------------
    // Comprobación de Controles Declarados (Honestidad Metrológica)
    // -------------------------------------------------------------------------
    std::vector<MeasurementPointConfig> declaredPoints;
    std::unordered_set<std::string> seenPoints;

    for (const auto& snap : ctx.controlSnapshots)
    {
        if (snap.controlId.empty() || snap.controlId == "unspecified_param")
            continue;
        if (snap.confirmationStatus == "unknown")
            continue;
        if (!snap.normalizedValue.has_value())
            continue;

        double val = *snap.normalizedValue;
        if (val < 0.0 || val > 1.0)
            continue;

        std::string key = snap.controlId + "@" + std::to_string(val);
        if (seenPoints.find(key) == seenPoints.end())
        {
            seenPoints.insert(key);
            MeasurementPointConfig pt;
            pt.parameter = snap.controlId;
            pt.normalizedValue = val;
            pt.semanticId = snap.controlId;
            declaredPoints.push_back(pt);
        }
    }

    // Caso B: Exploración incompleta (sin controles declarados válidos)
    if (declaredPoints.empty())
    {
        PromotionDraft draft;
        draft.targetId = ctx.targetId;
        draft.targetName = ctx.targetName;
        draft.targetDeviceType = ctx.targetDeviceType.empty() ? "SyntheticFixture" : ctx.targetDeviceType;
        draft.observedNotes = ctx.observedNotes;
        draft.undeclaredSnapshots = ctx.controlSnapshots;
        draft.sourceExplorationId = ctx.explorationSessionId;
        draft.sourceExplorationHash = explorationHash;
        draft.diagnosticReason = "La exploracion contiene controles o posiciones no declaradas. "
                                 "No es posible crear una MeasurementRecipe ejecutable sin un "
                                 "semanticId de parametro declarado o un conjunto de puntos seleccionado.";

        ValidationDiagnostic diag;
        diag.severity = DiagnosticSeverity::Error;
        diag.code = "ERR_PROMOTION_CONTROL_UNDECLARED";
        diag.jsonPointer = "/exploration/controlStateSnapshots/0";
        diag.message = draft.diagnosticReason;

        res.draft = std::move(draft);
        res.diagnostics.push_back(diag);
        return res;
    }

    // -------------------------------------------------------------------------
    // Caso A: Exploración con datos suficientes -> Derivación de MeasurementRecipe
    // -------------------------------------------------------------------------
    MeasurementRecipe recipe;
    recipe.schemaVersion = "1.0";
    recipe.kind = "abd.measurement-recipe";

    std::string safeTarget = sanitizeId(ctx.targetId.empty() ? "target" : ctx.targetId);
    std::string safeSession = sanitizeId(ctx.explorationSessionId.empty() ? "session" : ctx.explorationSessionId);
    recipe.recipeId = "recipe_promoted_" + safeTarget + "_" + safeSession;

    recipe.displayName = customRecipeName.empty()
                             ? ("Receta promovida: " + (ctx.targetName.empty() ? "Target" : ctx.targetName))
                             : customRecipeName;
    recipe.description = "Receta formal derivada de exploracion ad-hoc (Toma Libre).";
    recipe.assistanceLevel = AssistanceLevel::Configurable;
    recipe.revision = 1;

    // Restricciones de target
    recipe.targetConstraints.targetKinds = { ctx.targetDeviceType.empty() ? "SyntheticFixture" : ctx.targetDeviceType };
    recipe.targetConstraints.allowedSampleRatesHz = { static_cast<int>(ctx.sampleRate > 0.0 ? ctx.sampleRate : 48000) };
    recipe.targetConstraints.channels = ctx.channels > 0 ? ctx.channels : 2;

    // Excitación
    if (ctx.observedNotes.empty())
    {
        NoteExcitationConfig defNote;
        defNote.midiNote = 60;
        defNote.velocity = 0.8;
        defNote.gateMs = 250.0;
        defNote.settlingMs = 50.0;
        recipe.excitation.notes.push_back(defNote);
    }
    else
    {
        recipe.excitation.notes = ctx.observedNotes;
    }
    recipe.excitation.repetitions = ctx.repetitions > 0 ? ctx.repetitions : 3;

    // Medición
    recipe.measurement.points = declaredPoints;
    recipe.measurement.calibrationPolicy = "Required";
    recipe.measurement.analysisPolicy = "CanonicalV1";

    // Política de evaluación
    recipe.evaluationPolicy.minimumSnrDb = 50.0;
    recipe.evaluationPolicy.maximumThdPercent = 1.0;
    recipe.evaluationPolicy.f0ToleranceCents = 10.0;

    // Provenance científico estricto (Corrección 1: no usar sourceRecipeDocumentHash)
    recipe.provenance.authoringSource = "promoted_from_exploration";
    recipe.provenance.sourceKind = "exploration";
    recipe.provenance.sourceExplorationId = ctx.explorationSessionId;
    recipe.provenance.sourceExplorationHash = explorationHash;
    recipe.provenance.promotionToolVersion = "1.0.0";
    recipe.provenance.documentationRef = "Promovido desde Toma Libre (Exploration)";
    recipe.provenance.sourceRecipeDocumentHash = ""; // Excluido obligatoriamente

    // Serialización y validación formal
    std::string jsonStr = MeasurementRecipeService::serializeRecipeToJson(recipe);
    RecipeLoadResult loadRes = recipeService_.loadAndValidateJson(jsonStr);

    if (loadRes.isSuccess())
    {
        res.promotedRecipe = std::move(loadRes.recipe);
        res.recipeDocumentHash = loadRes.recipeDocumentHash;

        PromotionRecord promoRec;
        promoRec.kind = "abd.promotion-record";
        promoRec.promotionRecordId = "promotion_" + safeSession;
        promoRec.promotedAtIso8601 = promotedAtIso8601.empty() ? "2026-09-24T00:00:00Z" : promotedAtIso8601;
        promoRec.sourceExplorationId = ctx.explorationSessionId;
        promoRec.sourceExplorationHash = explorationHash;
        promoRec.promotedRecipeDocumentHash = loadRes.recipeDocumentHash;
        promoRec.promotionToolVersion = "1.0.0";
        res.promotionRecord = std::move(promoRec);
    }
    else
    {
        res.diagnostics = std::move(loadRes.diagnostics);
    }

    return res;
}

} // namespace abdaudiolab::profiling
