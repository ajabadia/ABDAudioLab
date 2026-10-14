// ==============================================================================
// src/math/LabEnvelopeCurveAnalyzer.cpp
// Fase 4 — Implementación del motor de análisis offline de curvas de envolvente.
// ==============================================================================

#include "LabEnvelopeCurveAnalyzer.h"

#include <cmath>
#include <numeric>
#include <algorithm>

namespace abdaudiolab::math
{

// -----------------------------------------------------------------------------
// Regresión lineal Y = m X + c sobre (timeSec, measuredHz)
// -----------------------------------------------------------------------------

static float safeDivide(float num, float den) noexcept
{
    if (std::abs(den) < 1e-12f)
        return 0.0f;
    return num / den;
}

EnvelopeRegressionResult LabEnvelopeCurveAnalyzer::analyzeTrajectory(
    const std::vector<EnvelopePoint>& points)
{
    EnvelopeRegressionResult result;
    result.valid = false;

    if (points.size() < 2)
        return result;

    const size_t n = points.size();

    double meanX = 0.0;
    double meanY = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        meanX += points[i].timeSec;
        meanY += points[i].measuredHz;
    }
    meanX /= static_cast<double>(n);
    meanY /= static_cast<double>(n);

    double num  = 0.0;
    double den  = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double dx = static_cast<double>(points[i].timeSec) - meanX;
        const double dy = static_cast<double>(points[i].measuredHz) - meanY;
        num += dx * dy;
        den += dx * dx;
    }

    if (den <= 0.0)
        return result;

    const double m = num / den;
    const double c = meanY - m * meanX;

    result.slope     = static_cast<float>(m);
    result.intercept = static_cast<float>(c);

    // Residuos e Y ajustado.
    result.residuals.resize(n);
    result.fittedHz.resize(n);

    double ssRes  = 0.0;
    double ssTot  = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double yFit = static_cast<double>(result.intercept)
                          + static_cast<double>(result.slope) * points[i].timeSec;
        result.fittedHz[i] = static_cast<float>(yFit);

        const double resid = static_cast<double>(points[i].measuredHz) - yFit;
        result.residuals[i] = static_cast<float>(resid);

        ssRes += resid * resid;
        const double dy = static_cast<double>(points[i].measuredHz) - meanY;
        ssTot += dy * dy;
    }

    result.residualMean   = static_cast<float>(ssRes / static_cast<double>(n));
    result.residualStdDev = static_cast<float>(std::sqrt(ssRes / static_cast<double>(n)));

    if (ssTot <= 0.0)
    {
        result.rSquared = (ssRes <= 0.0) ? 1.0f : 0.0f;
        result.valid    = true;
        return result;
    }

    result.rSquared = static_cast<float>(1.0 - (ssRes / ssTot));
    result.valid    = true;

    return result;
}

// -----------------------------------------------------------------------------
// Estadísticos de residuos
// -----------------------------------------------------------------------------

float LabEnvelopeCurveAnalyzer::residualsMean(const std::vector<float>& residuals) noexcept
{
    if (residuals.empty())
        return 0.0f;

    double sum = 0.0;
    for (float r : residuals)
        sum += static_cast<double>(r);

    return static_cast<float>(sum / static_cast<double>(residuals.size()));
}

float LabEnvelopeCurveAnalyzer::residualsStdDev(const std::vector<float>& residuals) noexcept
{
    if (residuals.size() < 2)
        return 0.0f;

    double mean = 0.0;
    for (float r : residuals)
        mean += static_cast<double>(r);
    mean /= static_cast<double>(residuals.size());

    double var = 0.0;
    for (float r : residuals)
    {
        const double d = static_cast<double>(r) - mean;
        var += d * d;
    }

    return static_cast<float>(std::sqrt(var / static_cast<double>(residuals.size())));
}

// -----------------------------------------------------------------------------
// Detección de breakpoint: desaceleración de pendiente local + signo persistente
// -----------------------------------------------------------------------------

namespace
{
    // Pendiente local en una ventana de samples sobre los puntos medidos.
    float localSlopeHzPerSec(const std::vector<EnvelopePoint>& points,
                             size_t start,
                             size_t window)
    {
        if (start + window > points.size())
            window = static_cast<size_t>(points.size() - start);

        if (window < 2)
            return 0.0f;

        const float tFirst = points[start].timeSec;
        const float tLast  = points[start + window - 1].timeSec;
        const float dt     = tLast - tFirst;
        if (dt <= 0.0f)
            return 0.0f;

        const float yFirst = points[start].measuredHz;
        const float yLast  = points[start + window - 1].measuredHz;

        return (yLast - yFirst) / dt;
    }

    // Signo medio de los residuos en una ventana.
    float residualSignMean(const std::vector<float>& residuals,
                           size_t start,
                           size_t window)
    {
        if (start + window > residuals.size())
            window = static_cast<size_t>(residuals.size() - start);

        if (window == 0)
            return 0.0f;

        double sum = 0.0;
        for (size_t i = start; i < start + window; ++i)
            sum += (residuals[i] >= 0.0f) ? 1.0 : -1.0;

        return static_cast<float>(sum / static_cast<double>(window));
    }
}

EnvelopeBreakpointResult LabEnvelopeCurveAnalyzer::detectBreakpoint(
    const std::vector<EnvelopePoint>& points,
    const EnvelopeRegressionResult& reg,
    float slopeWindowSamples,
    float residualPersistenceWindow,
    float slopeDropRatioThreshold)
{
    EnvelopeBreakpointResult result;
    result.detected = false;

    if (points.size() < 4 || !reg.valid)
        return result;

    const size_t slopeWin = static_cast<size_t>(std::max(2.0f, slopeWindowSamples));
    const size_t residWin = static_cast<size_t>(std::max(2.0f, residualPersistenceWindow));

    // Pendiente de referencia: ventana temprana de la curva, antes del posible
    // cuello de botella / meseta.
    const float earlySlope = localSlopeHzPerSec(points, 0, slopeWin);
    if (std::abs(earlySlope) < 1e-6f)
        return result;

    const float earlySlopeAbs = std::abs(earlySlope);

    for (size_t i = 1; i + slopeWin <= points.size(); ++i)
    {
        const float local = localSlopeHzPerSec(points, i, slopeWin);
        const float localAbs = std::abs(local);

        // Desaceleración de pendiente: caída relativa respecto a la referencia.
        const float dropRatio = earlySlopeAbs > 0.0f
            ? (localAbs / earlySlopeAbs)
            : 0.0f;

        if (dropRatio < slopeDropRatioThreshold)
        {
            // Confirmación por signo persistente en residuos alrededor del punto.
            const float signMean = residualSignMean(reg.residuals, i, residWin);
            const bool persistentSign = std::abs(signMean) > 0.5f;

            if (persistentSign)
            {
                result.detected           = true;
                result.breakpointTimeSec  = points[i].timeSec;
                result.localSlopeHzPerSec = local;
                result.residualSignMean   = signMean;
                result.reason = "Slope deceleration below threshold with persistent residual sign";
                return result;
            }
        }
    }

    return result;
}

} // namespace abdaudiolab::math
