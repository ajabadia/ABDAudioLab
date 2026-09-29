#pragma once

#include <string>
#include <cstdint>
#include "MidiTransportTypes.h"
#include "IMidiResponseAwaiter.h"
#include "hardware/preflight/HardwareTransportPreflightTypes.h"

namespace abdaudiolab::hardware
{

/**
 * @brief Complete forensic evidence record for hardware dispatch attempts and outcomes.
 * Guarantees traceability of target, plan, message digests, timestamps, and fail-closed state.
 */
struct HardwareDispatchEvidenceRecord
{
    std::string targetProfileId;
    std::string targetContractId;
    std::string targetResolutionSource;
    std::string benchSessionId;
    std::string recipeDocumentHash;
    std::string resolvedExecutionPlanHash;

    MidiPortSelection portSelection;
    HardwareIdentityState identityState { HardwareIdentityState::IdentityUnavailable };

    uint64_t sequenceNumber { 0 };
    uint64_t monotonicTimestampMs { 0 };

    std::string semanticId;
    std::string nativeParameterId;

    std::string commandDigest;
    std::string messageDigest;

    MidiTransportError transportError { MidiTransportError::None };
    MidiResponseOutcome responseOutcome { MidiResponseOutcome::Unsupported };

    std::string diagnosticCode;
    std::string diagnosticMessage;

    bool consentWasGranted { false };
    bool exportBlocked { false };
};

} // namespace abdaudiolab::hardware
