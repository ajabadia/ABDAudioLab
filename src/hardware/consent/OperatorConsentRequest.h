#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "hardware/transport/MidiTransportTypes.h"
#include "hardware/preflight/HardwareTransportPreflightTypes.h"

namespace abdaudiolab::hardware
{

/**
 * @brief Provenance origin of the target profile.
 */
enum class ProfileProvenanceOrigin
{
    BuiltIn = 0,        // Bundled with ABDAudioLab release
    LocalUserCreated,   // Authored or confirmed locally by the operator
    ImportedUntrusted,  // Downloaded or imported without approved signature / review
    ImportedTrusted     // Imported with explicit operator approval or trusted provenance
};

/**
 * @brief Decisions issued by the operator or the consent validation gate.
 */
enum class OperatorConsentDecision
{
    Granted = 0,
    Denied,
    Expired,
    Invalidated,
    NotRequired,
    BlockedByIdentityMismatch
};

/**
 * @brief Immutable request for operator consent on a specific physical command.
 */
struct OperatorConsentRequest
{
    std::string requestId;

    std::string targetProfileId;
    std::string targetDisplayName;
    std::string vendor;
    ProfileProvenanceOrigin profileOrigin { ProfileProvenanceOrigin::BuiltIn };

    MidiPortSelection portSelection;

    HardwareIdentityState identityState { HardwareIdentityState::IdentityUnavailable };
    HardwarePreflightDisposition preflightDisposition { HardwarePreflightDisposition::Blocked };

    std::string recipeDocumentHash;
    std::string resolvedExecutionPlanHash;
    std::string recipeContextId;
    std::string executionPlanContextId;

    std::string semanticId;
    std::string parameterDisplayName;

    double normalizedValue { 0.0 };
    std::optional<int> rawValue;

    std::optional<MidiCcMessage> ccMessage;
    std::optional<MidiSysExMessage> sysExMessage;

    std::string targetContractId;
    std::string targetResolutionSource;
    std::string benchSessionId;

    std::string messageDigest;
    std::string commandCanonicalization { "abdaudiolab::hardware::OperatorConsentService::CanonicalV1" };
    std::string commandDigest;
    std::string humanReadableSummary;

    int minimumInterMessageDelayMs { 0 };
    int maximumMessagesPerSecond { 0 };

    bool profileIsBuiltIn { false };
    bool profileIsTrusted { false };
    bool requiresExplicitConfirmation { false };
    bool requiresIdentityConfirmation { false };
    bool requiresResponseAck { false };
    bool containsSysEx { false };
};

/**
 * @brief Immutable result of the operator consent evaluation.
 */
struct OperatorConsentResult
{
    OperatorConsentDecision decision { OperatorConsentDecision::Denied };

    HardwareIdentityState resultingIdentityState { HardwareIdentityState::IdentityUnavailable };
    bool readyForDispatch { false };

    std::string requestId;
    std::string commandDigest;
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

} // namespace abdaudiolab::hardware
