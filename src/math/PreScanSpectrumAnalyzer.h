#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include "LabAnalyticEngine.h"

namespace abdaudiolab::math
{

/**
 * @class PreScanSpectrumAnalyzer
 * @brief Windowed FFT and dynamic THD/Peak analyzer for continuous parameter pre-scans.
 */
class PreScanSpectrumAnalyzer
{
public:
    /**
     * Analiza un barrido continuo de ruido blanco a través de un filtro con resonancia.
     * Rastrea el pico máximo espectral en Hz para correlacionarlo con el control MIDI.
     */
    static PreScanResult analyzeFilterNoiseSweep (const std::vector<float>& buffer, 
                                                  double sampleRate, 
                                                  float startMidiVal = 0.0f, 
                                                  float endMidiVal = 127.0f, 
                                                  int fftOrder = 11, 
                                                  float windowHopMs = 100.0f);

    /**
     * Analiza un barrido continuo de ganancia con un tono fundamental fijo.
     * Calcula la distorsión armónica total (THD%) instantánea de los armónicos H2 a H5.
     */
    static PreScanResult analyzeSaturationToneSweep (const std::vector<float>& buffer, 
                                                     double sampleRate, 
                                                     float fundamentalHz = 1000.0f, 
                                                     float startMidiVal = 0.0f, 
                                                     float endMidiVal = 127.0f, 
                                                     int fftOrder = 11, 
                                                     float windowHopMs = 100.0f);

    /**
     * Función auxiliar pura para extraer THD% de una ventana de muestras dada.
     */
    static float calculateInstantaneousThd (const float* windowSamples, 
                                            int fftSize, 
                                            double sampleRate, 
                                            float fundamentalHz);
};

} // namespace abdaudiolab::math
