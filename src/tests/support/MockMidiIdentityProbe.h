#pragma once

#include "hardware/preflight/IMidiIdentityProbe.h"

namespace abdaudiolab::tests
{

class MockMidiIdentityProbe final : public hardware::IMidiIdentityProbe
{
public:
    void setNextProbeResult(const hardware::HardwareIdentityProbeResult& result)
    {
        nextResult_ = result;
    }

    void setNextOutcome(hardware::IdentityProbeOutcome outcome, 
                        std::string mfr = {}, 
                        std::string model = {}, 
                        std::string version = {})
    {
        nextResult_.outcome = outcome;
        nextResult_.observedManufacturerId = std::move(mfr);
        nextResult_.observedModelId = std::move(model);
        nextResult_.observedFirmwareVersion = std::move(version);
        if (outcome == hardware::IdentityProbeOutcome::Mismatch)
            nextResult_.diagnosticCode = "ERR_IDENTITY_MISMATCH";
        else if (outcome == hardware::IdentityProbeOutcome::Verified)
            nextResult_.diagnosticCode = "OK";
        else if (outcome == hardware::IdentityProbeOutcome::Timeout)
            nextResult_.diagnosticCode = "ERR_PROBE_TIMEOUT";
        else if (outcome == hardware::IdentityProbeOutcome::TransportFailure)
            nextResult_.diagnosticCode = "ERR_PROBE_TRANSPORT";
    }

    hardware::HardwareIdentityProbeResult probe(
        const profiling::TargetProfile& profile,
        const hardware::MidiPortSelection& outputPort) override
    {
        lastProbedProfileId_ = profile.targetProfileId;
        lastProbedPort_ = outputPort;
        probeCallCount_++;
        return nextResult_;
    }

    [[nodiscard]] int probeCallCount() const noexcept { return probeCallCount_; }
    [[nodiscard]] const std::string& lastProbedProfileId() const noexcept { return lastProbedProfileId_; }
    [[nodiscard]] const hardware::MidiPortSelection& lastProbedPort() const noexcept { return lastProbedPort_; }

    void reset()
    {
        nextResult_ = hardware::HardwareIdentityProbeResult { hardware::IdentityProbeOutcome::NotSupported };
        lastProbedProfileId_.clear();
        lastProbedPort_ = {};
        probeCallCount_ = 0;
    }

private:
    hardware::HardwareIdentityProbeResult nextResult_ { hardware::IdentityProbeOutcome::NotSupported };
    std::string lastProbedProfileId_;
    hardware::MidiPortSelection lastProbedPort_;
    int probeCallCount_ { 0 };
};

} // namespace abdaudiolab::tests
