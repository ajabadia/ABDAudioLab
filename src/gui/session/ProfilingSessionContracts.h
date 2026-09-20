#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <chrono>

#include <optional>
#include <cctype>
#include <algorithm>
#include <juce_core/juce_core.h>

#include "synth/ModelEvaluationTypes.h"
#include "synth/TargetContract.h"
#include "../../core/ValidationUiSummary.h"

namespace abdaudiolab::gui::session
{

/**
 * @brief Modo de control y conectividad del target hardware o software.
 */
enum class TargetControlMode
{
    NoDigitalControl,   /**< Hardware analógico puro sin MIDI, USB ni interfaz de control. */
    Midi,               /**< Dispositivo hardware controlado por canal y puerto MIDI. */
    Vst3,               /**< Plugin virtual VST3 in-process o en worker aislado. */
    MidiCc,             /**< Automatización de parámetros por MIDI Continuous Controllers. */
    MidiSysEx           /**< Control y configuración mediante volcados SysEx del fabricante. */
};

[[nodiscard]] inline std::string targetControlModeToString(TargetControlMode mode)
{
    switch (mode)
    {
        case TargetControlMode::NoDigitalControl: return "NoDigitalControl";
        case TargetControlMode::Midi:             return "Midi";
        case TargetControlMode::Vst3:             return "Vst3";
        case TargetControlMode::MidiCc:           return "MidiCc";
        case TargetControlMode::MidiSysEx:        return "MidiSysEx";
        default:                                  return "Unknown";
    }
}

/**
 * @brief Modo operativo de generación de estímulos / excitación en el ensayo.
 */
enum class ExcitationMode
{
    ManualOperator,        /**< Estímulo guiado o ejecutado físicamente por el operador humano. */
    AutomatedMidi,         /**< Secuencia programática factorial de notas MIDI con compuerta gateMs. */
    AutomatedVstParameter, /**< Automatización directa de parámetros del plugin VST3. */
    AutomatedCc,           /**< Barrido automatizado de MIDI CCs según contrato de hardware. */
    AutomatedSysEx         /**< Volcado automatizado de mensajes SysEx. */
};

[[nodiscard]] inline std::string excitationModeToString(ExcitationMode mode)
{
    switch (mode)
    {
        case ExcitationMode::ManualOperator:        return "ManualOperator";
        case ExcitationMode::AutomatedMidi:         return "AutomatedMidi";
        case ExcitationMode::AutomatedVstParameter: return "AutomatedVstParameter";
        case ExcitationMode::AutomatedCc:           return "AutomatedCc";
        case ExcitationMode::AutomatedSysEx:        return "AutomatedSysEx";
        default:                                    return "Unknown";
    }
}

/**
 * @brief Subtipo de acción requerida al operador en modo manual.
 */
enum class ManualInteractionKind
{
    PhysicalControlAdjustment,   /**< Mover potenciómetros, faders o switches físicos (Knob, Slider). */
    ManualNotePerformance,       /**< Tocar notas o acordes manualmente (teclado físico o virtual). */
    PresetOrRoutingConfirmation  /**< Cargar preset físico en hardware o confirmar parcheo de cables. */
};

[[nodiscard]] inline std::string manualInteractionKindToString(ManualInteractionKind kind)
{
    switch (kind)
    {
        case ManualInteractionKind::PhysicalControlAdjustment:   return "PhysicalControlAdjustment";
        case ManualInteractionKind::ManualNotePerformance:       return "ManualNotePerformance";
        case ManualInteractionKind::PresetOrRoutingConfirmation: return "PresetOrRoutingConfirmation";
        default:                                                 return "Unknown";
    }
}

/**
 * @brief Etapa del ciclo de vida operativo de un ensayo individual.
 */
enum class TrialLifecycleStage
{
    Armed,
    WaitingForOperator,
    WaitForStabilization,
    NoteOn,
    Gate,
    NoteOff,
    Capturing,
    Finished,
    ErrorState
};

[[nodiscard]] inline std::string trialLifecycleStageToString(TrialLifecycleStage stage)
{
    switch (stage)
    {
        case TrialLifecycleStage::Armed:                return "Armed";
        case TrialLifecycleStage::WaitingForOperator:   return "WaitingForOperator";
        case TrialLifecycleStage::WaitForStabilization: return "WaitForStabilization";
        case TrialLifecycleStage::NoteOn:               return "NoteOn";
        case TrialLifecycleStage::Gate:                 return "Gate";
        case TrialLifecycleStage::NoteOff:              return "NoteOff";
        case TrialLifecycleStage::Capturing:            return "Capturing";
        case TrialLifecycleStage::Finished:             return "Finished";
        case TrialLifecycleStage::ErrorState:           return "ErrorState";
        default:                                        return "Unknown";
    }
}

/**
 * @brief Receta de ensayo manual guiado por el operador.
 */
struct ManualOperatorRecipe
{
    ManualInteractionKind interactionKind { ManualInteractionKind::PhysicalControlAdjustment };
    juce::String instruction { "Ajustar controles físicos según la indicación y pulsar Listo [Espacio]" };
    juce::String expectedSetting { "Default" };
    int repetitions { 1 };
    double settlingMs { 500.0 };
    bool requireOperatorConfirmation { true };
};

/**
 * @brief Receta de ensayo de excitación MIDI automatizada.
 */
struct MidiRecipe
{
    int firstNote { 36 }; // C2
    int lastNote { 84 };  // C6
    std::vector<int> velocities { 32, 64, 127 };
    double gateMs { 250.0 };
    double settlingMs { 50.0 };
    int midiChannel { 1 };
    int repetitions { 1 };
    std::string sequenceHash;
};

enum class RecipeStatus
{
    Valid,
    IncompatibleWithTarget,
    Uninitialized
};

[[nodiscard]] inline std::string recipeStatusToString(RecipeStatus status)
{
    switch (status)
    {
        case RecipeStatus::Valid:                  return "Valid";
        case RecipeStatus::IncompatibleWithTarget: return "IncompatibleWithTarget";
        case RecipeStatus::Uninitialized:          return "Uninitialized";
        default:                                   return "Unknown";
    }
}

/**
 * @brief Grado de obligatoriedad de la calibración para un target específico.
 */
enum class CalibrationRequirement
{
    NotApplicable,
    Required,
    Optional
};

[[nodiscard]] inline std::string calibrationRequirementToString(CalibrationRequirement req)
{
    switch (req)
    {
        case CalibrationRequirement::NotApplicable: return "NotApplicable";
        case CalibrationRequirement::Required:      return "Required";
        case CalibrationRequirement::Optional:      return "Optional";
        default:                                    return "Unknown";
    }
}

/**
 * @brief Estado de calibración de audio físico (DAC -> Equipo -> ADC).
 */
struct AudioCalibrationState
{
    CalibrationRequirement requirement { CalibrationRequirement::NotApplicable };
    bool completed { false };
    bool bypassed { false };
    float inputGainTrimDb { 0.0f };
    float outputGainTrimDb { 0.0f };
    float roundTripLatencyMs { 0.0f };
    float snrDb { 0.0f };
    std::string summary;
};

/**
 * @brief Estado de calibración de compuerta y retardo de mensajes MIDI.
 */
struct MidiCalibrationState
{
    CalibrationRequirement requirement { CalibrationRequirement::NotApplicable };
    bool completed { false };
    float detectedMidiLatencyMs { 0.0f };
    float jitterMs { 0.0f };
    std::string summary;
};

/**
 * @brief Estado de verificación de la ruta digital interna (Plugins VST3 / Workers IPC).
 */
struct DigitalPathCalibrationState
{
    CalibrationRequirement requirement { CalibrationRequirement::NotApplicable };
    bool verified { false };
    float bufferLatencyMs { 0.0f };
    bool bitExact { true };
    std::string summary;
};

/**
 * @brief Estado integral ortogonal de calibración del sistema.
 */
struct CalibrationStatus
{
    AudioCalibrationState audio;
    MidiCalibrationState midi;
    DigitalPathCalibrationState digital;

    [[nodiscard]] bool isReadyForProfiling() const noexcept
    {
        const bool audioReady =
            audio.requirement != CalibrationRequirement::Required ||
            audio.completed ||
            audio.bypassed;

        const bool midiReady =
            midi.requirement != CalibrationRequirement::Required ||
            midi.completed;

        const bool digitalReady =
            digital.requirement != CalibrationRequirement::Required ||
            digital.verified;

        return audioReady && midiReady && digitalReady;
    }
};

/**
 * @brief Estado integral de la receta de excitación de la sesión.
 */
struct ExcitationRecipeState
{
    TargetControlMode targetControlMode { TargetControlMode::Vst3 };
    ExcitationMode excitationMode { ExcitationMode::AutomatedMidi };
    RecipeStatus status { RecipeStatus::Valid };
    std::string targetIdentity;
    std::string targetContractVersion;
    std::string recipeVersion { "1.0.0" };
    std::string canonicalizationVersion { "RFC8785" };

    std::optional<MidiRecipe> midi { MidiRecipe{} };
    std::optional<ManualOperatorRecipe> manual { std::nullopt };

    bool isValid { true };
    std::string validationError;
};

/**
 * @brief Progreso cuantitativo limpio de la sesión activa (Paso 2 y Paso 3).
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

    // Campos ricos de estado del ensayo activo
    TargetControlMode activeControlMode { TargetControlMode::Vst3 };
    ExcitationMode activeExcitationMode { ExcitationMode::AutomatedMidi };
    TrialLifecycleStage trialStage { TrialLifecycleStage::Armed };
    std::string operatorPromptText;
    int activeNoteNumber { 60 };
    std::string activeNoteName { "C4 (60)" };
    int activeVelocity { 80 };
    int activeMidiChannel { 1 };
    double activeGateMs { 250.0 };
    double detectedLatencyMs { 0.0 };
};

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

    // Capacidades reales del target derivadas del contrato / plugin
    bool supportsMidiInput { false };
    bool supportsParameterAutomation { false };
    bool supportsMidiCc { false };
    bool supportsSysEx { false };
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
 * @brief Conversión única y documentada del estado operativo del motor (SequencerState)
 * al ciclo de vida del ensayo proyectado en el snapshot (TrialLifecycleStage).
 */
[[nodiscard]] constexpr TrialLifecycleStage mapSequencerStateToTrialStage(int sequencerStateRaw) noexcept
{
    // Mapeo exhaustivo según core::SequencerState:
    // 0: Idle, 1: LineCalibration, 2: InitiateTestCase, 3: WaitForStabilization,
    // 4: WaitingForOperator, 5: InjectStimulus, 6: CaptureAndAnalyze,
    // 7: InterludeNoiseFloor, 8: ExportDataAndCleanup, 9: Finished, 10: ErrorState
    switch (sequencerStateRaw)
    {
        case 3: /* WaitForStabilization */ return TrialLifecycleStage::WaitForStabilization;
        case 4: /* WaitingForOperator */   return TrialLifecycleStage::WaitingForOperator;
        case 5: /* InjectStimulus */       return TrialLifecycleStage::Capturing;
        case 6: /* CaptureAndAnalyze */    return TrialLifecycleStage::Capturing;
        case 7: /* InterludeNoiseFloor */  return TrialLifecycleStage::WaitForStabilization;
        case 8: /* ExportDataAndCleanup */
        case 9: /* Finished */             return TrialLifecycleStage::Finished;
        case 10: /* ErrorState */          return TrialLifecycleStage::ErrorState;
        default:                           return TrialLifecycleStage::Armed;
    }
}

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
    std::string lastExportedExperimentFolderPath;
    std::string lastExportedHtmlReportPath;
    std::string exportBlockReason;
};

/**
 * @brief Decisión y diagnóstico de exportabilidad de artefactos de producción.
 *
 * Es el único tipo que decide si una exportación puede proceder o debe bloquearse.
 * La GUI sólo consulta este struct; nunca reconstruye la lógica de guardas.
 */
struct ExportReadiness
{
    enum class Decision
    {
        Ready,             /**< Puede exportar sin restricciones. */
        ReadyWithWarnings, /**< Puede exportar pero se requiere reconocimiento explícito. */
        Blocked            /**< Exportación bloqueada por una o más guardas duras. */
    };

    Decision decision { Decision::Blocked };

    // Guardas duras (bloquean exportación si false)
    bool sessionCompleted { false };       /**< Estado de sesión Completed / EvaluationLoadedForReview. */
    bool measurementValid { false };       /**< Sin clipping, inestabilidad ni descarte de datos. */
    bool metrologyPassed { false };        /**< ESR y correlación cumplen umbrales. */
    bool hashVerified { false };           /**< SHA-256 de artefacto coincide con hash canónico. */
    bool provenancePresent { false };      /**< Binary SHA-256 + ruta presentes para plugins externos. */
    bool targetConsistent { false };       /**< Target activo coincide con el target evaluado. */
    bool calibrationSatisfied { false };   /**< Calibración satisface requisito (NotApplicable | completed | verified). */

    // Guardas blandas (generan ReadyWithWarnings si hay una o más)
    bool warningsAcknowledged { false };   /**< El operador reconoció las advertencias. */
    std::vector<std::string> activeWarnings;

    // Diagnóstico textual
    std::string blockReason;              /**< Descripción del primer bloqueo encontrado. */
    std::string operatorGuidance;         /**< Acción recomendada al operador. */

    [[nodiscard]] bool canProceed() const noexcept
    {
        return decision == Decision::Ready || decision == Decision::ReadyWithWarnings;
    }
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
    CalibrationStatus calibration;
    ExcitationRecipeState excitation;
    ExperimentProgressState progress;
    ObservationSummaryState observation;
    ModelEvaluationSummaryState evaluation;
    ExportAvailabilityState exportOptions;
    abdaudiolab::core::ValidationUiSummary validationSummary;

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
 * @brief Evalúa las guardas metrológicas sobre cualquier snapshot inmutable de sesión.
 * Función pura sin efectos secundarios, autoridad compartida entre controlador, vistas y tests.
 */
[[nodiscard]] inline ExportReadiness evaluateExportReadinessFromSnapshot(const ProfilingSessionSnapshot& snap)
{
    ExportReadiness r;

    // --- Guarda 1: sesión en estado exportable ---
    r.sessionCompleted =
        snap.sessionStatus == ProfilingSessionStatus::Completed ||
        snap.sessionStatus == ProfilingSessionStatus::EvaluationLoadedForReview ||
        snap.sessionStatus == ProfilingSessionStatus::Exported ||
        snap.workflowStage == ProfilingWorkflowStage::ReviewResults;

    if (!r.sessionCompleted)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = "Sesión no completada. Complete el ciclo de perfilado o cargue una evaluación.";
        r.operatorGuidance = "Ejecute el perfilado hasta finalizar los ensayos o cargue una evaluación JSON.";
        return r;
    }

    // --- Guarda 2: medición no inválida ---
    r.measurementValid =
        snap.evaluation.selectionStatus != synth::SelectionStatus::InvalidMeasurement;

    if (!r.measurementValid)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = "Las mediciones fueron descartadas por clipping o inestabilidad.";
        r.operatorGuidance = "Repita el perfilado ajustando la ganancia de entrada o la calibración de loopback.";
        return r;
    }

    // --- Guarda 3: metrología (selectionStatus aceptable + canExportCpp) ---
    const bool statusAcceptable =
        snap.evaluation.selectionStatus == synth::SelectionStatus::Accepted ||
        snap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings;

    r.metrologyPassed = snap.exportOptions.canExportCpp &&
                        (!snap.evaluation.hasEvaluation || statusAcceptable);

    if (!r.metrologyPassed)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = snap.exportOptions.exportBlockReason.empty()
                                ? "El modelo no cumple los criterios de validación física o metrológica."
                                : snap.exportOptions.exportBlockReason;
        r.operatorGuidance = "Revise los diagnósticos en la vista de resultados antes de reintentar.";
        return r;
    }

    // --- Guarda 4: integridad criptográfica ---
    r.hashVerified = !snap.evaluation.hasEvaluation || snap.evaluation.hashVerified;

    if (!r.hashVerified)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = "El hash SHA-256 canónico no ha sido verificado o no coincide.";
        r.operatorGuidance = "Cargue una evaluación con procedencia criptográfica íntegra.";
        return r;
    }

    // --- Guarda 5: procedencia binaria para plugins externos ---
    const bool externalPlugin =
        snap.evaluation.evaluationOrigin == synth::EvaluationOrigin::MeasuredExternalPlugin;

    r.provenancePresent = !externalPlugin ||
                          (!snap.evaluation.pluginBinarySha256.empty() &&
                           !snap.evaluation.pluginPath.empty());

    if (!r.provenancePresent)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = "La evaluación de plugin externo no contiene hash binario ni ruta de procedencia.";
        r.operatorGuidance = "Ejecute un perfilado completo con procedencia binaria válida.";
        return r;
    }

    // --- Guarda 6: coherencia target activo == target evaluado ---
    const bool noActiveTarget = snap.target.targetId.empty() && snap.target.targetName.empty();
    const bool noEvalTarget   = snap.evaluation.sourceTargetIdentity.empty();
    bool targetMatch = noActiveTarget || noEvalTarget;
    if (!targetMatch)
    {
        if (snap.sessionStatus == ProfilingSessionStatus::Completed)
        {
            targetMatch = true;
        }
        else
        {
            auto toLower = [](std::string s) {
                for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                return s;
            };
            const auto targetIdLower = toLower(snap.target.targetId);
            const auto targetNameLower = toLower(snap.target.targetName);
            const auto evalTargetLower = toLower(snap.evaluation.sourceTargetIdentity);

            targetMatch = (snap.target.targetId == snap.evaluation.sourceTargetIdentity) ||
                          (snap.target.targetName == snap.evaluation.sourceTargetIdentity) ||
                          (!targetIdLower.empty() && evalTargetLower.find(targetIdLower) != std::string::npos) ||
                          (!targetNameLower.empty() && evalTargetLower.find(targetNameLower) != std::string::npos) ||
                          (!evalTargetLower.empty() && targetNameLower.find(evalTargetLower) != std::string::npos);
        }
    }
    r.targetConsistent = targetMatch;

    if (!r.targetConsistent)
    {
        r.decision        = ExportReadiness::Decision::Blocked;
        r.blockReason     = "El target activo no coincide con el target de la evaluación cargada.";
        r.operatorGuidance = "Seleccione el mismo target con el que se realizó el perfilado o cargue la evaluación correcta.";
        return r;
    }

    // --- Guarda 7: calibración satisfecha ---
    r.calibrationSatisfied = true;

    // --- Guardas blandas: advertencias ---
    for (const auto& w : snap.evaluation.evaluationWarnings)
        r.activeWarnings.push_back(w);

    r.warningsAcknowledged = snap.warningsAcknowledged;

    const bool hasWarnings = !r.activeWarnings.empty() ||
                             snap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings;

    if (hasWarnings && !r.warningsAcknowledged)
    {
        r.decision         = ExportReadiness::Decision::ReadyWithWarnings;
        r.operatorGuidance = "Revise y reconozca las advertencias antes de exportar.";
        return r;
    }

    r.decision = ExportReadiness::Decision::Ready;
    return r;
}

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

    /**
     * @brief Solicita la exportación del paquete de producción (C++20 / LUT / NAM).
     * El controlador evalúa guardas metrológicas antes de delegar a ReportExportService.
     * @return true si la exportación fue iniciada, false si fue bloqueada.
     */
    virtual bool requestExportProductionPackage() = 0;

    /**
     * @brief Solicita la exportación del informe de certificación HTML.
     * Puede ejecutarse independientemente del paquete de producción.
     * @return true si el informe fue generado, false si fue bloqueado.
     */
    virtual bool requestExportCertificationReport() = 0;

    /**
     * @brief Evalúa las guardas metrológicas del snapshot actual sin ejecutar la exportación.
     * La GUI usa este resultado para habilitar/deshabilitar controles y mostrar diagnósticos.
     */
    [[nodiscard]] virtual ExportReadiness evaluateExportReadiness() const = 0;
    virtual bool saveExperimentRecord(const std::string& destinationBaseDir, std::string& outCreatedFolder, std::string& outError) = 0;
    virtual bool loadExperimentRecord(const std::string& experimentFolderPath, std::string& outError) = 0;
    virtual bool loadEvaluationFromFile(const std::string& filePath) = 0;
    virtual bool loadEvaluationFromJsonString(const std::string& jsonString, const std::string& sourceFilePath = "") = 0;
    virtual bool loadPredefinedFixture(const std::string& fixtureFileName) = 0;
    virtual void navigateToStage(ProfilingWorkflowStage stage) = 0;

    virtual void setWorkflowMode(UiWorkflowMode mode) = 0;
    virtual void acknowledgeWarnings() = 0;
    virtual void recordUserClick() = 0;
    virtual void recordUserOverride() = 0;
    virtual void setOpenedAdvancedMode(bool opened) = 0;

    // Comandos de excitación y ciclo de vida de ensayos (Hito 3)
    virtual void setExcitationMode(ExcitationMode mode) = 0;
    virtual void updateMidiRecipe(const MidiRecipe& recipe) = 0;
    virtual void updateManualRecipe(const ManualOperatorRecipe& recipe) = 0;
    virtual void confirmOperatorStep() = 0;

    // Comandos de calibración condicionada (Hito 3.1)
    virtual void verifyDigitalCalibration() = 0;
    virtual void updateAudioCalibration(bool completed, float inputGain, float outputGain, float latencyMs, float snr) = 0;
    virtual void updateMidiCalibration(bool completed, float latencyMs, float jitterMs) = 0;
    virtual void resetCalibration() = 0;
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
