#include "HardwareTransportPreflightService.h"

namespace abdaudiolab::hardware
{

HardwarePreflightResult HardwareTransportPreflightService::evaluate(
    const abdaudiolab::profiling::TargetProfile& profile,
    const MidiPortSelection& portSelection,
    IMidiTransport& transport,
    IMidiIdentityProbe* identityProbe)
{
    HardwarePreflightResult result;
    result.portSelection = portSelection;
    result.targetProfileId = profile.targetProfileId;

    // 1. Ensure transport is open, or attempt to open it.
    if (!transport.isOpen())
    {
        const MidiTransportOpenResult openResult = transport.open(portSelection);
        if (!openResult.opened)
        {
            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::Blocked;
            result.identityState = HardwareIdentityState::IdentityUnavailable;
            result.diagnosticCode = openResult.diagnosticCode.empty() 
                ? "ERR_MIDI_OUTPUT_OPEN_FAILED" 
                : openResult.diagnosticCode;
            result.diagnosticMessage = "Failed to open MIDI output port: " + openResult.message;
            return result;
        }
    }

    // 2. If no probe is provided or supported, evaluate identity availability against policy.
    if (identityProbe == nullptr)
    {
        result.identityState = HardwareIdentityState::IdentityUnavailable;

        if (profile.transportPolicy.requiresVerifiedIdentity)
        {
            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::Blocked;
            result.diagnosticCode = "ERR_VERIFIED_IDENTITY_REQUIRED";
            result.diagnosticMessage = "TargetProfile requires verified identity, but no identity probe is available.";
            return result;
        }

        if (profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity)
        {
            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
            result.diagnosticCode = "AWAITING_USER_CONFIRMATION";
            result.diagnosticMessage = "Device identity cannot be probed automatically; awaiting operator confirmation.";
            return result;
        }

        result.readyForConsentOrDispatch = false;
        result.disposition = HardwarePreflightDisposition::Blocked;
        result.diagnosticCode = "ERR_UNVERIFIED_IDENTITY_NOT_ALLOWED";
        result.diagnosticMessage = "Device identity cannot be verified and unverified operation is disallowed by policy.";
        return result;
    }

    // 3. Execute identity probe.
    const HardwareIdentityProbeResult probeResult = identityProbe->probe(profile, portSelection);

    switch (probeResult.outcome)
    {
        case IdentityProbeOutcome::Verified:
        {
            result.readyForConsentOrDispatch = true;
            result.disposition = HardwarePreflightDisposition::Ready;
            result.identityState = HardwareIdentityState::IdentityVerified;
            result.diagnosticCode = "OK";
            result.diagnosticMessage = "Device identity verified successfully.";
            return result;
        }

        case IdentityProbeOutcome::Mismatch:
        {
            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::Blocked;
            result.identityState = HardwareIdentityState::IdentityMismatch;
            result.diagnosticCode = "ERR_IDENTITY_MISMATCH";
            result.diagnosticMessage = "Device identity mismatch: observed manufacturer=" 
                + probeResult.observedManufacturerId + ", model=" + probeResult.observedModelId;
            return result;
        }

        case IdentityProbeOutcome::TransportFailure:
        {
            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::Blocked;
            result.identityState = HardwareIdentityState::IdentityUnavailable;
            result.diagnosticCode = "ERR_IDENTITY_PROBE_TRANSPORT_FAILED";
            result.diagnosticMessage = "Identity probe transport failed: " + probeResult.diagnosticMessage;
            return result;
        }

        case IdentityProbeOutcome::NotSupported:
        case IdentityProbeOutcome::Unavailable:
        case IdentityProbeOutcome::Timeout:
        default:
        {
            result.identityState = HardwareIdentityState::IdentityUnavailable;

            if (profile.transportPolicy.requiresVerifiedIdentity)
            {
                result.readyForConsentOrDispatch = false;
                result.disposition = HardwarePreflightDisposition::Blocked;
                result.diagnosticCode = "ERR_VERIFIED_IDENTITY_REQUIRED";
                result.diagnosticMessage = "TargetProfile requires verified identity, but probe was unavailable or timed out.";
                return result;
            }

            if (profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity)
            {
                result.readyForConsentOrDispatch = false;
                result.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
                result.diagnosticCode = "AWAITING_USER_CONFIRMATION";
                result.diagnosticMessage = "Device identity probe timed out or unavailable; awaiting operator confirmation.";
                return result;
            }

            result.readyForConsentOrDispatch = false;
            result.disposition = HardwarePreflightDisposition::Blocked;
            result.diagnosticCode = "ERR_UNVERIFIED_IDENTITY_NOT_ALLOWED";
            result.diagnosticMessage = "Device identity could not be verified and policy disallows unverified operation.";
            return result;
        }
    }
}

} // namespace abdaudiolab::hardware
