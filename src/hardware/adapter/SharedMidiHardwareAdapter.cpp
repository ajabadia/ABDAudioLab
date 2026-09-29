/**
 * @file SharedMidiHardwareAdapter.cpp
 * @brief Implementation of boundary adapter for HITO-SHARED-SYNC / SS5.
 * @author ABDSynths
 * @date 2026
 */

#include "SharedMidiHardwareAdapter.h"

namespace abdaudiolab::hardware
{

HardwareIdentityState SharedMidiHardwareAdapter::mapSharedIdentityState(
    abd::hwid::HardwareMidiIdentityState sharedState) noexcept
{
    switch (sharedState)
    {
        case abd::hwid::HardwareMidiIdentityState::PortAvailable:
            return HardwareIdentityState::PortAvailable;

        case abd::hwid::HardwareMidiIdentityState::IdentityVerified:
            return HardwareIdentityState::IdentityVerified;

        case abd::hwid::HardwareMidiIdentityState::IdentityUnavailable:
            return HardwareIdentityState::IdentityUnavailable;

        case abd::hwid::HardwareMidiIdentityState::IdentityMismatch:
            return HardwareIdentityState::IdentityMismatch;

        case abd::hwid::HardwareMidiIdentityState::UserConfirmedUnverified:
            return HardwareIdentityState::UserConfirmedUnverified;
    }

    return HardwareIdentityState::IdentityUnavailable;
}

SharedEndpointInfo SharedMidiHardwareAdapter::adaptSnapshot(
    const abd::hwid::SharedMidiEndpointSnapshot& snapshot) noexcept
{
    SharedEndpointInfo info;
    info.stableDeviceId = snapshot.stableDeviceId;
    info.displayName = snapshot.displayName;
    info.endpointKind = snapshot.endpointKind;
    info.identityState = mapSharedIdentityState(snapshot.identityState);
    info.isSysExVerified = (info.identityState == HardwareIdentityState::IdentityVerified);
    info.eligibleForAutomaticDiscovery = snapshot.eligibleForAutomaticDiscovery;
    info.inquiryDecision = snapshot.inquiryDecision;
    info.isPhysicalBenchEligible = isPhysicalBenchEligible(snapshot);

    // Warning badge required for Unknown endpoints or Virtual Loopback / Driver
    info.requiresWarningBadge = (snapshot.endpointKind == abd::hwid::MidiEndpointKind::Unknown ||
                                snapshot.endpointKind == abd::hwid::MidiEndpointKind::VirtualLoopback ||
                                snapshot.endpointKind == abd::hwid::MidiEndpointKind::VirtualDriver);

    info.diagnosticCode = snapshot.diagnosticCode;
    info.diagnosticMessage = snapshot.diagnosticMessage;
    return info;
}

bool SharedMidiHardwareAdapter::isPhysicalBenchEligible(
    const abd::hwid::SharedMidiEndpointSnapshot& snapshot) noexcept
{
    // Identity mismatch strictly disqualifies any endpoint from physical bench preflight
    if (snapshot.identityState == abd::hwid::HardwareMidiIdentityState::IdentityMismatch)
        return false;

    // Virtual endpoints are strictly forbidden from physical bench preflight
    if (snapshot.endpointKind == abd::hwid::MidiEndpointKind::VirtualLoopback ||
        snapshot.endpointKind == abd::hwid::MidiEndpointKind::VirtualDriver)
    {
        return false;
    }

    // Unknown endpoints cannot be automatically accepted for physical bench preflight
    // without out-of-band inspection and explicit local policy
    if (snapshot.endpointKind == abd::hwid::MidiEndpointKind::Unknown)
    {
        return false;
    }

    // Physical USB or DIN interface are acceptable for preflight
    return (snapshot.endpointKind == abd::hwid::MidiEndpointKind::PhysicalUsb ||
            snapshot.endpointKind == abd::hwid::MidiEndpointKind::PhysicalDinInterface);
}

OperatorConfirmationResult SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(
    const OperatorConfirmationContext& context) noexcept
{
    OperatorConfirmationResult result;

    if (context.operatorId.empty())
    {
        result.confirmed = false;
        result.identityState = HardwareIdentityState::IdentityUnavailable;
        result.diagnosticCode = "ERR_OPERATOR_ID_REQUIRED";
        result.diagnosticMessage = "Operator ID cannot be empty for manual hardware confirmation.";
        return result;
    }

    if (context.targetContractId.empty())
    {
        result.confirmed = false;
        result.identityState = HardwareIdentityState::IdentityUnavailable;
        result.diagnosticCode = "ERR_TARGET_CONTRACT_REQUIRED";
        result.diagnosticMessage = "Target contract ID cannot be empty for manual hardware confirmation.";
        return result;
    }

    if (context.selectedPortStableDeviceId.empty())
    {
        result.confirmed = false;
        result.identityState = HardwareIdentityState::IdentityUnavailable;
        result.diagnosticCode = "ERR_PORT_ID_REQUIRED";
        result.diagnosticMessage = "Selected port stable device ID cannot be empty for manual hardware confirmation.";
        return result;
    }

    if (context.priorIdentityState != HardwareIdentityState::IdentityUnavailable)
    {
        result.confirmed = false;
        result.identityState = context.priorIdentityState;
        result.diagnosticCode = "ERR_INVALID_PRIOR_IDENTITY_STATE";
        result.diagnosticMessage = "Operator confirmation can only be applied when prior identity state is IdentityUnavailable.";
        return result;
    }

    if (!context.explicitConfirmationRecorded)
    {
        result.confirmed = false;
        result.identityState = HardwareIdentityState::IdentityUnavailable;
        result.diagnosticCode = "ERR_EXPLICIT_CONFIRMATION_REQUIRED";
        result.diagnosticMessage = "Explicit affirmative operator confirmation must be recorded.";
        return result;
    }

    // All fail-closed gates passed: yield UserConfirmedUnverified
    result.confirmed = true;
    result.identityState = HardwareIdentityState::UserConfirmedUnverified;
    result.diagnosticCode = "OK_OPERATOR_CONFIRMED";
    result.diagnosticMessage = "Operator explicitly confirmed non-verifiable hardware profile under local authority.";
    return result;
}

abd::hwid::SharedMidiEndpointSnapshot SharedMidiHardwareAdapter::createSnapshot(
    const abd::hwid::MidiEndpointDescriptor& endpoint,
    const abd::hwid::MidiEndpointSafetyPolicy& policy,
    const abd::hwid::BroadcastInquiryAuthorization& authorization,
    abd::hwid::HardwareMidiIdentityState identityState) noexcept
{
    abd::hwid::SharedMidiEndpointSnapshot snap;
    snap.stableDeviceId = endpoint.stableDeviceId;
    snap.displayName = endpoint.displayName;
    snap.endpointKind = endpoint.kind;
    snap.identityState = identityState;
    snap.isSysExVerified = abd::hwid::toLegacyIsSysExVerified(identityState);
    snap.eligibleForAutomaticDiscovery = abd::hwid::isAutomaticDiscoveryAllowed(endpoint, policy);
    snap.inquiryDecision = abd::hwid::evaluateUniversalInquiryEligibility(endpoint, policy, authorization);

    if (identityState == abd::hwid::HardwareMidiIdentityState::IdentityVerified)
    {
        snap.diagnosticCode = "IDENT_VERIFIED";
        snap.diagnosticMessage = "Hardware identity verified via SysEx signature.";
    }
    else if (identityState == abd::hwid::HardwareMidiIdentityState::IdentityMismatch)
    {
        snap.diagnosticCode = "IDENT_MISMATCH";
        snap.diagnosticMessage = "Connected hardware identity contradicts target profile.";
    }
    else
    {
        snap.diagnosticCode = "IDENT_UNVERIFIED";
        snap.diagnosticMessage = "Hardware identity unverified or unavailable.";
    }

    return snap;
}

} // namespace abdaudiolab::hardware
