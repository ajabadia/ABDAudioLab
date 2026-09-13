#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include "ISynthTarget.h"
#include "MidiExcitationSequence.h"
#include "SynthPresetState.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Política de tolerancia y umbrales metrológicos para la auditoría previa de targets.
 */
struct TargetAuditPolicy
{
    std::string auditPolicyId { "audit_policy_canonical" };
    std::string auditPolicyVersion { "1.0.0" };

    double functionalCorrelationMin { 0.999 };    /**< Coeficiente Pearson mínimo para equivalencia funcional. */
    double functionalEsrMaxDb { -60.0 };           /**< Error-to-Signal Ratio máximo en dB para equivalencia funcional. */
    double pitchDifferenceMaxCents { 0.10 };       /**< Desviación tonal máxima tolerable en cents. */
    double generationDeltaRmsMinDb { 12.0 };       /**< Diferencia mínima entre RMS en nota y en silencio para generadores. */
    double confidenceLevel { 0.95 };               /**< Nivel de confianza estadística. */
    double interNoteSettlingShortSec { 0.80 };     /**< Reposo corto (post-envolvente) para prueba de transitorios residuales. */
    double interNoteSettlingLongSec { 2.00 };      /**< Reposo largo para confirmar desvanecimiento de colas de efecto. */
};

/**
 * @brief Nivel de equivalencia acústica entre dos tomas renderizadas.
 */
enum class AudioEquivalenceLevel
{
    ByteIdentical,            /**< Muestra a muestra y byte a byte idéntico (SHA-256 idéntico, RMSE = 0). */
    FunctionallyEquivalent,   /**< Diferencias numéricas o sub-muestras despreciables (ESR < -60 dB, rho > 0.999). */
    StatisticallyEquivalent,  /**< Misma distribución espectral/RMS pero con desfase de oscilador libre. */
    Divergent                 /**< Comportamiento físico o audiblemente distinto. */
};

[[nodiscard]] inline std::string audioEquivalenceToString(AudioEquivalenceLevel level)
{
    switch (level)
    {
        case AudioEquivalenceLevel::ByteIdentical:           return "ByteIdentical";
        case AudioEquivalenceLevel::FunctionallyEquivalent:  return "FunctionallyEquivalent";
        case AudioEquivalenceLevel::StatisticallyEquivalent: return "StatisticallyEquivalent";
        case AudioEquivalenceLevel::Divergent:               return "Divergent";
        default:                                             return "Unknown";
    }
}

/**
 * @brief Capacidad de reinicialización del estado y fases del target.
 */
enum class ResetCapability
{
    Resettable,
    PartiallyResettable,
    NotResettable,
    Unknown
};

[[nodiscard]] inline std::string resetCapabilityToString(ResetCapability cap)
{
    switch (cap)
    {
        case ResetCapability::Resettable:          return "Resettable";
        case ResetCapability::PartiallyResettable: return "PartiallyResettable";
        case ResetCapability::NotResettable:       return "NotResettable";
        default:                                   return "Unknown";
    }
}

/**
 * @brief Clasificación de determinismo del target.
 */
enum class DeterminismClass
{
    Deterministic,             /**< Repetible byte a byte en condiciones idénticas. */
    DeterministicAfterReset,   /**< Fase libre en curso continuo, pero determinista tras resetState(). */
    StochasticWithSeed,        /**< Variación aleatoria pero controlable por semilla determinista. */
    StochasticUnseeded,        /**< Variación estocástica continua no controlable (ruido analógico, drift). */
    Stateful,                  /**< No determinista debido a acumulación de memoria interna. */
    Unknown
};

[[nodiscard]] inline std::string determinismClassToString(DeterminismClass det)
{
    switch (det)
    {
        case DeterminismClass::Deterministic:           return "Deterministic";
        case DeterminismClass::DeterministicAfterReset: return "DeterministicAfterReset";
        case DeterminismClass::StochasticWithSeed:      return "StochasticWithSeed";
        case DeterminismClass::StochasticUnseeded:      return "StochasticUnseeded";
        case DeterminismClass::Stateful:                return "Stateful";
        default:                                        return "Unknown";
    }
}

/**
 * @brief Clasificación de persistencia de estado entre notas consecutivas.
 */
enum class InterNoteState
{
    Stateless,
    StatefulBehaviorDetected
};

[[nodiscard]] inline std::string interNoteStateToString(InterNoteState state)
{
    return (state == InterNoteState::Stateless) ? "Stateless" : "StatefulBehaviorDetected";
}

/**
 * @brief Causa física o algorítmica candidata del comportamiento persistente.
 */
enum class ResidualCause
{
    None,
    OscillatorPhase,     /**< Oscilador de funcionamiento libre (free-running phase). */
    EnvelopeState,       /**< Etapa de release no concluida o re-disparo de envolvente legato. */
    FilterState,         /**< Resonancia acumulada en integradores del filtro TPT/ZDF. */
    EffectTail,          /**< Cola residual de retardo, reverb o chorus. */
    VoiceAllocator,      /**< Rotación cíclica de voces polifónicas con calibraciones distintas. */
    Unknown
};

[[nodiscard]] inline std::string residualCauseToString(ResidualCause cause)
{
    switch (cause)
    {
        case ResidualCause::None:            return "None";
        case ResidualCause::OscillatorPhase: return "OscillatorPhase";
        case ResidualCause::EnvelopeState:   return "EnvelopeState";
        case ResidualCause::FilterState:     return "FilterState";
        case ResidualCause::EffectTail:      return "EffectTail";
        case ResidualCause::VoiceAllocator:  return "VoiceAllocator";
        default:                             return "Unknown";
    }
}

/**
 * @brief Clasificación de generación autónoma de audio y reactividad a notas.
 */
enum class GenerationClass
{
    GenerativeAudioObserved,            /**< Produce sonido con ataque/decay acoplado a NoteOn. */
    AudioObservedButNotNoteResponsive,  /**< Produce audio constante (zumbido/ruido) que no responde a notas. */
    SilentOutput,                       /**< Salida nula o nivel de reposo absoluto. */
    UnsupportedForMidiRecipe,           /**< Target no apto para recetas de notas MIDI (efecto puro sin sinte). */
    Inconclusive
};

[[nodiscard]] inline std::string generationClassToString(GenerationClass gen)
{
    switch (gen)
    {
        case GenerationClass::GenerativeAudioObserved:           return "GenerativeAudioObserved";
        case GenerationClass::AudioObservedButNotNoteResponsive: return "AudioObservedButNotNoteResponsive";
        case GenerationClass::SilentOutput:                      return "SilentOutput";
        case GenerationClass::UnsupportedForMidiRecipe:          return "UnsupportedForMidiRecipe";
        default:                                                 return "Inconclusive";
    }
}

/**
 * @brief Causa precisa de advertencia o fallo en la prueba de State Round-Trip.
 */
enum class RoundTripFailureCause
{
    None,
    Unsupported,             /**< El target declara no soportar volcado/restauración binaria. */
    BinaryStateMismatch,     /**< Los bytes restaurados difieren de los bytes iniciales. */
    ParameterMismatch,       /**< Los parámetros normalizados efectivos cambiaron tras restore. */
    BehavioralMismatch,      /**< El audio renderizado difiere acústicamente tras restaurar el estado. */
    NonDeterministicRender,  /**< La disparidad procede de aleatoriedad del target, no del estado. */
    InvalidStateData,        /**< Datos de estado corruptos o vacíos. */
    PluginException,
    Unknown
};

[[nodiscard]] inline std::string roundTripFailureCauseToString(RoundTripFailureCause cause)
{
    switch (cause)
    {
        case RoundTripFailureCause::None:                   return "None";
        case RoundTripFailureCause::Unsupported:            return "Unsupported";
        case RoundTripFailureCause::BinaryStateMismatch:    return "BinaryStateMismatch";
        case RoundTripFailureCause::ParameterMismatch:      return "ParameterMismatch";
        case RoundTripFailureCause::BehavioralMismatch:     return "BehavioralMismatch";
        case RoundTripFailureCause::NonDeterministicRender: return "NonDeterministicRender";
        case RoundTripFailureCause::InvalidStateData:       return "InvalidStateData";
        case RoundTripFailureCause::PluginException:        return "PluginException";
        default:                                            return "Unknown";
    }
}

/**
 * @brief Estado general de la prueba de State Round-Trip.
 */
enum class StateRoundTripStatus
{
    Passed,
    StateRoundTripWarning,
    Unsupported
};

[[nodiscard]] inline std::string stateRoundTripStatusToString(StateRoundTripStatus status)
{
    switch (status)
    {
        case StateRoundTripStatus::Passed:                return "Passed";
        case StateRoundTripStatus::StateRoundTripWarning: return "StateRoundTripWarning";
        case StateRoundTripStatus::Unsupported:           return "Unsupported";
        default:                                          return "Unknown";
    }
}

/**
 * @brief Dictamen de aprobación para la fase de excitación de parámetros.
 */
enum class ApprovalStatus
{
    Approved,               /**< Determinista, reiniciable y de generación autónoma confirmada. */
    ApprovedWithWarnings,   /**< Requiere instrucciones operativas (reset forzado, settling extendido). */
    Rejected,               /**< No determinista, corruptor de estado o inestable para mediciones. */
    Unsupported             /**< Target incompatible con la receta (efecto mudo, sin MIDI). */
};

[[nodiscard]] inline std::string approvalStatusToString(ApprovalStatus status)
{
    switch (status)
    {
        case ApprovalStatus::Approved:             return "Approved";
        case ApprovalStatus::ApprovedWithWarnings: return "ApprovedWithWarnings";
        case ApprovalStatus::Rejected:             return "Rejected";
        case ApprovalStatus::Unsupported:          return "Unsupported";
        default:                                   return "Unknown";
    }
}

/**
 * @brief Evidencia cuantitativa de determinismo y repetibilidad.
 */
struct DeterminismEvidence
{
    AudioEquivalenceLevel run1vs2Equivalence { AudioEquivalenceLevel::Divergent };
    double run1vs2Rmse { 0.0 };
    double run1vs2Correlation { 0.0 };
    double run1vs2EsrDb { 0.0 };
    std::string run1Sha256;
    std::string run2Sha256;
    bool byteIdentical { false };
    bool functionallyEquivalent { false };
    bool statisticallyEquivalent { false };
    bool resetEliminatesDrift { false };
};

/**
 * @brief Evidencia cuantitativa de persistencia de estado entre notas.
 */
struct StatefulnessEvidence
{
    bool crossOrderDependencyDetected { false };
    ResidualCause detectedCause { ResidualCause::None };
    double tailRmsDb { -120.0 };
    double interNoteCorrelationShort { 1.0 };
    double interNoteCorrelationLong { 1.0 };
    double recommendedSettlingTimeSec { 0.05 };
};

/**
 * @brief Evidencia cuantitativa de generación autónoma y respuesta a notas.
 */
struct GenerationEvidence
{
    double restRmsDb { -120.0 };
    double activeRmsDb { -120.0 };
    double deltaRmsDb { 0.0 };
    bool noteResponsive { false };
    bool onsetDetected { false };
    bool pitchDetected { false };
    bool audioOutputPresent { false };
};

/**
 * @brief Evidencia cuantitativa del ciclo State Round-Trip (3 capas).
 */
struct RoundTripEvidence
{
    bool binaryIdentical { false };
    bool parameterIdentical { false };
    bool behaviorIdentical { false };
    std::string initialBinaryHash;
    std::string restoredBinaryHash;
    double audioComparisonRmse { 0.0 };
    RoundTripFailureCause failureCause { RoundTripFailureCause::None };
    std::string failureDetails;
};

/**
 * @brief Instrucciones operativas generadas por el auditor para el motor de excitación.
 */
struct OperationalInstructions
{
    bool resetBeforeEachTrial { true };
    double recommendedSettlingTimeMs { 50.0 };
    bool useStatisticalAveraging { false };
    bool exactHashComparisonPermitted { true };
    std::string operationalGuidance;
};

/**
 * @brief Reporte exhaustivo de diagnóstico de un target auditado.
 */
struct TargetAuditReport
{
    std::string auditProtocolId;
    std::string auditProtocolVersion;
    TargetAuditPolicy effectivePolicy;

    double sampleRate { 96000.0 };
    int blockSize { 256 };
    std::string eventSequenceHash;

    DeterminismClass determinism { DeterminismClass::Unknown };
    ResetCapability resetCapability { ResetCapability::Unknown };
    InterNoteState interNoteState { InterNoteState::Stateless };
    GenerationClass generation { GenerationClass::Inconclusive };
    StateRoundTripStatus stateRoundTrip { StateRoundTripStatus::Unsupported };
    ApprovalStatus approvalStatus { ApprovalStatus::Rejected };
    bool isApprovedForParameterExcitation { false };

    DeterminismEvidence determinismEvidence;
    StatefulnessEvidence statefulnessEvidence;
    GenerationEvidence generationEvidence;
    RoundTripEvidence roundTripEvidence;

    OperationalInstructions operationalInstructions;
    std::vector<std::string> warnings;
    std::vector<std::string> limitations;
    std::string summaryMessage;
};

/**
 * @brief Auditor diagnóstico previo de targets para perfilado e identificación de sistemas.
 */
class TargetAuditor
{
public:
    explicit TargetAuditor(TargetAuditPolicy policy = TargetAuditPolicy{})
        : policy_(std::move(policy))
    {
    }

    [[nodiscard]] const TargetAuditPolicy& getPolicy() const noexcept { return policy_; }
    void setPolicy(const TargetAuditPolicy& policy) noexcept { policy_ = policy; }

    /**
     * @brief Ejecuta el protocolo completo de auditoría previa sobre el target especificado.
     * Precondición: Hilo de control / auditoría (nunca desde callback de tiempo real).
     */
    [[nodiscard]] TargetAuditReport auditTarget(ISynthTarget& target,
                                                const ProcessingSpec& spec = ProcessingSpec{});

private:
    TargetAuditPolicy policy_;

    // Subrutinas de análisis de señales
    [[nodiscard]] static double computeRms(const float* data, size_t count) noexcept;
    [[nodiscard]] static double computeRmsDb(const float* data, size_t count) noexcept;
    [[nodiscard]] static double computeRmse(const std::vector<float>& a, const std::vector<float>& b) noexcept;
    [[nodiscard]] static double computeCorrelation(const std::vector<float>& a, const std::vector<float>& b) noexcept;
    [[nodiscard]] static double computeEsrDb(const std::vector<float>& clean, const std::vector<float>& test) noexcept;
    [[nodiscard]] static AudioEquivalenceLevel evaluateEquivalence(const std::vector<float>& a,
                                                                   const std::vector<float>& b,
                                                                   const TargetAuditPolicy& policy,
                                                                   double& outRmse,
                                                                   double& outCorr,
                                                                   double& outEsrDb) noexcept;

    // Métodos de generación de secuencias patrón
    [[nodiscard]] static MidiExcitationSequence makeSingleNoteSequence(int noteNumber,
                                                                      float velocity,
                                                                      double gateDurationSec,
                                                                      double postSilenceSec,
                                                                      double sampleRate);
    [[nodiscard]] static MidiExcitationSequence makeDoubleNoteSequence(int note1,
                                                                      int note2,
                                                                      float velocity,
                                                                      double gateDurationSec,
                                                                      double interSilenceSec,
                                                                      double postSilenceSec,
                                                                      double sampleRate);
};

} // namespace abdaudiolab::synth
