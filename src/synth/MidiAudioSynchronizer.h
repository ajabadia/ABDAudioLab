#pragma once

#include <vector>
#include <string>
#include "SynthObservation.h"
#include "MidiExcitationSequence.h"

namespace abdaudiolab::synth
{

/**
 * @brief Calibración base de la cadena de transporte MIDI/Audio.
 */
struct ChainTransportCalibration
{
    bool isCalibrated { false };
    double transportOffsetEstimateMs { 0.0 };
    double transportOffsetUncertaintyMs { 0.0 };
    double calibrationPresetIntrinsicOnsetMs { 0.15 }; // Cota intrínseca del ataque percusivo de calibración
    double onsetDetectorUncertaintyMs { 0.05 };
    int calibrationPassesCount { 0 };
    std::string calibrationNotes;
};

/**
 * @brief Sincronizador de eventos MIDI y audio grabado, con detección de onset basada en función de novedad.
 */
class MidiAudioSynchronizer
{
public:
    MidiAudioSynchronizer() = default;
    ~MidiAudioSynchronizer() = default;

    /**
     * @brief Calcula la función de novedad (Spectral Flux / Pendiente Hilbert) sobre la señal grabada.
     */
    static std::vector<float> computeNoveltyFunction(const std::vector<float>& audioBuffer,
                                                     double sampleRate,
                                                     size_t hopSize = 64);

    /**
     * @brief Estima la muestra de onset del primer transitorio detectable a partir de la función de novedad.
     * 
     * @param novelty Curva de novedad.
     * @param hopSize Tamaño del salto entre muestras de novedad.
     * @param preRollSamples Muestras previas al Note-On programado (para estimar el suelo de ruido/umbral).
     * @param sampleRate Frecuencia de muestreo.
     * @return Índice absoluto de muestra del onset detectado.
     */
    static size_t detectOnsetSample(const std::vector<float>& novelty,
                                    size_t hopSize,
                                    size_t preRollSamples,
                                    double sampleRate);

    /**
     * @brief Calibra el offset de transporte de la cadena mediante repeticiones del preset impulsivo de referencia.
     */
    static ChainTransportCalibration calibrateTransportOffset(const std::vector<std::vector<float>>& calibrationTakes,
                                                             double sampleRate,
                                                             size_t scheduledNoteOnSample,
                                                             double intrinsicOnsetMs = 0.15);

    /**
     * @brief Extrae las métricas de sincronización y ataque neto para una toma dada.
     */
    static TimingMetrics synchronizeTrial(const std::vector<float>& audioBuffer,
                                          double sampleRate,
                                          const TimedMidiEvent& noteOnEvent,
                                          const ChainTransportCalibration& calibration,
                                          double rawAttackMs);
};

} // namespace abdaudiolab::synth
