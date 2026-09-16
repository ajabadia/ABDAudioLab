#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

#include "ProfilingSessionContracts.h"
#include "ProfilingSessionCoordinator.h"
#include "../../core/ExperimentRecord.h"

namespace abdaudiolab::gui::session
{

/**
 * @brief Coordinador fino y desacoplado de sesión para la interfaz de usuario (Fase 16 / UX_SIMPLIFICATION_PLAN).
 * Implementa IProfilingSessionCommands y orquesta la máquina de estados publicando snapshots inmutables
 * secuenciados hacia la presentación sin tocar el hilo de audio en tiempo real.
 */
class ProfilingSessionController : public IProfilingSessionCommands, public ICoordinatorListener
{
public:
    ProfilingSessionController();
    ~ProfilingSessionController() override;

    // Registro de observadores de eventos de presentación
    void addListener(IProfilingSessionEventListener* listener);
    void removeListener(IProfilingSessionEventListener* listener);
    void removeAllListeners();

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
    bool saveExperimentRecord(const std::string& destinationBaseDir, std::string& outCreatedFolder, std::string& outError) override;
    bool loadExperimentRecord(const std::string& experimentFolderPath, std::string& outError) override;
    bool loadEvaluationFromFile(const std::string& filePath) override;
    bool loadEvaluationFromJsonString(const std::string& jsonString, const std::string& sourceFilePath = "") override;
    bool loadPredefinedFixture(const std::string& fixtureFileName) override;
    void navigateToStage(ProfilingWorkflowStage stage) override;

    [[nodiscard]] core::ExperimentRecord buildCurrentExperimentRecord() const;

    static juce::File getEvaluationsDirectory();

    void setWorkflowMode(UiWorkflowMode mode) override;
    void acknowledgeWarnings() override;
    void recordUserClick() override;
    void recordUserOverride() override;
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

    void updateModelEvaluation(const synth::ModelEvaluation& eval);

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

    [[nodiscard]] ProfilingSessionCoordinator* getCoordinator() noexcept;

    // Implementación de ICoordinatorListener
    void onCoordinatorSnapshotUpdated(const CoordinatorSnapshot& snapshot) override;
    void onCoordinatorCompleted(uint64_t runId, uint64_t sessionGeneration, const synth::ModelEvaluation& candidate) override;
    void onCoordinatorCancelled(uint64_t runId, uint64_t sessionGeneration) override;
    void onCoordinatorFailed(uint64_t runId, uint64_t sessionGeneration, const std::string& error) override;

private:

    mutable std::recursive_mutex stateMutex_;
    ProfilingSessionSnapshot currentSnapshot_;
    ModelEvaluationSummaryState previousEvaluation_;
    ExportAvailabilityState previousExportOptions_;
    std::atomic<uint64_t> sequenceCounter_ { 0 };

    std::mutex listenersMutex_;
    std::vector<IProfilingSessionEventListener*> listeners_;
    std::shared_ptr<std::atomic<bool>> aliveToken_;
    std::unique_ptr<ProfilingSessionCoordinator> coordinator_;

    struct CallbackContext
    {
        std::shared_ptr<std::atomic<bool>> alive;
        std::string sessionId;
        uint64_t generation { 0 };
    };

    [[nodiscard]] CallbackContext createCallbackContextLocked() const
    {
        return { aliveToken_, currentSnapshot_.sessionId, currentSnapshot_.controllerGeneration };
    }

    [[nodiscard]] bool isValidCallbackContext(const CallbackContext& ctx) const
    {
        if (!ctx.alive || !ctx.alive->load(std::memory_order_acquire))
            return false;
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        return (currentSnapshot_.sessionId == ctx.sessionId &&
                currentSnapshot_.controllerGeneration == ctx.generation);
    }

    void publishSnapshotLocked();
    void notifyAlertListeners(const UiAlert& alert, const CallbackContext& ctx);
    void notifyStageListeners(ProfilingWorkflowStage stage, const CallbackContext& ctx);
    void notifyStatusListeners(ProfilingSessionStatus status, const CallbackContext& ctx);

    [[nodiscard]] bool canTransitionTo(ProfilingSessionStatus newStatus) const;
    static uint64_t getCurrentTimeMs();
};

} // namespace abdaudiolab::gui::session
