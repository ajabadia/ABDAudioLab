/**
 * @file test_CaptureAutomationAndFair_T3.cpp
 * @brief Fase 20.11 T3.2, T3.3, T3.4 - Automatización de Captura, Inyección de Estado/SysEx y Telemetría FAIR
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <string>
#include <regex>
#include <nlohmann/json.hpp>

#include "measurement/MeasurementCaptureCoordinator.h"
#include "measurement/MeasurementStimulusCoordinator.h"
#include "synth/ISynthTarget.h"
#include "synth/SysExContracts.h"
#include "synth/Sha256.h"

using namespace abdaudiolab;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

namespace
{

class LifecycleOrderAuditorTarget : public ISynthTarget
{
public:
    enum class Step
    {
        None,
        Prepared,
        StateReset,
        StateLoaded,
        StateVerified,
        Rendered
    };

    std::vector<Step> executionTrace;
    bool forceStateVerificationFailure { false };
    bool forceStateLoadFailure { false };
    bool wasResetCalledBeforeLoad { false };
    bool wasPreparedCalledBeforeReset { false };
    ProcessingSpec currentSpec;
    SynthPresetState currentState;

    void prepare(const ProcessingSpec& spec) override
    {
        currentSpec = spec;
        executionTrace.push_back(Step::Prepared);
    }

    void resetState() override
    {
        wasPreparedCalledBeforeReset = (!executionTrace.empty() && executionTrace.back() == Step::Prepared);
        executionTrace.push_back(Step::StateReset);
    }

    bool loadState(const SynthPresetState& state) override
    {
        if (forceStateLoadFailure)
            return false;

        currentState = state;
        wasResetCalledBeforeLoad = (!executionTrace.empty() && executionTrace.back() == Step::StateReset);
        executionTrace.push_back(Step::StateLoaded);
        return true;
    }

    StateAppliedStatus verifyState() const override
    {
        const_cast<LifecycleOrderAuditorTarget*>(this)->executionTrace.push_back(Step::StateVerified);
        if (forceStateVerificationFailure)
            return StateAppliedStatus::Failed;
        return StateAppliedStatus::Passed;
    }

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int /*repetitionIndex*/ = 0) override
    {
        executionTrace.push_back(Step::Rendered);
        
        // Generar respuesta determinista basada en el estado y eventos
        float baseFreq = 440.0f;
        if (!sequence.canonicalEvents.empty())
        {
            for (const auto& ev : sequence.canonicalEvents)
            {
                if (ev.bytes.size() >= 3 && (ev.bytes[0] & 0xF0) == 0x90)
                {
                    int note = ev.bytes[1];
                    baseFreq = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
                    break;
                }
            }
        }

        double phase = 0.0;
        double phaseInc = (2.0 * 3.14159265358979323846 * baseFreq) / currentSpec.sampleRate;

        for (size_t i = 0; i < destinationAudio.size(); ++i)
        {
            destinationAudio[i] = static_cast<float>(0.5 * std::sin(phase));
            phase += phaseInc;
            if (phase > 2.0 * 3.14159265358979323846)
                phase -= 2.0 * 3.14159265358979323846;
        }
    }

    [[nodiscard]] TargetTimingInfo timingInfo() const override
    {
        TargetTimingInfo t;
        t.declaredLatencySamples = 64.0;
        t.timingDescription = "mock_timing";
        return t;
    }

    [[nodiscard]] bool supportsBinaryState() const override { return true; }
    StateTransferResult getState(std::vector<uint8_t>& out) const override
    {
        out = currentState.rawSysEx;
        StateTransferResult res;
        res.supported = true;
        res.succeeded = true;
        res.byteCount = out.size();
        return res;
    }
    StateTransferResult setState(const std::vector<uint8_t>& in) override
    {
        currentState.rawSysEx = in;
        StateTransferResult res;
        res.supported = true;
        res.succeeded = true;
        res.byteCount = in.size();
        return res;
    }
};

std::vector<uint8_t> createTestDx7Voice()
{
    std::vector<uint8_t> msg;
    msg.reserve(163);
    msg.push_back(0xF0);
    msg.push_back(0x43);
    msg.push_back(0x00);
    msg.push_back(0x00);
    msg.push_back(0x01);
    msg.push_back(0x1B);
    for (size_t i = 0; i < 155; ++i)
        msg.push_back(static_cast<uint8_t>((i * 7) & 0x7F));
    uint8_t chk = Dx7SysExValidator::computeDx7Checksum(msg.data() + 6, 155);
    msg.push_back(chk);
    msg.push_back(0xF7);
    return msg;
}

} // namespace

TEST_CASE("Fase 20.11 T3.2 - Strict execution lifecycle order in MeasurementCaptureCoordinator", "[capture][lifecycle][t3]")
{
    LifecycleOrderAuditorTarget target;
    MeasurementSpec spec;
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 256;
    spec.stimulus.durationSec = 0.05;

    SynthPresetState preset;
    preset.presetName = "LifecycleTestPreset";
    preset.stateStatus = StateAppliedStatus::Passed;

    SECTION("Strict order: prepare -> resetState -> loadState -> verifyState -> render")
    {
        CaptureResult res = MeasurementCaptureCoordinator::captureSynchronous(&target, spec, &preset);

        REQUIRE(res.status == MeasurementStatus::completed);
        REQUIRE(target.executionTrace.size() >= 5);
        REQUIRE(target.executionTrace[0] == LifecycleOrderAuditorTarget::Step::Prepared);
        REQUIRE(target.executionTrace[1] == LifecycleOrderAuditorTarget::Step::StateReset);
        REQUIRE(target.executionTrace[2] == LifecycleOrderAuditorTarget::Step::StateLoaded);
        REQUIRE(target.executionTrace[3] == LifecycleOrderAuditorTarget::Step::StateVerified);
        REQUIRE(target.executionTrace[4] == LifecycleOrderAuditorTarget::Step::Rendered);

        REQUIRE(target.wasPreparedCalledBeforeReset);
        REQUIRE(target.wasResetCalledBeforeLoad);
    }

    SECTION("Fails immediately if state load fails without leaking audio")
    {
        target.forceStateLoadFailure = true;
        CaptureResult res = MeasurementCaptureCoordinator::captureSynchronous(&target, spec, &preset);

        REQUIRE(res.status == MeasurementStatus::failed);
        REQUIRE(res.reason == "state_load_failed");
        REQUIRE(res.capturedAudio.empty());
    }

    SECTION("Fails immediately if state verification fails without rendering")
    {
        target.forceStateVerificationFailure = true;
        CaptureResult res = MeasurementCaptureCoordinator::captureSynchronous(&target, spec, &preset);

        REQUIRE(res.status == MeasurementStatus::failed);
        REQUIRE(res.reason == "state_verification_failed");
        REQUIRE(res.capturedAudio.empty());
        
        // Comprobar que NUNCA llegó a Step::Rendered
        for (auto s : target.executionTrace)
        {
            REQUIRE(s != LifecycleOrderAuditorTarget::Step::Rendered);
        }
    }
}

TEST_CASE("Fase 20.11 T3.2/T3.3 - SysEx injection and FAIR telemetry metadata", "[capture][sysex][fair][t3]")
{
    LifecycleOrderAuditorTarget target;
    MeasurementSpec spec;
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.execution.numChannels = 2;
    spec.stimulus.durationSec = 0.1;

    auto voiceBytes = createTestDx7Voice();
    SysExArtifact sysEx = Dx7SysExValidator::createArtifact(voiceBytes);

    REQUIRE(sysEx.semanticStatus == "valid");

    CaptureResult res = MeasurementCaptureCoordinator::captureSynchronousWithSysEx(&target, spec, sysEx);

    REQUIRE(res.status == MeasurementStatus::completed);
    REQUIRE(!res.capturedAudio.empty());
    REQUIRE(!res.capturedAudioSha256.empty());

    // Verificación de Telemetría FAIR (T3.3)
    const auto& meta = res.telemetryMetadata;
    REQUIRE(meta.sampleRateHz == 48000.0);
    REQUIRE(meta.bufferSize == 512);
    REQUIRE(meta.numChannels == 2);
    REQUIRE(meta.effectiveLatencySamples == 64.0);
    REQUIRE(meta.hardwareClockDriftPpm == 0.0);
    REQUIRE(meta.underruns == 0);
    REQUIRE(meta.overruns == 0);
    REQUIRE(meta.sha256Audio == res.capturedAudioSha256);
    REQUIRE(meta.sha256Midi == res.stimulusSha256);
    REQUIRE(meta.sha256SysEx == sysEx.sha256);
    REQUIRE(meta.sha256State == sysEx.sha256);

    // Formato ISO 8601 UTC en marcas temporales
    std::regex iso8601Regex(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    REQUIRE(std::regex_match(meta.wallClockStartIso, iso8601Regex));
    REQUIRE(std::regex_match(meta.wallClockEndIso, iso8601Regex));

    // Serialización JSON conforme a FAIR
    std::string jsonStr = meta.toJsonString(2);
    REQUIRE(!jsonStr.empty());
    
    nlohmann::json parsed = nlohmann::json::parse(jsonStr);
    REQUIRE(parsed["sampleRateHz"] == 48000.0);
    REQUIRE(parsed["bufferSize"] == 512);
    REQUIRE(parsed["sha256Audio"] == res.capturedAudioSha256);
    REQUIRE(parsed["sha256SysEx"] == sysEx.sha256);
    REQUIRE(parsed["wallClockStartIso"] == meta.wallClockStartIso);
    REQUIRE(parsed["wallClockEndIso"] == meta.wallClockEndIso);
}

TEST_CASE("Fase 20.11 T3.4 - Bit-exact reproducibility pipeline", "[capture][reproducibility][t3]")
{
    LifecycleOrderAuditorTarget target;
    MeasurementSpec spec;
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 256;
    spec.stimulus.durationSec = 0.05;

    auto voiceBytes = createTestDx7Voice();
    SysExArtifact sysEx = Dx7SysExValidator::createArtifact(voiceBytes);

    CaptureResult run1 = MeasurementCaptureCoordinator::captureSynchronousWithSysEx(&target, spec, sysEx);
    CaptureResult run2 = MeasurementCaptureCoordinator::captureSynchronousWithSysEx(&target, spec, sysEx);

    REQUIRE(run1.status == MeasurementStatus::completed);
    REQUIRE(run2.status == MeasurementStatus::completed);

    // Bit-exact matching
    REQUIRE(run1.capturedAudioSha256 == run2.capturedAudioSha256);
    REQUIRE(run1.stimulusSha256 == run2.stimulusSha256);
    REQUIRE(run1.presetStateSha256 == run2.presetStateSha256);
    REQUIRE(run1.telemetryMetadata.sha256SysEx == run2.telemetryMetadata.sha256SysEx);
    REQUIRE(run1.numSamples == run2.numSamples);
}
