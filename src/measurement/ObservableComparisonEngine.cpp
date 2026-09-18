/**
 * @file ObservableComparisonEngine.cpp
 * @brief Implementation of common-space observable comparison with robust metrics and zero-variance detection.
 * @author ABDSynths
 * @date 2026
 */

#include "ObservableComparisonEngine.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::measurement
{

namespace
{

struct NormalizedSeries
{
    std::vector<double> timesMs;
    std::vector<std::optional<double>> normValues;
    bool isConstant { false };
    double peakTimeMs { 0.0 };
    double firstValidTimeMs { 0.0 };
    double durationMs { 0.0 };
};

NormalizedSeries extractAndNormalize(const EnvelopeTrajectory& traj)
{
    NormalizedSeries s;
    if (traj.points.empty())
        return s;

    s.timesMs.reserve(traj.points.size());
    s.normValues.reserve(traj.points.size());

    double minVal = 1e12;
    double maxVal = -1e12;
    bool hasValid = false;

    for (const auto& pt : traj.points)
    {
        s.timesMs.push_back(pt.timeMs);
        if (pt.value.has_value())
        {
            minVal = std::min(minVal, *pt.value);
            maxVal = std::max(maxVal, *pt.value);
            hasValid = true;
        }
    }

    if (!hasValid || (maxVal - minVal) < 1e-9)
    {
        s.isConstant = true;
        for (const auto& pt : traj.points)
        {
            if (pt.value.has_value())
                s.normValues.push_back(0.5);
            else
                s.normValues.push_back(std::nullopt);
        }
    }
    else
    {
        double range = maxVal - minVal;
        double maxFound = -1e12;
        for (const auto& pt : traj.points)
        {
            if (pt.value.has_value())
            {
                double norm = (*pt.value - minVal) / range;
                s.normValues.push_back(norm);
                if (norm > maxFound)
                {
                    maxFound = norm;
                    s.peakTimeMs = pt.timeMs;
                }
            }
            else
            {
                s.normValues.push_back(std::nullopt);
            }
        }
    }

    bool foundFirst = false;
    for (const auto& pt : traj.points)
    {
        if (pt.value.has_value())
        {
            if (!foundFirst)
            {
                s.firstValidTimeMs = pt.timeMs;
                foundFirst = true;
            }
            s.durationMs = pt.timeMs - s.firstValidTimeMs;
        }
    }

    return s;
}

std::optional<double> interpolateAt(const NormalizedSeries& ref, double targetTimeMs)
{
    if (ref.timesMs.empty())
        return std::nullopt;

    if (targetTimeMs < ref.timesMs.front() || targetTimeMs > ref.timesMs.back())
        return std::nullopt;

    auto it = std::lower_bound(ref.timesMs.begin(), ref.timesMs.end(), targetTimeMs);
    if (it == ref.timesMs.end())
        return std::nullopt;

    size_t idx1 = static_cast<size_t>(std::distance(ref.timesMs.begin(), it));
    if (idx1 == 0)
        return ref.normValues[0];

    size_t idx0 = idx1 - 1;
    double t0 = ref.timesMs[idx0];
    double t1 = ref.timesMs[idx1];

    if (!ref.normValues[idx0].has_value() || !ref.normValues[idx1].has_value())
        return std::nullopt;

    double v0 = *ref.normValues[idx0];
    double v1 = *ref.normValues[idx1];

    if (std::abs(t1 - t0) < 1e-9)
        return v0;

    double frac = (targetTimeMs - t0) / (t1 - t0);
    return v0 + frac * (v1 - v0);
}

} // anonymous namespace

ObservableComparisonReport ObservableComparisonEngine::compare(
    const EnvelopeTrajectory& observedTrajectory,
    const std::optional<EnvelopeTrajectory>& referenceTrajectory,
    const std::string& nativePath,
    const ComparisonConfig& config) noexcept
{
    ObservableComparisonReport report;
    report.observableDomain = envelopeDomainToString(observedTrajectory.domain);
    report.nativeParameterPath = nativePath;
    report.comparisonSpace = config.comparisonSpace;
    report.normalizationMethod = config.normalizationMethod;
    report.nativeTrajectoryDerivation = "declared_proxy";
    report.phaseDistortionProxy = "not_claimed";
    report.comparisonLabel = "observable_agreement";
    report.sourceGridId = observedTrajectory.temporalGridId;

    if (!referenceTrajectory.has_value())
    {
        report.comparisonStatus = "not_compared";
        report.limitations = "No observable reference trajectory provided for this native parameter.";
        return report;
    }

    report.targetGridId = referenceTrajectory->temporalGridId;

    if (observedTrajectory.points.empty() || referenceTrajectory->points.empty())
    {
        report.comparisonStatus = "not_observable";
        report.limitations = "One or both trajectories contain zero observation points.";
        return report;
    }

    auto obsNorm = extractAndNormalize(observedTrajectory);
    auto refNorm = extractAndNormalize(*referenceTrajectory);

    // Determine alignment transform
    bool directMatch = (obsNorm.timesMs.size() == refNorm.timesMs.size());
    if (directMatch)
    {
        for (size_t i = 0; i < obsNorm.timesMs.size(); ++i)
        {
            if (std::abs(obsNorm.timesMs[i] - refNorm.timesMs[i]) > 0.05)
            {
                directMatch = false;
                break;
            }
        }
    }

    report.alignmentTransform = directMatch ? AlignmentTransform::None : AlignmentTransform::Interpolation;

    // Collect paired observation values
    std::vector<std::pair<double, double>> pairs;
    pairs.reserve(obsNorm.timesMs.size());

    for (size_t i = 0; i < obsNorm.timesMs.size(); ++i)
    {
        if (!obsNorm.normValues[i].has_value())
            continue;

        double obsVal = *obsNorm.normValues[i];
        double t = obsNorm.timesMs[i];

        std::optional<double> refVal;
        if (directMatch)
        {
            refVal = refNorm.normValues[i];
        }
        else
        {
            refVal = interpolateAt(refNorm, t);
        }

        if (refVal.has_value())
        {
            pairs.emplace_back(obsVal, *refVal);
        }
    }

    report.validPairCount = pairs.size();
    size_t totalExpected = std::max(obsNorm.timesMs.size(), refNorm.timesMs.size());
    report.coverageRatio = (totalExpected > 0) ? (static_cast<double>(pairs.size()) / static_cast<double>(totalExpected)) : 0.0;

    if (pairs.size() < config.minValidPairs)
    {
        report.comparisonStatus = "insufficient_alignment";
        report.limitations = "Insufficient valid observation pairs (" + std::to_string(pairs.size()) +
                             " < " + std::to_string(config.minValidPairs) + ") for reliable statistical comparison.";
        return report;
    }

    // Timing landmark errors
    report.onsetErrorMs = obsNorm.firstValidTimeMs - refNorm.firstValidTimeMs;
    report.peakTimeErrorMs = obsNorm.peakTimeMs - refNorm.peakTimeMs;
    report.durationErrorMs = obsNorm.durationMs - refNorm.durationMs;

    // Statistical metrics
    double sumAbsErr = 0.0;
    double maxAbsErr = 0.0;
    double sumSqErr = 0.0;
    double sumObs = 0.0;
    double sumRef = 0.0;

    for (const auto& [o, r] : pairs)
    {
        double diff = std::abs(o - r);
        sumAbsErr += diff;
        maxAbsErr = std::max(maxAbsErr, diff);
        sumSqErr += (o - r) * (o - r);
        sumObs += o;
        sumRef += r;
    }

    double n = static_cast<double>(pairs.size());
    report.meanAbsoluteError = sumAbsErr / n;
    report.maxAbsoluteError = maxAbsErr;
    report.trajectoryRmse = std::sqrt(sumSqErr / n);

    // Pearson correlation r
    if (obsNorm.isConstant || refNorm.isConstant)
    {
        // Zero variance: Pearson is undefined
        report.correlation = std::nullopt;
        report.comparisonStatus = "not_observable";
        report.limitations = "Zero variance detected in trajectory signal; correlation is mathematically undefined.";
        return report;
    }

    double meanObs = sumObs / n;
    double meanRef = sumRef / n;
    double sOO = 0.0;
    double sRR = 0.0;
    double sOR = 0.0;

    for (const auto& [o, r] : pairs)
    {
        double dO = o - meanObs;
        double dR = r - meanRef;
        sOO += dO * dO;
        sRR += dR * dR;
        sOR += dO * dR;
    }

    if (sOO < 1e-12 || sRR < 1e-12)
    {
        report.correlation = std::nullopt;
        report.comparisonStatus = "not_observable";
        report.limitations = "Near-zero variance across sampled observation points.";
        return report;
    }

    double rVal = sOR / std::sqrt(sOO * sRR);
    report.correlation = std::clamp(rVal, -1.0, 1.0);

    if (report.coverageRatio.value_or(0.0) < config.minCoverageRatio)
    {
        report.comparisonStatus = "insufficient_alignment";
        report.limitations = "Valid pair coverage ratio (" + std::to_string(*report.coverageRatio) +
                             ") is below required threshold (" + std::to_string(config.minCoverageRatio) + ").";
    }
    else
    {
        report.comparisonStatus = "compared";
        report.limitations = "Comparison conducted strictly in common normalized observable space. "
                             "Acoustic centroid / rolloff is an observed proxy and not a direct measurement of hardware internal parameters.";
    }

    return report;
}

} // namespace abdaudiolab::measurement
