/**
 * @file AnalogChainCompensationContracts.h
 * @brief Canonical metrological contracts for 3-tier analog chain calibration,
 *        reversible regularized compensation, physical saturation diagnosis,
 *        and hardware provenance (T20.12-3).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "AnalogDutCharacterizationContracts.h"
#include "FineLatencyContracts.h"
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Impedance descriptor with explicit knowledge state and provenance.
 * Never represents unknown impedance as 0.0 Ohms.
 */
struct ImpedanceDescriptor
{
    std::optional<double> valueOhms;
    std::string source { "unknown" }; /**< "datasheet", "measured", "user_supplied", "unknown" */
    std::string measurementUncertainty { "not_specified" };

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        if (valueOhms.has_value())
            j["valueOhms"] = *valueOhms;
        else
            j["valueOhms"] = nullptr;
        j["source"] = source;
        j["measurementUncertainty"] = measurementUncertainty;
        return j;
    }
};

/**
 * @brief Metadatos de conexión física de hardware para la cadena de prueba.
 */
struct HardwareConnectionMetadata
{
    std::string interfaceModel { "Unknown Interface" };
    std::string firmwareVersion { "Unknown" };
    ImpedanceDescriptor dutOutputImpedance;
    ImpedanceDescriptor interfaceInputImpedance;
    std::string cableDescription { "Standard balanced TRS" };
    std::string wiringTopology { "Balanced" }; /**< "Balanced", "Unbalanced", "PseudoBalanced" */
    double padGainDb { 0.0 };
    bool phantomPowerActive { false };
    std::optional<double> terminationOhms;
    std::optional<double> ambientTemperatureCelsius;

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["ambientTemperatureCelsius"] = ambientTemperatureCelsius.has_value() ? nlohmann::json(*ambientTemperatureCelsius) : nlohmann::json(nullptr);
        j["cableDescription"] = cableDescription;
        j["dutOutputImpedance"] = dutOutputImpedance.toJson();
        j["firmwareVersion"] = firmwareVersion;
        j["interfaceInputImpedance"] = interfaceInputImpedance.toJson();
        j["interfaceModel"] = interfaceModel;
        j["padGainDb"] = padGainDb;
        j["phantomPowerActive"] = phantomPowerActive;
        j["terminationOhms"] = terminationOhms.has_value() ? nlohmann::json(*terminationOhms) : nlohmann::json(nullptr);
        j["wiringTopology"] = wiringTopology;
        return j;
    }
};

/**
 * @brief Diagnóstico físico segregado de saturación (ADC vs DUT vs Previo).
 */
struct PhysicalSaturationDiagnosis
{
    bool adcClipEvidence { false };
    int clipSampleCount { 0 };
    int positiveRailCount { 0 };
    int negativeRailCount { 0 };
    int64_t firstClipSample { -1 };
    int64_t lastClipSample { -1 };
    double clipThreshold { 0.999 };

    bool dutCompressionEvidence { false };
    double observedCompressionDb { 0.0 };

    std::string preampOverloadEvidence { "not_assessed" }; /**< "suspected", "confirmed", "not_assessed" */
    std::string status { "linear" }; /**< "linear", "dut_saturating", "measurement_invalid_due_to_adc_clipping" */
    double confidence { 1.0 };
    std::string diagnosticLimitations { "none" };

    [[nodiscard]] bool isValidForDistortionMetrics() const noexcept
    {
        return !adcClipEvidence && status != "measurement_invalid_due_to_adc_clipping";
    }

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["adcClipEvidence"] = adcClipEvidence;
        j["clipSampleCount"] = clipSampleCount;
        j["clipThreshold"] = clipThreshold;
        j["confidence"] = confidence;
        j["diagnosticLimitations"] = diagnosticLimitations;
        j["dutCompressionEvidence"] = dutCompressionEvidence;
        j["firstClipSample"] = firstClipSample;
        j["lastClipSample"] = lastClipSample;
        j["negativeRailCount"] = negativeRailCount;
        j["observedCompressionDb"] = observedCompressionDb;
        j["positiveRailCount"] = positiveRailCount;
        j["preampOverloadEvidence"] = preampOverloadEvidence;
        j["status"] = status;
        return j;
    }
};

/**
 * @brief Capa 1: Registro formal de calibración de cadena (Loopback Reference).
 */
struct ChainCalibrationRecord
{
    std::string calibrationId;
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    double roundTripLatencySamples { 0.0 };
    double roundTripLatencyMs { 0.0 };
    double clockDriftPpm { 0.0 };
    double noiseFloorDbfs { -96.0 };
    double residualThdDbfs { -80.0 };
    double flatnessRippleDb { 0.0 };
    std::string stimulusSha256;
    std::string rawResponseSha256;
    HardwareConnectionMetadata connectionInfo;

    std::string status { "chain_invalid" }; /**< "valid", "degraded", "chain_invalid" */
    std::string evaluationNotes { "" };

    // Metadatos de respuesta en frecuencia
    std::vector<double> frequencyGridHz;
    std::vector<double> magnitudeResponseDb;
    std::vector<double> phaseResponseRad;

    [[nodiscard]] bool isValid() const noexcept { return status == "valid"; }
    [[nodiscard]] bool isDegraded() const noexcept { return status == "degraded"; }

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["blockSize"] = blockSize;
        j["calibrationId"] = calibrationId;
        j["clockDriftPpm"] = clockDriftPpm;
        j["connectionInfo"] = connectionInfo.toJson();
        j["evaluationNotes"] = evaluationNotes;
        j["flatnessRippleDb"] = flatnessRippleDb;
        j["noiseFloorDbfs"] = noiseFloorDbfs;
        j["rawResponseSha256"] = rawResponseSha256;
        j["residualThdDbfs"] = residualThdDbfs;
        j["roundTripLatencyMs"] = roundTripLatencyMs;
        j["roundTripLatencySamples"] = roundTripLatencySamples;
        j["sampleRateHz"] = sampleRateHz;
        j["status"] = status;
        j["stimulusSha256"] = stimulusSha256;
        j["frequencyGridHz"] = frequencyGridHz;
        j["magnitudeResponseDb"] = magnitudeResponseDb;
        j["phaseResponseRad"] = phaseResponseRad;
        return j;
    }
};

/**
 * @brief Máscara espectral de validez y parámetros de regularización.
 */
struct CompensationValidity
{
    double minChainMagnitudeDb { -30.0 };
    double configuredMaxInverseGainDb { 12.0 };
    double appliedMaxInverseGainDb { 12.0 };
    std::string gainLimitPolicy { "capped" }; /**< "capped", "strict", "unconstrained" */
    double regularizationLambda { 1e-4 };
    double validBandwidthLowHz { 20.0 };
    double validBandwidthHighHz { 20000.0 };
    double noiseAmplificationEstimateDb { 0.0 };
    std::string status { "valid" }; /**< "valid", "degraded", "invalid" */
    std::string reason { "nominal" };

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["appliedMaxInverseGainDb"] = appliedMaxInverseGainDb;
        j["configuredMaxInverseGainDb"] = configuredMaxInverseGainDb;
        j["gainLimitPolicy"] = gainLimitPolicy;
        j["noiseAmplificationEstimateDb"] = noiseAmplificationEstimateDb;
        j["reason"] = reason;
        j["regularizationLambda"] = regularizationLambda;
        j["status"] = status;
        j["validBandwidthHighHz"] = validBandwidthHighHz;
        j["validBandwidthLowHz"] = validBandwidthLowHz;
        return j;
    }
};

/**
 * @brief Capa 2: Medición cruda y analizada de DUT + Cadena (sin compensar).
 */
struct DutPlusChainMeasurementRecord
{
    std::string measurementId;
    std::string rawCaptureSha256;
    double sampleRateHz { 48000.0 };
    size_t sampleCount { 0 };
    PhysicalSaturationDiagnosis saturationDiagnosis;
    HardwareConnectionMetadata connectionInfo;

    // Métricas analizadas directamente del crudo
    double rawRmsDbfs { -96.0 };
    double rawPeakDbfs { -96.0 };
    std::optional<HarmonicDistortionResult> rawThd;
    std::optional<FrequencyResponseResult> rawFrequencyResponse;

    std::string status { "nominal" }; /**< "nominal", "measurement_invalid_due_to_adc_clipping" */

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["connectionInfo"] = connectionInfo.toJson();
        j["measurementId"] = measurementId;
        j["rawCaptureSha256"] = rawCaptureSha256;
        j["rawPeakDbfs"] = rawPeakDbfs;
        j["rawRmsDbfs"] = rawRmsDbfs;
        j["sampleCount"] = sampleCount;
        j["sampleRateHz"] = sampleRateHz;
        j["saturationDiagnosis"] = saturationDiagnosis.toJson();
        j["status"] = status;
        if (rawThd.has_value())
            j["rawThd"] = rawThd->toJson();
        if (rawFrequencyResponse.has_value())
            j["rawFrequencyResponse"] = rawFrequencyResponse->toJson();
        return j;
    }
};

/**
 * @brief Capa 3: Medición compensada reversiblemente con procedencia criptográfica completa.
 */
struct CompensatedDutMeasurementRecord
{
    std::string measurementId;
    std::string rawCaptureSha256;
    std::string chainReferenceSha256;
    std::string compensationModelSha256;
    std::string algorithmVersion { "abdaudiolab-chain-comp-1.0" };
    double inputSampleRateHz { 48000.0 };

    bool isReversible { true };
    std::string status { "compensation_not_applied" }; /**< "compensation_applied", "compensation_degraded", "compensation_not_applied", "incomplete_provenance" */

    CompensationValidity validity;
    std::optional<HarmonicDistortionResult> compensatedThd;
    std::optional<FrequencyResponseResult> compensatedFrequencyResponse;

    // Métricas comparativas
    double magnitudeCorrectionMaxDb { 0.0 };
    double latencyCompensatedSamples { 0.0 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["algorithmVersion"] = algorithmVersion;
        j["chainReferenceSha256"] = chainReferenceSha256;
        j["compensationModelSha256"] = compensationModelSha256;
        j["inputSampleRateHz"] = inputSampleRateHz;
        j["isReversible"] = isReversible;
        j["latencyCompensatedSamples"] = latencyCompensatedSamples;
        j["magnitudeCorrectionMaxDb"] = magnitudeCorrectionMaxDb;
        j["measurementId"] = measurementId;
        j["rawCaptureSha256"] = rawCaptureSha256;
        j["status"] = status;
        j["validity"] = validity.toJson();
        if (compensatedThd.has_value())
            j["compensatedThd"] = compensatedThd->toJson();
        if (compensatedFrequencyResponse.has_value())
            j["compensatedFrequencyResponse"] = compensatedFrequencyResponse->toJson();
        return j;
    }

    [[nodiscard]] std::string toCanonicalJson() const
    {
        return toJson().dump();
    }
};

} // namespace abdaudiolab::measurement
