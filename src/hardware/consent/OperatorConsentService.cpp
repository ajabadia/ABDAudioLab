#include "OperatorConsentService.h"
#include "../../synth/Sha256.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::hardware
{

std::string OperatorConsentService::computeCommandDigest(const OperatorConsentRequest& request)
{
    std::ostringstream ss;
    ss << "TARGET:" << request.targetProfileId << "|"
       << "VENDOR:" << request.vendor << "|"
       << "PORT_ID:" << request.portSelection.stableDeviceId << "|"
       << "PORT_NAME:" << request.portSelection.displayName << "|"
       << "RECIPE_HASH:" << request.recipeDocumentHash << "|"
       << "PLAN_HASH:" << request.resolvedExecutionPlanHash << "|"
       << "SEMANTIC_ID:" << request.semanticId << "|"
       << "NORM_VAL:" << std::fixed << std::setprecision(6) << request.normalizedValue << "|";

    if (request.rawValue.has_value())
    {
        ss << "RAW_VAL:" << *request.rawValue << "|";
    }

    if (request.ccMessage.has_value())
    {
        ss << "CC:" << static_cast<int>(request.ccMessage->channel) << ":"
           << static_cast<int>(request.ccMessage->controllerNumber) << ":"
           << static_cast<int>(request.ccMessage->value) << "|";
    }

    if (request.sysExMessage.has_value())
    {
        ss << "SYSEX:";
        for (const auto b : request.sysExMessage->bytes)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        ss << "|";
    }

    ss << "DELAY_MS:" << request.minimumInterMessageDelayMs;

    return synth::Sha256::computeHex(ss.str());
}

OperatorConsentRequest OperatorConsentService::buildRequest(
    const profiling::TargetProfile& profile,
    ProfileProvenanceOrigin origin,
    const HardwarePreflightResult& preflight,
    const std::string& recipeDocumentHash,
    const std::string& resolvedExecutionPlanHash,
    const std::string& semanticId,
    double normalizedValue,
    const std::optional<MidiCcMessage>& ccMessage,
    const std::optional<MidiSysExMessage>& sysExMessage)
{
    OperatorConsentRequest req;
    req.targetProfileId = profile.targetProfileId;
    req.targetDisplayName = profile.displayName;
    req.vendor = profile.vendor;
    req.profileOrigin = origin;
    req.portSelection = preflight.portSelection;
    req.identityState = preflight.identityState;
    req.preflightDisposition = preflight.disposition;

    req.recipeDocumentHash = recipeDocumentHash;
    req.resolvedExecutionPlanHash = resolvedExecutionPlanHash;
    req.semanticId = semanticId;
    req.parameterDisplayName = semanticId;
    req.normalizedValue = normalizedValue;

    req.ccMessage = ccMessage;
    req.sysExMessage = sysExMessage;
    req.containsSysEx = sysExMessage.has_value();

    if (ccMessage.has_value())
    {
        req.rawValue = static_cast<int>(ccMessage->value);
    }
    else if (sysExMessage.has_value() && !sysExMessage->bytes.empty())
    {
        req.rawValue = static_cast<int>(std::round(normalizedValue * 127.0));
    }

    req.minimumInterMessageDelayMs = profile.transportPolicy.minimumInterMessageDelayMs;
    req.maximumMessagesPerSecond = profile.transportPolicy.maximumMessagesPerSecond;

    req.profileIsBuiltIn = (origin == ProfileProvenanceOrigin::BuiltIn);
    req.profileIsTrusted = (origin == ProfileProvenanceOrigin::BuiltIn || origin == ProfileProvenanceOrigin::ImportedTrusted);

    req.requiresIdentityConfirmation = (preflight.identityState == HardwareIdentityState::IdentityUnavailable &&
                                        preflight.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);

    // Explicit confirmation mandated if:
    // 1. Policy declares requiresExplicitConfirmation
    // 2. Contains SysEx message (always explicit confirmation)
    // 3. Profile provenance is untrusted import
    // 4. Target identity is unverified and requires operator confirmation
    req.requiresExplicitConfirmation = profile.transportPolicy.requiresExplicitConfirmation ||
                                       req.containsSysEx ||
                                       (origin == ProfileProvenanceOrigin::ImportedUntrusted) ||
                                       req.requiresIdentityConfirmation;

    req.commandDigest = computeCommandDigest(req);
    req.requestId = "REQ_" + req.commandDigest.substr(0, 16);

    // Generate human-readable summary
    std::ostringstream summary;
    summary << "Target: " << req.vendor << " " << req.targetDisplayName << " (" << req.targetProfileId << ")\n"
            << "Port: " << req.portSelection.displayName << " [" << req.portSelection.stableDeviceId << "]\n"
            << "Parameter: " << req.semanticId << " (Normalized: " << req.normalizedValue << ")\n";
    if (req.containsSysEx)
    {
        summary << "Type: SysEx Transmission (" << req.sysExMessage->bytes.size() << " bytes)\n";
    }
    else if (req.ccMessage.has_value())
    {
        summary << "Type: MIDI CC (Ch " << static_cast<int>(req.ccMessage->channel)
                << ", CC " << static_cast<int>(req.ccMessage->controllerNumber)
                << ", Value " << static_cast<int>(req.ccMessage->value) << ")\n";
    }
    if (req.requiresIdentityConfirmation)
    {
        summary << "WARNING: Device identity is unverified automatically. Operator confirmation required.\n";
    }
    if (origin == ProfileProvenanceOrigin::ImportedUntrusted)
    {
        summary << "WARNING: Untrusted profile import. Verification required before transmission.\n";
    }

    req.humanReadableSummary = summary.str();
    return req;
}

bool OperatorConsentService::requiresExplicitConsent(const OperatorConsentRequest& request)
{
    return request.requiresExplicitConfirmation;
}

OperatorConsentResult OperatorConsentService::grantConsent(
    const OperatorConsentRequest& request,
    const std::string& operatorNote)
{
    OperatorConsentResult result;
    result.requestId = request.requestId;
    result.commandDigest = request.commandDigest;

    // Reject immediately if identity mismatched
    if (request.identityState == HardwareIdentityState::IdentityMismatch)
    {
        result.decision = OperatorConsentDecision::BlockedByIdentityMismatch;
        result.resultingIdentityState = HardwareIdentityState::IdentityMismatch;
        result.readyForDispatch = false;
        result.diagnosticCode = "ERR_IDENTITY_MISMATCH_BLOCKED";
        result.diagnosticMessage = "Operation strictly blocked: hardware identity mismatches target profile.";
        return result;
    }

    // Evaluate identity transition for unverified target
    if (request.identityState == HardwareIdentityState::IdentityUnavailable)
    {
        if (request.preflightDisposition == HardwarePreflightDisposition::AwaitingUserConfirmation)
        {
            result.decision = OperatorConsentDecision::Granted;
            result.resultingIdentityState = HardwareIdentityState::UserConfirmedUnverified;
            result.readyForDispatch = true;
            result.diagnosticCode = "OK";
            result.diagnosticMessage = "Consent granted by operator; unverified target identity confirmed.";
            return result;
        }

        // Identity unavailable and not awaiting confirmation -> Blocked
        result.decision = OperatorConsentDecision::Denied;
        result.resultingIdentityState = HardwareIdentityState::IdentityUnavailable;
        result.readyForDispatch = false;
        result.diagnosticCode = "ERR_VERIFIED_IDENTITY_REQUIRED";
        result.diagnosticMessage = "Operation blocked: TargetProfile policy forbids unverified dispatch.";
        return result;
    }

    // Default case: IdentityVerified or PortAvailable
    result.decision = OperatorConsentDecision::Granted;
    result.resultingIdentityState = request.identityState;
    result.readyForDispatch = true;
    result.diagnosticCode = "OK";
    result.diagnosticMessage = operatorNote.empty() ? "Consent granted by operator." : operatorNote;
    return result;
}

OperatorConsentResult OperatorConsentService::denyConsent(
    const OperatorConsentRequest& request,
    const std::string& denialReason)
{
    OperatorConsentResult result;
    result.requestId = request.requestId;
    result.commandDigest = request.commandDigest;
    result.decision = OperatorConsentDecision::Denied;
    result.resultingIdentityState = request.identityState;
    result.readyForDispatch = false;
    result.diagnosticCode = "ERR_OPERATOR_DENIED";
    result.diagnosticMessage = denialReason.empty() ? "Operator cancelled physical command." : denialReason;
    return result;
}

OperatorConsentResult OperatorConsentService::validateConsentToken(
    const OperatorConsentRequest& originalRequest,
    const OperatorConsentResult& priorConsent,
    const std::string& currentPlanHash,
    const MidiPortSelection& currentPort,
    const std::optional<MidiCcMessage>& currentCc,
    const std::optional<MidiSysExMessage>& currentSysEx)
{
    OperatorConsentResult result = priorConsent;

    if (priorConsent.decision != OperatorConsentDecision::Granted)
    {
        result.readyForDispatch = false;
        result.diagnosticCode = "ERR_CONSENT_NOT_GRANTED";
        result.diagnosticMessage = "Prior consent was not in Granted state.";
        return result;
    }

    // Anti-TOCTOU validation: verify that plan hash, port, and message payloads are identical
    const bool planMatches = (currentPlanHash == originalRequest.resolvedExecutionPlanHash);
    const bool portMatches = (currentPort == originalRequest.portSelection);
    const bool ccMatches = (currentCc == originalRequest.ccMessage);
    const bool sysExMatches = (currentSysEx == originalRequest.sysExMessage);

    if (!planMatches || !portMatches || !ccMatches || !sysExMatches)
    {
        result.decision = OperatorConsentDecision::Invalidated;
        result.readyForDispatch = false;
        result.diagnosticCode = "ERR_CONSENT_INVALIDATED_TOCTOU";
        result.diagnosticMessage = "Execution plan, target port, or message payload changed after consent was granted.";
        return result;
    }

    result.readyForDispatch = true;
    return result;
}

} // namespace abdaudiolab::hardware
