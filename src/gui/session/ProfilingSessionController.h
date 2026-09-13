#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

#include "ProfilingSessionContracts.h"

namespace abdaudiolab::gui::session
{

/**
 * @brief Coordinador fino y desacoplado de sesión para la interfaz de usuario (Fase 16 / UX_SIMPLIFICATION_PLAN).
 * Implementa IProfilingSessionCommands y orquesta la máquina de estados publicando snapshots inmutables
 * secuenciados hacia la presentación sin tocar el hilo de audio en tiempo real.
 */
class ProfilingSessionController : public IProfilingSessionCommands
{
public:
    ProfilingSessionController();
    ~ProfilingSessionController() override;

    // Registro de observadores de eventos de presentación
    void addListener(IProfilingSessionEventListener* listener);
    void removeListener(IProfilingSessionEventListener* listener);

    // Consulta del snapshot actual
    [[nodiscard]] ProfilingSessionSnapshot getCurrentSnapshot() const;

    // Implementación de IProfilingSessionCommands
    bool selectTarget(const TargetSelectionState& target) override;
    bool requestAudit() override;
    bool startProfiling() override;
    bool pauseProfiling() override;
    bool resumeProfiling() override;
    bool cancelProfiling() override;
    bool exportModel(const std::string& format, const std::string& destinationPath) override;
    void navigateToStage(ProfilingWorkflowStage stage) override;

    void setWorkflowMode(UiWorkflowMode mode) override;
    void acknowledgeWarnings() override;
    void recordUserClick() override;
    void setOpenedAdvancedMode(bool opened) override;

    [[nodiscard]] uint64_t getActiveGeneration() const noexcept;

    // Métodos de actualización de telemetría de backend (invocados por workers de sesión)
    void updateAuditResult(synth::ApprovalStatus status,
                           const std::string& determinism,
                           const std::string& resetCap,
                           double settlingMs,
                           bool reqReset,
                           const std::vector<std::string>& warnings,
                           const std::string& guidance);

    void updateProgress(int currentTrial, int totalTrials,
                        double elapsedSec, double remainingSec,
                        const std::string& currentStimulus);

    void updateObservation(double rmsDb, double peakDb, double pitchHz,
                           bool clipping, bool silence, double snrDb);

    void updateModelEvaluation(synth::SelectionStatus status,
                               const std::string& bestModelType,
                               double esrDb, double correlation,
                               double stimuliMeetingCriterionPct,
                               const std::string& domain,
                               double cpuFactor,
                               const std::vector<std::string>& warnings,
                               const std::vector<std::string>& limitingFactors);

    void raiseAlert(UiAlert::Severity severity,
                    std::string title,
                    std::string cause,
                    std::string impact,
                    std::string action,
                    std::string consequence);

    void completeProfiling();
    void failSession(const std::string& reason);

private:
    mutable std::mutex stateMutex_;
    ProfilingSessionSnapshot currentSnapshot_;
    std::atomic<uint64_t> sequenceCounter_ { 0 };

    std::mutex listenersMutex_;
    std::vector<IProfilingSessionEventListener*> listeners_;

    void publishSnapshotLocked();
    void notifyAlertListeners(const UiAlert& alert);
    void notifyStageListeners(ProfilingWorkflowStage stage);
    void notifyStatusListeners(ProfilingSessionStatus status);

    [[nodiscard]] bool canTransitionTo(ProfilingSessionStatus newStatus) const;
    static uint64_t getCurrentTimeMs();
};

} // namespace abdaudiolab::gui::session
