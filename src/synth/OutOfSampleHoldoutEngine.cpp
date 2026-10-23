#include "synth/OutOfSampleHoldoutEngine.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace abdaudiolab::synth
{

OutOfSampleHoldoutEngine::OutOfSampleHoldoutEngine(HoldoutEngineConfig config)
    : config_(std::move(config))
{
}

std::vector<std::vector<TypedCoordinate>> OutOfSampleHoldoutEngine::createDefaultReservedCoordinates() const
{
    std::vector<std::vector<TypedCoordinate>> allCoords(4);

    auto makePoint = [](double drive, double cutoff, double resonance) {
        std::vector<TypedCoordinate> pt;
        TypedCoordinate c1;
        c1.dimensionId = "Drive";
        c1.kind = DimensionKind::Continuous;
        c1.continuousValue = drive;
        pt.push_back(c1);

        TypedCoordinate c2;
        c2.dimensionId = "Cutoff";
        c2.kind = DimensionKind::Continuous;
        c2.continuousValue = cutoff;
        pt.push_back(c2);

        TypedCoordinate c3;
        c3.dimensionId = "Resonance";
        c3.kind = DimensionKind::Continuous;
        c3.continuousValue = resonance;
        pt.push_back(c3);
        return pt;
    };

    allCoords[0] = makePoint(0.25, 0.80, 0.15); // Sub 1: Amplitud progresiva (Drive moderado)
    allCoords[1] = makePoint(0.50, 0.50, 0.40); // Sub 2: Ráfagas de transitorios
    allCoords[2] = makePoint(0.75, 0.35, 0.70); // Sub 3: Multitono denso (no linealidad alta)
    allCoords[3] = makePoint(0.90, 0.20, 0.85); // Sub 4: Señal pseudo-musical

    return allCoords;
}

HoldoutDataset OutOfSampleHoldoutEngine::buildHoldoutDataset(const TargetProcessCallback& targetProcessor,
                                                            const std::vector<std::vector<TypedCoordinate>>& reservedCoordinates)
{
    std::vector<std::vector<TypedCoordinate>> coords = reservedCoordinates;
    if (coords.size() < 4)
        coords = createDefaultReservedCoordinates();

    const double sr = config_.sampleRate > 0.0 ? config_.sampleRate : 96000.0;

    // 1. Generación determinista de las 4 sub-baterías de estímulos
    std::vector<std::vector<float>> stimuli(4);
    stimuli[0] = HoldoutStimulusGenerator::generateDynamicAmplitudeSteps(sr, 440.0, 0.06, 0.005);
    stimuli[1] = HoldoutStimulusGenerator::generateTransientBursts(sr, 4, 0.020, 0.060, 880.0);
    stimuli[2] = HoldoutStimulusGenerator::generateDenseMultitone(sr, 0.20, { 250.0, 550.0, 1200.0, 3100.0 }, -3.0);
    stimuli[3] = HoldoutStimulusGenerator::generatePseudoMusicalSequence(sr, 0.30, 220.0);

    const std::vector<std::string> pointIds = {
        "holdout_sub1_dynamic_steps",
        "holdout_sub2_transient_bursts",
        "holdout_sub3_dense_multitone",
        "holdout_sub4_pseudo_musical"
    };

    std::vector<HoldoutValidationPoint> validationPoints;
    validationPoints.reserve(4);

    for (size_t s = 0; s < 4; ++s)
    {
        HoldoutValidationPoint pt;
        pt.pointId = pointIds[s];
        pt.coordinates = coords[s];
        pt.inputStimulusAudio = stimuli[s];

        if (targetProcessor)
            pt.targetGroundTruthAudio = targetProcessor(pt.inputStimulusAudio, pt.coordinates);
        else
            pt.targetGroundTruthAudio = pt.inputStimulusAudio;

        // Cómputo de propiedades acústicas sobre la respuesta target
        const auto& targetAudio = pt.targetGroundTruthAudio;
        double sumSq = 0.0;
        int zeroCrossings = 0;

        for (size_t i = 0; i < targetAudio.size(); ++i)
        {
            double sample = static_cast<double>(targetAudio[i]);
            sumSq += sample * sample;
            if (i > 0)
            {
                if ((targetAudio[i] >= 0.0f && targetAudio[i - 1] < 0.0f) ||
                    (targetAudio[i] < 0.0f && targetAudio[i - 1] >= 0.0f))
                {
                    zeroCrossings++;
                }
            }
        }

        if (!targetAudio.empty() && sumSq > 1e-12)
        {
            double meanSq = sumSq / static_cast<double>(targetAudio.size());
            pt.measuredRmsDb = 10.0 * std::log10(meanSq);
            double zcr = static_cast<double>(zeroCrossings) / static_cast<double>(targetAudio.size());
            pt.measuredSpectralCentroidHz = 0.5 * zcr * sr;
        }
        else
        {
            pt.measuredRmsDb = -120.0;
            pt.measuredSpectralCentroidHz = 0.0;
        }

        // Hash acústico determinista
        std::ostringstream ss;
        ss << pt.pointId << ":" << std::fixed << std::setprecision(2)
           << pt.measuredRmsDb << ":" << pt.measuredSpectralCentroidHz << ":"
           << targetAudio.size();
        pt.acousticHash = Sha256::computeHex(ss.str());

        validationPoints.push_back(pt);
    }

    return HoldoutDataset(config_.datasetId, std::move(validationPoints), "EvaluationPhaseOnly");
}

bool OutOfSampleHoldoutEngine::checkDataLeakage(const HoldoutDataset& holdout,
                                               const std::vector<std::vector<TypedCoordinate>>& candidateTrainingPoints) const
{
    for (const auto& trainPt : candidateTrainingPoints)
    {
        if (holdout.containsCoordinate(trainPt, config_.distancePolicy, config_.leakageTolerance))
            return true; // Fuga de datos detectada: punto de entrenamiento colisiona con el holdout
    }
    return false;
}

UncertaintyDecomposition OutOfSampleHoldoutEngine::estimateUncertainty(const std::vector<std::vector<float>>& repeatedPasses,
                                                                      const std::vector<float>& modelPrediction,
                                                                      double instrumentalNoiseFloorDb)
{
    UncertaintyDecomposition decomp;

    if (repeatedPasses.empty() || modelPrediction.empty())
        return decomp;

    const size_t P = repeatedPasses.size();
    size_t N = modelPrediction.size();
    for (const auto& pass : repeatedPasses)
        N = std::min(N, pass.size());

    if (N == 0)
        return decomp;

    // 1. Media de pasadas empíricas y varianza aleatoria punto a punto
    double sumRandVar = 0.0;
    double sumSignalEnergy = 0.0;
    double sumModelSse = 0.0;

    for (size_t n = 0; n < N; ++n)
    {
        double meanVal = 0.0;
        for (size_t p = 0; p < P; ++p)
            meanVal += static_cast<double>(repeatedPasses[p][n]);
        meanVal /= static_cast<double>(P);

        sumSignalEnergy += meanVal * meanVal;

        if (P > 1)
        {
            double varPoint = 0.0;
            for (size_t p = 0; p < P; ++p)
            {
                double diff = static_cast<double>(repeatedPasses[p][n]) - meanVal;
                varPoint += diff * diff;
            }
            sumRandVar += varPoint / static_cast<double>(P - 1);
        }

        double modelErr = meanVal - static_cast<double>(modelPrediction[n]);
        sumModelSse += modelErr * modelErr;
    }

    const double energy = std::max(1e-12, sumSignalEnergy);
    const double randVar = (P > 1) ? (sumRandVar / static_cast<double>(N)) : 0.0;
    const double modelMse = sumModelSse / static_cast<double>(N);

    // 2. Descomposición de componentes
    // Ruido instrumental (ruido térmico base del convertidor/loopback)
    decomp.instrumentalUncertaintyDb = std::clamp(std::pow(10.0, instrumentalNoiseFloorDb / 20.0) * 8.686, 0.01, 1.0);

    // Dispersión aleatoria (variabilidad entre tomas)
    double relRandRms = std::sqrt(std::max(0.0, randVar) / (energy / static_cast<double>(N)));
    decomp.randomDispersalDb = std::clamp(relRandRms * 8.686, 0.02, 3.0);

    // Error estructural del modelo (error no explicado excluyendo ruido aleatorio)
    double structMse = std::max(0.0, modelMse - randVar);
    double relStructRms = std::sqrt(structMse / (energy / static_cast<double>(N)));
    decomp.structuralModelErrorDb = std::clamp(relStructRms * 8.686, 0.05, 10.0);

    // Incertidumbre combinada (suma en cuadratura)
    decomp.combinedUncertaintyDb = std::sqrt(
        decomp.instrumentalUncertaintyDb * decomp.instrumentalUncertaintyDb +
        decomp.randomDispersalDb * decomp.randomDispersalDb +
        decomp.structuralModelErrorDb * decomp.structuralModelErrorDb
    );

    return decomp;
}

} // namespace abdaudiolab::synth
