/**
 * @file ThermalDriftAnalyzer.h
 * @brief Evaluates thermal drift and component hysteresis (Delta f / Delta T) across profiling runs.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../export/LutExporter.h"

namespace abdaudiolab::math
{

/**
 * @struct ThermalDriftReport
 * @brief Holds quantified drift metrics across session timelines.
 */
struct ThermalDriftReport
{
    bool validComparison { false };
    float deltaTempCelsius { 0.0f };
    float cutoffDriftHz { 0.0f };
    float cutoffDriftHzPerCelsius { 0.0f };
    float gainDriftDb { 0.0f };
    float gainDriftDbPerCelsius { 0.0f };
    float thdDriftPercent { 0.0f };
    int pointsEvaluated { 0 };
};

/**
 * @class ThermalDriftAnalyzer
 * @brief Analyzes frequency cutoff and level drift across multiple passes or warm-up intervals.
 */
class ThermalDriftAnalyzer
{
public:
    /**
     * @brief Compares initial vs final measurement passes to calculate thermal sensitivity.
     * @param initialPoints First set of points (cold / initial state).
     * @param finalPoints Second set of points (warmed-up / final state).
     * @param initialTempC Ambient temperature during initial run.
     * @param finalTempC Ambient temperature during final run.
     * @return ThermalDriftReport with Hz/°C and dB/°C metrics.
     */
    static ThermalDriftReport analyzeDrift(const std::vector<exporting::MeasuredPoint>& initialPoints,
                                          const std::vector<exporting::MeasuredPoint>& finalPoints,
                                          float initialTempC,
                                          float finalTempC)
    {
        ThermalDriftReport report;
        if (initialPoints.empty() || finalPoints.empty())
            return report;

        report.deltaTempCelsius = finalTempC - initialTempC;
        size_t count = std::min(initialPoints.size(), finalPoints.size());

        float sumCutoffDelta = 0.0f;
        float sumGainDelta = 0.0f;
        float sumThdDelta = 0.0f;

        for (size_t i = 0; i < count; ++i)
        {
            float f0 = initialPoints[i].muSigmaValue.mean;
            float f1 = finalPoints[i].muSigmaValue.mean;
            sumCutoffDelta += (f1 - f0);

            float g0 = initialPoints[i].secondaryValue.mean;
            float g1 = finalPoints[i].secondaryValue.mean;
            sumGainDelta += (g1 - g0);

            float thd0 = initialPoints[i].thdPercent;
            float thd1 = finalPoints[i].thdPercent;
            sumThdDelta += (thd1 - thd0);
        }

        report.pointsEvaluated = static_cast<int>(count);
        report.cutoffDriftHz = sumCutoffDelta / static_cast<float>(count);
        report.gainDriftDb = sumGainDelta / static_cast<float>(count);
        report.thdDriftPercent = sumThdDelta / static_cast<float>(count);
        report.validComparison = true;

        if (std::abs(report.deltaTempCelsius) > 0.05f)
        {
            report.cutoffDriftHzPerCelsius = report.cutoffDriftHz / report.deltaTempCelsius;
            report.gainDriftDbPerCelsius = report.gainDriftDb / report.deltaTempCelsius;
        }

        return report;
    }
};

} // namespace abdaudiolab::math
