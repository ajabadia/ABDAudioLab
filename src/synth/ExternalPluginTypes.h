#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace abdaudiolab::synth
{

/**
 * @brief Modo de aislamiento de la ejecución del plugin externo.
 */
enum class PluginExecutionIsolation
{
    InProcessExternalBinary,   /**< Binario cargado en el espacio de memoria del host vía AudioPluginFormatManager. */
    OutOfProcessExternalBinary /**< Proceso host aislado separado con comunicación IPC (fases avanzadas). */
};

/**
 * @brief Entrada de archivo individual dentro del bundle VST3.
 */
struct PluginBundleFileEntry
{
    std::string relativePath;
    uint64_t fileSize { 0 };
    std::string fileSha256;
};

/**
 * @brief Identidad metrológica e inmutable de un plugin externo en disco.
 */
struct PluginIdentity
{
    std::string absolutePath;
    std::string binaryHash;             /**< SHA-256 del binario DLL/so principal. */
    std::string bundleHash;             /**< SHA-256 canónico del contenido completo del bundle. */
    std::vector<PluginBundleFileEntry> bundleFiles;

    std::string pluginUid;
    std::string pluginName;
    std::string manufacturer;
    std::string version;
    std::string format { "VST3" };
    std::string architecture;
    PluginExecutionIsolation executionIsolation { PluginExecutionIsolation::InProcessExternalBinary };
};

/**
 * @brief Rol funcional del parámetro para separar controles musicales de infraestructura de pruebas.
 */
enum class ParameterRole
{
    AudioControl,       /**< Control de síntesis musical o procesado acústico directo. */
    TestInfrastructure, /**< Parámetro técnico de test (latencia forzada, modo de transporte, suavizado). */
    PresetManagement,   /**< Selección de programa o gestión de estado. */
    Unknown
};

/**
 * @brief Evidencia del origen de la categorización semántica de un parámetro.
 */
enum class SemanticEvidence
{
    DeclaredByPlugin,    /**< Metadatos nativos explícitos proporcionados por el SDK o descriptor. */
    InferredFromName,    /**< Inferencia heurística basada en nombres estándar ("cutoff", "attack"). */
    InferredFromBehavior,/**< Inferencia a posteriori derivada de la observación acústica. */
    Unknown
};

/**
 * @brief Estado de certeza semántica del parámetro.
 */
enum class SemanticStatus
{
    Declared,   /**< Semántica explícitamente declarada y verificada. */
    Inferred,   /**< Semántica conjeturada sujeta a validación experimental. */
    Unverified, /**< Parámetro descubierto sin semántica conocida. */
    Ambiguous   /**< Parámetro con nombres contradictorios o genéricos ("Macro 1", "Shape"). */
};

/**
 * @brief Evidencia metrológica de suavizado en parámetros.
 */
struct SmoothingEvidence
{
    bool declaredSmoothing { false }; /**< Declarado en metadatos del plugin. */
    bool observedSmoothing { false }; /**< Medido experimentalmente en la respuesta acústica. */
    bool smoothingKnown { false };    /**< Si existe certeza sobre el comportamiento de filtrado. */
    double effectiveSmoothingTimeMs { 0.0 };
};

/**
 * @brief Trazabilidad de precisión temporal y retardo de eventos.
 */
struct TimingPrecisionTrace
{
    int64_t requestedAbsoluteSample { 0 };
    int requestedBlockIndex { 0 };
    int requestedSampleOffset { 0 };
    int64_t observedOnsetSample { 0 };
    int64_t measuredDeltaSamples { 0 };
    double onsetUncertaintySamples { 0.0 };
};

/**
 * @brief Desglose formal de latencias del target sin asunciones previas.
 */
struct LatencyBreakdown
{
    double declaredLatencySamples { 0.0 };           /**< Retorno directo de getLatencySamples(). */
    double measuredEventToOutputSamples { 0.0 };     /**< Muestras medidas entre NoteOn y primer audio no nulo. */
    double intrinsicAttackSamples { 0.0 };           /**< Duración del ataque hasta el pico acústico. */
    double effectiveAutomationLatencySamples { 0.0 };/**< Retardo efectivo observado en cambios de control. */
};

} // namespace abdaudiolab::synth
