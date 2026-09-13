#pragma once

#include <vector>
#include <cstdint>
#include "SynthObservation.h"

namespace abdaudiolab::synth
{

/**
 * @brief Analizador de envolvente ADSR guiado por eventos reales de Note-On / Note-Off.
 */
class SynthEnvelopeAnalyzer
{
public:
    SynthEnvelopeAnalyzer() = default;
    ~SynthEnvelopeAnalyzer() = default;

    /**
     * @brief Extrae los parámetros de envolvente de una toma individual teniendo en cuenta la duración real de la compuerta.
     * 
     * @param audioBuffer Señal de audio capturada.
     * @param sampleRate Frecuencia de muestreo (Hz).
     * @param noteOnSample Muestra donde se programó el Note-On.
     * @param noteOffSample Muestra donde se programó el Note-Off.
     * @param detectedOnsetSample Muestra detectada del primer transitorio acústico.
     * @return EnvelopeMetrics con los 4 parámetros y su estado formal de observabilidad.
     */
    static EnvelopeMetrics analyzeEnvelope(const std::vector<float>& audioBuffer,
                                           double sampleRate,
                                           size_t noteOnSample,
                                           size_t noteOffSample,
                                           size_t detectedOnsetSample);
};

} // namespace abdaudiolab::synth
