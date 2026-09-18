/**
 * @file MeasurementSessionContracts.cpp
 * @brief Implementation of Unified Measurement Session and Historical Re-analysis Engine.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementSessionContracts.h"
#include "AnalogDutCharacterizer.h"
#include <algorithm>

namespace abdaudiolab::measurement
{

bool SessionReanalysisEngine::reanalyzeHarmonicDistortion(
    MeasurementSession& session,
    const std::string& captureId,
    std::span<const float> rawSamples,
    const SpectralAnalysisConfig& newConfig,
    const std::string& newAnalyzerVersion,
    DerivedArtifactReference& outDerivedArtifact,
    std::string& outError)
{
    outError.clear();

    // 1. Localizar la referencia de captura cruda en la sesión
    auto it = std::find_if(session.rawCaptures.begin(), session.rawCaptures.end(),
                           [&](const RawCaptureReference& r) { return r.captureId == captureId; });

    if (it == session.rawCaptures.end())
    {
        outError = "Capture ID '" + captureId + "' not found in session '" + session.sessionId + "'";
        return false;
    }

    const auto& capRef = *it;

    // 2. Verificación Estricta de Fixity Criptográfica (Inmutabilidad)
    std::string computedHash = synth::Sha256::computeHex(rawSamples.data(), rawSamples.size_bytes());
    if (computedHash != capRef.rawAudioSha256)
    {
        outError = "Fixity verification FAILED for capture '" + captureId +
                   "': Expected " + capRef.rawAudioSha256 + " but computed " + computedHash;
        return false;
    }

    // 3. Ejecución del motor analítico solicitado (reutilizando AnalogDutCharacterizer y MeasurementDspUtils)
    auto distResult = AnalogDutCharacterizer::measureHarmonicDistortion(
        rawSamples, newConfig.sampleRateHz, 1000.0, ThdConvention::FundamentalReferenced,
        2, 10, newConfig.window, newConfig.fftSize);

    // 4. Configuración canónica e identificación del nuevo artefacto derivado
    const std::string configPayload = "window=" + measurementWindowToString(newConfig.window) +
                                      "&fftSize=" + std::to_string(newConfig.fftSize) +
                                      "&integrationBins=" + std::to_string(newConfig.harmonicIntegrationBins) +
                                      "&analyzer=" + newAnalyzerVersion;
    std::string configHash = synth::Sha256::computeHex(configPayload);

    DerivedArtifactReference artifact;
    artifact.artifactId = "deriv_" + std::to_string(session.derivedArtifacts.size() + 1) + "_" + captureId;
    artifact.analyzerVersion = newAnalyzerVersion;
    artifact.configHash = configHash;
    artifact.artifactType = "harmonic_distortion";
    artifact.targetRawCaptureSha256 = capRef.rawAudioSha256;
    artifact.createdAtUtc = "2026-09-18T12:00:00Z";
    artifact.metrics = distResult.toCanonicalJson();
    artifact.artifactSha256 = synth::Sha256::computeHex(artifact.metrics.dump());

    // 5. Preservación aditiva: se añade el nuevo artefacto derivado SIN alterar artefactos anteriores
    session.derivedArtifacts.push_back(artifact);
    outDerivedArtifact = artifact;

    return true;
}

} // namespace abdaudiolab::measurement
