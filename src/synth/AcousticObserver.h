#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <numeric>
#include "ExperimentPlan.h"
#include "SynthPitchEstimator.h"
#include "SynthEnvelopeAnalyzer.h"

namespace abdaudiolab::synth
{

/**
 * @brief Rasgos acústicos cuantificados dentro de una ventana de observación temporal.
 */
struct WindowAcousticFeatures
{
    std::string windowId;
    std::string targetParameterId;
    std::string domain;

    double spectralCentroidHz { 0.0 };
    double rmsDb { -180.0 };
    double peakDb { -180.0 };
    double pitchHz { 0.0 };
    double pitchCents { 0.0 };
    double attackMs { 0.0 };
    bool clippingDetected { false };
    bool hasAudibleEffect { false };

    ObservationOutcome outcome { ObservationOutcome::Inconclusive };
    std::string diagnosisDetails;
};

/**
 * @brief Observación individual de un escalón de parámetro.
 */
struct StepObservation
{
    double parameterValue { 0.0 };
    WindowAcousticFeatures features;
};

/**
 * @brief Estimación de sensibilidad direccional / Jacobiano con pares emparejados y varianza.
 */
struct JacobianEstimate
{
    std::string parameterId;
    double delta { 0.05 };
    double dCentroid_dParam { 0.0 };
    double dRms_dParam { 0.0 };
    double estimatorVarianceCentroid { 0.0 };
    int pairedTrialsCount { 1 };
    bool isStatisticallySignificant { false };
};

/**
 * @brief Análisis diferencial por pares Delta y = y(p + Delta p) - y(p) y cancelación.
 */
struct PairwiseDifferentialAnalysis
{
    std::string parameterId;
    double deltaValue { 0.0 };
    double deltaRmsDb { -180.0 };
    double baselineRmsDb { -180.0 };
    double snrDb { 0.0 };
    PairwiseSubtractionReliability reliability { PairwiseSubtractionReliability::NotReliable };
    std::vector<float> differentialAudio;
};

/**
 * @brief Análisis de respuesta del target a una rampa continua de parámetro.
 */
struct RampProcessingAnalysis
{
    std::string parameterId;
    std::string requestedCurve { "LinearRamp" };
    std::string transportResolution { "TransportedSampleAccurate" };
    std::string observedResponseResolution { "SampleContinuous" };
    RampProcessingResponse responseType { RampProcessingResponse::TransportedSampleAccurate };
    double effectiveLatencyMs { 0.0 };
    bool smoothingDetected { false };
    double effectiveSmoothingTimeMs { 0.0 };
    bool blockQuantizationDetected { false };
    double requestedSlope { 0.0 };
    double observedSlope { 0.0 };
    double monotonicityScore { 0.0 };
    int observedStepCount { 0 };

    std::string observedFeatureMonotonicity { "PASS" };
    std::string physicalParameterMonotonicity { "NOT_ESTABLISHED" };
};

/**
 * @brief Observador acústico metrológico para validar el impacto físico de los controles.
 */
class AcousticObserver
{
public:
    explicit AcousticObserver(double sampleRate = 96000.0);

    /**
     * @brief Extrae los rasgos acústicos de cada ventana de observación del plan.
     */
    [[nodiscard]] std::vector<WindowAcousticFeatures> analyzeWindows(
        const ExperimentPlan& plan,
        const TargetExecutionTrace& trace,
        const ExcitationUncertaintyBudget& budget) const;

    /**
     * @brief Analiza respuestas por escalones para verificar monotonicidad y rango dinámico.
     */
    [[nodiscard]] std::vector<StepObservation> analyzeStepResponses(
        const ExperimentPlan& plan,
        const TargetExecutionTrace& trace,
        const ExcitationUncertaintyBudget& budget) const;

    /**
     * @brief Analiza la respuesta del target ante una rampa de parámetro.
     */
    [[nodiscard]] RampProcessingAnalysis analyzeRampResponse(
        const ExperimentPlan& plan,
        const TargetExecutionTrace& trace) const;

    /**
     * @brief Calcula la estimación central del Jacobiano con tomas emparejadas.
     */
    [[nodiscard]] JacobianEstimate estimateJacobian(
        const ExperimentPlan& plan,
        const TargetExecutionTrace& trace,
        const ExcitationUncertaintyBudget& budget) const;

    /**
     * @brief Evalúa la resta diferencial por pares Delta y = y(p + Delta p) - y(p).
     */
    [[nodiscard]] PairwiseDifferentialAnalysis analyzePairwiseDifferential(
        const ExperimentPlan& plan,
        const TargetExecutionTrace& trace,
        const ExcitationUncertaintyBudget& budget) const;

    /**
     * @brief Calcula el centroide espectral mediante DFT/FFT sobre una ventana de audio.
     */
    [[nodiscard]] double computeSpectralCentroid(const float* audio, size_t count) const noexcept;

private:
    double sampleRate_ { 96000.0 };
    SynthEnvelopeAnalyzer envelopeAnalyzer_;
};

} // namespace abdaudiolab::synth
