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
 * @class LabEnvelopeCurveAnalyzer
 * @brief Motor offline para responder a la pregunta central de la Fase 4:
 *
 *        ¿Es el byte neutro 128 una etapa lineal en la curva de la envolvente?
 *
 *        - Calcula la recta de mínimos cuadrados Y = mX + c.
 *        - Obtiene los residuos muestra a muestra y el coeficiente R^2.
 *        - Detecta breakpoints por desaceleración de la pendiente local (\(\Delta Hz / \Delta t \to 0\))
 *          combinada con signo persistente de los residuos.
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

private:
    static EnvelopeRegressionResult computeLeastSquares(const std::vector<EnvelopePoint>& points);
    static EnvelopeBreakpointResult findFirstSaturationBreakpoint(
        const std::vector<EnvelopePoint>& points,
        const std::vector<float>& residuals,
        float slopeWindowSamples,
        float residualPersistenceWindow,
        float slopeDropRatioThreshold);
};

} // namespace abdaudiolab::math
