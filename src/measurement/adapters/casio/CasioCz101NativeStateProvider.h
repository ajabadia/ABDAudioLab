/**
 * @file CasioCz101NativeStateProvider.h
 * @brief Concrete INativeStateProvider implementation for Casio CZ-101 patches.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../../ComplexEnvelopeOrchestratorContracts.h"
#include "CasioCz101SysExContracts.h"
#include "CasioCz101ObservableBinding.h"

namespace abdaudiolab::measurement::adapters::casio
{

class CasioCz101NativeStateProvider : public INativeStateProvider
{
public:
    explicit CasioCz101NativeStateProvider(CasioCz101NativePatchState patchState);
    ~CasioCz101NativeStateProvider() override = default;

    [[nodiscard]] std::string getModelIdentifier() const override;
    [[nodiscard]] std::string getStateSha256() const override;
    [[nodiscard]] std::vector<NativeEnvelopeBinding> getBindings() const override;
    [[nodiscard]] std::vector<EnvelopeStageDescriptor> getNativeStageDescriptors(
        const std::string& nativePath) const override;
    [[nodiscard]] std::optional<EnvelopeTrajectory> getObservableReference(
        const std::string& nativePath) const override;

private:
    CasioCz101NativePatchState state_;
};

} // namespace abdaudiolab::measurement::adapters::casio
