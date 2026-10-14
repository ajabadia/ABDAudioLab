#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "synth/ModelEvaluationTypes.h"
#include "synth/HoldoutStimulusGenerator.h"

namespace abdaudiolab::synth
{

/**
 * @struct HoldoutEngineConfig
 * @brief Parámetros de configuración del motor de validación fuera de muestra.
 */
struct HoldoutEngineConfig
{
    std::string datasetId { "holdout_canonical_v1" };
    double sampleRate { 96000.0 };
    int blockSize { 256 };
    DistancePolicy distancePolicy;
    double leakageTolerance { 1e-3 };
};

/**
 * @class OutOfSampleHoldoutEngine
 * @brief Orquestador metrológico de la batería de validación ciega fuera de muestra (*out-of-sample*).
 *
 * Garantiza el aislamiento estricto entre el espacio de entrenamiento/calibración y el de validación,
 * sintetiza las 4 sub-baterías de estímulos ciegos, captura la respuesta del target,
 * audita fugas de datos (*data leakage*) y descompone formalmente la incertidumbre instrumental vs estructural.
 */
class OutOfSampleHoldoutEngine
{
public:
    using TargetProcessCallback = std::function<std::vector<float>(const std::vector<float>& stimulusAudio,
                                                                   const std::vector<TypedCoordinate>& coords)>;

    explicit OutOfSampleHoldoutEngine(HoldoutEngineConfig config = {});

    [[nodiscard]] const HoldoutEngineConfig& getConfig() const noexcept { return config_; }
    void setConfig(const HoldoutEngineConfig& config) noexcept { config_ = config; }

    /**
     * @brief Construye un HoldoutDataset completo e inmutable ejecutando las 4 sub-baterías frente al target dado.
     *
     * @param targetProcessor Función o lambda que simula o captura la respuesta de audio del target para cada estímulo y coordenada.
     * @param reservedCoordinates Coordenadas tipadas a reservar para cada una de las 4 sub-baterías (mínimo 4 vectores).
     */
    [[nodiscard]] HoldoutDataset buildHoldoutDataset(const TargetProcessCallback& targetProcessor,
                                                    const std::vector<std::vector<TypedCoordinate>>& reservedCoordinates = {});

    /**
     * @brief Audita si un conjunto propuesto de puntos de entrenamiento o calibración colisiona con el dataset reservado.
     *
     * @param holdout Dataset reservado a auditar.
     * @param candidateTrainingPoints Puntos de control que el optimizador/calibrador pretende explorar.
     * @return true si se detecta fuga de datos (distancia < leakageTolerance); false si son estrictamente disjuntos.
     */
    [[nodiscard]] bool checkDataLeakage(const HoldoutDataset& holdout,
                                        const std::vector<std::vector<TypedCoordinate>>& candidateTrainingPoints) const;

    /**
     * @brief Descompone la incertidumbre empírica a partir de múltiples tomas repetidas frente a la predicción del modelo.
     *
     * @param repeatedPasses Búferes de audio capturados en P pasadas idénticas (P >= 2).
     * @param modelPrediction Búfer de audio predicho por el modelo candidato.
     * @param instrumentalNoiseFloorDb Piso de ruido del loopback medido en la calibración (dB, ej. -96 dBFS).
     */
    [[nodiscard]] static UncertaintyDecomposition estimateUncertainty(const std::vector<std::vector<float>>& repeatedPasses,
                                                                      const std::vector<float>& modelPrediction,
                                                                      double instrumentalNoiseFloorDb = -96.0);

private:
    HoldoutEngineConfig config_;

    [[nodiscard]] std::vector<std::vector<TypedCoordinate>> createDefaultReservedCoordinates() const;
};

} // namespace abdaudiolab::synth
