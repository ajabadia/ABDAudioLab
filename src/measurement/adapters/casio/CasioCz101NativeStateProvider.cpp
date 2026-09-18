/**
 * @file CasioCz101NativeStateProvider.cpp
 * @brief Implementation of INativeStateProvider for Casio CZ-101.
 * @author ABDSynths
 * @date 2026
 */

#include "CasioCz101NativeStateProvider.h"
#include <algorithm>

namespace abdaudiolab::measurement::adapters::casio
{

CasioCz101NativeStateProvider::CasioCz101NativeStateProvider(CasioCz101NativePatchState patchState)
    : state_(std::move(patchState))
{
}

std::string CasioCz101NativeStateProvider::getModelIdentifier() const
{
    return state_.modelIdentifier;
}

std::string CasioCz101NativeStateProvider::getStateSha256() const
{
    return state_.sourceSysExSha256;
}

std::vector<NativeEnvelopeBinding> CasioCz101NativeStateProvider::getBindings() const
{
    return CasioCz101ObservableBinding::bindNativePatchToObservables(state_);
}

std::vector<EnvelopeStageDescriptor> CasioCz101NativeStateProvider::getNativeStageDescriptors(
    const std::string& nativePath) const
{
    std::vector<EnvelopeStageDescriptor> descriptors;

    const std::array<CzEnvelopeStage, 8>* stageSource = nullptr;
    if (nativePath == "line1.dco.envelope") stageSource = &state_.dco1Pitch;
    else if (nativePath == "line1.dcw.envelope") stageSource = &state_.dcw1Timbre;
    else if (nativePath == "line1.dca.envelope") stageSource = &state_.dca1Amplitude;
    else if (nativePath == "line2.dco.envelope") stageSource = &state_.dco2Pitch;
    else if (nativePath == "line2.dcw.envelope") stageSource = &state_.dcw2Timbre;
    else if (nativePath == "line2.dca.envelope") stageSource = &state_.dca2Amplitude;

    if (stageSource == nullptr)
        return descriptors;

    descriptors.reserve(8);
    for (int i = 0; i < 8; ++i)
    {
        const auto& s = (*stageSource)[static_cast<size_t>(i)];
        EnvelopeStageDescriptor d;
        d.stageIndex = i + 1;
        d.parameterization = "rate_level";
        d.rateOrSlope = static_cast<double>(s.rate);
        d.targetLevel = static_cast<double>(s.level);
        d.levelUnit = "native_cz_level";
        d.isSustainPoint = s.isSustainPoint;
        d.isEndKeyOnPoint = s.isEndPoint;
        descriptors.push_back(d);

        if (s.isEndPoint)
            break; // Stop after end point
    }

    return descriptors;
}

std::optional<EnvelopeTrajectory> CasioCz101NativeStateProvider::getObservableReference(
    const std::string& nativePath) const
{
    // Build physically justified reference projection
    if (nativePath != "line1.dcw.envelope" && nativePath != "line2.dcw.envelope" &&
        nativePath != "line1.dco.envelope" && nativePath != "line1.dca.envelope")
    {
        return std::nullopt;
    }

    EnvelopeTrajectory ref;
    ref.trajectoryLabel = state_.modelIdentifier + "." + nativePath + ".observable_ref";
    ref.phaseDistortionProxy = "not_claimed";
    ref.nativeEnvelopeReconstruction = "not_claimed";
    ref.temporalGridId = "native_reference_grid";

    if (nativePath.find("dcw") != std::string::npos)
    {
        ref.domain = EnvelopeDomain::Timbre;
        ref.inferredStages = getNativeStageDescriptors(nativePath);

        // Project Rate/Level stages to a normalized synthetic time series [0, 1]
        double currentTimeMs = 0.0;
        int frameIdx = 0;

        EnvelopeObservationPoint startPt;
        startPt.frameIndex = frameIdx++;
        startPt.timeMs = currentTimeMs;
        startPt.value = 0.0; // Starts from 0 or initial
        startPt.unit = "normalized_0_1";
        ref.points.push_back(startPt);

        for (const auto& st : ref.inferredStages)
        {
            // Transition duration based on Rate (higher rate = faster = shorter duration)
            double rate = st.rateOrSlope.value_or(50.0);
            double duration = std::max(5.0, (100.0 - rate) * 10.0); // Simple calibrated time proxy for reference
            currentTimeMs += duration;

            double targetNorm = st.targetLevel.value_or(0.0) / 99.0;

            EnvelopeObservationPoint pt;
            pt.frameIndex = frameIdx++;
            pt.timeMs = currentTimeMs;
            pt.value = targetNorm;
            pt.unit = "normalized_0_1";
            ref.points.push_back(pt);
        }

        return ref;
    }

    if (nativePath.find("dco") != std::string::npos)
    {
        ref.domain = EnvelopeDomain::Pitch;
        ref.inferredStages = getNativeStageDescriptors(nativePath);
        return ref;
    }

    if (nativePath.find("dca") != std::string::npos)
    {
        ref.domain = EnvelopeDomain::Amplitude;
        ref.inferredStages = getNativeStageDescriptors(nativePath);
        return ref;
    }

    return std::nullopt;
}

} // namespace abdaudiolab::measurement::adapters::casio
