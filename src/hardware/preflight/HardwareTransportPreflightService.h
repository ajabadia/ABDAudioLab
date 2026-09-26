#pragma once

#include "HardwareTransportPreflightTypes.h"
#include "IMidiIdentityProbe.h"
#include "hardware/transport/IMidiTransport.h"
#include "profiling/TargetProfile.h"

namespace abdaudiolab::hardware
{

class HardwareTransportPreflightService
{
public:
    /**
     * @brief Evaluates preflight conditions for hardware dispatch.
     * 
     * Invariants:
     * - D2.3 never produces UserConfirmedUnverified (reserved for D2.4 OperatorConsentService).
     * - If transport fails to open -> Blocked with ERR_MIDI_OUTPUT_OPEN_FAILED.
     * - If probe produces Mismatch -> Blocked with ERR_IDENTITY_MISMATCH.
     * - If probe is Verified -> Ready with IdentityVerified.
     * - If identity is Unavailable:
     *     - if requiresVerifiedIdentity == true -> Blocked with ERR_VERIFIED_IDENTITY_REQUIRED.
     *     - if allowsUserConfirmedUnverifiedIdentity == true -> AwaitingUserConfirmation.
     *     - otherwise -> Blocked with ERR_UNVERIFIED_IDENTITY_NOT_ALLOWED.
     * 
     * @param profile Target profile specifying transportPolicy.
     * @param portSelection Requested MIDI port.
     * @param transport Output transport abstraction.
     * @param identityProbe Optional identity probe (may be nullptr if unsupported).
     * @return HardwarePreflightResult containing disposition, identity state, and diagnostic codes.
     */
    static HardwarePreflightResult evaluate(
        const abdaudiolab::profiling::TargetProfile& profile,
        const MidiPortSelection& portSelection,
        IMidiTransport& transport,
        IMidiIdentityProbe* identityProbe = nullptr);
};

} // namespace abdaudiolab::hardware
