#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include "IMonotonicClock.h"
#include "IMidiTransport.h"
#include "IMidiResponseAwaiter.h"
#include "HardwareDispatchEvidenceRecord.h"
#include "hardware/consent/OperatorConsentRequest.h"
#include "hardware/consent/OperatorConsentService.h"
#include "profiling/TargetProfile.h"

namespace abdaudiolab::hardware
{

/**
 * @brief Dispatch decisions returned by HardwareDispatchScheduler.
 */
enum class DispatchDecision
{
    DispatchNow = 0,        // Pacing and consent satisfied; command accepted/transmitted
    PendingPacingWindow,    // Pacing delay or rate limit in progress; wait until earliestAllowedMonotonicTimeMs
    BlockedByConsent,       // Consent missing, denied, expired, or invalidated by TOCTOU
    BlockedByPreflight,     // Target identity mismatch or port unavailable
    Failed                  // Transport write failed, port disconnected, or system error
};

/**
 * @brief Structured outcome of a hardware dispatch evaluation or attempt.
 */
struct HardwareDispatchDecision
{
    DispatchDecision decision { DispatchDecision::Failed };
    uint64_t earliestAllowedMonotonicTimeMs { 0 };
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

/**
 * @brief Governs safe hardware command dispatch under the exclusive authority of ProfilingSequencer.
 * 
 * Invariants:
 * - Real-time audio threads NEVER sleep or block on MIDI pacing.
 * - Monotonic wall-clock time is governed via IMonotonicClock (injectable for unit tests).
 * - Anti-TOCTOU validation verifies consent token before every transmission.
 * - IMidiTransport only writes bytes; the scheduler controls pacing windows.
 */
class HardwareDispatchScheduler
{
public:
    explicit HardwareDispatchScheduler(IMonotonicClock& clock) noexcept;
    ~HardwareDispatchScheduler() = default;

    /**
     * @brief Evaluates whether a physical command can be transmitted right now according to consent and pacing.
     */
    [[nodiscard]] HardwareDispatchDecision evaluateDispatchWindow(
        const OperatorConsentRequest& request,
        const OperatorConsentResult& consent,
        const std::string& activePlanHash,
        const MidiPortSelection& activePort,
        const std::optional<MidiCcMessage>& activeCc,
        const std::optional<MidiSysExMessage>& activeSysEx,
        const profiling::TransportPolicy& policy) const;

    /**
     * @brief Validates consent and pacing, and transmits CC if window is open.
     */
    HardwareDispatchDecision dispatchCc(
        IMidiTransport& transport,
        const OperatorConsentRequest& request,
        const OperatorConsentResult& consent,
        const std::string& activePlanHash,
        const MidiPortSelection& activePort,
        const MidiCcMessage& message,
        const profiling::TransportPolicy& policy);

    /**
     * @brief Validates consent and pacing, and transmits SysEx if window is open.
     */
    HardwareDispatchDecision dispatchSysEx(
        IMidiTransport& transport,
        const OperatorConsentRequest& request,
        const OperatorConsentResult& consent,
        const std::string& activePlanHash,
        const MidiPortSelection& activePort,
        const MidiSysExMessage& message,
        const profiling::TransportPolicy& policy);

    /**
     * @brief Validates consent, pacing, and executes CC transmission, optionally awaiting ACK response.
     * Records forensic HardwareDispatchEvidenceRecord capturing full status and fail-closed state.
     */
    HardwareDispatchEvidenceRecord executeDispatchCc(
        IMidiTransport& transport,
        IMidiResponseAwaiter* awaiter,
        const OperatorConsentRequest& request,
        const OperatorConsentResult& consent,
        const std::string& activePlanHash,
        const MidiPortSelection& activePort,
        const MidiCcMessage& message,
        const profiling::TransportPolicy& policy,
        const ExpectedMidiResponse& expectedResponse = {});

    /**
     * @brief Validates consent, pacing, and executes SysEx transmission, optionally awaiting ACK response.
     * Records forensic HardwareDispatchEvidenceRecord capturing full status and fail-closed state.
     */
    HardwareDispatchEvidenceRecord executeDispatchSysEx(
        IMidiTransport& transport,
        IMidiResponseAwaiter* awaiter,
        const OperatorConsentRequest& request,
        const OperatorConsentResult& consent,
        const std::string& activePlanHash,
        const MidiPortSelection& activePort,
        const MidiSysExMessage& message,
        const profiling::TransportPolicy& policy,
        const ExpectedMidiResponse& expectedResponse = {});

    /**
     * @brief Resets scheduler transmission history and pacing state.
     */
    void resetPacing() noexcept;

    /**
     * @brief Returns timestamp of the most recent successful transmission in milliseconds.
     */
    [[nodiscard]] uint64_t getLastTransmissionMonotonicMs() const noexcept;

private:
    IMonotonicClock& clock_;
    uint64_t lastTransmissionMonotonicMs_ { 0 };
    bool hasTransmitted_ { false };
    uint64_t sequenceCounter_ { 0 };

    // Rate limiting tracking
    uint64_t rateWindowStartMs_ { 0 };
    int messagesSentInCurrentSecond_ { 0 };
};

} // namespace abdaudiolab::hardware
