/**
 * @file CasioCz101ObservableBinding.h
 * @brief Unilateral declarative binding and metrological comparison between Casio CZ-101 native state and observables.
 * @author ABDSynths
 * @date 2026
 *
 * Enforces unilateral metrological honesty:
 *   Native SysEx -> Decoded Rate/Level
 *   Observed Audio -> Centroid / Rolloff / RMS
 * Audio does not guarantee unique reconstruction of internal hardware rate/level.
 * phaseDistortionProxy and nativeEnvelopeReconstruction remain strictly "not_claimed".
 */

#pragma once

#include "CasioCz101SysExContracts.h"
#include <vector>

namespace abdaudiolab::measurement::adapters::casio
{

class CasioCz101ObservableBinding
{
public:
    CasioCz101ObservableBinding() = default;
    ~CasioCz101ObservableBinding() = default;

    /**
     * @brief Maps native DCW parameters and context into a declarative TimbreObservable result.
     * @param input Contextual DCW mapping input.
     * @return CasioCzDcwMappingResult with honest claims and stage descriptors.
     */
    [[nodiscard]] static CasioCzDcwMappingResult mapDcwToTimbreObservable(
        const CasioCzDcwMappingInput& input) noexcept;

    /**
     * @brief Binds all native envelopes in a decoded patch state to generic observable descriptors.
     * @param state Decoded native CZ-101 patch state.
     * @return Vector of generic NativeEnvelopeBinding descriptors.
     */
    [[nodiscard]] static std::vector<NativeEnvelopeBinding> bindNativePatchToObservables(
        const CasioCz101NativePatchState& state) noexcept;

    /**
     * @brief Compares a native binding with an observed acoustic trajectory.
     * @param binding Native envelope binding descriptor.
     * @param observedTrajectory Observed trajectory from audio analysis.
     * @return NativeObservableComparison documenting correlation and explicit limitations.
     */
    [[nodiscard]] static NativeObservableComparison compareNativeWithObserved(
        const NativeEnvelopeBinding& binding,
        const EnvelopeTrajectory& observedTrajectory) noexcept;
};

} // namespace abdaudiolab::measurement::adapters::casio
