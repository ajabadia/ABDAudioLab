#include <catch2/catch_test_macros.hpp>
#include "hardware/transport/HardwareDispatchScheduler.h"
#include "hardware/consent/OperatorConsentService.h"
#include "tests/support/MockMidiTransport.h"
#include "tests/support/MockMonotonicClock.h"
#include "tests/support/MockMidiResponseAwaiter.h"
#include "profiling/TargetProfile.h"
#include "gui/session/ProfilingSessionContracts.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::tests;
using namespace abdaudiolab::gui::session;

namespace {

TargetProfile makeTestProfile(std::string profileId, bool requiresAck = false, int timeoutMs = 200)
{
    TargetProfile p;
    p.targetProfileId = std::move(profileId);
    p.displayName = "Test Device";
    p.vendor = "Acme Synths";
    p.targetKind = "HardwareDigital";
    p.hasExplicitTransportPolicy = true;
    p.transportPolicy.minimumInterMessageDelayMs = 0;
    p.transportPolicy.maximumMessagesPerSecond = 0;
    p.transportPolicy.requiresResponseAck = requiresAck;
    p.transportPolicy.responseTimeoutMs = timeoutMs;
    return p;
}

MidiPortSelection makeTestPort(std::string devId = "port_test_1")
{
    return MidiPortSelection {
        std::move(devId),
        "Mock USB MIDI Interface",
        "hw-target-canonical"
    };
}

HardwarePreflightResult makePreflight(HardwareIdentityState state = HardwareIdentityState::IdentityVerified)
{
    HardwarePreflightResult r;
    r.targetProfileId = "hw-target-canonical";
    r.identityState = state;
    r.disposition = (state == HardwareIdentityState::IdentityMismatch) 
        ? HardwarePreflightDisposition::Blocked 
        : HardwarePreflightDisposition::Ready;
    r.portSelection = makeTestPort();
    r.readyForConsentOrDispatch = (state != HardwareIdentityState::IdentityMismatch);
    return r;
}

MidiCcMessage makeCc(uint8_t ctrl = 19, uint8_t val = 64)
{
    MidiCcMessage msg;
    msg.channel = 1;
    msg.controllerNumber = ctrl;
    msg.value = val;
    msg.targetProfileId = "hw-target-canonical";
    msg.semanticId = "filter_cutoff";
    msg.sequenceNumber = 1;
    return msg;
}

ProfilingSessionSnapshot createSnapshotFromEvidence(const HardwareDispatchEvidenceRecord& evidence)
{
    ProfilingSessionSnapshot snap;
    snap.sessionId = "session_hw_dispatch_recovery";
    snap.sessionStatus = evidence.exportBlocked ? ProfilingSessionStatus::Failed : ProfilingSessionStatus::Completed;
    snap.workflowStage = ProfilingWorkflowStage::ReviewResults;
    snap.target.targetId = evidence.targetProfileId;
    snap.target.targetName = evidence.targetProfileId;
    snap.exportOptions.canExportCpp = !evidence.exportBlocked;
    snap.exportOptions.exportBlockReason = evidence.exportBlocked ? evidence.diagnosticMessage : "";
    snap.evaluation.hasEvaluation = true;
    snap.evaluation.selectionStatus = evidence.exportBlocked 
        ? abdaudiolab::synth::SelectionStatus::InvalidMeasurement 
        : abdaudiolab::synth::SelectionStatus::Accepted;
    snap.evaluation.sourceTargetIdentity = evidence.targetProfileId;
    snap.evaluation.hashVerified = true;
    return snap;
}

} // namespace

TEST_CASE("D2.6: Transport Failure Recovery - WriteFailed Fail-Closed and Evidence Preservation", "[hardware][recovery]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makeTestPort();
    transport.open(port);

    const auto profile = makeTestProfile("hw-target-canonical");
    const auto preflight = makePreflight();
    const std::string planHash = "plan_hash_reliable";
    const auto cc = makeCc(19, 64);

    const auto req = OperatorConsentService::buildRequest(
        profile, ProfileProvenanceOrigin::BuiltIn, preflight,
        "recipe_alpha", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
    const auto consent = OperatorConsentService::grantConsent(req);

    SECTION("MockMidiTransport::WriteFailed triggers fail-closed, complete evidence and blocks export")
    {
        transport.failNextSend(MidiTransportError::WriteFailed, "ERR_MIDI_TRANSPORT_WRITE_FAILED");

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        // 1. Evidence captures complete forensic record
        REQUIRE(evidence.exportBlocked);
        REQUIRE(evidence.transportError == MidiTransportError::WriteFailed);
        REQUIRE(evidence.diagnosticCode == "ERR_MIDI_TRANSPORT_WRITE_FAILED");
        REQUIRE(evidence.targetProfileId == "hw-target-canonical");
        REQUIRE(evidence.recipeDocumentHash == "recipe_alpha");
        REQUIRE(evidence.resolvedExecutionPlanHash == planHash);
        REQUIRE(evidence.portSelection.stableDeviceId == port.stableDeviceId);
        REQUIRE(evidence.commandDigest == req.commandDigest);
        REQUIRE_FALSE(evidence.messageDigest.empty());
        REQUIRE(evidence.sequenceNumber > 0);
        REQUIRE(evidence.monotonicTimestampMs == 1000);

        // 2. Export readiness strictly blocked
        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Blocked);
        REQUIRE_FALSE(readiness.canProceed());
    }

    SECTION("MockMidiTransport::Disconnected triggers immediate abort and blocks next message")
    {
        transport.simulateDisconnect();

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE(evidence.transportError == MidiTransportError::Disconnected);
        REQUIRE(evidence.diagnosticCode == "ERR_MIDI_TRANSPORT_DISCONNECTED");
        REQUIRE(transport.sentCc().empty());

        // Subsequent message is also blocked because transport remains disconnected
        const auto secondEvidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(secondEvidence.exportBlocked);
        REQUIRE(secondEvidence.transportError == MidiTransportError::Disconnected);
        REQUIRE(transport.sentCc().empty());

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Blocked);
    }
}

TEST_CASE("D2.6: Transport Failure Recovery - ACK Response Awaiter Policy", "[hardware][recovery]")
{
    MockMonotonicClock clock(2000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makeTestPort();
    transport.open(port);

    const auto preflight = makePreflight();
    const std::string planHash = "plan_ack_verify";
    const auto cc = makeCc(19, 64);
    MockMidiResponseAwaiter awaiter;

    SECTION("Timeout with requiresResponseAck == true -> ERR_MIDI_TARGET_NO_RESPONSE and fail-closed")
    {
        const auto profileAckRequired = makeTestProfile("hw-target-canonical", true, 150);
        const auto req = OperatorConsentService::buildRequest(
            profileAckRequired, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_ack", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        awaiter.setNextResponse(MidiResponseOutcome::Timeout, {}, "TIMEOUT", "Target did not reply");

        const auto evidence = scheduler.executeDispatchCc(
            transport, &awaiter, req, consent, planHash, port, cc, profileAckRequired.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE(evidence.responseOutcome == MidiResponseOutcome::Timeout);
        REQUIRE(evidence.diagnosticCode == "ERR_MIDI_TARGET_NO_RESPONSE");
        REQUIRE(awaiter.callCount() == 1);
        REQUIRE(awaiter.lastTimeoutMs() == 150);

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Blocked);
    }

    SECTION("Timeout with requiresResponseAck == false -> NOT an error, export unblocked")
    {
        const auto profileNoAck = makeTestProfile("hw-target-canonical", false, 0);
        const auto req = OperatorConsentService::buildRequest(
            profileNoAck, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_noack", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        // Awaiter not invoked or timeout ignored when requiresResponseAck is false
        const auto evidence = scheduler.executeDispatchCc(
            transport, &awaiter, req, consent, planHash, port, cc, profileNoAck.transportPolicy);

        REQUIRE_FALSE(evidence.exportBlocked);
        REQUIRE(evidence.diagnosticCode == "OK");
        REQUIRE(awaiter.callCount() == 0); // No waiting required for non-ACK hardware

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Ready);
        REQUIRE(readiness.canProceed());
    }

    SECTION("Successful ACK with requiresResponseAck == true -> OK, export unblocked")
    {
        const auto profileAckRequired = makeTestProfile("hw-target-canonical", true, 150);
        const auto req = OperatorConsentService::buildRequest(
            profileAckRequired, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_ack", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        awaiter.setNextResponse(MidiResponseOutcome::Received, { 0x06 }, "OK", "ACK frame received");

        const auto evidence = scheduler.executeDispatchCc(
            transport, &awaiter, req, consent, planHash, port, cc, profileAckRequired.transportPolicy);

        REQUIRE_FALSE(evidence.exportBlocked);
        REQUIRE(evidence.responseOutcome == MidiResponseOutcome::Received);
        REQUIRE(evidence.diagnosticCode == "OK");
        REQUIRE(awaiter.callCount() == 1);

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Ready);
    }
}

TEST_CASE("D2.6: Transport Failure Recovery - Preflight Identity Mismatch and TOCTOU Invalidation", "[hardware][recovery]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makeTestPort();
    transport.open(port);

    const auto profile = makeTestProfile("hw-target-canonical");
    const std::string planHash = "plan_safe";
    const auto cc = makeCc(19, 64);

    SECTION("IdentityMismatch causes zero send attempts and blocks export")
    {
        const auto preflightMismatch = makePreflight(HardwareIdentityState::IdentityMismatch);
        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflightMismatch,
            "recipe_x", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE(evidence.diagnosticCode == "ERR_IDENTITY_MISMATCH_BLOCKED");
        REQUIRE(transport.sentCc().empty());
        REQUIRE(transport.sentSysEx().empty());

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Blocked);
    }

    SECTION("Consent TOCTOU invalidation causes zero send attempts and blocks export")
    {
        const auto preflight = makePreflight(HardwareIdentityState::IdentityVerified);
        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_x", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        // Tamper with plan hash right before dispatch
        const std::string tamperedPlanHash = "plan_tampered_attack";

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, tamperedPlanHash, port, cc, profile.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE_FALSE(evidence.consentWasGranted);
        REQUIRE(evidence.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
        REQUIRE(transport.sentCc().empty());
        REQUIRE(transport.sentSysEx().empty());

        const auto snap = createSnapshotFromEvidence(evidence);
        const auto readiness = evaluateExportReadinessFromSnapshot(snap);
        REQUIRE(readiness.decision == ExportReadiness::Decision::Blocked);
    }
}
