#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace abdaudiolab::hardware
{

/**
 * @brief Outcomes of waiting for a hardware response/ACK.
 */
enum class MidiResponseOutcome
{
    Received = 0,
    Timeout,
    Mismatch,
    Disconnected,
    Unsupported
};

/**
 * @brief Expected response descriptor for targeted acknowledgment.
 */
struct ExpectedMidiResponse
{
    std::string targetProfileId;
    std::string expectedHeaderHex;
    size_t expectedMinimumBytes { 0 };
};

/**
 * @brief Result of waiting for a response/ACK frame.
 */
struct MidiResponseWaitResult
{
    MidiResponseOutcome outcome { MidiResponseOutcome::Unsupported };
    std::vector<uint8_t> receivedBytes;
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

/**
 * @brief Pure interface for awaiting response/ACK frames from hardware.
 * Decoupled from output transport (IMidiTransport) and identity probe (IMidiIdentityProbe).
 */
class IMidiResponseAwaiter
{
public:
    virtual ~IMidiResponseAwaiter() = default;

    virtual MidiResponseWaitResult waitForResponse(
        const ExpectedMidiResponse& expected,
        uint64_t timeoutMs) = 0;
};

} // namespace abdaudiolab::hardware
