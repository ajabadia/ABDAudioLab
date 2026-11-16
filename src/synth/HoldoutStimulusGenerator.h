#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <numbers>
#include <cstddef>

namespace abdaudiolab::synth
{

/**
 * @struct StimulusAcousticProperties
 * @brief Propiedades metrológicas descriptivas de una señal de estímulo generada.
 */
struct StimulusAcousticProperties
{
    double peakAmplitude { 0.0 };
    double peakDbfs { -120.0 };
    double rmsAmplitude { 0.0 };
    double rmsDbfs { -120.0 };
    double crestFactor { 0.0 };
    double crestFactorDb { 0.0 };
    size_t sampleCount { 0 };
    double durationSeconds { 0.0 };
};

/**
 * @class HoldoutStimulusGenerator
 * @brief Generador determinista de estímulos acústicos reservados para validación ciega fuera de muestra.
 *
 * Todas las señales son sintetizadas de forma determinista (fase reproducible sin dispersión estocástica),
 * garantizando la idoneidad metrológica para pruebas de repetibilidad y comparación de modelos.
 */
class HoldoutStimulusGenerator
{
public:
    /**
     * @brief Sub-batería 1: Escalas dinámicas de amplitud en 5 niveles discretos.
     * Niveles: -30 dBFS, -20 dBFS, -12 dBFS, -6 dBFS y -3 dBFS.
     * Emplea transiciones de rampa cosenoidal de Hann para evitar clics y dispersión espectral espuria.
     *
     * @param sampleRate Frecuencia de muestreo (Hz).
     * @param fundamentalHz Frecuencia del tono senoidal base (Hz, nominal 440.0 Hz).
     * @param stepDurationSec Duración de cada escalón (segundos, nominal 0.08 s).
     * @param transitionSec Duración de la ventana de transición suave entre niveles (segundos).
     */
    [[nodiscard]] static std::vector<float> generateDynamicAmplitudeSteps(double sampleRate = 96000.0,
                                                                         double fundamentalHz = 440.0,
                                                                         double stepDurationSec = 0.08,
                                                                         double transitionSec = 0.005);

    /**
     * @brief Sub-batería 2: Ráfagas de transitorios rápidos seguidos de reposo (Burst & Ringing).
     * Permite evaluar la respuesta al escalón, sobreoscilaciones (*overshoot*) y tiempo de asentamiento.
     *
     * @param sampleRate Frecuencia de muestreo (Hz).
     * @param burstCount Número de ráfagas periódicas.
     * @param burstDurationSec Duración de la excitación activa (nominal 0.025 s).
     * @param restDurationSec Duración del silencio de reposo (nominal 0.075 s).
     * @param carrierHz Frecuencia de la portadora de la ráfaga (nominal 880.0 Hz).
     */
    [[nodiscard]] static std::vector<float> generateTransientBursts(double sampleRate = 96000.0,
                                                                    int burstCount = 4,
                                                                    double burstDurationSec = 0.025,
                                                                    double restDurationSec = 0.075,
                                                                    double carrierHz = 880.0);

    /**
     * @brief Sub-batería 3: Multitono armónicamente denso (Multitone IMD Probe).
     * Suma de frecuencias primas/no conmensurables con distribución de fases de Newman/Schroeder
     * para minimizar el factor de cresta y maximizar la excitación de productos de intermodulación.
     *
     * @param sampleRate Frecuencia de muestreo (Hz).
     * @param durationSec Duración total de la señal multitono (segundos).
     * @param frequencies Lista de frecuencias componentes (Hz).
     * @param targetPeakDbfs Nivel de pico objetivo (nominal -3.0 dBFS).
     */
    [[nodiscard]] static std::vector<float> generateDenseMultitone(double sampleRate = 96000.0,
                                                                  double durationSec = 0.25,
                                                                  const std::vector<double>& frequencies = { 250.0, 550.0, 1200.0, 3100.0 },
                                                                  double targetPeakDbfs = -3.0);

    /**
     * @brief Sub-batería 4: Señal pseudo-musical representativa con envolvente de ataque percusivo y decaimiento.
     * Simula un fragmento de instrumento real (acorde/arpegio dinámico) con múltiples armónicos y relajación natural.
     *
     * @param sampleRate Frecuencia de muestreo (Hz).
     * @param durationSec Duración de la secuencia musical (segundos).
     * @param rootHz Frecuencia fundamental base del acorde (nominal 220.0 Hz).
     */
    [[nodiscard]] static std::vector<float> generatePseudoMusicalSequence(double sampleRate = 96000.0,
                                                                         double durationSec = 0.40,
                                                                         double rootHz = 220.0);

    /**
     * @brief Analiza y extrae las propiedades acústicas y metrológicas de un búfer de audio.
     */
    [[nodiscard]] static StimulusAcousticProperties analyzeAcousticProperties(const std::vector<float>& buffer,
                                                                              double sampleRate = 96000.0);
};

} // namespace abdaudiolab::synth
