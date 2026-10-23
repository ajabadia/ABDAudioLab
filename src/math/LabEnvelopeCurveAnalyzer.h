// ==============================================================================
// src/math/LabEnvelopeCurveAnalyzer.h
// Fase 4 — Motor utilitario para análisis offline de trayectorias de curvas
//          de envolvente / barrido de Hz mediante regresión lineal y detección
//          de breakpoints por desaceleración de pendiente local.
// ==============================================================================

#pragma once

#include "../math/LabAnalyticEngine.h"

#include <vector>
#include <string>

namespace abdaudiolab::math
{

/**
 * @brief Punto de una curva de barrido para análisis offline de envolvente.
 *
 * Mantiene la misma semántica que PreScanPoint pero con nombre explícito para
 * el flujo de la Fase 4: eje X = tiempo (segundos), eje Y = Hz medidos.
 */
struct EnvelopePoint
{
    float timeSec   { 0.0f };
    float measuredHz { 0.0f };

    EnvelopePoint() = default;
    EnvelopePoint(float timeSec, float measuredHz)
        : timeSec(timeSec), measuredHz(measuredHz)
    {
    }
};

/**
 * @brief Resultado de la regresión lineal de una curva de barrido.
 */
struct EnvelopeRegressionResult
{
    float slope         { 0.0f };   // m  (Hz / s)
    float intercept     { 0.0f };   // c  (Hz)
    float rSquared      { 0.0f };   // R^2
    float residualMean  { 0.0f };
    float residualStdDev { 0.0f };

    std::vector<float> residuals;   // Y_measured - Y_model, sample a sample
    std::vector<float> fittedHz;    // Y_model(t)

    bool valid { false };
};

/**
 * @brief Resultado de detección de saturación / breakpoint en la curva.
 */
struct EnvelopeBreakpointResult
{
    float breakpointTimeSec { 0.0f };
    float localSlopeHzPerSec { 0.0f };
    float residualSignMean { 0.0f };
    bool detected { false };
    std::string reason;
};

/**
 * @brief Modo de estimación adaptativa de decaimiento acústico/envolvente ISO 3382 (HITO-DSP-24.4).
 */
enum class ReverbEstimatorMode
{
    None,
    T30, /**< Regresión lineal entre -5 dB y -35 dB (extrapolada a -60 dB). */
    T20, /**< Regresión lineal entre -5 dB y -25 dB (fallback por rango dinámico limitado). */
    T10  /**< Regresión lineal entre -5 dB y -15 dB (fallback de bajo SNR). */
};

/**
 * @brief Resultado de la integral retrógrada de Schroeder con truncamiento por Noise-Floor Knee
 * y regresión adaptativa T30/T20/T10 con validación R^2 (HITO-DSP-24.4).
 */
struct SchroederDecayResult
{
    std::vector<float> edcDb;          /**< Curva de decaimiento energético normalizada L_EDC[n] en dB (desde pico hasta knee). */
    size_t peakSampleIndex { 0 };      /**< Índice del pico inicial desde el que arranca la integración. */
    size_t kneeSampleIndex { 0 };      /**< Índice de truncamiento N_knee (+6 dB sobre suelo de ruido estacionario). */
    float noiseFloorDbfs { -120.0f };  /**< Potencia media del suelo de ruido en cola (segunda mitad) en dBFS. */
    float rt60Seconds { 0.0f };        /**< Tiempo de reverberación / decaimiento extrapolado a -60 dB: RT60 = -60 / m. */
    float slopeDbPerSec { 0.0f };      /**< Pendiente ajustada m (dB/s). */
    float interceptDb { 0.0f };        /**< Intercepto ajustado c (dB). */
    float rSquared { 0.0f };           /**< Coeficiente de determinación R^2 del tramo seleccionado. */
    ReverbEstimatorMode modeUsed { ReverbEstimatorMode::None };
    bool valid { false };
};

/**
 * @class LabEnvelopeCurveAnalyzer
 * @brief Motor offline para responder a la pregunta central de la Fase 4:
 *
 *        ¿Es el byte neutro 128 una etapa lineal en la curva de la envolvente?
 *
 *        - Calcula la recta de mínimos cuadrados Y = mX + c.
 *        - Obtiene los residuos muestra a muestra y el coeficiente R^2.
 *        - Detecta breakpoints por desaceleración de la pendiente local (\(\Delta Hz / \Delta t \to 0\))
 *          combinada con signo persistente de los residuos.
 *        - Integra decaimientos mediante Integral Retrógrada de Schroeder con truncamiento
 *          por Noise-Floor Knee y estimación adaptativa T30/T20/T10 (HITO-DSP-24.4).
 */
class LabEnvelopeCurveAnalyzer
{
public:
    LabEnvelopeCurveAnalyzer() = default;
    ~LabEnvelopeCurveAnalyzer() = default;

    // ------------------------------------------------------------------
    // Regresión lineal sobre trayectoria
    // ------------------------------------------------------------------
    static EnvelopeRegressionResult analyzeTrajectory(const std::vector<EnvelopePoint>& points);

    // ------------------------------------------------------------------
    // Integral Retrógrada de Schroeder con Noise-Floor Knee y T30/T20/T10 (HITO-DSP-24.4)
    // ------------------------------------------------------------------
    static SchroederDecayResult analyzeSchroederDecay(const std::vector<float>& impulseResponse,
                                                      double sampleRate,
                                                      float minRSquared = 0.95f);

    // ------------------------------------------------------------------
    // Detección de breakpoint / saturación
    // ------------------------------------------------------------------
    static EnvelopeBreakpointResult detectBreakpoint(const std::vector<EnvelopePoint>& points,
                                                     const EnvelopeRegressionResult& reg,
                                                     float slopeWindowSamples = 12,
                                                     float residualPersistenceWindow = 9,
                                                     float slopeDropRatioThreshold = 0.25f);

    // ------------------------------------------------------------------
    // Utilidades para reportes
    // ------------------------------------------------------------------
    [[nodiscard]] static float residualsMean(const std::vector<float>& residuals) noexcept;
    [[nodiscard]] static float residualsStdDev(const std::vector<float>& residuals) noexcept;
};

} // namespace abdaudiolab::math

