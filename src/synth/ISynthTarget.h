#pragma once

#include <vector>
#include <string>
#include <memory>
#include "SynthPresetState.h"
#include "MidiExcitationSequence.h"

namespace abdaudiolab::synth
{

/**
 * @brief Especificación de procesamiento por bloques de audio.
 */
struct ProcessingSpec
{
    double sampleRate { 96000.0 };
    int blockSize { 256 };
    int numChannels { 2 };
};

/**
 * @brief Información y metrología temporal del target de síntesis.
 */
struct TargetTimingInfo
{
    bool isPhysicalHardware { false };
    bool isDirectPlugin { false };
    double declaredLatencySamples { 0.0 };
    double measuredTransportLatencyMs { 0.0 };
    double timingJitterMs { 0.0 };
    std::string timingDescription;
};

/**
 * @brief Interfaz canónica común para adquisición de audio y control de sintetizadores.
 * 
 * Desacopla por completo el análisis DSP, la estadística y los informes
 * de la procedencia del sonido:
 * - SyntheticSynthTarget: Fixture matemático determinista.
 * - PluginSynthTarget: Hosting VST/VST3 directo en RAM con sampleOffset preciso por bloque.
 * - HardwareSynthTarget: Despacho MIDI físico + retorno ADC.
 */
class ISynthTarget
{
public:
    virtual ~ISynthTarget() = default;

    /**
     * @brief Aplica el preset / estado al sintetizador o plugin.
     */
    virtual bool loadState(const SynthPresetState& state) = 0;

    /**
     * @brief Verifica si el estado se aplicó correctamente.
     */
    [[nodiscard]] virtual StateAppliedStatus verifyState() const = 0;

    /**
     * @brief Prepara el motor con la frecuencia de muestreo y tamaño de bloque.
     */
    virtual void prepare(const ProcessingSpec& spec) = 0;

    /**
     * @brief Resetea las voces, fases y osciladores a reposo antes de una nueva toma.
     */
    virtual void resetState() = 0;

    /**
     * @brief Renderiza una toma completa dada una secuencia de excitación MIDI.
     */
    virtual void render(const MidiExcitationSequence& sequence,
                        std::vector<float>& destinationAudio,
                        int repetitionIndex = 0) = 0;

    /**
     * @brief Obtiene la información de latencia y temporización del target.
     */
    [[nodiscard]] virtual TargetTimingInfo timingInfo() const = 0;

    /**
     * @brief Resultado enriquecido de transferencia de estado binario.
     * PRECONDICIÓN: Invocable únicamente desde hilos de control / auditoría.
     * NUNCA invocable desde el callback de procesamiento de audio en tiempo real.
     */
    struct StateTransferResult
    {
        bool supported { false };
        bool succeeded { false };
        std::string errorCode;
        std::size_t byteCount { 0 };
        std::string stateDataHash; // SHA-256 de los bytes brutos exactos devueltos/aplicados
    };

    /**
     * @brief Obtiene el volcado binario crudo del estado completo del target.
     * Precondición: Control thread only.
     */
    virtual StateTransferResult getState(std::vector<uint8_t>& /*stateData*/) const
    {
        return StateTransferResult{ false, false, "NOT_SUPPORTED", 0, "" };
    }

    /**
     * @brief Restaura el volcado binario crudo del estado completo del target.
     * Precondición: Control thread only.
     */
    virtual StateTransferResult setState(const std::vector<uint8_t>& /*stateData*/)
    {
        return StateTransferResult{ false, false, "NOT_SUPPORTED", 0, "" };
    }

    /**
     * @brief Informa si el target admite serialización/restauración de estado binario.
     */
    [[nodiscard]] virtual bool supportsBinaryState() const
    {
        return false;
    }
};

} // namespace abdaudiolab::synth
