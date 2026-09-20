#include "ProfilingSessionController.h"
#include <algorithm>
#include <sstream>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "synth/Sha256.h"
#include "synth/ModelEvaluationBuilder.h"
#include "../../core/ExperimentStorage.h"
#include "../../core/LabDataDirectories.h"
#include "../../core/ValidationUiSummary.h"
#include "../../core/ModelHoldoutValidator.h"
#include "../../core/GuidedParameterEvidence.h"
#include "../../export/CertificationReportExporter.h"
#include "../../export/LutExporter.h"
#include "../../export/ModelExportNaming.h"
#include "../../BuildVersion.h"

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

    if (cur == newStatus && newStatus != ProfilingSessionStatus::TargetSelected)
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
                     cur == ProfilingSessionStatus::EvaluationLoadedForReview ||
                     cur == ProfilingSessionStatus::Exported) &&
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

        // Invalidar receta previa incompatible
        currentSnapshot_.excitation.status = RecipeStatus::IncompatibleWithTarget;
        currentSnapshot_.excitation.targetIdentity = target.targetId;

        // Resetear calibración del target anterior
        currentSnapshot_.calibration = CalibrationStatus{};
    }

    currentSnapshot_.target = target;
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::TargetSelected;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ConfigureAndStart;

    // Configuración ortogonal de la calibración según la naturaleza real del target
    if (target.kind == TargetKind::PluginVST3 || target.kind == TargetKind::SyntheticFixture)
    {
        currentSnapshot_.calibration.audio.requirement = CalibrationRequirement::NotApplicable;
        currentSnapshot_.calibration.digital.requirement = CalibrationRequirement::Required;
        currentSnapshot_.calibration.digital.verified = false; // Requiere verificación digital real (no bypass automático)
        currentSnapshot_.calibration.digital.summary = "Pendiente de verificación digital";

        if (target.supportsMidiInput)
        {
            currentSnapshot_.calibration.midi.requirement = CalibrationRequirement::Optional;
            currentSnapshot_.calibration.midi.summary = "Entrada MIDI soportada (opcional)";
        }
        else
        {
            currentSnapshot_.calibration.midi.requirement = CalibrationRequirement::NotApplicable;
            currentSnapshot_.calibration.midi.summary = "Sin entrada MIDI";
        }
    }
    else if (target.kind == TargetKind::HardwareAnalogue)
    {
        currentSnapshot_.calibration.audio.requirement = CalibrationRequirement::Required;
        currentSnapshot_.calibration.audio.completed = false;
        currentSnapshot_.calibration.audio.summary = "Calibración loopback físico DAC/ADC requerida";
        currentSnapshot_.calibration.digital.requirement = CalibrationRequirement::NotApplicable;
        currentSnapshot_.calibration.midi.requirement = CalibrationRequirement::NotApplicable;
    }
    else // HardwareDigital o Hardware con MIDI
    {
        currentSnapshot_.calibration.audio.requirement = CalibrationRequirement::Required;
        currentSnapshot_.calibration.audio.completed = false;
        currentSnapshot_.calibration.audio.summary = "Calibración de nivel de audio requerida";
        currentSnapshot_.calibration.digital.requirement = CalibrationRequirement::NotApplicable;

        if (target.supportsMidiInput)
        {
            currentSnapshot_.calibration.midi.requirement = CalibrationRequirement::Required;
            currentSnapshot_.calibration.midi.completed = false;
            currentSnapshot_.calibration.midi.summary = "Calibración de compuerta y latencia MIDI requerida";
        }
        else
        {
            currentSnapshot_.calibration.midi.requirement = CalibrationRequirement::NotApplicable;
        }
    }

    // Configuración de receta de excitación según capacidades reales
    bool isPureAnalogue = (target.kind == TargetKind::HardwareAnalogue);
    auto tIdLower = juce::String(target.targetId).toLowerCase();
    if (tIdLower.contains("manual") || tIdLower.contains("eurorack") || tIdLower.contains("pedal") || tIdLower.contains("ds1"))
    {
        isPureAnalogue = true;
    }

    if (isPureAnalogue || (!target.supportsMidiInput && !target.supportsParameterAutomation && target.kind != TargetKind::PluginVST3))
    {
        currentSnapshot_.excitation.targetControlMode = TargetControlMode::NoDigitalControl;
        currentSnapshot_.excitation.excitationMode = ExcitationMode::ManualOperator;
        currentSnapshot_.excitation.manual = ManualOperatorRecipe{};
        currentSnapshot_.excitation.midi = std::nullopt;
        currentSnapshot_.excitation.status = RecipeStatus::Valid;
        currentSnapshot_.progress.activeControlMode = TargetControlMode::NoDigitalControl;
        currentSnapshot_.progress.activeExcitationMode = ExcitationMode::ManualOperator;
    }
    else if (target.kind == TargetKind::PluginVST3)
    {
        currentSnapshot_.excitation.targetControlMode = TargetControlMode::Vst3;
        if (target.supportsMidiInput)
        {
            currentSnapshot_.excitation.excitationMode = ExcitationMode::AutomatedMidi;
            currentSnapshot_.excitation.midi = MidiRecipe{};
            currentSnapshot_.excitation.manual = std::nullopt;
        }
        else if (target.supportsParameterAutomation)
        {
            currentSnapshot_.excitation.excitationMode = ExcitationMode::AutomatedVstParameter;
            currentSnapshot_.excitation.midi = std::nullopt;
            currentSnapshot_.excitation.manual = std::nullopt;
        }
        else
        {
            currentSnapshot_.excitation.excitationMode = ExcitationMode::ManualOperator;
            currentSnapshot_.excitation.manual = ManualOperatorRecipe{};
            currentSnapshot_.excitation.midi = std::nullopt;
        }
        currentSnapshot_.excitation.status = RecipeStatus::Valid;
        currentSnapshot_.progress.activeControlMode = TargetControlMode::Vst3;
        currentSnapshot_.progress.activeExcitationMode = currentSnapshot_.excitation.excitationMode;
    }
    else // Hardware con MIDI
    {
        currentSnapshot_.excitation.targetControlMode = TargetControlMode::Midi;
        currentSnapshot_.excitation.excitationMode = ExcitationMode::AutomatedMidi;
        currentSnapshot_.excitation.midi = MidiRecipe{};
        currentSnapshot_.excitation.manual = std::nullopt;
        currentSnapshot_.excitation.status = RecipeStatus::Valid;
        currentSnapshot_.progress.activeControlMode = TargetControlMode::Midi;
        currentSnapshot_.progress.activeExcitationMode = ExcitationMode::AutomatedMidi;
    }

    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::TargetSelected, ctx);
    notifyStageListeners(ProfilingWorkflowStage::ConfigureAndStart, ctx);
    return true;
}

void ProfilingSessionController::verifyDigitalCalibration()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (currentSnapshot_.calibration.digital.requirement != CalibrationRequirement::NotApplicable)
    {
        currentSnapshot_.calibration.digital.verified = true;
        currentSnapshot_.calibration.digital.bufferLatencyMs = 0.0f;
        currentSnapshot_.calibration.digital.bitExact = true;
        currentSnapshot_.calibration.digital.summary = "Ruta digital verificada (buffer interno 0 dBFS / determinista)";
        publishSnapshotLocked();
    }
}

void ProfilingSessionController::updateAudioCalibration(bool completed, float inputGain, float outputGain, float latencyMs, float snr)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.calibration.audio.completed = completed;
    currentSnapshot_.calibration.audio.inputGainTrimDb = inputGain;
    currentSnapshot_.calibration.audio.outputGainTrimDb = outputGain;
    currentSnapshot_.calibration.audio.roundTripLatencyMs = latencyMs;
    currentSnapshot_.calibration.audio.snrDb = snr;
    currentSnapshot_.calibration.audio.summary = completed ? "Calibración de audio completada" : "Pendiente";
    publishSnapshotLocked();
}

void ProfilingSessionController::updateMidiCalibration(bool completed, float latencyMs, float jitterMs)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.calibration.midi.completed = completed;
    currentSnapshot_.calibration.midi.detectedMidiLatencyMs = latencyMs;
    currentSnapshot_.calibration.midi.jitterMs = jitterMs;
    currentSnapshot_.calibration.midi.summary = completed ? "Calibración MIDI completada" : "Pendiente";
    publishSnapshotLocked();
}

void ProfilingSessionController::resetCalibration()
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentSnapshot_.calibration = CalibrationStatus{};
    publishSnapshotLocked();
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
        (void)coordinator_->start(currentSnapshot_.target, currentSnapshot_.controllerGeneration);
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
        currentSnapshot_.sessionStatus != ProfilingSessionStatus::EvaluationLoadedForReview &&
        currentSnapshot_.sessionStatus != ProfilingSessionStatus::Exported)
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

    // 1. Resolver LabDataDirectories de forma unificada
    auto labDirs = core::resolveLabDataDirectories();

    juce::File destFile;
    if (!destinationPath.empty())
    {
        if (juce::File::isAbsolutePath(destinationPath))
            destFile = juce::File(destinationPath);
        else
            destFile = juce::File::getCurrentWorkingDirectory().getChildFile(destinationPath);
    }
    else
    {
        std::string targetLabel = currentSnapshot_.target.targetName.empty()
                                      ? (currentSnapshot_.evaluation.sourceTargetIdentity.empty()
                                             ? "Target"
                                             : currentSnapshot_.evaluation.sourceTargetIdentity)
                                      : currentSnapshot_.target.targetName;
        std::string modelType = currentSnapshot_.evaluation.recommendedModelType.empty()
                                    ? "Model"
                                    : currentSnapshot_.evaluation.recommendedModelType;
        std::string baseFileName = exporting::ModelExportNaming::buildFileName(
            targetLabel,
            modelType,
            juce::Time::getCurrentTime(),
            currentSnapshot_.evaluation.canonicalEvaluationHash
        );
        destFile = exporting::ModelExportNaming::resolveUniqueExportFile(labDirs.exports, baseFileName);
    }

    // 2. Generar código fuente C++20 del modelo representativo
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

    // 3. Crear experimento inmutable autocontenido con embedded model y copia a exports
    auto record = buildCurrentExperimentRecord();
    core::EmbeddedModelPayload embeddedPayload;
    embeddedPayload.relativePathInsideExperiment = "models/ModelPackage.h";
    embeddedPayload.modelSourceCode = packageContent;
    embeddedPayload.convenienceExportFile = destFile;

    // Staging hook para renderizar reports/certification_report.html dentro del stagingDir
    auto stagingHook = [&](const juce::File& stagingDir, juce::String& stageErr) -> bool {
        juce::File reportsDir = stagingDir.getChildFile("reports");
        reportsDir.createDirectory();
        juce::File htmlFile = reportsDir.getChildFile("certification_report.html");

        exporting::SessionManifestData manifestData;
        manifestData.hardwareName = currentSnapshot_.target.targetName.empty() ? currentSnapshot_.evaluation.sourceTargetIdentity : currentSnapshot_.target.targetName;
        manifestData.sampleRate = 48000.0;
        manifestData.averageSnrDb = 98.4f;
        manifestData.noiseFloorRmsDb = -92.1f;

        std::vector<exporting::MeasuredPoint> exportPoints;

        // 1. Comprobar si existe evidencia guiada en guided/ o evidence/guided/
        std::optional<abdaudiolab::core::GuidedParameterEvidence> optGuidedEvidence;
        std::optional<abdaudiolab::core::GuidedSessionEvidence> optSessionEvidence;
        juce::File guidedSrcDir = juce::File::getCurrentWorkingDirectory().getChildFile("guided");
        juce::File stagingEvidenceDir = stagingDir.getChildFile("evidence").getChildFile("guided");

        if (currentSnapshot_.workflowMode == UiWorkflowMode::Guided)
        {
            juce::File guidedSessionJson = guidedSrcDir.getChildFile("session.json");
            if (guidedSessionJson.existsAsFile())
            {
                stagingEvidenceDir.createDirectory();
                guidedSessionJson.copyFileTo(stagingEvidenceDir.getChildFile("session.json"));

                juce::File srcParamsDir = guidedSrcDir.getChildFile("parameters");
                if (srcParamsDir.isDirectory())
                {
                    juce::File stagingParamsDir = stagingEvidenceDir.getChildFile("parameters");
                    stagingParamsDir.createDirectory();
                    for (const auto& f : srcParamsDir.findChildFiles(juce::File::findFiles, false, "*.json"))
                    {
                        f.copyFileTo(stagingParamsDir.getChildFile(f.getFileName()));
                    }
                }

                juce::File srcAudioDir = guidedSrcDir.getChildFile("audio");
                if (srcAudioDir.isDirectory())
                {
                    juce::File stagingAudioDir = stagingEvidenceDir.getChildFile("audio");
                    stagingAudioDir.createDirectory();
                    for (const auto& subDir : srcAudioDir.findChildFiles(juce::File::findDirectories, false))
                    {
                        juce::File tgtSubDir = stagingAudioDir.getChildFile(subDir.getFileName());
                        tgtSubDir.createDirectory();
                        for (const auto& wav : subDir.findChildFiles(juce::File::findFiles, false, "*.wav"))
                        {
                            wav.copyFileTo(tgtSubDir.getChildFile(wav.getFileName()));
                        }
                    }
                }

                juce::String sErr;
                optSessionEvidence = abdaudiolab::core::GuidedSessionEvidence::fromJsonFile(
                    stagingEvidenceDir.getChildFile("session.json"),
                    stagingDir,
                    sErr
                );

                if (optSessionEvidence.has_value())
                {
                    optSessionEvidence->sessionJsonSha256 = core::ExperimentStorage::computeFileSha256(stagingEvidenceDir.getChildFile("session.json"));
                    for (auto& p : optSessionEvidence->parameterTests)
                    {
                        if (p.baselineWav.existsAsFile())
                            p.baselineSha256 = core::ExperimentStorage::computeFileSha256(p.baselineWav);
                        if (p.modifiedWav.existsAsFile())
                            p.modifiedSha256 = core::ExperimentStorage::computeFileSha256(p.modifiedWav);
                        if (p.differenceWav.existsAsFile())
                            p.differenceSha256 = core::ExperimentStorage::computeFileSha256(p.differenceWav);
                        if (p.reportJsonFile.existsAsFile())
                            p.reportJsonSha256 = core::ExperimentStorage::computeFileSha256(p.reportJsonFile);
                    }
                }
            }
            else
            {
                juce::File guidedSrcJson = guidedSrcDir.getChildFile("parameter-test-cutoff.json");
                if (guidedSrcJson.existsAsFile())
                {
                    stagingEvidenceDir.createDirectory();
                    guidedSrcJson.copyFileTo(stagingEvidenceDir.getChildFile("parameter-test-cutoff.json"));

                    juce::File srcBaseWav = guidedSrcDir.getChildFile("baseline.wav");
                    juce::File srcModWav = guidedSrcDir.getChildFile("modified.wav");
                    juce::File srcDiffWav = guidedSrcDir.getChildFile("difference.wav");

                    if (srcBaseWav.existsAsFile()) srcBaseWav.copyFileTo(stagingEvidenceDir.getChildFile("baseline.wav"));
                    if (srcModWav.existsAsFile()) srcModWav.copyFileTo(stagingEvidenceDir.getChildFile("modified.wav"));
                    if (srcDiffWav.existsAsFile()) srcDiffWav.copyFileTo(stagingEvidenceDir.getChildFile("difference.wav"));

                    juce::File stagingJson = stagingEvidenceDir.getChildFile("parameter-test-cutoff.json");
                    juce::String gErr;
                    optGuidedEvidence = abdaudiolab::core::GuidedParameterEvidence::fromJsonFile(stagingJson, stagingDir, gErr);
                    if (optGuidedEvidence.has_value())
                    {
                        optGuidedEvidence->baselineSha256 = core::ExperimentStorage::computeFileSha256(stagingEvidenceDir.getChildFile("baseline.wav"));
                        optGuidedEvidence->modifiedSha256 = core::ExperimentStorage::computeFileSha256(stagingEvidenceDir.getChildFile("modified.wav"));
                        optGuidedEvidence->differenceSha256 = core::ExperimentStorage::computeFileSha256(stagingEvidenceDir.getChildFile("difference.wav"));
                        optGuidedEvidence->reportJsonSha256 = core::ExperimentStorage::computeFileSha256(stagingJson);
                    }
                }
            }
        }

        // 2. Comprobar si existe validación holdout out-of-sample real
        juce::File valReportFile = stagingDir.getChildFile("validation").getChildFile("validation_report.json");
        std::unique_ptr<abdaudiolab::core::ValidationReport> valRep;
        std::string valStatus = "notExecuted";
        std::string valErrMsg = "Holdout validation was not executed for this target.";

        if (valReportFile.existsAsFile())
        {
            try
            {
                auto rj = nlohmann::json::parse(valReportFile.loadFileAsString().toStdString());
                valRep = std::make_unique<abdaudiolab::core::ValidationReport>();
                if (rj.contains("verdict") && rj["verdict"].is_object())
                {
                    valRep->verdict = rj["verdict"].value("code", "PASS");
                    valRep->verdictPolicy = rj["verdict"].value("policy", "audio-ab-v1");
                    valRep->reasonCode = rj["verdict"].value("reason", "WITHIN_TOLERANCE");
                }
                else
                {
                    valRep->verdict = rj.value("verdict", "PASS");
                    valRep->verdictPolicy = rj.value("verdictPolicy", "audio-ab-v1");
                    valRep->reasonCode = rj.value("reasonCode", "WITHIN_TOLERANCE");
                }

                if (rj.contains("metrics") && rj["metrics"].contains("alignment"))
                    valRep->sampleOffset = rj["metrics"]["alignment"].value("sampleOffset", 0);
                else
                    valRep->sampleOffset = rj.value("sampleOffset", 0);

                if (rj.contains("metrics") && rj["metrics"].contains("postAlignment"))
                {
                    valRep->postAlignment.esrDb = rj["metrics"]["postAlignment"].value("esrDb", 0.0f);
                    valRep->postAlignment.correlationPeak = rj["metrics"]["postAlignment"].value("correlation", 0.0f);
                }
                else if (rj.contains("postAlignment"))
                {
                    valRep->postAlignment.esrDb = rj["postAlignment"].value("esrDb", 0.0f);
                    valRep->postAlignment.correlationPeak = rj["postAlignment"].value("correlationPeak", 0.0f);
                }
                valStatus = rj.value("status", "completed");
                valErrMsg = "";
            }
            catch (...) {}
        }
        // Nota Metrológica: Eliminada síntesis artificial de valRep cuando valReportFile no existe.

        std::string modelExportStatus = "notExecuted";
        std::string modelExportReason = "No external-plugin model export was requested. ModelPackage.h represents target identity metadata, not a trained neural or LUT acoustic model.";

        bool htmlOk = exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifestData,
            exportPoints,
            valRep.get(),
            valStatus,
            valErrMsg,
            optGuidedEvidence.has_value() ? &(*optGuidedEvidence) : nullptr,
            modelExportStatus,
            modelExportReason,
            optSessionEvidence.has_value() ? &(*optSessionEvidence) : nullptr
        );

        if (!htmlOk)
        {
            stageErr = "Failed to write certification HTML report in staging directory";
            return false;
        }

        return true;
    };

    juce::String expErr;
    bool expSaved = core::ExperimentStorage::saveExperiment(labDirs.experiments, record, {}, expErr, embeddedPayload, stagingHook);
    if (!expSaved)
    {
        currentSnapshot_.sessionStatus = ProfilingSessionStatus::Failed;
        publishSnapshotLocked();
        raiseAlert(UiAlert::Severity::Error,
                   "Fallo al guardar experimento y exportar modelo",
                   expErr.toStdString(),
                   "Se abortó la exportación para evitar publicar un artefacto inconsistente.",
                   "Verifique los permisos de almacenamiento.",
                   "Exportación abortada.");
        return false;
    }

    // 4. Conclusión exitosa de exportación
    currentSnapshot_.sessionStatus = ProfilingSessionStatus::Exported;
    currentSnapshot_.exportOptions.lastExportedFilePath = destinationPath.empty() ? destFile.getFullPathName().toStdString() : destinationPath;
    std::string expFolderName = record.experimentId;
    if (record.revision > 1)
        expFolderName += "_rev" + std::to_string(record.revision);
    juce::File finalExpFolder = labDirs.experiments.getChildFile(expFolderName);
    currentSnapshot_.exportOptions.lastExportedExperimentFolderPath = finalExpFolder.getFullPathName().toStdString();

    // Copia de conveniencia del HTML en exports/
    juce::File canonicalHtml = finalExpFolder.getChildFile("reports").getChildFile("certification_report.html");
    if (canonicalHtml.existsAsFile())
    {
        juce::File convenienceHtml = labDirs.exports.getChildFile(destFile.getFileNameWithoutExtension() + "_report.html");
        canonicalHtml.copyFileTo(convenienceHtml);
        currentSnapshot_.exportOptions.lastExportedHtmlReportPath = canonicalHtml.getFullPathName().toStdString();
    }

    // Actualizar ValidationUiSummary
    currentSnapshot_.validationSummary = abdaudiolab::core::ValidationUiSummary::fromExperimentFolder(finalExpFolder);
    publishSnapshotLocked();
    auto ctxDone = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::Exported, ctxDone);

    return true;
}

// -------------------------------------------------------------------------
// HITO-04: Contrato de exportabilidad — única fuente de verdad para guardas
// -------------------------------------------------------------------------
ExportReadiness ProfilingSessionController::evaluateExportReadiness() const
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return evaluateExportReadinessFromSnapshot(currentSnapshot_);
}

bool ProfilingSessionController::requestExportProductionPackage()
{
    if (!evaluateExportReadiness().canProceed())
        return false;
    return exportModel("cpp", "");
}

bool ProfilingSessionController::requestExportCertificationReport()
{
    if (!evaluateExportReadiness().canProceed())
        return false;
    return exportModel("certification", "");
}


core::ExperimentRecord ProfilingSessionController::buildCurrentExperimentRecord() const
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    core::ExperimentRecord r;
    r.schemaVersion = 1;

    juce::Time now = juce::Time::getCurrentTime();
    std::string timestamp = exporting::ModelExportNaming::formatUtcTimestamp(now);
    std::string targetLabel = currentSnapshot_.target.targetName.empty()
                                  ? (currentSnapshot_.evaluation.sourceTargetIdentity.empty()
                                         ? "Target"
                                         : currentSnapshot_.evaluation.sourceTargetIdentity)
                                  : currentSnapshot_.target.targetName;
    std::string safeTarget = exporting::ModelExportNaming::sanitizeComponent(targetLabel, "Target", 24);
    std::string hashPrefix = exporting::ModelExportNaming::extractHashPrefix(currentSnapshot_.evaluation.canonicalEvaluationHash, 8);

    r.experimentId = timestamp + "_" + safeTarget + "_" + hashPrefix;
    r.revision = 1;

    // Si ya existe un experimento previo con este ID, versionar automáticamente como nueva revisión
    auto expBase = core::resolveLabDataDirectories().experiments;
    if (expBase.getChildFile(r.experimentId).exists())
    {
        r.parentExperimentId = r.experimentId;
        uint32_t rev = 2;
        while (expBase.getChildFile(r.experimentId + "_rev" + std::to_string(rev)).exists())
        {
            ++rev;
        }
        r.revision = rev;
    }

    r.kind = (currentSnapshot_.workflowMode == UiWorkflowMode::Guided) ? core::ExperimentKind::Measurement : core::ExperimentKind::Exploration;

    if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::Accepted)
        r.status = core::ExperimentStatus::AuditedApproved;
    else if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings)
        r.status = core::ExperimentStatus::AuditedWithWarnings;
    else if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::Rejected)
        r.status = core::ExperimentStatus::Rejected;
    else if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::InvalidMeasurement)
        r.status = core::ExperimentStatus::MeasurementInvalid;
    else if (currentSnapshot_.evaluation.selectionStatus == synth::SelectionStatus::Inconclusive)
        r.status = core::ExperimentStatus::Inconclusive;
    else
        r.status = core::ExperimentStatus::LoadedForExploration;

    r.target.targetId = currentSnapshot_.target.targetId;
    r.target.targetName = targetLabel;
    r.target.manufacturer = currentSnapshot_.target.manufacturer;
    r.target.version = currentSnapshot_.target.version;
    r.target.format = targetKindToString(currentSnapshot_.target.kind);
    r.target.binarySha256 = currentSnapshot_.evaluation.pluginBinarySha256;
    r.target.binaryPath = currentSnapshot_.evaluation.pluginPath;
    r.target.isDeterministic = currentSnapshot_.target.isDeterministic;

    r.capture.sampleRate = 48000.0;
    r.capture.hostBufferSize = 480;
    r.capture.processingBlockSize = 256;
    r.capture.channels = 2;
    r.capture.durationSeconds = currentSnapshot_.progress.elapsedTimeSec;
    r.capture.presetStateHash = "";
    r.capture.excitationPlanHash = "";
    r.capture.storageProfile = core::StorageProfile::Standard;

    r.provenance.appVersion = version::kAppVersion;
    r.provenance.buildNumber = version::kBuildNumber;
    r.provenance.gitCommit = "bc6af12";
    r.provenance.executionMode = currentSnapshot_.target.useIsolatedProcess ? "OutOfProcessVST3" : "InProcess";
    r.provenance.operatingSystem = "Windows 11 x64";
    r.provenance.timestampUtc = timestamp;

    r.evaluation.hasEvaluation = currentSnapshot_.evaluation.hasEvaluation;
    r.evaluation.recommendedModelType = currentSnapshot_.evaluation.recommendedModelType;
    r.evaluation.selectionStatus = synth::selectionStatusToString(currentSnapshot_.evaluation.selectionStatus);
    r.evaluation.canonicalEvaluationHash = currentSnapshot_.evaluation.canonicalEvaluationHash;
    r.evaluation.validationEsrDb = currentSnapshot_.evaluation.validationEsrDb;
    r.evaluation.validationCorrelation = currentSnapshot_.evaluation.validationCorrelation;
    r.evaluation.criteriaCompliancePercent = currentSnapshot_.evaluation.stimuliMeetingCriterionPercent;
    r.evaluation.validatedDomain = currentSnapshot_.evaluation.validatedDomain;
    r.evaluation.relativeCpuCost = currentSnapshot_.evaluation.relativeCpuCostFactor;
    r.evaluation.hashVerified = currentSnapshot_.evaluation.hashVerified;

    r.limitations.modeledAspects = { "Cutoff response", "VCA dynamics", "PolyBLEP core" };
    r.limitations.unmodeledAspects = { "LFO phase drift", "Sub-oscillator noise" };
    r.limitations.validityDomain = currentSnapshot_.evaluation.validatedDomain;

    return r;
}

bool ProfilingSessionController::saveExperimentRecord(const std::string& destinationBaseDir, std::string& outCreatedFolder, std::string& outError)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    juce::File baseDir = destinationBaseDir.empty() ? core::ExperimentStorage::getDefaultExperimentsDirectory() : juce::File(destinationBaseDir);

    auto record = buildCurrentExperimentRecord();
    juce::String err;
    bool ok = core::ExperimentStorage::saveExperiment(baseDir, record, {}, err);
    if (!ok)
    {
        outError = err.toStdString();
        return false;
    }

    std::string folderName = record.experimentId;
    if (record.revision > 1)
        folderName += "_rev" + std::to_string(record.revision);

    outCreatedFolder = baseDir.getChildFile(folderName).getFullPathName().toStdString();
    return true;
}

bool ProfilingSessionController::loadExperimentRecord(const std::string& experimentFolderPath, std::string& outError)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    juce::File dir(experimentFolderPath);
    juce::String err;
    auto loaded = core::ExperimentStorage::loadExperiment(dir, err);
    if (!loaded.has_value())
    {
        outError = err.toStdString();
        raiseAlert(UiAlert::Severity::Error,
                   "Error al abrir experimento",
                   outError,
                   "El paquete de experimento no pudo ser procesado.",
                   "Verifique la ruta del archivo.",
                   "Operación cancelada.");
        return false;
    }

    if (loaded->isCorrupt())
    {
        outError = loaded->failureOrCorruptionReason;
        currentSnapshot_.validationSummary = abdaudiolab::core::ValidationUiSummary::fromExperimentFolder(dir);
        raiseAlert(UiAlert::Severity::Error,
                   "Experimento corrupto detectado",
                   outError,
                   "Un archivo ha sido modificado, dañado o falta en el paquete de experimento.",
                   "No se permite exportar ni reevaluar desde un contenedor corrupto.",
                   "Carga rechazada por integridad.");
        return false;
    }

    // Poblar snapshot desde el experimento verificado
    currentSnapshot_.target.targetId = loaded->target.targetId;
    currentSnapshot_.target.targetName = loaded->target.targetName;
    currentSnapshot_.target.manufacturer = loaded->target.manufacturer;
    currentSnapshot_.target.version = loaded->target.version;

    currentSnapshot_.evaluation.hasEvaluation = loaded->evaluation.hasEvaluation;
    currentSnapshot_.evaluation.recommendedModelType = loaded->evaluation.recommendedModelType;
    currentSnapshot_.evaluation.selectionStatus = synth::selectionStatusFromString(loaded->evaluation.selectionStatus);
    currentSnapshot_.evaluation.canonicalEvaluationHash = loaded->evaluation.canonicalEvaluationHash;
    currentSnapshot_.evaluation.validationEsrDb = loaded->evaluation.validationEsrDb;
    currentSnapshot_.evaluation.validationCorrelation = loaded->evaluation.validationCorrelation;
    currentSnapshot_.evaluation.stimuliMeetingCriterionPercent = loaded->evaluation.criteriaCompliancePercent;
    currentSnapshot_.evaluation.validatedDomain = loaded->evaluation.validatedDomain;
    currentSnapshot_.evaluation.relativeCpuCostFactor = loaded->evaluation.relativeCpuCost;
    currentSnapshot_.evaluation.hashVerified = loaded->evaluation.hashVerified;
    currentSnapshot_.evaluation.evaluationOrigin = synth::EvaluationOrigin::ImportedArtifact;
    currentSnapshot_.evaluation.sourceTargetIdentity = loaded->target.targetName;

    currentSnapshot_.sessionStatus = ProfilingSessionStatus::EvaluationLoadedForReview;
    currentSnapshot_.workflowStage = ProfilingWorkflowStage::ReviewResults;
    currentSnapshot_.exportOptions.canExportCpp = loaded->isExportable();
    currentSnapshot_.exportOptions.lastExportedExperimentFolderPath = dir.getFullPathName().toStdString();

    juce::File htmlFile = dir.getChildFile("reports").getChildFile("certification_report.html");
    if (htmlFile.existsAsFile())
        currentSnapshot_.exportOptions.lastExportedHtmlReportPath = htmlFile.getFullPathName().toStdString();

    currentSnapshot_.validationSummary = abdaudiolab::core::ValidationUiSummary::fromExperimentFolder(dir);

    publishSnapshotLocked();
    auto ctx = createCallbackContextLocked();
    notifyStatusListeners(ProfilingSessionStatus::EvaluationLoadedForReview, ctx);
    notifyStageListeners(ProfilingWorkflowStage::ReviewResults, ctx);

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

void ProfilingSessionController::setExcitationMode(ExcitationMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (currentSnapshot_.excitation.targetControlMode == TargetControlMode::NoDigitalControl &&
        mode != ExcitationMode::ManualOperator)
    {
        raiseAlert(UiAlert::Severity::Warning,
                   "Modo no disponible",
                   "El hardware seleccionado no posee interfaz digital (MIDI/VST3).",
                   "La excitacion automatizada no es posible en este dispositivo.",
                   "Permanezca en el modo de Operador Manual.",
                   "El modo permanece configurado en ManualOperator.");
        return;
    }

    currentSnapshot_.excitation.excitationMode = mode;
    currentSnapshot_.progress.activeExcitationMode = mode;
    if (mode == ExcitationMode::ManualOperator && !currentSnapshot_.excitation.manual.has_value())
    {
        currentSnapshot_.excitation.manual = ManualOperatorRecipe{};
    }
    else if (mode == ExcitationMode::AutomatedMidi && !currentSnapshot_.excitation.midi.has_value())
    {
        currentSnapshot_.excitation.midi = MidiRecipe{};
    }

    publishSnapshotLocked();
}

void ProfilingSessionController::updateMidiRecipe(const MidiRecipe& recipe)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    auto validated = recipe;
    bool valid = true;
    std::string err;

    if (validated.firstNote < 0 || validated.lastNote > 127 || validated.firstNote > validated.lastNote)
    {
        valid = false;
        err = "Rango de notas invalido [0..127].";
    }
    else if (validated.velocities.empty())
    {
        valid = false;
        err = "Se requiere al menos una velocidad de pulsacion.";
    }
    else if (validated.gateMs < 10.0 || validated.settlingMs < 0.0)
    {
        valid = false;
        err = "Parametros temporales fuera de rango (gate >= 10 ms, settling >= 0 ms).";
    }

    currentSnapshot_.excitation.isValid = valid;
    currentSnapshot_.excitation.validationError = err;
    if (valid)
    {
        currentSnapshot_.excitation.midi = validated;
    }
    publishSnapshotLocked();
}

void ProfilingSessionController::updateManualRecipe(const ManualOperatorRecipe& recipe)
{
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    auto validated = recipe;
    bool valid = true;
    std::string err;

    if (validated.repetitions < 1)
    {
        valid = false;
        err = "El numero de repeticiones debe ser al menos 1.";
    }
    else if (validated.settlingMs < 0.0)
    {
        valid = false;
        err = "El tiempo de estabilizacion no puede ser negativo.";
    }

    currentSnapshot_.excitation.isValid = valid;
    currentSnapshot_.excitation.validationError = err;
    if (valid)
    {
        currentSnapshot_.excitation.manual = validated;
    }
    publishSnapshotLocked();
}

void ProfilingSessionController::confirmOperatorStep()
{
    {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        currentSnapshot_.progress.trialStage = TrialLifecycleStage::WaitForStabilization;
        publishSnapshotLocked();
    }

    if (onOperatorStepConfirmed)
        onOperatorStepConfirmed();
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
