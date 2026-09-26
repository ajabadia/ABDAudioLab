#pragma once

#include "OperatorConsentRequest.h"
#include "profiling/TargetProfile.h"
#include "hardware/preflight/HardwareTransportPreflightTypes.h"

namespace abdaudiolab::hardware
{

class OperatorConsentService
{
public:
    /**
     * @brief Computes immutable deterministic command digest (SHA-256) binding plan, recipe, port and message.
     */
    static std::string computeCommandDigest(const OperatorConsentRequest& request);

    /**
     * @brief Builds a strongly typed OperatorConsentRequest from preflight, profile, plan and command.
     */
    static OperatorConsentRequest buildRequest(
        const profiling::TargetProfile& profile,
        ProfileProvenanceOrigin origin,
        const HardwarePreflightResult& preflight,
        const std::string& recipeDocumentHash,
        const std::string& resolvedExecutionPlanHash,
        const std::string& semanticId,
        double normalizedValue,
        const std::optional<MidiCcMessage>& ccMessage,
        const std::optional<MidiSysExMessage>& sysExMessage);

    /**
     * @brief Evaluates whether this specific request strictly mandates explicit operator approval.
     */
    static bool requiresExplicitConsent(const OperatorConsentRequest& request);

    /**
     * @brief Grants consent for the request.
     * Invariants:
     * - IdentityMismatch is strictly rejected (BlockedByIdentityMismatch, readyForDispatch = false).
     * - IdentityUnavailable with AwaitingUserConfirmation transitions to UserConfirmedUnverified if allowed.
     */
    static OperatorConsentResult grantConsent(
        const OperatorConsentRequest& request,
        const std::string& operatorNote = "");

    /**
     * @brief Denies or cancels consent. Guarantees 0 bytes authorized.
     */
    static OperatorConsentResult denyConsent(
        const OperatorConsentRequest& request,
        const std::string& denialReason = "Operator cancelled");

    /**
     * @brief Anti-TOCTOU validation: verifies prior consent against active execution state before transmission.
     */
    static OperatorConsentResult validateConsentToken(
        const OperatorConsentRequest& originalRequest,
        const OperatorConsentResult& priorConsent,
        const std::string& currentPlanHash,
        const MidiPortSelection& currentPort,
        const std::optional<MidiCcMessage>& currentCc,
        const std::optional<MidiSysExMessage>& currentSysEx);
};

} // namespace abdaudiolab::hardware
