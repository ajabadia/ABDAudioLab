#include "ProfilingSessionController.h"
#include <algorithm>
#include <juce_events/juce_events.h>

namespace abdaudiolab::gui::session
{

ProfilingSessionController::ProfilingSessionController()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.sessionId = "session_" + std::to_string(getCurrentTimeMs()) + "_0";
    currentSnapshot_.monotonicSequence = ++sequenceCounter_;
    currentSnapshot_.timestampMs = getCurrentTimeMs();
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Idle;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::TargetSelection;
}

ProfilingSessionController::~ProfilingSessionController()
{
    std::lock_guard<std::mutex> lock(listenersMutex_);
    listeners_.clear();
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

ProfilingSessionSnapshot ProfilingSessionController::getCurrentSnapshot() const
{
    std::lock_guard<std::mutex> lock(stateMutex_);
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
                    cur == ProfilingSessionStatus::AuditRejected ||
                    cur == ProfilingSessionStatus::UnsupportedTarget ||
                    cur == ProfilingSessionStatus::Cancelled ||
                    cur == ProfilingSessionStatus::Failed);

        case ProfilingSessionStatus::Auditing:
            return (cur == ProfilingSessionStatus::TargetSelected ||
                    cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::AuditRejected);

        case ProfilingSessionStatus::ReadyToProfile:
            return (cur == ProfilingSessionStatus::Auditing);

        case ProfilingSessionStatus::Profiling:
            return (cur == ProfilingSessionStatus::ReadyToProfile ||
                    cur == ProfilingSessionStatus::Paused);

        case ProfilingSessionStatus::Paused:
            return (cur == ProfilingSessionStatus::Profiling);

        case ProfilingSessionStatus::Completed:
            return (cur == ProfilingSessionStatus::Profiling);

        case ProfilingSessionStatus::Exporting:
            return (cur == ProfilingSessionStatus::Completed &&
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

    // Notificación desacoplada y segura hacia los listeners
    auto notifyAction = [this, snapCopy]() {
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

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
        {
            notifyAction();
        }
        else
        {
            juce::MessageManager::callAsync(notifyAction);
        }
    }
    else
    {
        notifyAction();
    }
}

void ProfilingSessionController::notifyAlertListeners(const UiAlert& alert)
{
    auto notifyAction = [this, alert]() {
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

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            notifyAction();
        else
            juce::MessageManager::callAsync(notifyAction);
    }
    else
    {
        notifyAction();
    }
}

void ProfilingSessionController::notifyStageListeners(ProfilingWorkflowStage stage)
{
    auto notifyAction = [this, stage]() {
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

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            notifyAction();
        else
            juce::MessageManager::callAsync(notifyAction);
    }
    else
    {
        notifyAction();
    }
}

void ProfilingSessionController::notifyStatusListeners(ProfilingSessionStatus status)
{
    auto notifyAction = [this, status]() {
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

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (mm->isThisTheMessageThread())
            notifyAction();
        else
            juce::MessageManager::callAsync(notifyAction);
    }
    else
    {
        notifyAction();
    }
}

bool ProfilingSessionController::selectTarget(const TargetSelectionState& target)
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::TargetSelected))
        return false;

    // Si cambia de target respecto al anterior, invalidar resultados previos e incrementar generación
    if (currentSnapshot_.target.targetId != target.targetId)
    {
        currentSnapshot_.controllerGeneration++;
        currentSnapshot_.sessionId = "session_" + std::to_string(getCurrentTimeMs()) + "_" + std::to_string(currentSnapshot_.controllerGeneration);
        currentSnapshot_.audit = TargetAuditState{};
        currentSnapshot_.evaluation = ModelEvaluationSummaryState{};
        currentSnapshot_.exportOptions = ExportAvailabilityState{};
        currentSnapshot_.progress = ExperimentProgressState{};
        currentSnapshot_.activeAlerts.clear();
    }

    currentSnapshot_.target = target;
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::TargetSelected;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ConfigureAndStart;

    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::TargetSelected);
    notifyStageListeners(ProfilingWorkflowStage::ConfigureAndStart);
    return true;
}

bool ProfilingSessionController::requestAudit()
{
    std::lock_guard<std::mutex> lock(stateMutex_);

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
    notifyStatusListeners(ProfilingSessionStatus::Auditing);
    return true;
}

bool ProfilingSessionController::startProfiling()
{
    std::lock_guard<std::mutex> lock(stateMutex_);

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

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Profiling;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ProfilingActive;

    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Profiling);
    notifyStageListeners(ProfilingWorkflowStage::ProfilingActive);
    return true;
}

bool ProfilingSessionController::pauseProfiling()
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Paused))
        return false;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Paused;
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Paused);
    return true;
}

bool ProfilingSessionController::resumeProfiling()
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Profiling))
        return false;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Profiling;
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Profiling);
    return true;
}

bool ProfilingSessionController::cancelProfiling()
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!canTransitionTo(ProfilingSessionStatus::Cancelled))
        return false;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Cancelled;
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Cancelled);
    return true;
}

bool ProfilingSessionController::exportModel([[maybe_unused]] const std::string& format, const std::string& destinationPath)
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (currentSnapshot_.sessionStatus != ProfilingSessionStatus::Completed)
    {
        raiseAlert(UiAlert::Severity::Warning,
                   "Sesión no completada",
                   "Se solicitó exportar un modelo sin haber completado las mediciones.",
                   "El modelo resultante estaría incompleto o no verificado.",
                   "Complete el ciclo de perfilado antes de exportar.",
                   "Exportación cancelada.");
        return false;
    }

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

    if (!canTransitionTo(ProfilingSessionStatus::Exporting))
        return false;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Exporting;
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Exporting);

    // Conclusión inmediata de exportación hacia el estado Exported
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Exported;
    currentSnapshot_.exportOptions.lastExportedFilePath = destinationPath;
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Exported);

    return true;
}

void ProfilingSessionController::navigateToStage(ProfilingWorkflowStage stage)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (currentSnapshot_.workflowStage != stage)
    {
        currentSnapshot_.workflowStage = stage;
        publishSnapshotLocked();
        notifyStageListeners(stage);
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
    std::lock_guard<std::mutex> lock(stateMutex_);

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
        notifyStatusListeners(ProfilingSessionStatus::ReadyToProfile);
    }
    else if (status == synth::ApprovalStatus::Unsupported)
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::UnsupportedTarget;
        notifyStatusListeners(ProfilingSessionStatus::UnsupportedTarget);
    }
    else
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::AuditRejected;
        notifyStatusListeners(ProfilingSessionStatus::AuditRejected);
    }

    publishSnapshotLocked();
}

void ProfilingSessionController::updateProgress(int currentTrial, int totalTrials,
                                                double elapsedSec, double remainingSec,
                                                const std::string& currentStimulus)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
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
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.observation.lastRmsDb = rmsDb;
    currentSnapshot_.observation.lastPeakDb = peakDb;
    currentSnapshot_.observation.lastEstimatedPitchHz = pitchHz;
    currentSnapshot_.observation.clippingDetected = clipping;
    currentSnapshot_.observation.silenceDetected = silence;
    currentSnapshot_.observation.snrEstimateDb = snrDb;

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
    std::lock_guard<std::mutex> lock(stateMutex_);
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

    // Habilitar exportación solo si no fue descartada
    bool canExport = (status == synth::SelectionStatus::Accepted ||
                      status == synth::SelectionStatus::AcceptedWithWarnings);
    currentSnapshot_.exportOptions.canExportCpp = canExport;
    currentSnapshot_.exportOptions.canExportJson = canExport;
    currentSnapshot_.exportOptions.canExportNam = canExport;
    currentSnapshot_.exportOptions.canExportLut = canExport;

    publishSnapshotLocked();
}

void ProfilingSessionController::completeProfiling()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (canTransitionTo(ProfilingSessionStatus::Completed))
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::Completed;
        currentSnapshot_.workflowStage = ProfilingWorkflowStage::ReviewResults;
        publishSnapshotLocked();
        notifyStatusListeners(ProfilingSessionStatus::Completed);
        notifyStageListeners(ProfilingWorkflowStage::ReviewResults);
    }
}

void ProfilingSessionController::failSession(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Failed;
    raiseAlert(UiAlert::Severity::Error,
               "Error crítico de sesión",
               reason,
               "La operación no pudo continuar.",
               "Reinicie la sesión o revise la configuración de audio.",
               "Sesión detenida.");
    publishSnapshotLocked();
    notifyStatusListeners(ProfilingSessionStatus::Failed);
}

void ProfilingSessionController::raiseAlert(UiAlert::Severity severity,
                                            std::string title,
                                            std::string cause,
                                            std::string impact,
                                            std::string action,
                                            std::string consequence)
{
    UiAlert alert;
    alert.severity = severity;
    alert.title = std::move(title);
    alert.cause = std::move(cause);
    alert.impact = std::move(impact);
    alert.recommendedAction = std::move(action);
    alert.consequenceIfIgnored = std::move(consequence);
    alert.timestampMs = getCurrentTimeMs();

    currentSnapshot_.activeAlerts.push_back(alert);
    notifyAlertListeners(alert);
}

void ProfilingSessionController::setWorkflowMode(UiWorkflowMode mode)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (currentSnapshot_.workflowMode != mode)
    {
        currentSnapshot_.workflowMode = mode;
        publishSnapshotLocked();
    }
}

void ProfilingSessionController::acknowledgeWarnings()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.warningsAcknowledged = true;
    publishSnapshotLocked();
}

void ProfilingSessionController::recordUserClick()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.clickCount++;
}

void ProfilingSessionController::setOpenedAdvancedMode(bool opened)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentSnapshot_.openedAdvancedMode = opened;
    publishSnapshotLocked();
}

uint64_t ProfilingSessionController::getActiveGeneration() const noexcept
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentSnapshot_.controllerGeneration;
}

} // namespace abdaudiolab::gui::session
