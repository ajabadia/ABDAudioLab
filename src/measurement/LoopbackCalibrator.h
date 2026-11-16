/**
 * @file LoopbackCalibrator.h
 * @brief Calibrador metrológico de loopback analógico (DAC -> cable de referencia -> ADC) para Fase 20.11 T4.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class LoopbackCalibrator
 * @brief Calibrador de auditoría metrológica offline (Fase 20.11 T4) para registros `LoopbackCalibrationRecord`.
 *
 * Nota Arquitectónica (Frontera de Dominio):
 * - `abdaudiolab::measurement::LoopbackCalibrator` (alias `LoopbackAuditCalibrator`) produce registros
 *   estructurados de auditoría (`LoopbackCalibrationRecord`) con segregación estricta de artefactos
 *   (`loopback_reference`, `dut_plus_chain`, `compensated_result`).
 * - Para la calibración en vivo del motor DSP, síntesis de filtros FIR de fase mínima (`synthesizeMinimumPhaseFir`)
 *   y perfiles `.abdloop` con suavizado psicoacústico ERB, véase `abdaudiolab::math::LoopbackCalibrator`
 *   en `src/math/LoopbackCalibrator.h`.
 *
 * Criterio Metrológico Fundamental:
 * - La latencia se documenta como latencia total de ida y vuelta (round-trip), sin
 *   descomposiciones especulativas ni asignaciones artificiales.
 * - Los artefactos quedan segregados en 3 capas independientes:
 *   1. loopback_reference (tarjeta y cable sin DUT)
 *   2. dut_plus_chain (cadena completa con el DUT insertado, señal cruda inmutable)
 *   3. compensated_result (resultado corregido matemáticamente sin sobreescribir el crudo).
 */
class LoopbackCalibrator
{
public:
    /**
     * @brief Genera un estímulo canónico de calibración con lead-in de silencio y log-sweep.
     * @param sampleRate Frecuencia de muestreo en Hz.
     * @param sweepDurationSec Duración del barrido logarítmico en segundos.
     * @param leadInSilenceSec Silencio previo para evaluación metrológica de piso de ruido.
     * @param levelDbfs Nivel nominal del estímulo en dBFS.
     * @return Vector con muestras flotantes normalizadas.
     */
    static std::vector<float> generateCalibrationStimulus(double sampleRate,
                                                          double sweepDurationSec = 1.0,
                                                          double leadInSilenceSec = 0.05,
                                                          float levelDbfs = -6.0f);

    /**
     * @brief Analiza una toma de loopback directo (DAC -> cable -> ADC) frente al estímulo emitido.
     * @param stimulusAudio Muestras del estímulo original emitido.
     * @param responseAudio Muestras capturadas por el canal de entrada ADC.
     * @param sampleRate Frecuencia de muestreo en Hz.
     * @param blockSize Tamaño de bloque del buffer de audio.
     * @param customCalibrationId Identificador opcional para trazabilidad (o vacío para auto-generar).
     * @return LoopbackCalibrationRecord Registro estructurado de calibración con evaluación pass/fail.
     */
    static LoopbackCalibrationRecord analyzeLoopback(const std::vector<float>& stimulusAudio,
                                                     const std::vector<float>& responseAudio,
                                                     double sampleRate,
                                                     int blockSize = 512,
                                                     const std::string& customCalibrationId = "");
};

/** @brief Alias desambiguado frente a `abdaudiolab::math::LoopbackCalibrator`. */
using LoopbackAuditCalibrator = LoopbackCalibrator;

} // namespace abdaudiolab::measurement
