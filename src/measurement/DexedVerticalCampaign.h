/**
 * @file DexedVerticalCampaign.h
 * @brief Orquestador y exportador de la Prueba Vertical de Integración y Exportación FAIR/LNL de Dexed.vst3 (Fase 20.11 T5).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include "MeasurementCaptureCoordinator.h"
#include "adapters/DynamicsMeasurementAdapter.h"
#include "adapters/ModulationMeasurementAdapter.h"
#include "../synth/ExternalPluginFixture.h"
#include "../synth/ExternalPluginTypes.h"
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <string>
#include <memory>

namespace abdaudiolab::measurement
{

/**
 * @struct DexedVerticalFixture
 * @brief Especificación del fixture real de Dexed.vst3 con fixity criptográfica inmutable.
 */
struct DexedVerticalFixture
{
    std::string presetName { "Dexed_Controlled_Init" };
    std::vector<uint8_t> presetBytes;
    std::string stateSha256;
    std::string pluginBinarySha256;
    std::string componentUid { "VST3-Dexed-3f015740-d7709eec" };
    int midiNote { 60 }; /**< C4 */
    std::vector<int> velocityGrid { 32, 64, 96, 127 };
    MeasurementExecutionDomain executionDomain { MeasurementExecutionDomain::Vst3OfflineDigital };
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    double noteDurationSec { 0.5 };
    double releaseDurationSec { 0.5 };

    [[nodiscard]] bool verifyFixity() const;
};

/**
 * @struct DexedCampaignResults
 * @brief Resultados consolidados de la campaña vertical FAIR/LNL de Dexed.
 */
struct DexedCampaignResults
{
    std::string containerId;
    DexedVerticalFixture fixture;
    MeasurementSpec spec;
    PluginIdentity pluginIdentity;
    std::string stimulusJson;
    std::string stimulusSha256;

    MeasurementResult measurementResult;
    DynamicResponseResult dynamicResult;
    ModulationResultData modulationResult;
    CaptureArtifactMetadata captureTelemetry;

    std::vector<float> referenceAudioSamples;
    std::vector<DynamicPoint> dynamicPoints;
};

/**
 * @class DexedVerticalCoordinator
 * @brief Ejecuta el ciclo metrológico de medición vertical sobre Dexed.vst3 real.
 */
class DexedVerticalCoordinator
{
public:
    /**
     * @brief Resuelve la ubicación física de Dexed.vst3 en el sistema.
     */
    static juce::File resolveDexedBinary();

    /**
     * @brief Ejecuta la campaña completa por velocidad independiente y evaluación de modulación.
     */
    static bool executeCampaign(juce::AudioPluginFormatManager& formatManager,
                                const juce::File& dexedBinary,
                                const DexedVerticalFixture& fixture,
                                DexedCampaignResults& outResults,
                                std::string& outError);
};

/**
 * @class DexedVerticalContainerExporter
 * @brief Empaqueta y persiste el contenedor FAIR/LNL de 12 artefactos según el schema abdaudiolab-fair-lnl-1.0.
 */
class DexedVerticalContainerExporter
{
public:
    static constexpr const char* kLnlSchemaVersion = "abdaudiolab-fair-lnl-1.0";
    static constexpr const char* kManifestFormat = "artifact-list-v1";

    /**
     * @brief Exporta los 12 artefactos del contenedor FAIR/LNL y verifica los hashes inmutables.
     */
    static bool exportContainer(const juce::File& outputDir,
                                const DexedCampaignResults& results,
                                std::string& outError);
};

} // namespace abdaudiolab::measurement
