/**
 * @file MeasurementSessionContracts.h
 * @brief Canonical contracts for Unified Measurement Sessions, Control State
 *        Snapshots, Raw Capture Fixity, and Historical Re-analysis.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "AnalogDutCharacterizationContracts.h"
#include "AnalogChainCompensationContracts.h"
#include "MeasurementDspUtils.h"
#include "synth/Sha256.h"
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <nlohmann/json.hpp>

namespace abdaudiolab::measurement
{

/**
 * @brief Snapshot of a control parameter state during a measurement take.
 */
struct ControlStateSnapshot
{
    std::string controlId;
    std::string controlMethod { "MANUAL" }; /**< "MANUAL", "MIDI_CC", "NRPN", "SYSEX", "SOFTWARE_PLUGIN_PARAM" */
    std::optional<double> normalizedValue;
    std::optional<double> rawValue;
    std::string displayValue;
    std::string confirmationStatus { "automated" }; /**< "confirmed" (operator acknowledged), "automated", "unconfirmed" */

    [[nodiscard]] nlohmann::ordered_json toJson() const
    {
        nlohmann::ordered_json j;
        j["confirmationStatus"] = confirmationStatus;
        j["controlId"] = controlId;
        j["controlMethod"] = controlMethod;
        j["displayValue"] = displayValue;
        if (normalizedValue.has_value())
            j["normalizedValue"] = *normalizedValue;
        else
            j["normalizedValue"] = nullptr;
        if (rawValue.has_value())
            j["rawValue"] = *rawValue;
        else
            j["rawValue"] = nullptr;
        return j;
    }
};

/**
 * @brief Reference to an immutable raw audio capture tied to its parameter snapshot.
 */
struct RawCaptureReference
{
    std::string captureId;
    std::string rawAudioSha256;
    std::string filename;
    double sampleRateHz { 48000.0 };
    size_t sampleCount { 0 };
    int channels { 1 };
    PhysicalSaturationDiagnosis saturationDiagnosis;
    std::vector<ControlStateSnapshot> activeControls;

    [[nodiscard]] nlohmann::ordered_json toJson() const
    {
        nlohmann::ordered_json j;
        nlohmann::ordered_json ctrlArr = nlohmann::ordered_json::array();
        for (const auto& c : activeControls)
            ctrlArr.push_back(c.toJson());
        j["activeControls"] = ctrlArr;
        j["captureId"] = captureId;
        j["channels"] = channels;
        j["filename"] = filename;
        j["rawAudioSha256"] = rawAudioSha256;
        j["sampleCount"] = sampleCount;
        j["sampleRateHz"] = sampleRateHz;
        j["saturationDiagnosis"] = saturationDiagnosis.toJson();
        return j;
    }
};

/**
 * @brief Reference to a derived analysis artifact with full provenance.
 */
struct DerivedArtifactReference
{
    std::string artifactId;
    std::string analyzerVersion;
    std::string configHash;
    std::string artifactType; /**< "harmonic_distortion", "frequency_response", "clipping_threshold", "fair_report" */
    std::string artifactSha256;
    std::string targetRawCaptureSha256;
    std::string createdAtUtc;
    nlohmann::ordered_json metrics;

    [[nodiscard]] nlohmann::ordered_json toJson() const
    {
        nlohmann::ordered_json j;
        j["analyzerVersion"] = analyzerVersion;
        j["artifactId"] = artifactId;
        j["artifactSha256"] = artifactSha256;
        j["artifactType"] = artifactType;
        j["configHash"] = configHash;
        j["createdAtUtc"] = createdAtUtc;
        j["metrics"] = metrics;
        j["targetRawCaptureSha256"] = targetRawCaptureSha256;
        return j;
    }
};

/**
 * @brief Complete unified Measurement Session linking device profile,
 *        control parameters, raw immutable captures, and versioned derived results.
 */
struct MeasurementSession
{
    std::string sessionId;
    std::string profileId;
    std::string profileSha256;
    std::string deviceType;
    std::string targetFunction;
    std::string excitationPlanId;
    std::string captureConfigHash;
    std::string analyzerVersion;

    std::vector<ControlStateSnapshot> controlStates;
    std::vector<RawCaptureReference> rawCaptures;
    std::vector<DerivedArtifactReference> derivedArtifacts;

    [[nodiscard]] nlohmann::ordered_json toJson() const
    {
        nlohmann::ordered_json j;
        j["analyzerVersion"] = analyzerVersion;
        j["captureConfigHash"] = captureConfigHash;

        nlohmann::ordered_json ctrlArr = nlohmann::ordered_json::array();
        for (const auto& c : controlStates)
            ctrlArr.push_back(c.toJson());
        j["controlStates"] = ctrlArr;

        nlohmann::ordered_json derivArr = nlohmann::ordered_json::array();
        for (const auto& d : derivedArtifacts)
            derivArr.push_back(d.toJson());
        j["derivedArtifacts"] = derivArr;

        j["deviceType"] = deviceType;
        j["excitationPlanId"] = excitationPlanId;
        j["profileId"] = profileId;
        j["profileSha256"] = profileSha256;

        nlohmann::ordered_json rawArr = nlohmann::ordered_json::array();
        for (const auto& r : rawCaptures)
            rawArr.push_back(r.toJson());
        j["rawCaptures"] = rawArr;

        j["sessionId"] = sessionId;
        j["targetFunction"] = targetFunction;
        return j;
    }

    [[nodiscard]] std::string toCanonicalJson() const
    {
        return toJson().dump();
    }
};

/**
 * @class SessionReanalysisEngine
 * @brief Orquestador de re-análisis histórico desacoplado:
 *        ejecuta nuevas versiones algorítmicas sobre capturas crudas existentes
 *        preservando inalterados los audios y los informes precedentes.
 */
class SessionReanalysisEngine
{
public:
    /**
     * @brief Re-analiza la distorsión armónica de una captura existente con un nuevo algoritmo/configuración.
     * @param session Sesión a actualizar.
     * @param captureId Identificador de la captura cruda a re-procesar.
     * @param rawSamples Muestras de audio crudas leídas del WAV inmutable.
     * @param newConfig Configuración espectral del nuevo análisis.
     * @param newAnalyzerVersion Versión del nuevo motor analítico (e.g. "abdaudiolab-analyzer-2.0").
     * @param outDerivedArtifact Referencia al artefacto recién generado.
     * @param outError Mensaje descriptivo en caso de fallo (e.g. discrepancia de hash SHA-256).
     * @return true si la verificación de fixity fue positiva y se emitió el nuevo resultado derivado.
     */
    static bool reanalyzeHarmonicDistortion(
        MeasurementSession& session,
        const std::string& captureId,
        std::span<const float> rawSamples,
        const SpectralAnalysisConfig& newConfig,
        const std::string& newAnalyzerVersion,
        DerivedArtifactReference& outDerivedArtifact,
        std::string& outError);
};

} // namespace abdaudiolab::measurement
