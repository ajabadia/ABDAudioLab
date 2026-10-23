#include "synth/MultiObjectiveModelSelector.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace abdaudiolab::synth
{

MultiObjectiveModelSelector::MultiObjectiveModelSelector()
{
    setPreset(OptimizationPreset::Balanced);
}

MultiObjectiveModelSelector::MultiObjectiveModelSelector(OptimizationPreset preset)
{
    setPreset(preset);
}

MultiObjectiveModelSelector::MultiObjectiveModelSelector(double lambda, double mu, double sigma)
    : preset_(OptimizationPreset::Custom)
{
    weights_.lambda = lambda;
    weights_.mu = mu;
    weights_.uncertaintyMargin = sigma;
}

void MultiObjectiveModelSelector::setPreset(OptimizationPreset preset) noexcept
{
    preset_ = preset;
    applyPresetWeights();
}

void MultiObjectiveModelSelector::applyPresetWeights() noexcept
{
    switch (preset_)
    {
        case OptimizationPreset::StudioMaster:
            weights_.lambda = 0.05;  // Desprecia coste de CPU, prima fidelidad
            weights_.mu = 0.15;
            weights_.uncertaintyMargin = 0.03;
            break;

        case OptimizationPreset::LivePerformance:
            weights_.lambda = 0.60;  // Fuerte penalización a modelos pesados
            weights_.mu = 0.20;
            weights_.uncertaintyMargin = 0.03;
            break;

        case OptimizationPreset::Balanced:
        case OptimizationPreset::Custom:
        default:
            weights_.lambda = 0.25;  // Compromiso nominal
            weights_.mu = 0.20;
            weights_.uncertaintyMargin = 0.03;
            break;
    }
}

void MultiObjectiveModelSelector::addCandidate(std::shared_ptr<ICandidateModelEvaluator> candidate)
{
    if (candidate != nullptr)
        candidates_.push_back(std::move(candidate));
}

void MultiObjectiveModelSelector::clearCandidates() noexcept
{
    candidates_.clear();
}

double MultiObjectiveModelSelector::computeCompositeCost(double errorVal, double compCost, double uncertainty,
                                                         double lambda, double mu) noexcept
{
    return errorVal + lambda * compCost + mu * uncertainty;
}

ModelEvaluation MultiObjectiveModelSelector::evaluateAndSelect(const TargetAuditReport& audit,
                                                               const ExcitationSessionReport& excitation,
                                                               const HoldoutDataset& holdout,
                                                               CandidateBenchmarkingReport* outReport)
{
    if (candidates_.empty() || holdout.empty())
    {
        // Sin candidatos o sin holdout: generar informe incompleto/inconclusive
        ModelEvaluationBuilder fallbackBuilder;
        fallbackBuilder.withTargetAudit(audit);
        fallbackBuilder.withExcitationReport(excitation);
        if (!holdout.empty())
            fallbackBuilder.withHoldoutDataset(&holdout);

        ModelEvaluation fallbackEval = fallbackBuilder.build();
        fallbackEval.decision.status = SelectionStatus::Inconclusive;
        fallbackEval.decision.rationale = candidates_.empty()
                                        ? "No candidate models registered for benchmarking"
                                        : "Holdout dataset is empty";
        fallbackEval.computeCanonicalHash();
        return fallbackEval;
    }

    const auto& holdoutPoints = holdout.accessForEvaluation();
    std::vector<CandidateBenchmarkResult> benchmarkResults;
    benchmarkResults.reserve(candidates_.size());

    // 1. Evaluación acústica de cada candidato fuera de muestra
    for (const auto& candidate : candidates_)
    {
        CandidateBenchmarkResult res;
        res.modelId = candidate->getModelId();
        res.architecture = candidate->getArchitectureName();
        res.footprint = candidate->getResourceFootprint();

        double totalSse = 0.0;
        double totalTargetEnergy = 0.0;
        size_t totalSamples = 0;
        double maxAbsError = 0.0;
        double sumResidualLag0 = 0.0;
        double sumResidualLag1 = 0.0;

        for (const auto& pt : holdoutPoints)
        {
            std::vector<float> predicted = candidate->predictResponse(pt);
            const auto& truth = pt.targetGroundTruthAudio;

            size_t n = std::min(predicted.size(), truth.size());
            if (n == 0) continue;

            totalSamples += n;
            for (size_t i = 0; i < n; ++i)
            {
                double e = static_cast<double>(truth[i]) - static_cast<double>(predicted[i]);
                double sig = static_cast<double>(truth[i]);

                totalSse += e * e;
                totalTargetEnergy += sig * sig;
                double absE = std::abs(e);
                if (absE > maxAbsError)
                    maxAbsError = absE;

                sumResidualLag0 += e * e;
                if (i > 0)
                {
                    double prevE = static_cast<double>(truth[i - 1]) - static_cast<double>(predicted[i - 1]);
                    sumResidualLag1 += e * prevE;
                }
            }
        }

        // Métricas numéricas
        if (totalSamples > 0 && totalTargetEnergy > 1e-15)
        {
            double esr = totalSse / totalTargetEnergy;
            res.metrics.errorToSignalRatioDb = (esr > 1e-15) ? (10.0 * std::log10(esr)) : -180.0;
            res.metrics.rootMeanSquareError = std::sqrt(totalSse / static_cast<double>(totalSamples));
            res.metrics.peakError = maxAbsError;
            res.metrics.rSquaredScore = std::max(0.0, 1.0 - (totalSse / totalTargetEnergy));
        }
        else
        {
            res.metrics.errorToSignalRatioDb = -180.0;
            res.metrics.rootMeanSquareError = 0.0;
            res.metrics.peakError = 0.0;
            res.metrics.rSquaredScore = 1.0;
        }

        // Diagnósticos de estructura del residuo
        if (sumResidualLag0 > 1e-12)
            res.diagnostics.peakAutocorrelation = std::abs(sumResidualLag1 / sumResidualLag0);
        else
            res.diagnostics.peakAutocorrelation = 0.0;

        res.diagnostics.residualCharacterization = (res.diagnostics.peakAutocorrelation < 0.15)
                                                ? "UnstructuredResidual"
                                                : "CorrelatedDynamicResidual";

        // Normalización de componentes de coste multiobjetivo [0, 1]
        // Error: 1.0 - R^2
        res.validationErrorScore = std::clamp(1.0 - res.metrics.rSquaredScore, 0.0, 1.0);

        // Coste computacional: CPU (60%), RAM (20%), Latencia (20%)
        double cpuNorm = std::clamp(res.footprint.cpuUsagePercentPerVoice / 10.0, 0.0, 1.0);
        double ramNorm = std::clamp(static_cast<double>(res.footprint.memoryBytes) / (2.0 * 1024.0 * 1024.0), 0.0, 1.0);
        double latNorm = std::clamp(static_cast<double>(res.footprint.introducedLatencySamples) / 128.0, 0.0, 1.0);
        res.computationalCostScore = 0.60 * cpuNorm + 0.20 * ramNorm + 0.20 * latNorm;

        // Incertidumbre: autocorrelación residual + varianza no explicada
        res.uncertaintyScore = std::clamp(0.60 * res.diagnostics.peakAutocorrelation + 0.40 * res.validationErrorScore, 0.0, 1.0);

        // Coste compuesto J(m)
        res.totalCompositeScore = computeCompositeCost(res.validationErrorScore,
                                                       res.computationalCostScore,
                                                       res.uncertaintyScore,
                                                       weights_.lambda,
                                                       weights_.mu);

        res.meetsStrictFidelity = (res.metrics.errorToSignalRatioDb <= -24.0 && res.metrics.rSquaredScore >= 0.95);
        benchmarkResults.push_back(res);
    }

    // 2. Líder de Fidelidad Pura (bestRawFidelityModelId)
    auto bestFidelityIt = std::min_element(benchmarkResults.begin(), benchmarkResults.end(),
        [](const CandidateBenchmarkResult& a, const CandidateBenchmarkResult& b) {
            return a.metrics.errorToSignalRatioDb < b.metrics.errorToSignalRatioDb;
        });
    std::string bestRawFidelityId = bestFidelityIt->modelId;
    double bestRawFidelityEsr = bestFidelityIt->metrics.errorToSignalRatioDb;
    bestFidelityIt->isRawFidelityLeader = true;

    // 3. Ordenar candidatos por puntuación compuesta J(m) ascendente (menor coste = óptimo)
    std::sort(benchmarkResults.begin(), benchmarkResults.end(),
        [](const CandidateBenchmarkResult& a, const CandidateBenchmarkResult& b) {
            return a.totalCompositeScore < b.totalCompositeScore;
        });

    const auto& winner = benchmarkResults.front();
    std::string runnerUpId;
    double runnerUpGap = 1.0;
    if (benchmarkResults.size() > 1)
    {
        runnerUpId = benchmarkResults[1].modelId;
        runnerUpGap = benchmarkResults[1].totalCompositeScore - winner.totalCompositeScore;
    }

    bool isInconclusive = (benchmarkResults.size() > 1 && runnerUpGap < weights_.uncertaintyMargin);

    // 4. Diagnóstico de Residuo Estructurado
    std::string residualDiag;
    if (winner.diagnostics.peakAutocorrelation >= 0.15)
    {
        std::ostringstream ss;
        ss << "Residuo dinámico correlacionado (lag-1 autocorrelación = "
           << std::fixed << std::setprecision(3) << winner.diagnostics.peakAutocorrelation
           << "): se detecta memoria o histéresis interna. Se recomienda considerar arquitectura recurrente (NAM/LSTM).";
        residualDiag = ss.str();
    }
    else
    {
        std::ostringstream ss;
        ss << "Residuo plano no estructurado (lag-1 autocorrelación = "
           << std::fixed << std::setprecision(3) << winner.diagnostics.peakAutocorrelation
           << "): compatible con modelo estático dentro de la resolución de medición.";
        residualDiag = ss.str();
    }

    // 5. Informe opcional detallado
    if (outReport != nullptr)
    {
        outReport->preset = preset_;
        outReport->weights = weights_;
        outReport->candidates = benchmarkResults;
        outReport->bestRawFidelityModelId = bestRawFidelityId;
        outReport->recommendedTargetModelId = winner.modelId;
        outReport->runnerUpModelId = runnerUpId;
        outReport->runnerUpGap = runnerUpGap;
        outReport->isInconclusive = isInconclusive;
        outReport->residualDiagnosis = residualDiag;

        std::ostringstream rat;
        rat << "Evaluados " << benchmarkResults.size() << " candidatos bajo preset "
            << optimizationPresetToString(preset_) << ". Ganador óptimo: " << winner.modelId
            << " (Score=" << std::fixed << std::setprecision(4) << winner.totalCompositeScore
            << "), Líder fidelidad pura: " << bestRawFidelityId
            << " (ESR=" << std::setprecision(1) << bestRawFidelityEsr << " dB).";
        if (isInconclusive)
            rat << " Decisión Inconclusive: diferencia con el segundo (" << runnerUpId
                << ") es " << std::setprecision(4) << runnerUpGap << " < margen " << weights_.uncertaintyMargin << ".";
        outReport->decisionRationale = rat.str();
    }

    // 6. Construcción del objeto canónico ModelEvaluation para el candidato ganador
    auto winnerCandidateIt = std::find_if(candidates_.begin(), candidates_.end(),
        [&](const std::shared_ptr<ICandidateModelEvaluator>& c) {
            return c->getModelId() == winner.modelId;
        });

    ModelArtifactDescriptor artifact = (*winnerCandidateIt)->getArtifactDescriptor();
    ModelEvaluationBuilder evalBuilder;
    evalBuilder.withTargetAudit(audit)
               .withExcitationReport(excitation)
               .withModelArtifact(artifact)
               .withHoldoutDataset(&holdout)
               .withCandidateEvaluator(winnerCandidateIt->get())
               .withResourceFootprint(winner.footprint);

    ModelEvaluation eval = evalBuilder.build();

    // Rellenar información extendida del selector multiobjetivo
    eval.decision.recommendedModelId = winner.modelId;
    eval.decision.bestRawFidelityModelId = bestRawFidelityId;
    eval.decision.runnerUpModelId = runnerUpId;
    eval.decision.selectionScore = winner.totalCompositeScore;
    eval.decision.runnerUpGap = runnerUpGap;
    eval.decision.uncertaintyMargin = weights_.uncertaintyMargin;

    if (isInconclusive)
    {
        eval.decision.status = SelectionStatus::Inconclusive;
        std::ostringstream ss;
        ss << "Margen frente al segundo candidato ('" << runnerUpId << "') es "
           << std::fixed << std::setprecision(4) << runnerUpGap
           << " < margen de incertidumbre " << weights_.uncertaintyMargin
           << " (modelos estadísticamente equivalentes).";
        eval.decision.rationale = ss.str();
        eval.warnings.push_back("RunnerUpMarginIndecision: Gap below uncertainty threshold");
    }

    eval.diagnostics = winner.diagnostics;
    eval.computeCanonicalHash();

    return eval;
}

} // namespace abdaudiolab::synth
