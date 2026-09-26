#pragma once

#include <string>
#include "hardware/transport/MidiTransportTypes.h"

namespace abdaudiolab::hardware
{

/**
 * @brief Identity Probe Outcomes (pure inquiry / handshake result).
 */
enum class IdentityProbeOutcome
{
    NotSupported = 0,   // Target does not support identity inquiry (e.g. analog pedal, pure 1-way MIDI)
    Verified,           // Device responded and identity matched target profile
    Unavailable,        // Target supports inquiry but did not respond / timed out
    Mismatch,           // Device responded with different manufacturer or model
    Timeout,            // Inquiry transmission/reception timed out
    TransportFailure    // Underlying probe transport failed to transmit or read
};

/**
 * @brief Result of an identity probe inquiry.
 */
struct HardwareIdentityProbeResult
{
    IdentityProbeOutcome outcome { IdentityProbeOutcome::NotSupported };
    std::string observedManufacturerId;
    std::string observedModelId;
    std::string observedFirmwareVersion;
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

/**
 * @brief 5 Normative Identity States.
 */
enum class HardwareIdentityState
{
    PortAvailable = 0,        // Port successfully opened at OS/transport level, but identity not tested
    IdentityVerified,         // Identity bidirectionally verified against TargetProfile
    IdentityUnavailable,      // Identity cannot be verified (unsupported, timeout, or no probe response)
    IdentityMismatch,         // Connected hardware responded with mismatching identity
    UserConfirmedUnverified   // Operator explicitly confirmed non-verifiable hardware (D2.4 only)
};

/**
 * @brief Preflight Disposition.
 * D2.3 only emits Ready, Blocked, or AwaitingUserConfirmation.
 */
enum class HardwarePreflightDisposition
{
    Ready = 0,                  // Fully verified and technically ready for consent/dispatch
    Blocked,                    // Blocked fail-closed (port open failed, mismatch, or required verified missing)
    AwaitingUserConfirmation    // Identity unavailable, but policy allows operator confirmation in D2.4
};

/**
 * @brief Immutable result of the preflight evaluation.
 */
struct HardwarePreflightResult
{
    bool readyForConsentOrDispatch { false };
    HardwarePreflightDisposition disposition { HardwarePreflightDisposition::Blocked };
    HardwareIdentityState identityState { HardwareIdentityState::IdentityUnavailable };
    std::string diagnosticCode;
    std::string diagnosticMessage;
    MidiPortSelection portSelection;
    std::string targetProfileId;
};

} // namespace abdaudiolab::hardware
