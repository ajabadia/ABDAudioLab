#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "Sha256.h"
#include "measurement/MeasurementContracts.h"

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

/**
 * @brief Descriptor individual de parámetro descubierto e introspeccionado.
 */
struct InspectedParameterInfo
{
    std::string id;
    std::string title;
    std::string unit;
    float normalizedValue { 0.0f };
    float plainValue { 0.0f };
    int stepCount { 0 };
    bool isDiscrete { false };
    bool isAutomatable { true };
    bool isMetaParameter { false };
};

/**
 * @brief Descriptor de bus de audio o MIDI expuesto por el componente.
 */
struct InspectedBusInfo
{
    std::string name;
    bool isInput { false };
    int defaultChannelCount { 0 };
};

/**
 * @brief Registro de introspección formal de módulo y fábrica de plugins VST3.
 */
struct InspectedPluginModule
{
    std::string canonicalPath;
    std::string binarySha256;
    std::string vendor;
    std::string version;
    std::string architecture;
    std::vector<std::string> componentUids;
    std::vector<std::string> componentNames;
    std::string selectedUid;
    std::vector<InspectedBusInfo> buses;
    std::vector<InspectedParameterInfo> parameters;
    int parameterCount { 0 };
};

/**
 * @brief Telemetría de ejecución de render por bloques en hosting VST3 (Fase 20.11 T2.4).
 */
struct RenderExecutionTelemetry
{
    int64_t totalSamplesRendered { 0 };
    int blocksProcessed { 0 };
    int underruns { 0 };
    int overruns { 0 };
    double pluginLatencySamples { 0.0 };
    double hostLatencySamples { 0.0 };
    double totalRenderTimeMs { 0.0 };
    bool bitExactDeterministic { true };
};

/**
 * @brief Nivel de equivalencia metrológica entre volcados de estado binario (Fase 20.11 T2.3).
 */
enum class StateEquivalence
{
    BitExact,               /**< Byte a byte idéntico, mismo hash SHA-256 canónico. */
    SemanticallyEquivalent, /**< Parámetros y controladores restaurados de forma idéntica, pero representación binaria reordenada o con relleno normalizado. */
    NotEquivalent           /**< Discrepancia acústica o paramétrica medible. */
};

inline const char* stateEquivalenceToString(StateEquivalence eq) noexcept
{
    switch (eq)
    {
        case StateEquivalence::BitExact: return "bit_exact";
        case StateEquivalence::SemanticallyEquivalent: return "semantically_equivalent";
        case StateEquivalence::NotEquivalent: return "not_equivalent";
    }
    return "unknown";
}

/**
 * @brief Fixture mínimo de preset/estado binario controlado para pruebas y metrología reproducible (Fase 20.11 T2.3).
 */
struct ControlledPresetFixture
{
    std::string name;
    std::string version { "1.0.0" };
    std::vector<uint8_t> presetBytes;
    std::string sha256;
    int nominalMidiNote { 60 };
    int nominalMidiVelocity { 100 };
    abdaudiolab::measurement::MeasurementExecutionDomain targetDomain {
        abdaudiolab::measurement::MeasurementExecutionDomain::Vst3OfflineDigital
    };

    [[nodiscard]] bool verifyFixity() const
    {
        if (presetBytes.empty())
            return false;
        return sha256 == Sha256::computeHex(presetBytes.data(), presetBytes.size());
    }

    static ControlledPresetFixture create(const std::string& presetName,
                                          const std::vector<uint8_t>& data,
                                          int note = 60,
                                          int velocity = 100,
                                          abdaudiolab::measurement::MeasurementExecutionDomain domain =
                                              abdaudiolab::measurement::MeasurementExecutionDomain::Vst3OfflineDigital)
    {
        ControlledPresetFixture fixture;
        fixture.name = presetName;
        fixture.version = "1.0.0";
        fixture.presetBytes = data;
        fixture.sha256 = Sha256::computeHex(data.data(), data.size());
        fixture.nominalMidiNote = note;
        fixture.nominalMidiVelocity = velocity;
        fixture.targetDomain = domain;
        return fixture;
    }
};

} // namespace abdaudiolab::synth

