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

SchroederDecayResult LabEnvelopeCurveAnalyzer::analyzeSchroederDecay(
    const std::vector<float>& impulseResponse,
    double sampleRate,
    float minRSquared)
{
    SchroederDecayResult res;
    if (impulseResponse.size() < 64 || sampleRate <= 0.0)
        return res;

    const size_t N = impulseResponse.size();

    // 1. Locate impulse peak index n_peak
    size_t peakIdx = 0;
    double maxSq = 0.0;
    std::vector<double> sq(N, 0.0);
    for (size_t i = 0; i < N; ++i)
    {
        const double v = std::isfinite(impulseResponse[i]) ? static_cast<double>(impulseResponse[i]) : 0.0;
        const double e = v * v;
        sq[i] = e;
        if (e > maxSq)
        {
            maxSq = e;
            peakIdx = i;
        }
    }

    if (maxSq <= 1e-24 || peakIdx + 16 >= N)
        return res;

    res.peakSampleIndex = peakIdx;

    // 2. Estimate stationary tail noise floor power sigma_noise^2 in the second half [N/2, N)
    const size_t halfStart = std::max(peakIdx + 1, N / 2);
    double sumSecondHalf = 0.0;
    for (size_t i = halfStart; i < N; ++i)
        sumSecondHalf += sq[i];
    const double meanSecondHalf = sumSecondHalf / static_cast<double>(N - halfStart);

    const size_t quarterStart = std::max(peakIdx + 1, (3 * N) / 4);
    double sumLastQuarter = 0.0;
    for (size_t i = quarterStart; i < N; ++i)
        sumLastQuarter += sq[i];
    const double meanLastQuarter = sumLastQuarter / static_cast<double>(N - quarterStart);

    // Use the more stationary tail estimate if the decay extends slightly past N/2
    const double noisePower = (meanLastQuarter < 0.5 * meanSecondHalf) ? meanLastQuarter : meanSecondHalf;
    res.noiseFloorDbfs = (noisePower > 1e-20)
        ? static_cast<float>(10.0 * std::log10(noisePower))
        : -200.0f;

    // 3. Detect Noise-Floor Knee N_knee (+6 dB above stationary noise floor: P_knee = 4 * sigma_noise^2)
    const size_t winLen = std::max<size_t>(1, static_cast<size_t>(std::round(0.005 * sampleRate)));
    size_t kneeIdx = N - 1;
    double peakWindowPower = maxSq;

    if (noisePower > 1e-18)
    {
        const double kneeThreshold = 4.0 * noisePower; // +6.02 dB above noise floor
        std::vector<double> prefix(N + 1, 0.0);
        for (size_t i = 0; i < N; ++i)
            prefix[i + 1] = prefix[i] + sq[i];

        auto windowPowerAt = [&](size_t idx) -> double {
            const size_t endIdx = std::min(N, idx + winLen);
            const size_t count = std::max<size_t>(1, endIdx - idx);
            return (prefix[endIdx] - prefix[idx]) / static_cast<double>(count);
        };

        peakWindowPower = std::max(windowPowerAt(peakIdx), 1e-20);

        // Scan forward from peakIdx with a short hold hysteresis to find where the envelope drops below kneeThreshold
        size_t foundKnee = peakIdx;
        for (size_t i = peakIdx; i < N; ++i)
        {
            if (windowPowerAt(i) >= kneeThreshold)
                foundKnee = i;
            else if (i > foundKnee + winLen * 4)
                break; // Sustained drop into stationary noise floor
        }
        kneeIdx = std::max(peakIdx + 8, std::min(N - 1, foundKnee));
    }

    res.kneeSampleIndex = kneeIdx;

    // 4. Backward Schroeder Integration from N_knee down to n_peak
    const size_t edcLen = (kneeIdx >= peakIdx) ? (kneeIdx - peakIdx + 1) : 0;
    if (edcLen < 8)
        return res;

    std::vector<double> edcLinear(edcLen, 0.0);
    double accum = 0.0;
    for (size_t offset = edcLen; offset-- > 0;)
    {
        // Subtract stationary noise power inside the signal window so noise bias does not flatten the curve
        const double cleanSq = std::max(0.0, sq[peakIdx + offset] - noisePower);
        accum += cleanSq;
        edcLinear[offset] = accum;
    }

    const double totalEnergy = edcLinear[0];
    if (totalEnergy <= 1e-24)
        return res;

    res.edcDb.resize(edcLen, -120.0f);
    for (size_t i = 0; i < edcLen; ++i)
    {
        const double ratio = std::max(edcLinear[i] / totalEnergy, 1e-12);
        res.edcDb[i] = static_cast<float>(10.0 * std::log10(ratio));
    }

    // Dynamic range available above the knee (+6 dB safety margin above noise floor)
    const double availableDynamicRangeDb = (noisePower > 1e-18)
        ? (10.0 * std::log10(std::max(peakWindowPower / (4.0 * noisePower), 1.0)))
        : 120.0;

    // 5. Adaptive linear regression T30 [-5, -35] -> T20 [-5, -25] -> T10 [-5, -15] with R^2 >= minRSquared
    struct CandidateRange
    {
        ReverbEstimatorMode mode;
        float topDb;
        float bottomDb;
        double requiredDynamicRangeDb;
    };

    const CandidateRange candidates[] = {
        { ReverbEstimatorMode::T30, -5.0f, -35.0f, 35.0 },
        { ReverbEstimatorMode::T20, -5.0f, -25.0f, 25.0 },
        { ReverbEstimatorMode::T10, -5.0f, -15.0f, 15.0 }
    };

    for (const auto& cand : candidates)
    {
        if (availableDynamicRangeDb < cand.requiredDynamicRangeDb)
            continue;

        std::vector<EnvelopePoint> pts;
        pts.reserve(edcLen);
        bool enteredBand = false;

        for (size_t i = 0; i < edcLen; ++i)
        {
            const float db = res.edcDb[i];
            if (!enteredBand && db <= cand.topDb)
                enteredBand = true;

            if (enteredBand)
            {
                if (db < cand.bottomDb)
                    break;
                const float tSec = static_cast<float>(static_cast<double>(i) / sampleRate);
                pts.emplace_back(tSec, db);
            }
        }

        if (pts.size() < 4)
            continue;

        const auto reg = analyzeTrajectory(pts);
        if (reg.valid && reg.slope < -1e-3f && reg.rSquared >= minRSquared)
        {
            res.slopeDbPerSec = reg.slope;
            res.interceptDb = reg.intercept;
            res.rSquared = reg.rSquared;
            res.rt60Seconds = -60.0f / reg.slope;
            res.modeUsed = cand.mode;
            res.valid = true;
            return res;
        }
    }

    return res;
}

} // namespace abdaudiolab::math

