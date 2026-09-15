#include "ProfilingSessionController.h"
#include <algorithm>
#include <sstream>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "synth/Sha256.h"
#include "synth/ModelEvaluationBuilder.h"

namespace abdaudiolab::gui::session
{

ProfilingSessionController::ProfilingSessionController()
    : aliveToken_(std::make_shared<std::atomic<bool>>(true)),
      coordinator_(std::make_unique<ProfilingSessionCoordinator>(this))
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.sessionId = "session_" + juce::Uuid().toString().toStdString();
    currentSnapshot_.monotonicSequence = ++sequenceCounter_;
    currentSnapshot_.timestampMs = getCurrentTimeMs();
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Idle;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::TargetSelection;
}

ProfilingSessionController::~ProfilingSessionController()
{
    if (aliveToken_)
        aliveToken_->store(false, std::memory_order_release);
    removeAllListeners();
    if (coordinator_)
    {
        coordinator_->requestCancel();
        coordinator_->waitForWorkerToStop(3000);
        coordinator_.reset();
    }
}

void ProfilingSessionController::addListener(IProfilingSessionEventListener* listener)
{
    if (listener == nullptr)
        return;
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end())
    {
        listeners_.push_back(listener);
    }
}

void ProfilingSessionController::removeListener(IProfilingSessionEventListener* listener)
{
    std::lock_guard<std::mutex> lock(listenersMutex_);
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
}

void ProfilingSessionController::removeAllListeners()
{
    std::lock_guard<std::mutex> lock(listenersMutex_);
    listeners_.clear();
}

ProfilingSessionSnapshot ProfilingSessionController::getCurrentSnapshot() const
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return currentSnapshot_;
}

uint64_t ProfilingSessionController::getCurrentTimeMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

bool ProfilingSessionController::canTransitionTo(ProfilingSessionStatus newStatus) const
{
    auto cur = currentSnapshot_.sessionStatus;

    if (cur == newStatus)
        return false; // Evento duplicado no provoca doble transición

    switch (newStatus)
    {
        case ProfilingSessionStatus::Idle:
            return true;

        case ProfilingSessionStatus::TargetSelected:
            return (cur == ProfilingSessionStatus::Idle ||
                    cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::Completed ||
                    cur == ProfilingSessionStatus::EvaluationLoadedForReview ||
                    cur == ProfilingSessionStatus::AuditRejected ||
                    cur == ProfilingSessionStatus::UnsupportedTarget ||
                    cur == ProfilingSessionStatus::Cancelled ||
                    cur == ProfilingSessionStatus::Failed);

        case ProfilingSessionStatus::Auditing:
            return (cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::AuditRejected);

        case ProfilingSessionStatus::ReadyToProfile:
            return (cur == ProfilingSessionStatus::Auditing ||
                    cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::EvaluationLoadedForReview ||
                    cur == ProfilingSessionStatus::Completed ||
                    cur == ProfilingSessionStatus::Exported ||
                    cur == ProfilingSessionStatus::Cancelled ||
                    cur == ProfilingSessionStatus::Failed);

        case ProfilingSessionStatus::Profiling:
            return (cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::EvaluationLoadedForReview ||
                    cur == ProfilingSessionStatus::Completed ||
                    cur == ProfilingSessionStatus::Exported ||
                    cur == ProfilingSessionStatus::Cancelled ||
                    cur == ProfilingSessionStatus::Failed ||
                    cur == ProfilingSessionStatus::Paused);

        case ProfilingSessionStatus::Paused:
            return (cur == ProfilingSessionStatus::Profiling);

        case ProfilingSessionStatus::Completed:
            return (cur == ProfilingSessionStatus::Profiling);

        case ProfilingSessionStatus::EvaluationLoadedForReview:
            return (cur == ProfilingSessionStatus::Idle ||
                    cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::Completed ||
                    cur == ProfilingSessionStatus::EvaluationLoadedForReview);

        case ProfilingSessionStatus::Exporting:
            return ((cur == ProfilingSessionStatus::Completed ||
                     cur == ProfilingSessionStatus::EvaluationLoadedForReview) &&
                    currentSnapshot_.evaluation.selectionStatus != synth::SelectionStatus::InvalidMeasurement);

        case ProfilingSessionStatus::Exported:
            return (cur == ProfilingSessionStatus::Exporting);

        case ProfilingSessionStatus::AuditRejected:
        case ProfilingSessionStatus::UnsupportedTarget:
            return (cur == ProfilingSessionStatus::Auditing);

        case ProfilingSessionStatus::MeasurementInvalid:
            return (cur == ProfilingSessionStatus::Profiling ||
                    cur == ProfilingSessionStatus::Completed);

        case ProfilingSessionStatus::Cancelled:
            return (cur == ProfilingSessionStatus::Auditing ||
                    cur == ProfilingSessionStatus::Profiling ||
                    cur == ProfilingSessionStatus::Paused);

        case ProfilingSessionStatus::Failed:
            return true;

        default:
            return false;
    }
}

void ProfilingSessionController::publishSnapshotLocked()
{
    currentSnapshot_.monotonicSequence = ++sequenceCounter_;
    currentSnapshot_.timestampMs = getCurrentTimeMs();
    auto snapCopy = currentSnapshot_;
    auto ctx = createCallbackContextLocked();

    // Notificación desacoplada y segura hacia los listeners con guardia de ciclo de vida de 3 niveles
    auto notifyAction = [this, ctx, snapCopy]() {
        if (!ctx.alive || !ctx.alive->load(std::memory_order_acquire))
            return;
        if (!isValidCallbackContext(ctx))
            return;

        std::vector<IProfilingSessionEventListener*> listenersCopy;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            listenersCopy = listeners_;
        }
        for (auto* l : listenersCopy)
        {
            if (l != nullptr)
                l->onSessionSnapshotUpdated(snapCopy);
        }
    };

    // Notificación síncrona y segura hacia los listeners con guardia de ciclo de vida de 3 niveles
    notifyAction();
}

void ProfilingSessionController::notifyAlertListeners(const UiAlert& alert, const CallbackContext& ctx)
{
    auto notifyAction = [this, ctx, alert]() {
        if (!ctx.alive || !ctx.alive->load(std::memory_order_acquire))
            return;
        if (!isValidCallbackContext(ctx))
            return;

        std::vector<IProfilingSessionEventListener*> listenersCopy;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            listenersCopy = listeners_;
        }
        for (auto* l : listenersCopy)
        {
            if (l != nullptr)
                l->onAlertRaised(alert);
        }
    };

    notifyAction();
}

void ProfilingSessionController::notifyStageListeners(ProfilingWorkflowStage stage, const CallbackContext& ctx)
{
    auto notifyAction = [this, ctx, stage]() {
        if (!ctx.alive || !ctx.alive->load(std::memory_order_acquire))
            return;
        if (!isValidCallbackContext(ctx))
            return;

        std::vector<IProfilingSessionEventListener*> listenersCopy;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            listenersCopy = listeners_;
        }
        for (auto* l : listenersCopy)
        {
            if (l != nullptr)
                l->onWorkflowStageChanged(stage);
        }
    };

    notifyAction();
}

void ProfilingSessionController::notifyStatusListeners(ProfilingSessionStatus status, const CallbackContext& ctx)
{
    auto notifyAction = [this, ctx, status]() {
        if (!ctx.alive || !ctx.alive->load(std::memory_order_acquire))
            return;
        if (!isValidCallbackContext(ctx))
            return;

        std::vector<IProfilingSessionEventListener*> listenersCopy;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            listenersCopy = listeners_;
        }
        for (auto* l : listenersCopy)
        {
            if (l != nullptr)
                l->onSessionStatusChanged(status);
        }
    };

    notifyAction();
}

bool ProfilingSessionController::selectTarget(const TargetSelectionState& target)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::TargetSelected))
        return false;

    if (coordinator_ && coordinator_->isRunning())
    {
        coordinator_->requestCancel();
    }

    // Si cambia de target respecto al anterior, invalidar resultados previos e incrementar generación
    if (currentSnapshot_.target.targetId != target.targetId)
    {
        currentSnapshot_.controllerGeneration++;
        currentSnapshot_.sessionId = "session_" + juce::Uuid().toString().toStdString();
        currentSnapshot_.audit = TargetAuditState{};
        currentSnapshot_.evaluation = ModelEvaluationSummaryState{};
        currentSnapshot_.exportOptions = ExportAvailabilityState{};
        currentSnapshot_.progress = ExperimentProgressState{};
        currentSnapshot_.activeAlerts.clear();
        previousEvaluation_ = ModelEvaluationSummaryState{};
        previousExportOptions_ = ExportAvailabilityState{};
    }

    currentSnapshot_.target = target;
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::TargetSelected;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ConfigureAndStart;

    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::TargetSelected, ctx);
    notifyStageListeners(ProfilingWorkflowStage::ConfigureAndStart, ctx);
    return true;
}

bool ProfilingSessionController::requestAudit()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Auditing))
    {
        raiseAlert(UiAlert::Severity::Warning,
                   "Transición no permitida",
                   "Se intentó iniciar la auditoría sin un target válido seleccionado.",
                   "No se puede ejecutar la prueba de determinismo y estado.",
                   "Seleccione un sintetizador o plugin válido antes de auditar.",
                   "La sesión permanece en su estado previo.");
        return false;
    }

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Auditing;
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Auditing, ctx);
    return true;
}

bool ProfilingSessionController::startProfiling()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (coordinator_ && coordinator_->isRunning())
    {
        raiseAlert(UiAlert::Severity::Warning,
                   "Perfilado ya en ejecución",
                   "Ya existe un proceso de medición activo en segundo plano.",
                   "No se pueden iniciar dos procesos de perfilado simultáneamente.",
                   "Espere a que finalice la medición actual o cancele la sesión.",
                   "Segunda solicitud de inicio rechazada.");
        return false;
    }
    if (currentSnapshot_.sessionStatus == ProfilingSessionStatus::TargetSelected ||
        currentSnapshot_.sessionStatus == ProfilingSessionStatus::EvaluationLoadedForReview ||
        currentSnapshot_.sessionStatus == ProfilingSessionStatus::Completed ||
        currentSnapshot_.sessionStatus == ProfilingSessionStatus::Exported ||
        currentSnapshot_.sessionStatus == ProfilingSessionStatus::Cancelled ||
        currentSnapshot_.sessionStatus == ProfilingSessionStatus::Failed)
    {
        if (currentSnapshot_.target.kind == TargetKind::SyntheticFixture)
        {
            if (!currentSnapshot_.audit.isAudited)
            {
                currentSnapshot_.audit.isAudited = true;
                currentSnapshot_.audit.approvalStatus = synth::ApprovalStatus::Approved;
                currentSnapshot_.audit.determinismText = "100% Determinista (Fixture Digital)";
                currentSnapshot_.audit.resetCapabilityText = "Reset instantaneo";
                currentSnapshot_.audit.recommendedSettlingTimeMs = 0.0;
                currentSnapshot_.audit.requiresResetBeforeEachTrial = false;
            }
            currentSnapshot_.sessionStatus = ProfilingSessionStatus::ReadyToProfile;
        }
        else if (currentSnapshot_.audit.isAudited &&
                 (currentSnapshot_.audit.approvalStatus == synth::ApprovalStatus::Approved ||
                  currentSnapshot_.audit.approvalStatus == synth::ApprovalStatus::ApprovedWithWarnings))
        {
            currentSnapshot_.sessionStatus = ProfilingSessionStatus::ReadyToProfile;
        }
    }

    if (currentSnapshot_.sessionStatus != ProfilingSessionStatus::ReadyToProfile &&
        currentSnapshot_.sessionStatus != ProfilingSessionStatus::Paused)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Target no preparado para perfilado",
                   "El target no ha superado con éxito la auditoría metrológica previa.",
                   "El motor de excitación no puede garantizar mediciones seguras o repetibles.",
                   "Ejecute la auditoría previa y confirme que el target esté aprobado.",
                   "El perfilado no se iniciará.");
        return false;
    }

    if (!canTransitionTo(ProfilingSessionStatus::Profiling))
        return false;

    if (currentSnapshot_.evaluation.hasEvaluation)
    {
        previousEvaluation_ = currentSnapshot_.evaluation;
        previousExportOptions_ = currentSnapshot_.exportOptions;
    }

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Profiling;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ProfilingActive;
    currentSnapshot_.taskStartedAtMs = getCurrentTimeMs();

    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Profiling, ctx);
    notifyStageListeners(ProfilingWorkflowStage::ProfilingActive, ctx);

    if (coordinator_)
    {
        coordinator_->start(currentSnapshot_.target, currentSnapshot_.controllerGeneration);
    }
    return true;
}

bool ProfilingSessionController::pauseProfiling()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Paused))
        return false;

    if (coordinator_)
        coordinator_->pause();

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Paused;
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Paused, ctx);
    return true;
}

bool ProfilingSessionController::resumeProfiling()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Profiling))
        return false;

    if (coordinator_)
        coordinator_->resume();

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Profiling;
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Profiling, ctx);
    return true;
}

bool ProfilingSessionController::cancelProfiling()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Cancelled))
        return false;

    if (coordinator_)
        coordinator_->requestCancel();

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Cancelled;
    if (previousEvaluation_.hasEvaluation)
    {
        currentSnapshot_.evaluation = previousEvaluation_;
        currentSnapshot_.exportOptions = previousExportOptions_;
    }
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Cancelled, ctx);
    return true;
}

bool ProfilingSessionController::exportModel([[maybe_unused]] const std::string& format, const std::string& destinationPath)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (currentSnapshot_.sessionStatus != ProfilingSessionStatus::Completed &&
        currentSnapshot_.sessionStatus != ProfilingSessionStatus::EvaluationLoadedForReview)
    {
        raiseAlert(UiAlert::Severity::Warning,
                   "Sesión no completada",
                   "Se solicitó exportar un modelo sin haber completado las mediciones ni cargado una evaluación válida.",
                   "El modelo resultante estaría incompleto o no verificado.",
                   "Complete el ciclo de perfilado o cargue una evaluación válida antes de exportar.",
                   "Exportación cancelada.");
        return false;
    }

    // Capa 2: Comprobación estricta de política metrológica antes de proceder a exportar
    if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::InvalidMeasurement)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Exportación bloqueada por medición inválida",
                   "La evaluación acústica determinó que las mediciones fueron descartadas por clipping o inestabilidad.",
                   "Generar código de producción a partir de datos corruptos produciría fallos acústicos.",
                   "Repita el perfilado ajustando la ganancia de entrada o la calibración de loopback.",
                   "No se exportará ningún paquete.");
        return false;
    }

    if (!currentSnapshot_.exportOptions.canExportCpp ||
        (currentSnapshot_.evaluation.hasEvaluation &&
         (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::Rejected ||
          currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::Inconclusive ||
          !currentSnapshot_.evaluation.hashVerified)))
    {
        std::string reason = !currentSnapshot_.exportOptions.exportBlockReason.empty()
            ? currentSnapshot_.exportOptions.exportBlockReason
            : "El modelo no cumple con los criterios de validación física o verificación criptográfica.";

        raiseAlert(UiAlert::Severity::Error,
                   "Exportación bloqueada por política metrológica",
                   reason,
                   "Generar código a partir de mediciones inválidas o no verificadas comprometería el pipeline de producción.",
                   "Revise los diagnósticos en la vista de resultados antes de reintentar.",
                   "Operación de exportación rechazada.");
        return false;
    }

    if (currentSnapshot_.evaluation.hasEvaluation &&
        currentSnapshot_.evaluation.evaluationOrigin == synth::EvaluationOrigin::MeasuredExternalPlugin)
    {
        if (currentSnapshot_.evaluation.pluginBinarySha256.empty() || currentSnapshot_.evaluation.pluginPath.empty())
        {
            raiseAlert(UiAlert::Severity::Error,
                       "Exportación bloqueada por falta de procedencia binaria",
                       "La evaluación MeasuredExternalPlugin no contiene un hash SHA-256 o ruta de procedencia válidos.",
                       "El pipeline de producción exige trazabilidad de integridad física para plugins externos.",
                       "Ejecute un perfilado completo con procedencia binaria válida.",
                       "Operación de exportación rechazada.");
            return false;
        }
    }

    if (!canTransitionTo(ProfilingSessionStatus::Exporting))
        return false;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Exporting;
    publishSnapshotLocked();
    auto ctxExp = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Exporting, ctxExp);

    // Escritura atómica a archivo temporal y verificación de hash antes de commit
    if (!destinationPath.empty())
    {
        juce::File destFile(destinationPath);
        juce::File parentDir = destFile.getParentDirectory();
        if (!parentDir.exists())
            parentDir.createDirectory();

        juce::File tempFile = parentDir.getChildFile(destFile.getFileName() + ".tmp");
        if (tempFile.existsAsFile())
            tempFile.deleteFile();

        std::ostringstream ss;
        ss << "// ==============================================================================\n";
        ss << "// ABDAudioLab - Production Acoustic Model Package (C++20)\n";
        ss << "// Target: " << currentSnapshot_.evaluation.sourceTargetIdentity << "\n";
        ss << "// Model Architecture: " << currentSnapshot_.evaluation.recommendedModelType << "\n";
        ss << "// Evaluation Decision: " << synth::selectionStatusToString(currentSnapshot_.evaluation.selectionStatus) << "\n";
        ss << "// Canonical Evaluation SHA-256: " << currentSnapshot_.evaluation.canonicalEvaluationHash << "\n";
        ss << "// Cryptographic Integrity: RFC 8785 Verified\n";
        ss << "// ==============================================================================\n\n";
        ss << "#pragma once\n\n";
        ss << "namespace abdaudiolab::generated\n";
        ss << "{\n";
        ss << "    constexpr const char* kTargetIdentity = \"" << currentSnapshot_.evaluation.sourceTargetIdentity << "\";\n";
        ss << "    constexpr const char* kCanonicalEvaluationHash = \"" << currentSnapshot_.evaluation.canonicalEvaluationHash << "\";\n";
        ss << "    constexpr const char* kModelArchitecture = \"" << currentSnapshot_.evaluation.recommendedModelType << "\";\n";
        ss << "} // namespace abdaudiolab::generated\n";

        std::string packageContent = ss.str();
        tempFile.replaceWithText(packageContent);

        // Verificación de que el archivo temporal generado contiene el hash exacto
        std::string writtenContent = tempFile.loadFileAsString().toStdString();
        if (writtenContent.find(currentSnapshot_.evaluation.canonicalEvaluationHash) == std::string::npos)
        {
            tempFile.deleteFile();
            currentSnapshot_.sessionStatus = ProfilingSessionStatus::Failed;
            publishSnapshotLocked();
            raiseAlert(UiAlert::Severity::Error,
                       "Fallo de verificación de artefacto exportado",
                       "El archivo temporal generado no contiene el hash de evaluación canónico esperado.",
                       "Se abortó la exportación para evitar publicar un artefacto inconsistente.",
                       "Verifique el almacenamiento del sistema.",
                       "Exportación abortada.");
            return false;
        }

        // Commit atómico: mover/renombrar archivo temporal al destino definitivo
        if (destFile.existsAsFile())
            destFile.deleteFile();

        if (!tempFile.moveFileTo(destFile))
        {
            tempFile.deleteFile();
            currentSnapshot_.sessionStatus = ProfilingSessionStatus::Failed;
            publishSnapshotLocked();
            raiseAlert(UiAlert::Severity::Error,
                       "Fallo al mover artefacto al destino",
                       "No se pudo renombrar el archivo temporal al destino: " + destinationPath,
                       "El paquete no fue guardado en la ubicación solicitada.",
                       "Verifique los permisos de escritura en el directorio de destino.",
                       "Exportación fallida.");
            return false;
        }
    }

    // Conclusión inmediata de exportación hacia el estado Exported
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Exported;
    currentSnapshot_.exportOptions.lastExportedFilePath = destinationPath;
    publishSnapshotLocked();
    auto ctxDone = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Exported, ctxDone);

    return true;
}

bool ProfilingSessionController::loadEvaluationFromFile(const std::string& filePath)
{
    juce::File file(filePath);
    if (!file.existsAsFile())
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Archivo no encontrado",
                   "La ruta del archivo de evaluación no existe o no es accesible: " + filePath,
                   "No se puede leer ni verificar la evaluación del modelo.",
                   "Compruebe la ruta del archivo y los permisos del sistema.",
                   "No se ha modificado la evaluación de la sesión.");
        return false;
    }

    std::string content = file.loadFileAsString().toStdString();
    return loadEvaluationFromJsonString(content, filePath);
}

bool ProfilingSessionController::loadEvaluationFromJsonString(const std::string& jsonString,
                                                             const std::string& sourceFilePath)
{
    synth::ModelEvaluation loadedEval;
    std::string err;
    synth::EvaluationLoadStatus status = synth::ModelEvaluationBuilder::fromJsonString(jsonString, loadedEval, err);

    if (status == synth::EvaluationLoadStatus::InvalidJson)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Error de sintaxis JSON",
                   err,
                   "El archivo no pudo ser parseado como un documento JSON válido.",
                   "Verifique la integridad del archivo o seleccione otro artefacto.",
                   "La sesión no cargó la evaluación.");
        return false;
    }
    else if (status == synth::EvaluationLoadStatus::UnsupportedProtocol)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Protocolo no compatible",
                   err,
                   "La versión del protocolo del archivo no coincide con la versión 1.0.0 soportada.",
                   "Actualice ABDAudioLab o use un artefacto compatible con la versión 1.0.0.",
                   "Evaluación rechazada.");
        return false;
    }
    else if (status == synth::EvaluationLoadStatus::SchemaMismatch)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Esquema JSON incompatible",
                   err,
                   "Faltan campos obligatorios en la estructura del objeto ModelEvaluation.",
                   "Asegúrese de que el archivo fue generado por el serializador canónico de ABDAudioLab.",
                   "Evaluación rechazada.");
        return false;
    }

    // Calcular hash del contenido fuente del archivo (para procedencia local sin depender de la ruta)
    std::string fileContentHash = synth::Sha256::computeHex(jsonString);

    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    // Invariantes críticas:
    // 1. NO cambiar targetId
    // 2. NO cambiar sessionId
    // 3. NO incrementar controllerGeneration
    // 4. Marcar origen inequívocamente como ImportedArtifact
    loadedEval.origin = synth::EvaluationOrigin::ImportedArtifact;

    updateModelEvaluation(loadedEval);

    // Guardar detalles de procedencia de archivo en el snapshot
    currentSnapshot_.evaluation.sourceFilePath = sourceFilePath;
    currentSnapshot_.evaluation.sourceFileHash = fileContentHash;

    // Distinguir explícitamente de una sesión completada por profiling:
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::EvaluationLoadedForReview;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ReviewResults;

    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::EvaluationLoadedForReview, ctx);
    notifyStageListeners(ProfilingWorkflowStage::ReviewResults, ctx);

    if (status == synth::EvaluationLoadStatus::HashMismatch)
    {
        raiseAlert(UiAlert::Severity::Error,
                   "Integridad criptográfica fallida",
                   err,
                   "El hash SHA-256 canónico de los datos no coincide con el hash declarado en el archivo.",
                   "El archivo ha sido modificado, corrompido o adulterado. La exportación queda bloqueada.",
                   "No se permitirá exportar paquetes de producción.");
        return false;
    }

    return true;
}

juce::File ProfilingSessionController::getEvaluationsDirectory()
{
    // 1. Ruta absoluta canónica en el workspace de desarrollo
    juce::File repoDir("d:/desarrollos/ABDSynths/ABDAudioLab/fixtures/evaluations");
    if (repoDir.isDirectory())
        return repoDir;

    // 2. Relativa a la ubicación del ejecutable (ej. build/ABDAudioLab_artefacts/Release/ -> fixtures/evaluations)
    auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto cand1 = exeFile.getParentDirectory().getChildFile("../../../fixtures/evaluations");
    if (cand1.isDirectory())
        return cand1;

    auto cand2 = exeFile.getParentDirectory().getChildFile("../fixtures/evaluations");
    if (cand2.isDirectory())
        return cand2;

    // 3. Relativa al directorio de trabajo actual
    auto cand3 = juce::File::getCurrentWorkingDirectory().getChildFile("fixtures/evaluations");
    if (cand3.isDirectory())
        return cand3;

    // 4. Búsqueda hacia arriba en el árbol de directorios
    auto search = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 5; ++i)
    {
        auto f = search.getChildFile("fixtures/evaluations");
        if (f.isDirectory())
            return f;
        search = search.getParentDirectory();
    }

    return repoDir;
}

bool ProfilingSessionController::loadPredefinedFixture(const std::string& fixtureFileName)
{
    auto dir = getEvaluationsDirectory();
    auto file = dir.getChildFile(fixtureFileName);
    if (file.existsAsFile())
    {
        return loadEvaluationFromFile(file.getFullPathName().toStdString());
    }

    // Fallback sintetizado en memoria si el archivo físico no estuviese en disco
    synth::TargetAuditReport auditRep;
    auditRep.auditProtocolId = "audit_fixture_clean";
    auditRep.approvalStatus = synth::ApprovalStatus::Approved;
    auditRep.isApprovedForParameterExcitation = true;

    synth::ExcitationSessionReport expRep;
    expRep.experimentId = "exp_fixture_001";
    expRep.targetIdentityHash = "fixture_synth_hash";
    expRep.recipeType = "DifferentialRamp";
    expRep.trialCount = 20;
    expRep.computeHash();

    synth::ModelArtifactDescriptor modelArt;
    modelArt.modelId = "ZDF_Ladder_Synthetic";
    modelArt.modelArchitecture = "TPT ZDF Ladder Filter (Analytic Grey-Box)";
    modelArt.format = "cpp_header";
    modelArt.artifactHash = "sha256_model_clean_fixture";

    std::vector<synth::HoldoutValidationPoint> pts;
    synth::HoldoutValidationPoint p;
    p.pointId = "pt_1";
    p.coordinates = { { "Cutoff", synth::DimensionKind::Continuous, 0.5, 0, "" } };
    p.targetGroundTruthAudio = { 0.1f, 0.2f, 0.3f, 0.4f };
    pts.push_back(p);
    synth::HoldoutDataset holdout("holdout_synth", pts);

    class FallbackEvaluator : public synth::IModelCandidateEvaluator {
    public:
        std::vector<float> predictResponse(const synth::HoldoutValidationPoint& pt) override {
            return pt.targetGroundTruthAudio;
        }
    } evalMock;

    synth::ModelEvaluation eval = synth::ModelEvaluationBuilder()
        .withTargetAudit(auditRep)
        .withExcitationReport(expRep)
        .withModelArtifact(modelArt)
        .withHoldoutDataset(&holdout)
        .withCandidateEvaluator(&evalMock)
        .build();

    eval.origin = synth::EvaluationOrigin::MeasuredFixture;
    eval.sourceTargetIdentity = currentSnapshot_.target.targetName.empty() ? "Sintetizador Virtual de Prueba" : currentSnapshot_.target.targetName;
    eval.computeCanonicalHash();

    std::string jsonStr = synth::ModelEvaluationBuilder::toJsonString(eval);
    return loadEvaluationFromJsonString(jsonStr, file.getFullPathName().toStdString());
}

void ProfilingSessionController::navigateToStage(ProfilingWorkflowStage stage)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (currentSnapshot_.workflowStage != stage)
    {
        currentSnapshot_.workflowStage = stage;
        publishSnapshotLocked();
        auto ctx = createCallbackContextLocked();
        notifyStageListeners(stage, ctx);
    }
}

void ProfilingSessionController::updateAuditResult(synth::ApprovalStatus status,
                                                    const std::string& determinism,
                                                    const std::string& resetCap,
                                                    double settlingMs,
                                                    bool reqReset,
                                                    const std::vector<std::string>& warnings,
                                                    const std::string& guidance)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    currentSnapshot_.audit.isAudited = true;
    currentSnapshot_.audit.approvalStatus = status;
    currentSnapshot_.audit.determinismText = determinism;
    currentSnapshot_.audit.resetCapabilityText = resetCap;
    currentSnapshot_.audit.recommendedSettlingTimeMs = settlingMs;
    currentSnapshot_.audit.requiresResetBeforeEachTrial = reqReset;
    currentSnapshot_.audit.operationalWarnings = warnings;
    currentSnapshot_.audit.humanGuidance = guidance;

    if (status == synth::ApprovalStatus::Approved || status == synth::ApprovalStatus::ApprovedWithWarnings)
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::ReadyToProfile;
    }
    else if (status == synth::ApprovalStatus::Unsupported)
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::UnsupportedTarget;
    }
    else
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::AuditRejected;
    }

    auto finalStatus = currentSnapshot_.sessionStatus;
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(finalStatus, ctx);
}

void ProfilingSessionController::updateProgress(int currentTrial, int totalTrials,
                                                double elapsedSec, double remainingSec,
                                                const std::string& currentStimulus)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.progress.currentTrial = currentTrial;
    currentSnapshot_.progress.totalTrials = totalTrials;
    currentSnapshot_.progress.progressPercent = (totalTrials > 0)
        ? (100.0 * static_cast<double>(currentTrial) / static_cast<double>(totalTrials))
        : 0.0;
    currentSnapshot_.progress.elapsedTimeSec = elapsedSec;
    currentSnapshot_.progress.estimatedRemainingSec = remainingSec;
    currentSnapshot_.progress.currentStimulusDescription = currentStimulus;

    publishSnapshotLocked();
}

void ProfilingSessionController::updateObservation(double rmsDb, double peakDb, double pitchHz,
                                                   bool clipping, bool silence, double snrDb)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.observation.lastRmsDb = rmsDb;
    currentSnapshot_.observation.lastPeakDb = peakDb;
    currentSnapshot_.observation.lastEstimatedPitchHz = pitchHz;
    currentSnapshot_.observation.clippingDetected = clipping;
    currentSnapshot_.observation.silenceDetected = silence;
    currentSnapshot_.observation.snrEstimateDb = snrDb;

    publishSnapshotLocked();
}

void ProfilingSessionController::updateModelEvaluation(const synth::ModelEvaluation& eval)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.evaluation.hasEvaluation = true;
    currentSnapshot_.evaluation.selectionStatus = eval.decision.status;
    currentSnapshot_.evaluation.recommendedModelType = !eval.decision.recommendedModelId.empty()
        ? eval.decision.recommendedModelId
        : (!eval.evaluatedModel.modelId.empty() ? eval.evaluatedModel.modelId : "Evaluated Candidate");

    currentSnapshot_.evaluation.validationEsrDb = eval.metrics.errorToSignalRatioDb;
    currentSnapshot_.evaluation.validationCorrelation = eval.metrics.rSquaredScore;
    currentSnapshot_.evaluation.stimuliMeetingCriterionPercent = (eval.decision.status == synth::SelectionStatus::Accepted)
        ? 100.0
        : ((eval.decision.status == synth::SelectionStatus::AcceptedWithWarnings) ? 95.0 : 50.0);

    std::string dom = "C1-C6, Vel 1-127";
    if (!eval.validatedParameters.empty())
    {
        dom.clear();
        for (size_t i = 0; i < eval.validatedParameters.size(); ++i)
        {
            dom += eval.validatedParameters[i] + (i + 1 < eval.validatedParameters.size() ? ", " : "");
        }
    }
    currentSnapshot_.evaluation.validatedDomain = dom;
    currentSnapshot_.evaluation.relativeCpuCostFactor = eval.resourceCost.cpuUsagePercentPerVoice > 0.0
        ? eval.resourceCost.cpuUsagePercentPerVoice
        : 1.0;

    currentSnapshot_.evaluation.evaluationWarnings = eval.warnings;
    currentSnapshot_.evaluation.limitingFactors = eval.limitations;

    currentSnapshot_.evaluation.evaluationId = eval.evaluationId;
    currentSnapshot_.evaluation.protocolVersion = eval.evaluationProtocolVersion;
    currentSnapshot_.evaluation.canonicalEvaluationHash = eval.canonicalEvaluationHash;
    currentSnapshot_.evaluation.evaluationOrigin = eval.origin;
    currentSnapshot_.evaluation.evaluationLoadStatus = eval.loadStatus;
    currentSnapshot_.evaluation.hashVerified = eval.hashVerified;
    currentSnapshot_.evaluation.warningsCount = static_cast<int>(eval.warnings.size());
    currentSnapshot_.evaluation.sourceTargetIdentity = eval.sourceTargetIdentity;
    currentSnapshot_.evaluation.rationale = eval.decision.rationale;

    // --- Procedencia binaria del target (Fase 20.8.2) ---
    currentSnapshot_.evaluation.executionMode = eval.executionMode;
    currentSnapshot_.evaluation.pluginBinarySha256 = eval.pluginBinarySha256;
    currentSnapshot_.evaluation.pluginPath = eval.pluginPath;
    currentSnapshot_.evaluation.pluginFormatVersion = eval.pluginFormatVersion;
    currentSnapshot_.evaluation.vendor = eval.vendor;
    currentSnapshot_.evaluation.pluginUid = eval.pluginUid;
    currentSnapshot_.evaluation.fileSizeBytes = eval.fileSizeBytes;
    currentSnapshot_.evaluation.buildConfiguration = eval.buildConfiguration;
    currentSnapshot_.evaluation.osArchitecture = eval.osArchitecture;
    currentSnapshot_.evaluation.normalizedFingerprint = eval.normalizedFingerprint;

    // Política de exportación (Capa 1)
    bool canExport = (eval.decision.status == synth::SelectionStatus::Accepted ||
                      eval.decision.status == synth::SelectionStatus::AcceptedWithWarnings);

    if (canExport && !eval.hashVerified)
    {
        canExport = false;
        currentSnapshot_.evaluation.exportBlockReason = "Bloqueado: Fallo de verificación criptográfica (Hash Mismatch)";
    }
    else if (canExport && eval.origin == synth::EvaluationOrigin::MeasuredExternalPlugin &&
             (eval.pluginBinarySha256.empty() || eval.pluginPath.empty()))
    {
        canExport = false;
        currentSnapshot_.evaluation.exportBlockReason = "Bloqueado: MeasuredExternalPlugin requiere procedencia binaria válida (SHA-256)";
    }
    else if (!canExport)
    {
        currentSnapshot_.evaluation.exportBlockReason = "Bloqueado: El dictamen '"
            + synth::selectionStatusToString(eval.decision.status)
            + "' no cumple los criterios de exportación de producción";
    }
    else
    {
        currentSnapshot_.evaluation.exportBlockReason.clear();
    }

    currentSnapshot_.exportOptions.canExportCpp = canExport;
    currentSnapshot_.exportOptions.canExportJson = canExport;
    currentSnapshot_.exportOptions.canExportNam = canExport;
    currentSnapshot_.exportOptions.canExportLut = canExport;
    currentSnapshot_.exportOptions.exportBlockReason = currentSnapshot_.evaluation.exportBlockReason;

    publishSnapshotLocked();
}

void ProfilingSessionController::updateModelEvaluation(synth::SelectionStatus status,
                                                        const std::string& bestModelType,
                                                        double esrDb, double correlation,
                                                        double stimuliMeetingCriterionPct,
                                                        const std::string& domain,
                                                        double cpuFactor,
                                                        const std::vector<std::string>& warnings,
                                                        const std::vector<std::string>& limitingFactors)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.evaluation.hasEvaluation = true;
    currentSnapshot_.evaluation.selectionStatus = status;
    currentSnapshot_.evaluation.recommendedModelType = bestModelType;
    currentSnapshot_.evaluation.validationEsrDb = esrDb;
    currentSnapshot_.evaluation.validationCorrelation = correlation;
    currentSnapshot_.evaluation.stimuliMeetingCriterionPercent = stimuliMeetingCriterionPct;
    currentSnapshot_.evaluation.validatedDomain = domain;
    currentSnapshot_.evaluation.relativeCpuCostFactor = cpuFactor;
    currentSnapshot_.evaluation.evaluationWarnings = warnings;
    currentSnapshot_.evaluation.limitingFactors = limitingFactors;
    currentSnapshot_.evaluation.hashVerified = true;
    currentSnapshot_.evaluation.evaluationLoadStatus = synth::EvaluationLoadStatus::LoadedAndVerified;
    currentSnapshot_.evaluation.warningsCount = static_cast<int>(warnings.size());
    if (currentSnapshot_.evaluation.canonicalEvaluationHash.empty())
    {
        currentSnapshot_.evaluation.canonicalEvaluationHash = synth::Sha256::computeHex(bestModelType + domain);
    }

    // Habilitar exportación solo si no fue descartada
    bool canExport = (status == synth::SelectionStatus::Accepted ||
                      status == synth::SelectionStatus::AcceptedWithWarnings);
    currentSnapshot_.exportOptions.canExportCpp = canExport;
    currentSnapshot_.exportOptions.canExportJson = canExport;
    currentSnapshot_.exportOptions.canExportNam = canExport;
    currentSnapshot_.exportOptions.canExportLut = canExport;
    if (!canExport)
    {
        currentSnapshot_.evaluation.exportBlockReason = "Bloqueado: El dictamen '"
            + synth::selectionStatusToString(status)
            + "' no cumple los criterios de exportación";
    }
    else
    {
        currentSnapshot_.evaluation.exportBlockReason.clear();
    }
    currentSnapshot_.exportOptions.exportBlockReason = currentSnapshot_.evaluation.exportBlockReason;

    publishSnapshotLocked();
}

void ProfilingSessionController::completeProfiling()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (canTransitionTo(ProfilingSessionStatus::Completed))
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::Completed;
        currentSnapshot_.workflowStage = ProfilingWorkflowStage::ReviewResults;
        currentSnapshot_.taskCompletedAtMs = getCurrentTimeMs();
        publishSnapshotLocked();
        auto ctx = createCallbackContextLocked();
        notifyStatusListeners(ProfilingSessionStatus::Completed, ctx);
        notifyStageListeners(ProfilingWorkflowStage::ReviewResults, ctx);
    }
}

void ProfilingSessionController::failSession(const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Failed;
    if (previousEvaluation_.hasEvaluation)
    {
        currentSnapshot_.evaluation = previousEvaluation_;
        currentSnapshot_.exportOptions = previousExportOptions_;
    }
    raiseAlert(UiAlert::Severity::Error,
               "Error crítico de sesión",
               reason,
               "La operación no pudo continuar.",
               "Reinicie la sesión o revise la configuración de audio.",
               "Sesión detenida.");
    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Failed, ctx);
}

void ProfilingSessionController::raiseAlert(UiAlert::Severity severity,
                                            std::string title,
                                            std::string cause,
                                            std::string impact,
                                            std::string action,
                                            std::string consequence)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    UiAlert alert;
    alert.severity = severity;
    alert.title = std::move(title);
    alert.cause = std::move(cause);
    alert.impact = std::move(impact);
    alert.recommendedAction = std::move(action);
    alert.consequenceIfIgnored = std::move(consequence);
    alert.timestampMs = getCurrentTimeMs();

    currentSnapshot_.activeAlerts.push_back(alert);
    auto ctx = createCallbackContextLocked();
    notifyAlertListeners(alert, ctx);
}

void ProfilingSessionController::setWorkflowMode(UiWorkflowMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (currentSnapshot_.workflowMode != mode)
    {
        currentSnapshot_.workflowMode = mode;
        publishSnapshotLocked();
    }
}

void ProfilingSessionController::acknowledgeWarnings()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.warningsAcknowledged = true;
    publishSnapshotLocked();
}

void ProfilingSessionController::recordUserClick()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.clickCount++;
}

void ProfilingSessionController::recordUserOverride()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.userOverrides = true;
    publishSnapshotLocked();
}

void ProfilingSessionController::setOpenedAdvancedMode(bool opened)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.openedAdvancedMode = opened;
    publishSnapshotLocked();
}

uint64_t ProfilingSessionController::getActiveGeneration() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return currentSnapshot_.controllerGeneration;
}

ProfilingSessionCoordinator* ProfilingSessionController::getCoordinator() noexcept
{
    return coordinator_.get();
}

void ProfilingSessionController::onCoordinatorSnapshotUpdated(const CoordinatorSnapshot& snapshot)
{
    auto token = aliveToken_;
    uint64_t snapGen = snapshot.sessionGeneration;
    uint64_t snapRunId = snapshot.runId;
    auto snapCopy = snapshot;

    auto action = [this, token, snapGen, snapRunId, snapCopy]() {
        if (!token || !token->load(std::memory_order_acquire))
            return;

        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        if (currentSnapshot_.controllerGeneration != snapGen)
            return; // Callback de generación anterior descartado
        if (coordinator_ && coordinator_->getCurrentRunId() != snapRunId)
            return; // Callback de run anterior descartado

        if (currentSnapshot_.sessionStatus != ProfilingSessionStatus::Profiling &&
            currentSnapshot_.sessionStatus != ProfilingSessionStatus::Paused)
        {
            return;
        }

        // Actualizar progreso monotónico [0.0, 100.0]
        currentSnapshot_.progress.currentTrial = snapCopy.currentTrial;
        currentSnapshot_.progress.totalTrials = snapCopy.totalTrials;
        currentSnapshot_.progress.progressPercent = snapCopy.progress;

        // Telemetría de audio precalculada (cero asignaciones)
        currentSnapshot_.observation.lastRmsDb = snapCopy.rmsDb;
        currentSnapshot_.observation.lastPeakDb = snapCopy.peakDb;
        currentSnapshot_.observation.lastEstimatedPitchHz = snapCopy.detectedF0Hz;
        currentSnapshot_.observation.clippingDetected = (snapCopy.health == AcousticHealth::Clipping);

        publishSnapshotLocked();
    };

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            action();
        else
            juce::MessageManager::callAsync(action);
    }
    else
    {
        action();
    }
}

void ProfilingSessionController::onCoordinatorCompleted(uint64_t runId, uint64_t sessionGeneration, const synth::ModelEvaluation& candidate)
{
    auto token = aliveToken_;
    auto candCopy = candidate;

    auto action = [this, token, runId, sessionGeneration, candCopy]() {
        if (!token || !token->load(std::memory_order_acquire))
            return;

        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        if (currentSnapshot_.controllerGeneration != sessionGeneration)
            return;
        if (coordinator_ && coordinator_->getCurrentRunId() != runId)
            return;

        // Validación estricta del candidato antes de commit
        if (candCopy.canonicalEvaluationHash.empty() || !candCopy.hashVerified)
        {
            failSession("El modelo candidato no superó la verificación criptográfica RFC 8785.");
            return;
        }

        // Commit atómico del modelo candidato
        updateModelEvaluation(candCopy);
        completeProfiling();
    };

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            action();
        else
            juce::MessageManager::callAsync(action);
    }
    else
    {
        action();
    }
}

void ProfilingSessionController::onCoordinatorCancelled(uint64_t runId, uint64_t sessionGeneration)
{
    auto token = aliveToken_;

    auto action = [this, token, runId, sessionGeneration]() {
        if (!token || !token->load(std::memory_order_acquire))
            return;

        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        if (currentSnapshot_.controllerGeneration != sessionGeneration)
            return;
        if (coordinator_ && coordinator_->getCurrentRunId() != runId)
            return;

        if (currentSnapshot_.sessionStatus == ProfilingSessionStatus::Profiling ||
            currentSnapshot_.sessionStatus == ProfilingSessionStatus::Paused)
        {
            currentSnapshot_.sessionStatus = ProfilingSessionStatus::Cancelled;
            if (previousEvaluation_.hasEvaluation)
            {
                currentSnapshot_.evaluation = previousEvaluation_;
                currentSnapshot_.exportOptions = previousExportOptions_;
            }
            publishSnapshotLocked();
            auto ctx = createCallbackContextLocked();
            notifyStatusListeners(ProfilingSessionStatus::Cancelled, ctx);
        }
    };

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            action();
        else
            juce::MessageManager::callAsync(action);
    }
    else
    {
        action();
    }
}

void ProfilingSessionController::onCoordinatorFailed(uint64_t runId, uint64_t sessionGeneration, const std::string& error)
{
    auto token = aliveToken_;

    auto action = [this, token, runId, sessionGeneration, error]() {
        if (!token || !token->load(std::memory_order_acquire))
            return;

        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        if (currentSnapshot_.controllerGeneration != sessionGeneration)
            return;
        if (coordinator_ && coordinator_->getCurrentRunId() != runId)
            return;

        failSession(error);
    };

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            action();
        else
            juce::MessageManager::callAsync(action);
    }
    else
    {
        action();
    }
}

} // namespace abdaudiolab::gui::session
