#pragma once

#include "ISynthTarget.h"
#include "SyntheticSynthFixture.h"

namespace abdaudiolab::synth
{

/**
 * @brief Adaptador de ISynthTarget para el simulador determinista SyntheticSynthFixture.
 */
class SyntheticSynthTarget : public ISynthTarget
{
public:
    explicit SyntheticSynthTarget(SyntheticSynthFixture& fixture)
        : fixture_(fixture)
    {
    }

    bool loadState(const SynthPresetState& state) override
    {
        currentState_ = state;
        isStateVerified_ = (state.stateStatus == StateAppliedStatus::Passed);
        return true;
    }

    [[nodiscard]] StateAppliedStatus verifyState() const override
    {
        return isStateVerified_ ? StateAppliedStatus::Passed : StateAppliedStatus::Unverified;
    }

    void prepare(const ProcessingSpec& spec) override
    {
        fixture_.setSampleRate(spec.sampleRate);
    }

    void resetState() override
    {
        fixture_.resetPhase();
        fixture_.resetRng();
    }

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int repetitionIndex = 0) override
    {
        auto res = fixture_.renderSequence(sequence, repetitionIndex);
        destinationAudio = std::move(res.audio);
    }

    [[nodiscard]] TargetTimingInfo timingInfo() const override
    {
        TargetTimingInfo info;
        info.isPhysicalHardware = false;
        info.isDirectPlugin = false;
        info.declaredLatencySamples = 0.0;
        info.measuredTransportLatencyMs = fixture_.getGroundTruth().transportLatencyMs;
        info.timingJitterMs = 0.08;
        info.timingDescription = "Synthetic Ground Truth Fixture";
        return info;
    }

    [[nodiscard]] bool supportsBinaryState() const override
    {
        return true;
    }

    StateTransferResult getState(std::vector<uint8_t>& stateData) const override
    {
        stateData = fixture_.serializeState();
        StateTransferResult res;
        res.supported = true;
        res.succeeded = true;
        res.byteCount = stateData.size();
        res.stateDataHash = Sha256::computeHex(stateData.data(), stateData.size());
        return res;
    }

    StateTransferResult setState(const std::vector<uint8_t>& stateData) override
    {
        StateTransferResult res;
        res.supported = true;
        res.byteCount = stateData.size();
        res.stateDataHash = Sha256::computeHex(stateData.data(), stateData.size());

        if (fixture_.deserializeState(stateData))
        {
            res.succeeded = true;
        }
        else
        {
            res.succeeded = false;
            res.errorCode = "DESERIALIZE_FAILED";
        }
        return res;
    }

private:
    SyntheticSynthFixture& fixture_;
    SynthPresetState currentState_;
    bool isStateVerified_ { true };
};

} // namespace abdaudiolab::synth
