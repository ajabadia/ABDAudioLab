/**
 * @file ObservableComparisonEngine.h
 * @brief Metrological comparison engine operating in declared common observable spaces.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeOrchestratorContracts.h"
#include <optional>

namespace abdaudiolab::measurement
{

class ObservableComparisonEngine
{
public:
    struct ComparisonConfig
    {
        std::string comparisonSpace { "normalized_0_1" };
        std::string normalizationMethod { "min_max" };
        double minCoverageRatio { 0.30 };
        size_t minValidPairs { 3 };
    };

    /**
     * @brief Compares an observed acoustic trajectory against an observable reference in a common normalized space.
     * @param observedTrajectory Acoustic trajectory observed by ComplexEnvelopeAnalyzer.
     * @param referenceTrajectory Projected reference trajectory from native intent.
     * @param nativePath Native parameter path identifier (e.g. "line1.dcw.envelope").
     * @param config Comparison parameters.
     * @return ObservableComparisonReport with RMSE, Pearson r, MAE, and honest limitations.
     */
    [[nodiscard]] static ObservableComparisonReport compare(
        const EnvelopeTrajectory& observedTrajectory,
        const std::optional<EnvelopeTrajectory>& referenceTrajectory,
        const std::string& nativePath,
        const ComparisonConfig& config = {}) noexcept;
};

} // namespace abdaudiolab::measurement
