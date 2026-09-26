#pragma once

#include "hardware/transport/IMidiResponseAwaiter.h"

namespace abdaudiolab::tests
{

class MockMidiResponseAwaiter final : public hardware::IMidiResponseAwaiter
{
public:
    void setNextResponse(hardware::MidiResponseOutcome outcome,
                         std::vector<uint8_t> bytes = {},
                         std::string diagCode = "OK",
                         std::string diagMsg = "Response received")
    {
        nextResult_.outcome = outcome;
        nextResult_.receivedBytes = std::move(bytes);
        nextResult_.diagnosticCode = std::move(diagCode);
        nextResult_.diagnosticMessage = std::move(diagMsg);
    }

    hardware::MidiResponseWaitResult waitForResponse(
        const hardware::ExpectedMidiResponse& expected,
        uint64_t timeoutMs) override
    {
        lastExpected_ = expected;
        lastTimeoutMs_ = timeoutMs;
        callCount_++;
        return nextResult_;
    }

    [[nodiscard]] int callCount() const noexcept { return callCount_; }
    [[nodiscard]] const hardware::ExpectedMidiResponse& lastExpected() const noexcept { return lastExpected_; }
    [[nodiscard]] uint64_t lastTimeoutMs() const noexcept { return lastTimeoutMs_; }

    void reset()
    {
        nextResult_ = hardware::MidiResponseWaitResult { hardware::MidiResponseOutcome::Unsupported, {}, "", "" };
        lastExpected_ = {};
        lastTimeoutMs_ = 0;
        callCount_ = 0;
    }

private:
    hardware::MidiResponseWaitResult nextResult_ { hardware::MidiResponseOutcome::Received, { 0x06 }, "OK", "ACK" };
    hardware::ExpectedMidiResponse lastExpected_;
    uint64_t lastTimeoutMs_ { 0 };
    int callCount_ { 0 };
};

} // namespace abdaudiolab::tests
