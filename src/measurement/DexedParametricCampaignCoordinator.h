/**
 * @file DexedParametricCampaignCoordinator.h
 * @brief Coordinator for offline isolated factorial parametric campaigns on Dexed.vst3.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "DexedParametricCampaignContracts.h"
#include "DexedVerticalCampaign.h"
#include "../synth/ExternalPluginFixture.h"
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <string>
#include <vector>
#include <memory>

namespace abdaudiolab::measurement
{

/**
 * @class DexedParametricCampaignCoordinator
 * @brief Orquestador de campañas factoriales aisladas (OFAT) sobre Dexed.vst3.
 *
 * Aplica aislamiento riguroso de variables:
 * - Campaña A: Variación de Algoritmo (1 vs 32) manteniendo Feedback = 0 constante.
 * - Campaña B: Variación de Feedback (0 vs 7) manteniendo Algoritmo = 1 constante.
 *
 * Registra formalmente requestedValue vs effectiveValue, parameterId, stateSha256
 * y fixtureRole ("canonical_pair").
 */
class DexedParametricCampaignCoordinator
{
public:
    /**
     * @brief Modifica un parámetro en el plugin Dexed, cuantiza al dominio nativo y registra el valor efectivo.
     */
    static bool applyParametricVariation(abdaudiolab::synth::ExternalPluginFixture& plug,
                                         const std::string& paramName,
                                         double requestedValue,
                                         ParametricRecord& outRecord,
                                         std::string& outError);

    /**
     * @brief Genera un fixture controlado canónico exploratorio con aislamiento de variables.
     */
    static DexedVerticalFixture createControlledVariantFixture(const DexedVerticalFixture& baseFixture,
                                                               int algorithm,
                                                               int feedback,
                                                               FixtureRole role = FixtureRole::CanonicalExploratoryPair);

    /**
     * @brief Ejecuta la Campaña Factorial A (Algoritmo 1 vs 32, Feedback 0) exportando contenedores FAIR independientes.
     */
    static bool executeCampaignA(juce::AudioPluginFormatManager& formatManager,
                                 const juce::File& dexedBinary,
                                 const juce::File& outputCampaignDir,
                                 ParametricCampaignManifest& outManifest,
                                 std::string& outError);

    /**
     * @brief Ejecuta la Campaña Factorial B (Feedback 0 vs 7, Algoritmo 1) exportando contenedores FAIR independientes.
     */
    static bool executeCampaignB(juce::AudioPluginFormatManager& formatManager,
                                 const juce::File& dexedBinary,
                                 const juce::File& outputCampaignDir,
                                 ParametricCampaignManifest& outManifest,
                                 std::string& outError);

    /**
     * @brief Genera un manifest y fixtures sintéticos/mock para entornos de CI/tests sin plugin VST3 binario.
     */
    static ParametricCampaignManifest generateSyntheticFactorialManifest(ParametricCampaignType type);

    /**
     * @brief Ejecuta la Campaña C: Barrido de Nivel de Salida de Operador Modulador (Proxy de Profundidad FM 0..99).
     */
    static bool executeModulationIndexCampaign(juce::AudioPluginFormatManager& formatManager,
                                              const juce::File& dexedBinary,
                                              const juce::File& outputCampaignDir,
                                              ParametricCampaignManifest& outManifest,
                                              std::string& outError);

    /**
     * @brief Ejecuta la Campaña D: Comparación de Ratios de Frecuencia Armónicos vs Inarmónicos (1.0 vs 2.0 vs 3.14).
     */
    static bool executeFrequencyRatioCampaign(juce::AudioPluginFormatManager& formatManager,
                                             const juce::File& dexedBinary,
                                             const juce::File& outputCampaignDir,
                                             ParametricCampaignManifest& outManifest,
                                             std::string& outError);

    /**
     * @brief Ejecuta la Campaña E: Trayectoria Temporal del Centroide Espectral C(t) por STFT.
     */
    static bool executeTemporalCentroidCampaign(juce::AudioPluginFormatManager& formatManager,
                                               const juce::File& dexedBinary,
                                               const juce::File& outputCampaignDir,
                                               FmModulationObservation& outObservation,
                                               std::string& outError);

    static FmModulationObservation generateSyntheticFmObservation(int outputLevel,
                                                                 double modulatorRatio = 1.0,
                                                                 bool simulateSilence = false);

    /**
     * @brief Estima el índice de modulación física beta a partir de la anulación de portadora Bessel J0.
     *
     * Requiere:
     * - Mínimo local estricto de 3 puntos: carrier(k) <= carrier(k-1) y carrier(k) <= carrier(k+1)
     * - Supresión de portadora >= carrierNullThresholdDb (default 24.0 dB)
     * - Bandas laterales observables
     * - Frecuencia moduladora (fmHz) observable
     * - Ratio compatible
     *
     * Si sólo hay supresión sin mínimo local: status = "not_estimated", reason = "suppression_without_local_null".
     * Si hay clipping o señal insuficiente: status = "unreliable".
     */
    static BetaNullObservation estimateBetaFromCarrierNull(const std::vector<CarrierSweepPoint>& sweepPoints,
                                                           const CarrierNullEstimationConfig& config = {});

    /**
     * @brief Ejecuta la Campaña F: Barrido de Escalado de Teclado con segregación estricta de Level Scaling y Rate Scaling.
     */
    static bool executeKeyboardScalingCampaign(juce::AudioPluginFormatManager& formatManager,
                                               const juce::File& dexedBinary,
                                               const juce::File& outputCampaignDir,
                                               KeyboardScalingCampaignResult& outResult,
                                               std::string& outError);

    static KeyboardScalingPointRecord generateSyntheticKeyboardScalingPoint(int note,
                                                                            int breakpoint = 60,
                                                                            int leftDepth = 50,
                                                                            int rightDepth = 50,
                                                                            int rateScaling = 3);

    /**
     * @brief Exporta el manifiesto en lote batch_manifest.json ordenado deterministamente:
     * note asc -> operatorLevel asc -> ratio asc.
     */
    static bool exportBatchManifest(const juce::File& outputDir,
                                    const std::string& campaignId,
                                    const std::string& campaignType,
                                    std::vector<BatchVariantItem> variants,
                                    BatchCampaignManifest& outManifest,
                                    std::string& outError);
};

} // namespace abdaudiolab::measurement
