/**
 * @file AnalogChainReversibleCompensator.h
 * @brief Unified metrological orchestrator for 3-tier analog chain calibration,
 *        reversible regularized compensation, and physical saturation diagnosis.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "AnalogChainCompensationContracts.h"
#include "AnalogDutCharacterizer.h"
#include "MeasurementDspUtils.h"
#include "FineLatencyContracts.h"
#include <span>
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class AnalogChainReversibleCompensator
 * @brief Orquestador de calibración y compensación analógica en 3 capas.
 * Reutiliza estrictamente FineLatencyAnalyzer, MeasurementDspUtils,
 * math::FarinaDeconvolver y AnalogDutCharacterizer.
 */
class AnalogChainReversibleCompensator
{
public:
    /**
     * @brief Evalúa la Capa 1: Calibración de la cadena mediante loopback directo.
     * @param stimulusAudio Señal patrón emitida por el DAC.
     * @param responseAudio Señal capturada por el ADC tras el cable de referencia.
     * @param sampleRate Frecuencia de muestreo en Hz.
     * @param blockSize Tamaño de bloque de audio.
     * @param connection Metadatos físicos del hardware e impedancias.
     * @param customId Identificador opcional para trazabilidad.
     * @return ChainCalibrationRecord con estado formal ("valid", "degraded", "chain_invalid").
     */
    static ChainCalibrationRecord evaluateChainCalibration(
        std::span<const float> stimulusAudio,
        std::span<const float> responseAudio,
        double sampleRate,
        int blockSize = 512,
        const HardwareConnectionMetadata& connection = {},
        const std::string& customId = "");

    /**
     * @brief Diagnostica saturación física segregando ADC, DUT y Previo.
     * @param rawCapture Muestras crudas de la captura de audio.
     * @param clipThreshold Umbral en amplitud para detección de rail del ADC (por defecto 0.999f).
     * @return PhysicalSaturationDiagnosis con evidencia segregada de clipping y compresión.
     */
    static PhysicalSaturationDiagnosis diagnosePhysicalSaturation(
        std::span<const float> rawCapture,
        float clipThreshold = 0.999f);

    /**
     * @brief Aplica compensación espectral regularizada reversible sobre la respuesta en frecuencia.
     * @param rawDutResponse Respuesta conjunta medida (Capa 2).
     * @param chainCalibration Calibración de referencia de la interfaz (Capa 1).
     * @param validityConfig Parámetros de máscara y regularización.
     * @param allowDegraded Permite compensar con calibración degradada (por defecto false).
     * @return CompensatedDutMeasurementRecord con máscara calculada y resultado desacoplado.
     */
    static CompensatedDutMeasurementRecord compensateFrequencyResponse(
        const FrequencyResponseResult& rawDutResponse,
        const std::string& rawCaptureSha256,
        const ChainCalibrationRecord& chainCalibration,
        const CompensationValidity& validityConfig = {},
        bool allowDegraded = false);

    /**
     * @brief Verifica si un registro o cadena ya ha sido compensada para evitar doble aplicación.
     */
    [[nodiscard]] static bool isAlreadyCompensated(const CompensatedDutMeasurementRecord& record) noexcept;
};

} // namespace abdaudiolab::measurement
