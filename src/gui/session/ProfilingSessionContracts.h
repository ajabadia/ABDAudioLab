#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <chrono>

#include "synth/ModelEvaluationTypes.h"
#include "synth/TargetContract.h"

namespace abdaudiolab::gui::session
{

/**
 * @brief Modo de visualización y orquestación de la interfaz de usuario.
 * Permite conmutar con seguridad entre el modo clásico (SuiteList + Plotter) y el flujo guiado de 3 pasos (SoundID).
 */
enum class UiWorkflowMode
{
    Classic, /**< Superficie de control clásica completa con SuiteList, Plotter y Drawers. */
    Guided   /**< Flujo guiado de tres pasos (Paso 1: Target, Paso 2: Start, Paso 3: Results). */
};

[[nodiscard]] inline std::string workflowModeToString(UiWorkflowMode mode)
{
    return (mode == UiWorkflowMode::Guided) ? "Guided" : "Classic";
}

/**
 * @brief Etapa del flujo de trabajo de perfilado (3 pasos guiados + soporte).
 */
enum class ProfilingWorkflowStage
{
    TargetSelection,   /**< Paso 1: Selección y verificación de conexión del target. */
    ConfigureAndStart, /**< Paso 2: Revisión breve de receta y botón iniciar perfilado. */
    ProfilingActive,   /**< Paso 2 (en curso): Ejecución de mediciones con feedback limpio. */
    ReviewResults,     /**< Paso 3: Modelo recomendado, dominio validado y exportación. */
    AdvancedSettings   /**< Vista de soporte: Parámetros técnicos profundos (opcional). */
};

[[nodiscard]] inline std::string workflowStageToString(ProfilingWorkflowStage stage)
{
    switch (stage)
    {
        case ProfilingWorkflowStage::TargetSelection:   return "TargetSelection";
        case ProfilingWorkflowStage::ConfigureAndStart: return "ConfigureAndStart";
        case ProfilingWorkflowStage::ProfilingActive:   return "ProfilingActive";
        case ProfilingWorkflowStage::ReviewResults:     return "ReviewResults";
        case ProfilingWorkflowStage::AdvancedSettings:  return "AdvancedSettings";
        default:                                        return "Unknown";
    }
}

/**
 * @brief Estado formal de la máquina de estados de la sesión de perfilado.
 */
enum class ProfilingSessionStatus
{
    Idle,                /**< Estado inicial sin target activo. */
    TargetSelected,      /**< Target elegido, pendiente de auditoría o validación previa. */
    Auditing,            /**< Auditoría metrológica previa en curso. */
    ReadyToProfile,      /**< Target auditado y listo para iniciar perfilado adaptativo. */
    Profiling,           /**< Ensayos de excitación y aprendizaje activo en ejecución. */
    Paused,              /**< Sesión pausada temporalmente por el usuario. */
    Completed,           /**< Ensayos concluidos, modelo sintetizado y evaluado en sesión activa. */
    EvaluationLoadedForReview, /**< Evaluación importada desde artefacto persistido para revisión / exportación. */
    Exporting,           /**< Exportación de paquete C++20 / JSON / NAM en curso. */
    Exported,            /**< Paquete exportado satisfactoriamente. */

    // Estados terminales de fallo o cancelación
    AuditRejected,       /**< Target rechazado en auditoría (no determinista, inestable o corruptor). */
    UnsupportedTarget,   /**< Target incompatible con la receta (ej. sin MIDI o sin audio). */
    MeasurementInvalid,  /**< Medición descartada por clipping, distorsión o artefactos de host. */
    Cancelled,           /**< Sesión cancelada por el usuario. */
    Failed               /**< Error crítico en host o hardware. */
};

[[nodiscard]] inline std::string sessionStatusToString(ProfilingSessionStatus status)
{
    switch (status)
    {
        case ProfilingSessionStatus::Idle:                      return "Idle";
        case ProfilingSessionStatus::TargetSelected:            return "TargetSelected";
        case ProfilingSessionStatus::Auditing:                  return "Auditing";
        case ProfilingSessionStatus::ReadyToProfile:            return "ReadyToProfile";
        case ProfilingSessionStatus::Profiling:                 return "Profiling";
        case ProfilingSessionStatus::Paused:                    return "Paused";
        case ProfilingSessionStatus::Completed:                 return "Completed";
        case ProfilingSessionStatus::EvaluationLoadedForReview: return "EvaluationLoadedForReview";
        case ProfilingSessionStatus::Exporting:                 return "Exporting";
        case ProfilingSessionStatus::Exported:                  return "Exported";
        case ProfilingSessionStatus::AuditRejected:             return "AuditRejected";
        case ProfilingSessionStatus::UnsupportedTarget:         return "UnsupportedTarget";
        case ProfilingSessionStatus::MeasurementInvalid:        return "MeasurementInvalid";
        case ProfilingSessionStatus::Cancelled:                 return "Cancelled";
        case ProfilingSessionStatus::Failed:                    return "Failed";
        default:                                                return "Unknown";
    }
}

/**
 * @brief Tipo o naturaleza física del target.
 */
enum class TargetKind
{
    HardwareAnalogue,
    HardwareDigital,
    PluginVST3,
    PluginAU,
    SyntheticFixture
};

[[nodiscard]] inline std::string targetKindToString(TargetKind kind)
{
    switch (kind)
    {
        case TargetKind::HardwareAnalogue: return "HardwareAnalogue";
        case TargetKind::HardwareDigital:  return "HardwareDigital";
        case TargetKind::PluginVST3:       return "PluginVST3";
        case TargetKind::PluginAU:         return "PluginAU";
        case TargetKind::SyntheticFixture: return "SyntheticFixture";
        default:                           return "Unknown";
    }
}

/**
 * @brief Estado de la selección y conectividad del target (Paso 1).
 */
struct TargetSelectionState
{
    std::string targetId;
    std::string targetName;
    std::string manufacturer;
    std::string version;
    TargetKind kind { TargetKind::PluginVST3 };

    bool isConnected { false };
    bool isDeterministic { true };
    std::string connectionSummary;

    std::string availableDomainDescription; /**< Ej: "Notas MIDI C1-C6, Vel 1-127, 8 controles" */
    int parameterCount { 0 };
    std::vector<std::string> initialWarnings;
    bool useIsolatedProcess { false }; /**< Si es true, el modo guiado utilizará el worker esclavo aislado (OutOfProcessVst3LifecycleAdapter) */
};

/**
 * @brief Resumen accesible del resultado de auditoría metrológica previa.
 */
struct TargetAuditState
{
    bool isAudited { false };
    synth::ApprovalStatus approvalStatus { synth::ApprovalStatus::Rejected };
    std::string determinismText;
    std::string resetCapabilityText;
    double recommendedSettlingTimeMs { 50.0 };
    bool requiresResetBeforeEachTrial { true };

    std::vector<std::string> operationalWarnings;
    std::string humanGuidance;
};

/**
 * @brief Progreso cuantitativo limpio de la sesión activa (Paso 2).
 */
struct ExperimentProgressState
{
    int currentTrial { 0 };
    int totalTrials { 0 };
    double progressPercent { 0.0 };

    double elapsedTimeSec { 0.0 };
    double estimatedRemainingSec { 0.0 };

    std::string activeRecipeName;
    std::string currentStimulusDescription;
};

/**
 * @brief Resumen de observación acústica y telemetría de salud.
 */
struct ObservationSummaryState
{
    double lastRmsDb { -120.0 };
    double lastPeakDb { -120.0 };
    double lastEstimatedPitchHz { 0.0 };
    bool clippingDetected { false };
    bool silenceDetected { false };
    double snrEstimateDb { 0.0 };
};

/**
 * @brief Resumen formal y no ambiguo de la evaluación de modelos (Paso 3).
 * Evita porcentajes vagos de "fidelidad 99.4%" y expone métricas físicas interpretables.
 */
struct ModelEvaluationSummaryState
{
    bool hasEvaluation { false };
    synth::SelectionStatus selectionStatus { synth::SelectionStatus::Inconclusive };

    std::string recommendedModelType;     /**< Ej. "TPT ZDF Ladder Filter (Analytic Grey-Box)" */
    double validationEsrDb { 0.0 };       /**< Error-to-Signal Ratio en decibelios (dB). */
    double validationCorrelation { 0.0 }; /**< Coeficiente de correlación espectro-temporal rho. */
    double stimuliMeetingCriterionPercent { 0.0 }; /**< % de puntos evaluados que cumplen ESR < tolerancia. */

    std::string validatedDomain;          /**< Ej. "C1–C6, velocity 30–127, cutoff [50 Hz, 16 kHz]" */
    double relativeCpuCostFactor { 1.0 }; /**< Coste computacional relativo a referencia (ej. 1.2x). */

    std::vector<std::string> limitingFactors;
    std::vector<std::string> evaluationWarnings;

    // --- Auditoría criptográfica, procedencia y control de dos capas ---
    std::string evaluationId;
    std::string protocolVersion { "1.0.0" };
    std::string canonicalEvaluationHash;
    synth::EvaluationOrigin evaluationOrigin { synth::EvaluationOrigin::SyntheticDemo };
    synth::EvaluationLoadStatus evaluationLoadStatus { synth::EvaluationLoadStatus::LoadedAndVerified };
    bool hashVerified { false };
    int warningsCount { 0 };
    std::string sourceTargetIdentity;
    std::string exportBlockReason;
    std::string rationale;

    // --- Procedencia física local y binaria (Fase 20.8.2) ---
    std::string sourceFilePath;
    std::string sourceFileHash;
    std::string executionMode;
    std::string pluginBinarySha256;
    std::string pluginPath;
    std::string pluginFormatVersion;
    std::string vendor;
    std::string pluginUid;
    uint64_t fileSizeBytes { 0 };
    std::string buildConfiguration;
    std::string osArchitecture;
    std::string normalizedFingerprint;
};

/**
 * @brief Disponibilidad de exportación de artefactos de producción.
 */
struct ExportAvailabilityState
{
    bool canExportCpp { false };
    bool canExportJson { false };
    bool canExportNam { false };
    bool canExportLut { false };
    std::string lastExportedFilePath;
    std::string exportBlockReason;
};

/**
 * @brief Alerta UX estructurada y formal con 4 campos obligatorios para reducción cognitiva.
 */
struct UiAlert
{
    enum class Severity { Info, Warning, Error } severity { Severity::Info };

    std::string title;
    std::string cause;               /**< ¿Por qué ha ocurrido esto? */
    std::string impact;              /**< ¿Cómo afecta a la medición o al modelo? */
    std::string recommendedAction;   /**< ¿Qué debe hacer el usuario? */
    std::string consequenceIfIgnored;/**< ¿Qué pasará si continúa sin actuar? */
    uint64_t timestampMs { 0 };
};

/**
 * @brief Snapshot inmutable de la sesión completa publicado periódicamente a la GUI.
 * Incorpora versión y secuencia monotónica estricta para evitar carreras y desactualizaciones.
 */
struct ProfilingSessionSnapshot
{
    std::string snapshotVersion { "1.0.0" };
    std::string sessionId;
    uint64_t monotonicSequence { 0 };
    uint64_t controllerGeneration { 0 };
    uint64_t timestampMs { 0 };

    UiWorkflowMode workflowMode { UiWorkflowMode::Classic };
    ProfilingWorkflowStage workflowStage { ProfilingWorkflowStage::TargetSelection };
    ProfilingSessionStatus sessionStatus { ProfilingSessionStatus::Idle };

    TargetSelectionState target;
    TargetAuditState audit;
    ExperimentProgressState progress;
    ObservationSummaryState observation;
    ModelEvaluationSummaryState evaluation;
    ExportAvailabilityState exportOptions;

    std::vector<UiAlert> activeAlerts;

    // Métricas de interacción y reducción cognitiva (UX Telemetry)
    int clickCount { 0 };
    bool openedAdvancedMode { false };
    bool warningsAcknowledged { false };
    bool userOverrides { false };
    uint64_t taskStartedAtMs { 0 };
    uint64_t taskCompletedAtMs { 0 };
};

/**
 * @brief Interfaz pura para envío de comandos desde la GUI (Command segregation).
 */
class IProfilingSessionCommands
{
public:
    virtual ~IProfilingSessionCommands() = default;

    virtual bool selectTarget(const TargetSelectionState& target) = 0;
    virtual bool requestAudit() = 0;
    virtual bool startProfiling() = 0;
    virtual bool pauseProfiling() = 0;
    virtual bool resumeProfiling() = 0;
    virtual bool cancelProfiling() = 0;
    virtual bool exportModel(const std::string& format, const std::string& destinationPath) = 0;
    virtual bool loadEvaluationFromFile(const std::string& filePath) = 0;
    virtual bool loadEvaluationFromJsonString(const std::string& jsonString, const std::string& sourceFilePath = "") = 0;
    virtual bool loadPredefinedFixture(const std::string& fixtureFileName) = 0;
    virtual void navigateToStage(ProfilingWorkflowStage stage) = 0;

    virtual void setWorkflowMode(UiWorkflowMode mode) = 0;
    virtual void acknowledgeWarnings() = 0;
    virtual void recordUserClick() = 0;
    virtual void recordUserOverride() = 0;
    virtual void setOpenedAdvancedMode(bool opened) = 0;
};

/**
 * @brief Interfaz para notificación de eventos discretos hacia la GUI (Event segregation).
 */
class IProfilingSessionEventListener
{
public:
    virtual ~IProfilingSessionEventListener() = default;

    virtual void onSessionSnapshotUpdated(const ProfilingSessionSnapshot& snapshot) = 0;
    virtual void onAlertRaised(const UiAlert& alert) = 0;
    virtual void onWorkflowStageChanged(ProfilingWorkflowStage newStage) = 0;
    virtual void onSessionStatusChanged(ProfilingSessionStatus newStatus) = 0;
};

} // namespace abdaudiolab::gui::session
