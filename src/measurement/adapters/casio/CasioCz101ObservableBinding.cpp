/**
 * @file CasioCz101ObservableBinding.cpp
 * @brief Implementation of declarative binding and honest metrological comparison for Casio CZ-101.
 * @author ABDSynths
 * @date 2026
 */

#include "CasioCz101ObservableBinding.h"
#include <algorithm>

namespace abdaudiolab::measurement::adapters::casio
{

CasioCzDcwMappingResult CasioCz101ObservableBinding::mapDcwToTimbreObservable(
    const CasioCzDcwMappingInput& input) noexcept
{
    CasioCzDcwMappingResult result;
    result.sourceSysExSha256 = input.sourceSysExSha256;
    result.phaseDistortionProxy = "not_claimed";
    result.nativeEnvelopeReconstruction = "not_claimed";

    if (input.lineIndex != 1 && input.lineIndex != 2)
    {
        result.mappingStatus = "invalid";
        result.nativeParameterPath = "unknown.dcw.envelope";
        return result;
    }

    result.nativeParameterPath = (input.lineIndex == 1) ? "line1.dcw.envelope" : "line2.dcw.envelope";

    // Validate envelope stages
    if (input.stages.empty())
    {
        result.mappingStatus = "invalid";
        return result;
    }

    // Determine active stages up to endStage if provided, else all 8
    int maxStage = 8;
    if (input.endStage.has_value() && input.endStage.value() >= 1 && input.endStage.value() <= 8)
    {
        maxStage = input.endStage.value();
    }

    result.inferredStages.reserve(static_cast<size_t>(maxStage));

    for (int i = 0; i < maxStage; ++i)
    {
        const auto& stage = input.stages[static_cast<size_t>(i)];
        EnvelopeStageDescriptor desc;
        desc.stageIndex = i + 1;
        desc.parameterization = "rate_level";
        desc.rateOrSlope = static_cast<double>(stage.rate);
        desc.targetLevel = static_cast<double>(stage.level);
        desc.levelUnit = "native_cz_level";
        desc.isSustainPoint = stage.isSustainPoint;
        desc.isEndKeyOnPoint = stage.isEndPoint;

        result.inferredStages.push_back(desc);
    }

    // Build the declarative observedDomain trajectory
    result.observedDomain.trajectoryLabel = input.modelIdentifier + "." + result.nativeParameterPath;
    result.observedDomain.domain = EnvelopeDomain::Timbre;
    result.observedDomain.phaseDistortionProxy = "not_claimed";
    result.observedDomain.nativeEnvelopeReconstruction = "not_claimed";
    result.observedDomain.inferredStages = result.inferredStages;

    result.mappingStatus = "mapped";
    return result;
}

std::vector<NativeEnvelopeBinding> CasioCz101ObservableBinding::bindNativePatchToObservables(
    const CasioCz101NativePatchState& state) noexcept
{
    std::vector<NativeEnvelopeBinding> bindings;
    bindings.reserve(6);

    // Line 1 bindings
    bindings.push_back({
        "line1.dco.envelope",
        EnvelopeDomain::Pitch,
        "rate_level",
        state.modelIdentifier,
        state.sourceSysExSha256
    });

    bindings.push_back({
        "line1.dcw.envelope",
        EnvelopeDomain::Timbre,
        "rate_level",
        state.modelIdentifier,
        state.sourceSysExSha256
    });

    bindings.push_back({
        "line1.dca.envelope",
        EnvelopeDomain::Amplitude,
        "rate_level",
        state.modelIdentifier,
        state.sourceSysExSha256
    });

    // Line 2 bindings if lineSelect uses Line 2 (1: 2, 2: 1+1', 3: 1+2')
    // In Casio CZ architecture, lineSelect 1 is Line 2 only; lineSelect 3 is Line 1+2'
    if (state.lineSelect == 1 || state.lineSelect == 3)
    {
        bindings.push_back({
            "line2.dco.envelope",
            EnvelopeDomain::Pitch,
            "rate_level",
            state.modelIdentifier,
            state.sourceSysExSha256
        });

        bindings.push_back({
            "line2.dcw.envelope",
            EnvelopeDomain::Timbre,
            "rate_level",
            state.modelIdentifier,
            state.sourceSysExSha256
        });

        bindings.push_back({
            "line2.dca.envelope",
            EnvelopeDomain::Amplitude,
            "rate_level",
            state.modelIdentifier,
            state.sourceSysExSha256
        });
    }

    return bindings;
}

NativeObservableComparison CasioCz101ObservableBinding::compareNativeWithObserved(
    const NativeEnvelopeBinding& binding,
    const EnvelopeTrajectory& observedTrajectory) noexcept
{
    NativeObservableComparison comp;
    comp.observableDomain = envelopeDomainToString(binding.observableDomain);
    comp.nativeParameterPath = binding.nativePath;

    if (observedTrajectory.points.empty())
    {
        comp.comparisonStatus = "not_compared";
        comp.limitations = "No observed trajectory points available for comparison.";
        return comp;
    }

    if (binding.observableDomain != observedTrajectory.domain)
    {
        comp.comparisonStatus = "inconclusive";
        comp.limitations = "Domain mismatch between native binding and observed trajectory.";
        return comp;
    }

    comp.comparisonStatus = "compared";
    comp.limitations = "Acoustic trajectory is an observed proxy (spectral centroid / RMS), not a direct reconstruction of internal hardware rate/level.";
    
    // Evaluate timing span and peak level difference if trajectory has valid points
    double minVal = 1e9;
    double maxVal = -1e9;
    for (const auto& pt : observedTrajectory.points)
    {
        if (pt.value.has_value())
        {
            minVal = std::min(minVal, pt.value.value());
            maxVal = std::max(maxVal, pt.value.value());
        }
    }

    if (maxVal >= minVal)
    {
        comp.levelError = maxVal - minVal;
    }

    if (!observedTrajectory.points.empty())
    {
        comp.timingErrorMs = observedTrajectory.points.back().timeMs - observedTrajectory.points.front().timeMs;
    }

    return comp;
}

} // namespace abdaudiolab::measurement::adapters::casio
