#include "HardwareDispatchScheduler.h"
#include "hardware/consent/OperatorConsentService.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "../../synth/Sha256.h"

namespace abdaudiolab::hardware
{

HardwareDispatchScheduler::HardwareDispatchScheduler(IMonotonicClock& clock) noexcept
    : clock_(clock)
{
}

void HardwareDispatchScheduler::resetPacing() noexcept
{
    lastTransmissionMonotonicMs_ = 0;
    hasTransmitted_ = false;
    rateWindowStartMs_ = 0;
    messagesSentInCurrentSecond_ = 0;
    sequenceCounter_ = 0;
}

uint64_t HardwareDispatchScheduler::getLastTransmissionMonotonicMs() const noexcept
{
    return lastTransmissionMonotonicMs_;
}

HardwareDispatchDecision HardwareDispatchScheduler::evaluateDispatchWindow(
    const OperatorConsentRequest& request,
    const OperatorConsentResult& consent,
    const std::string& activePlanHash,
    const MidiPortSelection& activePort,
    const std::optional<MidiCcMessage>& activeCc,
    const std::optional<MidiSysExMessage>& activeSysEx,
    const profiling::TransportPolicy& policy) const
{
    HardwareDispatchDecision decision;

    // 1. Preflight status validation
    if (request.identityState == HardwareIdentityState::IdentityMismatch)
    {
        decision.decision = DispatchDecision::BlockedByPreflight;
        decision.diagnosticCode = "ERR_IDENTITY_MISMATCH_BLOCKED";
        decision.diagnosticMessage = "Dispatch strictly blocked due to hardware identity mismatch.";
        return decision;
    }

    // 2. Anti-TOCTOU validation of the consent token against active execution parameters
    const auto consentValidation = OperatorConsentService::validateConsentToken(
        request, consent, activePlanHash, activePort, activeCc, activeSysEx);

    if (consentValidation.decision != OperatorConsentDecision::Granted)
    {
        decision.decision = DispatchDecision::BlockedByConsent;
        decision.diagnosticCode = consentValidation.diagnosticCode;
        decision.diagnosticMessage = consentValidation.diagnosticMessage;
        return decision;
    }

    const uint64_t now = clock_.nowMs();

    // 3. Minimum inter-message delay pacing validation
    if (hasTransmitted_ && policy.minimumInterMessageDelayMs > 0)
    {
        const uint64_t elapsed = (now >= lastTransmissionMonotonicMs_) ? (now - lastTransmissionMonotonicMs_) : 0;
        const uint64_t requiredDelay = static_cast<uint64_t>(policy.minimumInterMessageDelayMs);

        if (elapsed < requiredDelay)
        {
            decision.decision = DispatchDecision::PendingPacingWindow;
            decision.earliestAllowedMonotonicTimeMs = lastTransmissionMonotonicMs_ + requiredDelay;
            decision.diagnosticCode = "ERR_DISPATCH_PACING_PENDING";
            decision.diagnosticMessage = "Pacing delay window active. Transmission must wait until earliestAllowedMonotonicTimeMs.";
            return decision;
        }
    }

    // 4. Rate limiting per second validation
    if (policy.maximumMessagesPerSecond > 0)
    {
        if (now < rateWindowStartMs_ || now >= rateWindowStartMs_ + 1000)
        {
            // Window expired or reset; rate limit allows immediate send
        }
        else if (messagesSentInCurrentSecond_ >= policy.maximumMessagesPerSecond)
        {
            decision.decision = DispatchDecision::PendingPacingWindow;
            decision.earliestAllowedMonotonicTimeMs = rateWindowStartMs_ + 1000;
            decision.diagnosticCode = "ERR_DISPATCH_RATE_LIMIT_EXCEEDED";
            decision.diagnosticMessage = "Maximum messages per second limit reached. Transmission pending next 1s window.";
            return decision;
        }
    }

    decision.decision = DispatchDecision::DispatchNow;
    decision.earliestAllowedMonotonicTimeMs = now;
    decision.diagnosticCode = "OK";
    decision.diagnosticMessage = "Dispatch window is open.";
    return decision;
}

HardwareDispatchDecision HardwareDispatchScheduler::dispatchCc(
    IMidiTransport& transport,
    const OperatorConsentRequest& request,
    const OperatorConsentResult& consent,
    const std::string& activePlanHash,
    const MidiPortSelection& activePort,
    const MidiCcMessage& message,
    const profiling::TransportPolicy& policy)
{
    const auto eval = evaluateDispatchWindow(
        request, consent, activePlanHash, activePort, message, std::nullopt, policy);

    if (eval.decision != DispatchDecision::DispatchNow)
    {
        return eval;
    }

    // Attempt physical/mock transmission
    const auto sendResult = transport.sendCc(message);
    if (!sendResult.acceptedForTransmission)
    {
        HardwareDispatchDecision failedDecision;
        failedDecision.decision = DispatchDecision::Failed;
        if (sendResult.error == MidiTransportError::Disconnected)
        {
            failedDecision.diagnosticCode = "ERR_MIDI_TRANSPORT_DISCONNECTED";
        }
        else if (sendResult.error == MidiTransportError::NotOpen)
        {
            failedDecision.diagnosticCode = "ERR_MIDI_TRANSPORT_NOT_OPEN";
        }
        else
        {
            failedDecision.diagnosticCode = sendResult.diagnosticCode.empty() 
                ? "ERR_MIDI_TRANSPORT_WRITE_FAILED" 
                : sendResult.diagnosticCode;
        }
        failedDecision.diagnosticMessage = "Transport failed to write CC message: " + sendResult.message;
        return failedDecision;
    }

    // Update transmission timestamps and rate accounting
    const uint64_t now = clock_.nowMs();
    lastTransmissionMonotonicMs_ = now;
    hasTransmitted_ = true;

    if (now < rateWindowStartMs_ || now >= rateWindowStartMs_ + 1000)
    {
        rateWindowStartMs_ = now;
        messagesSentInCurrentSecond_ = 1;
    }
    else
    {
        messagesSentInCurrentSecond_++;
    }

    HardwareDispatchDecision successDecision;
    successDecision.decision = DispatchDecision::DispatchNow;
    successDecision.earliestAllowedMonotonicTimeMs = now;
    successDecision.diagnosticCode = "OK";
    successDecision.diagnosticMessage = "CC message transmitted successfully.";
    return successDecision;
}

HardwareDispatchDecision HardwareDispatchScheduler::dispatchSysEx(
    IMidiTransport& transport,
    const OperatorConsentRequest& request,
    const OperatorConsentResult& consent,
    const std::string& activePlanHash,
    const MidiPortSelection& activePort,
    const MidiSysExMessage& message,
    const profiling::TransportPolicy& policy)
{
    const auto eval = evaluateDispatchWindow(
        request, consent, activePlanHash, activePort, std::nullopt, message, policy);

    if (eval.decision != DispatchDecision::DispatchNow)
    {
        return eval;
    }

    // Attempt physical/mock transmission
    const auto sendResult = transport.sendSysEx(message);
    if (!sendResult.acceptedForTransmission)
    {
        HardwareDispatchDecision failedDecision;
        failedDecision.decision = DispatchDecision::Failed;
        if (sendResult.error == MidiTransportError::Disconnected)
        {
            failedDecision.diagnosticCode = "ERR_MIDI_TRANSPORT_DISCONNECTED";
        }
        else if (sendResult.error == MidiTransportError::NotOpen)
        {
            failedDecision.diagnosticCode = "ERR_MIDI_TRANSPORT_NOT_OPEN";
        }
        else
        {
            failedDecision.diagnosticCode = sendResult.diagnosticCode.empty() 
                ? "ERR_MIDI_TRANSPORT_WRITE_FAILED" 
                : sendResult.diagnosticCode;
        }
        failedDecision.diagnosticMessage = "Transport failed to write SysEx message: " + sendResult.message;
        return failedDecision;
    }

    // Update transmission timestamps and rate accounting
    const uint64_t now = clock_.nowMs();
    lastTransmissionMonotonicMs_ = now;
    hasTransmitted_ = true;

    if (now < rateWindowStartMs_ || now >= rateWindowStartMs_ + 1000)
    {
        rateWindowStartMs_ = now;
        messagesSentInCurrentSecond_ = 1;
    }
    else
    {
        messagesSentInCurrentSecond_++;
    }

    HardwareDispatchDecision successDecision;
    successDecision.decision = DispatchDecision::DispatchNow;
    successDecision.earliestAllowedMonotonicTimeMs = now;
    successDecision.diagnosticCode = "OK";
    successDecision.diagnosticMessage = "SysEx message transmitted successfully.";
    return successDecision;
}

HardwareDispatchEvidenceRecord HardwareDispatchScheduler::executeDispatchCc(
    IMidiTransport& transport,
    IMidiResponseAwaiter* awaiter,
    const OperatorConsentRequest& request,
    const OperatorConsentResult& consent,
    const std::string& activePlanHash,
    const MidiPortSelection& activePort,
    const MidiCcMessage& message,
    const profiling::TransportPolicy& policy,
    const ExpectedMidiResponse& expectedResponse)
{
    HardwareDispatchEvidenceRecord record;
    record.targetProfileId = request.targetProfileId;
    record.targetContractId = request.targetContractId;
    record.targetResolutionSource = request.targetResolutionSource;
    record.benchSessionId = request.benchSessionId;
    record.recipeDocumentHash = request.recipeDocumentHash;
    record.resolvedExecutionPlanHash = activePlanHash;
    record.portSelection = activePort;
    record.identityState = request.identityState;
    record.sequenceNumber = ++sequenceCounter_;
    record.monotonicTimestampMs = clock_.nowMs();
    record.semanticId = request.semanticId;
    record.nativeParameterId = request.semanticId;
    record.commandDigest = request.commandDigest;

    record.messageDigest = OperatorConsentService::computeMessageDigest(message);

    // 1. Preflight status validation
    if (request.identityState == HardwareIdentityState::IdentityMismatch)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.consentWasGranted = false;
        record.exportBlocked = true;
        record.diagnosticCode = "ERR_IDENTITY_MISMATCH_BLOCKED";
        record.diagnosticMessage = "Dispatch strictly blocked due to hardware identity mismatch.";
        return record;
    }

    // 2. Anti-TOCTOU validation of the consent token
    const auto consentValidation = OperatorConsentService::validateConsentToken(
        request, consent, activePlanHash, activePort, message, std::nullopt);

    if (consentValidation.decision != OperatorConsentDecision::Granted)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.consentWasGranted = false;
        record.exportBlocked = true;
        record.diagnosticCode = consentValidation.diagnosticCode;
        record.diagnosticMessage = consentValidation.diagnosticMessage;
        return record;
    }

    record.consentWasGranted = true;

    // 3. Pacing & dispatch
    const auto decision = dispatchCc(transport, request, consent, activePlanHash, activePort, message, policy);

    if (decision.decision == DispatchDecision::PendingPacingWindow)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.exportBlocked = false; // Pacing wait is not a terminal failure
        record.diagnosticCode = decision.diagnosticCode;
        record.diagnosticMessage = decision.diagnosticMessage;
        return record;
    }

    if (decision.decision != DispatchDecision::DispatchNow)
    {
        record.exportBlocked = true;
        if (decision.diagnosticCode == "ERR_MIDI_TRANSPORT_DISCONNECTED")
            record.transportError = MidiTransportError::Disconnected;
        else if (decision.diagnosticCode == "ERR_MIDI_TRANSPORT_NOT_OPEN")
            record.transportError = MidiTransportError::NotOpen;
        else
            record.transportError = MidiTransportError::WriteFailed;

        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.diagnosticCode = decision.diagnosticCode;
        record.diagnosticMessage = decision.diagnosticMessage;
        return record;
    }

    // Transport transmission succeeded
    record.transportError = MidiTransportError::None;

    // 4. Response / ACK evaluation if required by profile policy
    if (policy.requiresResponseAck)
    {
        if (awaiter == nullptr)
        {
            record.exportBlocked = true;
            record.responseOutcome = MidiResponseOutcome::Unsupported;
            record.diagnosticCode = "ERR_MIDI_AWAITER_MISSING";
            record.diagnosticMessage = "Policy requires response ACK but no response awaiter was provided.";
            return record;
        }

        const auto waitResult = awaiter->waitForResponse(expectedResponse, static_cast<uint64_t>(policy.responseTimeoutMs));
        record.responseOutcome = waitResult.outcome;

        if (waitResult.outcome == MidiResponseOutcome::Received)
        {
            record.exportBlocked = false;
            record.diagnosticCode = "OK";
            record.diagnosticMessage = "CC command dispatched and acknowledged.";
        }
        else if (waitResult.outcome == MidiResponseOutcome::Timeout)
        {
            record.exportBlocked = true;
            record.diagnosticCode = "ERR_MIDI_TARGET_NO_RESPONSE";
            record.diagnosticMessage = "Target timed out without required ACK.";
        }
        else if (waitResult.outcome == MidiResponseOutcome::Disconnected)
        {
            record.exportBlocked = true;
            record.transportError = MidiTransportError::Disconnected;
            record.diagnosticCode = "ERR_MIDI_TRANSPORT_DISCONNECTED";
            record.diagnosticMessage = "Device disconnected while awaiting ACK.";
        }
        else
        {
            record.exportBlocked = true;
            record.diagnosticCode = "ERR_MIDI_RESPONSE_MISMATCH";
            record.diagnosticMessage = "Received ACK mismatch or unsupported response.";
        }
        return record;
    }

    // requiresResponseAck == false: absence of ACK is not an error
    record.responseOutcome = MidiResponseOutcome::Unsupported;
    record.exportBlocked = false;
    record.diagnosticCode = "OK";
    record.diagnosticMessage = "CC message dispatched successfully (no ACK required).";
    return record;
}

HardwareDispatchEvidenceRecord HardwareDispatchScheduler::executeDispatchSysEx(
    IMidiTransport& transport,
    IMidiResponseAwaiter* awaiter,
    const OperatorConsentRequest& request,
    const OperatorConsentResult& consent,
    const std::string& activePlanHash,
    const MidiPortSelection& activePort,
    const MidiSysExMessage& message,
    const profiling::TransportPolicy& policy,
    const ExpectedMidiResponse& expectedResponse)
{
    HardwareDispatchEvidenceRecord record;
    record.targetProfileId = request.targetProfileId;
    record.targetContractId = request.targetContractId;
    record.targetResolutionSource = request.targetResolutionSource;
    record.benchSessionId = request.benchSessionId;
    record.recipeDocumentHash = request.recipeDocumentHash;
    record.resolvedExecutionPlanHash = activePlanHash;
    record.portSelection = activePort;
    record.identityState = request.identityState;
    record.sequenceNumber = ++sequenceCounter_;
    record.monotonicTimestampMs = clock_.nowMs();
    record.semanticId = request.semanticId;
    record.nativeParameterId = request.semanticId;
    record.commandDigest = request.commandDigest;

    record.messageDigest = OperatorConsentService::computeMessageDigest(message);

    // 1. Preflight status validation
    if (request.identityState == HardwareIdentityState::IdentityMismatch)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.consentWasGranted = false;
        record.exportBlocked = true;
        record.diagnosticCode = "ERR_IDENTITY_MISMATCH_BLOCKED";
        record.diagnosticMessage = "Dispatch strictly blocked due to hardware identity mismatch.";
        return record;
    }

    // 2. Anti-TOCTOU validation of the consent token
    const auto consentValidation = OperatorConsentService::validateConsentToken(
        request, consent, activePlanHash, activePort, std::nullopt, message);

    if (consentValidation.decision != OperatorConsentDecision::Granted)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.consentWasGranted = false;
        record.exportBlocked = true;
        record.diagnosticCode = consentValidation.diagnosticCode;
        record.diagnosticMessage = consentValidation.diagnosticMessage;
        return record;
    }

    record.consentWasGranted = true;

    // 3. Pacing & dispatch
    const auto decision = dispatchSysEx(transport, request, consent, activePlanHash, activePort, message, policy);

    if (decision.decision == DispatchDecision::PendingPacingWindow)
    {
        record.transportError = MidiTransportError::None;
        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.exportBlocked = false; // Timing pause, not terminal failure
        record.diagnosticCode = decision.diagnosticCode;
        record.diagnosticMessage = decision.diagnosticMessage;
        return record;
    }

    if (decision.decision != DispatchDecision::DispatchNow)
    {
        record.exportBlocked = true;
        if (decision.diagnosticCode == "ERR_MIDI_TRANSPORT_DISCONNECTED")
            record.transportError = MidiTransportError::Disconnected;
        else if (decision.diagnosticCode == "ERR_MIDI_TRANSPORT_NOT_OPEN")
            record.transportError = MidiTransportError::NotOpen;
        else
            record.transportError = MidiTransportError::WriteFailed;

        record.responseOutcome = MidiResponseOutcome::Unsupported;
        record.diagnosticCode = decision.diagnosticCode;
        record.diagnosticMessage = decision.diagnosticMessage;
        return record;
    }

    // Transport transmission succeeded
    record.transportError = MidiTransportError::None;

    // 4. Response / ACK evaluation if required by profile policy
    if (policy.requiresResponseAck)
    {
        if (awaiter == nullptr)
        {
            record.exportBlocked = true;
            record.responseOutcome = MidiResponseOutcome::Unsupported;
            record.diagnosticCode = "ERR_MIDI_AWAITER_MISSING";
            record.diagnosticMessage = "Policy requires response ACK but no response awaiter was provided.";
            return record;
        }

        const auto waitResult = awaiter->waitForResponse(expectedResponse, static_cast<uint64_t>(policy.responseTimeoutMs));
        record.responseOutcome = waitResult.outcome;

        if (waitResult.outcome == MidiResponseOutcome::Received)
        {
            record.exportBlocked = false;
            record.diagnosticCode = "OK";
            record.diagnosticMessage = "SysEx command dispatched and acknowledged.";
        }
        else if (waitResult.outcome == MidiResponseOutcome::Timeout)
        {
            record.exportBlocked = true;
            record.diagnosticCode = "ERR_MIDI_TARGET_NO_RESPONSE";
            record.diagnosticMessage = "Target timed out without required ACK.";
        }
        else if (waitResult.outcome == MidiResponseOutcome::Disconnected)
        {
            record.exportBlocked = true;
            record.transportError = MidiTransportError::Disconnected;
            record.diagnosticCode = "ERR_MIDI_TRANSPORT_DISCONNECTED";
            record.diagnosticMessage = "Device disconnected while awaiting ACK.";
        }
        else
        {
            record.exportBlocked = true;
            record.diagnosticCode = "ERR_MIDI_RESPONSE_MISMATCH";
            record.diagnosticMessage = "Received ACK mismatch or unsupported response.";
        }
        return record;
    }

    // requiresResponseAck == false
    record.responseOutcome = MidiResponseOutcome::Unsupported;
    record.exportBlocked = false;
    record.diagnosticCode = "OK";
    record.diagnosticMessage = "SysEx message dispatched successfully (no ACK required).";
    return record;
}

} // namespace abdaudiolab::hardware
