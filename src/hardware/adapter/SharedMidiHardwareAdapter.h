/**
 * @file SharedMidiHardwareAdapter.h
 * @brief Boundary adapter translating ABDSharedCode MIDI endpoint snapshots
 *        and safety policies into ABDAudioLab domain types without conceding authority.
 * @details Implements HITO-SHARED-SYNC / SS5 boundary contract:
 *          "Un endpoint compartido puede ser elegible para discovery, pero no queda por ello
 *           autorizado para una sesión metrológica, para un despacho físico ni para una exportación."
 *
 *          Boundary Rules:
 *          - ABDSharedCode produces read-only information and non-executable intentions.
 *          - ABDAudioLab retains 100% of consent, anti-TOCTOU, scheduling, session transport,
 *            evidence logging, fail-closed handling, and ExportReadiness governance.
 *          - UserConfirmedUnverified NEVER originates in ABDSharedCode.
 *          - Unknown endpoints require explicit warning badges and are NEVER auto-eligible
 *            for physical bench preflight.
 *          - Virtual endpoints are forbidden from physical bench preflight.
 *          - Shared eligibility NEVER confers dispatch authority (isSessionDispatchAllowed == false).
 *          - ZERO MIDI bytes transmitted during adaptation or evaluation.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <optional>
#include <HardwareMidiDetect/MidiEndpointSafetyPolicy.h>
#include <HardwareMidiDetect/HardwareContract.h>
#include "hardware/preflight/HardwareTransportPreflightTypes.h"
#include "gui/session/ProfilingSessionContracts.h"

namespace abdaudiolab::hardware
{

/**
 * @struct OperatorConfirmationContext
 * @brief Mandatory context required by ABDAudioLab to instantiate UserConfirmedUnverified.
 * @invariant ABDSharedCode cannot construct or accept this context.
 */
struct OperatorConfirmationContext
{
    std::string operatorId;
    std::string targetContractId;
    std::string selectedPortStableDeviceId;
    HardwareIdentityState priorIdentityState { HardwareIdentityState::IdentityUnavailable };
    bool explicitConfirmationRecorded { false };
};

/**
 * @struct OperatorConfirmationResult
 * @brief Fail-closed outcome of evaluating an operator confirmation request.
 */
struct OperatorConfirmationResult
{
    bool confirmed { false };
    HardwareIdentityState identityState { HardwareIdentityState::IdentityUnavailable };
    std::string diagnosticCode;
    std::string diagnosticMessage;

    [[nodiscard]] bool isSuccess() const noexcept { return confirmed && identityState == HardwareIdentityState::UserConfirmedUnverified; }
};

/**
 * @struct SharedEndpointInfo
 * @brief Local ABDAudioLab projection of a shared endpoint snapshot.
 */
struct SharedEndpointInfo
{
    std::string stableDeviceId;
    std::string displayName;
    abd::hwid::MidiEndpointKind endpointKind { abd::hwid::MidiEndpointKind::Unknown };
    HardwareIdentityState identityState { HardwareIdentityState::PortAvailable };
    bool isSysExVerified { false };
    bool eligibleForAutomaticDiscovery { false };
    abd::hwid::SysExInquiryDecision inquiryDecision { abd::hwid::SysExInquiryDecision::DisabledByPolicy };
    bool isPhysicalBenchEligible { false };
    bool requiresWarningBadge { false };
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

/**
 * @class SharedMidiHardwareAdapter
 * @brief Strict one-way boundary adapter from ABDSharedCode to ABDAudioLab.
 */
class SharedMidiHardwareAdapter
{
public:
    /**
     * @brief Map shared 5-state identity to ABDAudioLab local 5-state identity.
     */
    [[nodiscard]] static HardwareIdentityState mapSharedIdentityState(
        abd::hwid::HardwareMidiIdentityState sharedState) noexcept;

    /**
     * @brief Adapt a read-only shared endpoint snapshot into an ABDAudioLab domain info struct.
     */
    [[nodiscard]] static SharedEndpointInfo adaptSnapshot(
        const abd::hwid::SharedMidiEndpointSnapshot& snapshot) noexcept;

    /**
     * @brief Pure check of technical eligibility for physical test bench preflight evaluation.
     * @note SEMANTICS:
     * - true: The endpoint may advance to subsequent local physical bench preflight evaluation.
     * - false: The endpoint CANNOT automatically advance to physical preflight.
     * - true DOES NOT MEAN: identity verified, operator confirmed, consent granted,
     *   transport opened, session active, scheduler authorized, MIDI message permitted,
     *   nor ExportReadiness enabled.
     *
     * Rules:
     * - PhysicalUsb / PhysicalDinInterface: technically eligible if identity != Mismatch.
     * - VirtualLoopback / VirtualDriver: strictly FALSE.
     * - Unknown: strictly FALSE (requires local explicit policy and out-of-band operator inspection).
     */
    [[nodiscard]] static bool isPhysicalBenchEligible(
        const abd::hwid::SharedMidiEndpointSnapshot& snapshot) noexcept;

    /**
     * @brief Explicitly-named alias distinguishing evaluation eligibility from session authorization.
     */
    [[nodiscard]] static bool isEligibleForPhysicalBenchPreflightEvaluation(
        const abd::hwid::SharedMidiEndpointSnapshot& snapshot) noexcept
    {
        return isPhysicalBenchEligible(snapshot);
    }

    /**
     * @brief Pure authority boundary invariant:
     * A shared snapshot or shared eligibility NEVER confers session dispatch authority.
     * ALWAYS returns false.
     */
    [[nodiscard]] static constexpr bool isSessionDispatchAllowed(
        const abd::hwid::SharedMidiEndpointSnapshot& /*snapshot*/) noexcept
    {
        return false;
    }

    /**
     * @brief ABDAudioLab local factory for UserConfirmedUnverified.
     * Rejects:
     * - operatorId empty
     * - targetContractId empty
     * - selectedPortStableDeviceId empty
     * - priorIdentityState != IdentityUnavailable
     * - explicitConfirmationRecorded == false
     *
     * Invariants:
     * - Even on success, writes 0 bytes, schedules 0 dispatches, and leaves ExportReadiness Blocked.
     */
    [[nodiscard]] static OperatorConfirmationResult confirmUnverifiedOperatorIdentity(
        const OperatorConfirmationContext& context) noexcept;

    /**
     * @brief Helper to construct a boundary snapshot from shared parameters.
     */
    [[nodiscard]] static abd::hwid::SharedMidiEndpointSnapshot createSnapshot(
        const abd::hwid::MidiEndpointDescriptor& endpoint,
        const abd::hwid::MidiEndpointSafetyPolicy& policy,
        const abd::hwid::BroadcastInquiryAuthorization& authorization,
        abd::hwid::HardwareMidiIdentityState identityState) noexcept;
};

} // namespace abdaudiolab::hardware
