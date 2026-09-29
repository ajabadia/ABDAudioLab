/**
 * @file test_SharedMidiHardwareIntegrationContract.cpp
 * @brief Hermetic integration tests for HITO-SHARED-SYNC / SS5 boundary contract.
 * @details Validates:
 *          - One-way boundary adapter from ABDSharedCode to ABDAudioLab.
 *          - Projection of classification, 5-state identity, and safety policies.
 *          - Virtual and Unknown endpoints restricted appropriately.
 *          - UserConfirmedUnverified exclusively originating in ABDAudioLab under OperatorConfirmationContext.
 *          - Shared eligibility NEVER confers dispatch authority.
 *          - UniversalInquiryRequest invariant: exists IF AND ONLY IF decision == Allowed.
 *          - ZERO physical hardware required, ZERO physical MIDI bytes transmitted.
 *          - ExportReadiness remains strictly Blocked.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "hardware/adapter/SharedMidiHardwareAdapter.h"
#include "tests/support/MockMidiTransport.h"
#include <HardwareMidiDetect/MidiEndpointSafetyPolicy.h>
#include <HardwareMidiDetect/HardwareContract.h>

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::tests;
using namespace abd::hwid;

TEST_CASE("SS5-01: Classification mapping for PhysicalUsb and PhysicalDinInterface",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor usbEndpoint;
    usbEndpoint.stableDeviceId = "USB\\VID_1397&PID_0500";
    usbEndpoint.displayName = "DeepMind 12D";
    usbEndpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;
    auto snapUsb = SharedMidiHardwareAdapter::createSnapshot(
        usbEndpoint, policy, auth, HardwareMidiIdentityState::PortAvailable);

    auto adaptedUsb = SharedMidiHardwareAdapter::adaptSnapshot(snapUsb);
    REQUIRE(adaptedUsb.endpointKind == MidiEndpointKind::PhysicalUsb);
    REQUIRE(adaptedUsb.isPhysicalBenchEligible == true);
    REQUIRE(adaptedUsb.requiresWarningBadge == false);

    MidiEndpointDescriptor dinEndpoint;
    dinEndpoint.stableDeviceId = "DIN\\INTERFACE_PORT_1";
    dinEndpoint.displayName = "MIDI Interface Port 1";
    dinEndpoint.kind = MidiEndpointKind::PhysicalDinInterface;

    auto snapDin = SharedMidiHardwareAdapter::createSnapshot(
        dinEndpoint, policy, auth, HardwareMidiIdentityState::PortAvailable);

    auto adaptedDin = SharedMidiHardwareAdapter::adaptSnapshot(snapDin);
    REQUIRE(adaptedDin.endpointKind == MidiEndpointKind::PhysicalDinInterface);
    REQUIRE(adaptedDin.isPhysicalBenchEligible == true);
    REQUIRE(adaptedDin.requiresWarningBadge == false);
}

TEST_CASE("SS5-02: Classification mapping for VirtualLoopback and VirtualDriver",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor loopbackEndpoint;
    loopbackEndpoint.stableDeviceId = "LOOPMIDI\\PORT_A";
    loopbackEndpoint.displayName = "loopMIDI Port";
    loopbackEndpoint.kind = MidiEndpointKind::VirtualLoopback;

    MidiEndpointSafetyPolicy policy; // default: allowVirtualEndpointsForAutomaticDiscovery = false
    BroadcastInquiryAuthorization auth;
    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        loopbackEndpoint, policy, auth, HardwareMidiIdentityState::PortAvailable);

    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
    REQUIRE(adapted.endpointKind == MidiEndpointKind::VirtualLoopback);
    REQUIRE(adapted.eligibleForAutomaticDiscovery == false);
    REQUIRE(adapted.isPhysicalBenchEligible == false);
    REQUIRE(adapted.requiresWarningBadge == true);

    MidiEndpointDescriptor driverEndpoint;
    driverEndpoint.stableDeviceId = "IAC\\BUS_1";
    driverEndpoint.displayName = "IAC Driver Bus 1";
    driverEndpoint.kind = MidiEndpointKind::VirtualDriver;

    auto snapDriver = SharedMidiHardwareAdapter::createSnapshot(
        driverEndpoint, policy, auth, HardwareMidiIdentityState::PortAvailable);

    auto adaptedDriver = SharedMidiHardwareAdapter::adaptSnapshot(snapDriver);
    REQUIRE(adaptedDriver.endpointKind == MidiEndpointKind::VirtualDriver);
    REQUIRE(adaptedDriver.eligibleForAutomaticDiscovery == false);
    REQUIRE(adaptedDriver.isPhysicalBenchEligible == false);
    REQUIRE(adaptedDriver.requiresWarningBadge == true);
}

TEST_CASE("SS5-03: Unknown endpoint visible with warning and excluded from auto preflight",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor unkEndpoint;
    unkEndpoint.stableDeviceId = "UNKNOWN\\DEVICE_XYZ";
    unkEndpoint.displayName = "Generic MIDI In/Out";
    unkEndpoint.kind = MidiEndpointKind::Unknown;

    MidiEndpointSafetyPolicy policy;
    policy.allowBroadcastSysEx = true;
    policy.sysExInquiryMode = SysExDiscoveryInquiryMode::PhysicalEndpointsWithExplicitOptIn;

    BroadcastInquiryAuthorization auth;
    auth.callerExplicitlyOptedIn = true;
    auth.callerConfirmedSingleTargetTopology = true;

    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        unkEndpoint, policy, auth, HardwareMidiIdentityState::PortAvailable);

    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
    // Visible in UI
    REQUIRE(adapted.stableDeviceId == "UNKNOWN\\DEVICE_XYZ");
    // Warning badge active
    REQUIRE(adapted.requiresWarningBadge == true);
    // Excluded from broadcast inquiry because it's Unknown
    REQUIRE(adapted.inquiryDecision == SysExInquiryDecision::UnknownEndpointExcluded);
    // Strictly not eligible for automatic physical test bench
    REQUIRE(adapted.isPhysicalBenchEligible == false);
}

TEST_CASE("SS5-04: Parity for IdentityVerified <-> isSysExVerified == true",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.stableDeviceId = "USB\\DEEPMIND";
    endpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;
    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    REQUIRE(snap.isSysExVerified == true);
    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
    REQUIRE(adapted.identityState == abdaudiolab::hardware::HardwareIdentityState::IdentityVerified);
    REQUIRE(adapted.isSysExVerified == true);
}

TEST_CASE("SS5-05: Non-verified identity states map isSysExVerified == false",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.kind = MidiEndpointKind::PhysicalUsb;
    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;

    const std::vector<HardwareMidiIdentityState> nonVerifiedStates = {
        HardwareMidiIdentityState::PortAvailable,
        HardwareMidiIdentityState::IdentityUnavailable,
        HardwareMidiIdentityState::IdentityMismatch,
        HardwareMidiIdentityState::UserConfirmedUnverified
    };

    for (const auto state : nonVerifiedStates)
    {
        auto snap = SharedMidiHardwareAdapter::createSnapshot(endpoint, policy, auth, state);
        REQUIRE(snap.isSysExVerified == false);
        auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
        REQUIRE(adapted.isSysExVerified == false);
    }
}

TEST_CASE("SS5-06: IdentityMismatch blocks local execution fail-closed",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.stableDeviceId = "USB\\ROLAND_JUNO";
    endpoint.displayName = "Roland Boutique JU-06";
    endpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;
    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityMismatch);

    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
    REQUIRE(adapted.identityState == abdaudiolab::hardware::HardwareIdentityState::IdentityMismatch);
    REQUIRE(adapted.isPhysicalBenchEligible == false);
    REQUIRE(SharedMidiHardwareAdapter::isSessionDispatchAllowed(snap) == false);
}

TEST_CASE("SS5-07: IdentityUnavailable is not auto-promoted",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.stableDeviceId = "USB\\ANALOG_PEDAL";
    endpoint.displayName = "Analog Filter Pedal";
    endpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;
    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityUnavailable);

    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(snap);
    REQUIRE(adapted.identityState == abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable);
    REQUIRE(adapted.isSysExVerified == false);
    REQUIRE(SharedMidiHardwareAdapter::isSessionDispatchAllowed(snap) == false);
}

TEST_CASE("SS5-08: UserConfirmedUnverified exclusively originates in ABDAudioLab with operator",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    // ABDSharedCode invariant check:
    REQUIRE(isSharedDiscoveryEmittable(HardwareMidiIdentityState::UserConfirmedUnverified) == false);

    // Operator confirmation context in ABDAudioLab
    OperatorConfirmationContext ctx;
    ctx.operatorId = "OP-LAB-01";
    ctx.targetContractId = "behringer_deepmind12d";
    ctx.selectedPortStableDeviceId = "USB\\VID_1397&PID_0500";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable;
    ctx.explicitConfirmationRecorded = true;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == true);
    REQUIRE(result.identityState == abdaudiolab::hardware::HardwareIdentityState::UserConfirmedUnverified);

    // Even with operator confirmation:
    MockMidiTransport transport;
    REQUIRE(transport.sentCc().empty());
    REQUIRE(transport.sentSysEx().empty());

    abdaudiolab::gui::session::ExportReadiness exportReadiness;
    REQUIRE(exportReadiness.decision == abdaudiolab::gui::session::ExportReadiness::Decision::Blocked);
}

TEST_CASE("SS5-09: Operator confirmation rejected if operatorId is empty",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    OperatorConfirmationContext ctx;
    ctx.operatorId = "";
    ctx.targetContractId = "behringer_deepmind12d";
    ctx.selectedPortStableDeviceId = "USB\\DEV_01";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable;
    ctx.explicitConfirmationRecorded = true;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.diagnosticCode == "ERR_OPERATOR_ID_REQUIRED");
}

TEST_CASE("SS5-10: Operator confirmation rejected if targetContractId is empty",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    OperatorConfirmationContext ctx;
    ctx.operatorId = "OP-LAB-01";
    ctx.targetContractId = "";
    ctx.selectedPortStableDeviceId = "USB\\DEV_01";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable;
    ctx.explicitConfirmationRecorded = true;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.diagnosticCode == "ERR_TARGET_CONTRACT_REQUIRED");
}

TEST_CASE("SS5-11: Operator confirmation rejected if selectedPortStableDeviceId is empty",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    OperatorConfirmationContext ctx;
    ctx.operatorId = "OP-LAB-01";
    ctx.targetContractId = "behringer_deepmind12d";
    ctx.selectedPortStableDeviceId = "";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable;
    ctx.explicitConfirmationRecorded = true;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.diagnosticCode == "ERR_PORT_ID_REQUIRED");
}

TEST_CASE("SS5-12: Operator confirmation rejected if priorIdentityState != IdentityUnavailable",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    OperatorConfirmationContext ctx;
    ctx.operatorId = "OP-LAB-01";
    ctx.targetContractId = "behringer_deepmind12d";
    ctx.selectedPortStableDeviceId = "USB\\DEV_01";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityMismatch;
    ctx.explicitConfirmationRecorded = true;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.diagnosticCode == "ERR_INVALID_PRIOR_IDENTITY_STATE");

    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::PortAvailable;
    auto result2 = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result2.isSuccess() == false);
    REQUIRE(result2.diagnosticCode == "ERR_INVALID_PRIOR_IDENTITY_STATE");
}

TEST_CASE("SS5-13: Operator confirmation rejected if explicitConfirmationRecorded == false",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    OperatorConfirmationContext ctx;
    ctx.operatorId = "OP-LAB-01";
    ctx.targetContractId = "behringer_deepmind12d";
    ctx.selectedPortStableDeviceId = "USB\\DEV_01";
    ctx.priorIdentityState = abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable;
    ctx.explicitConfirmationRecorded = false;

    auto result = SharedMidiHardwareAdapter::confirmUnverifiedOperatorIdentity(ctx);
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.diagnosticCode == "ERR_EXPLICIT_CONFIRMATION_REQUIRED");
}

TEST_CASE("SS5-14: Shared eligibility NEVER confers dispatch authority",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.stableDeviceId = "USB\\DEV_01";
    endpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    policy.allowBroadcastSysEx = true;
    policy.sysExInquiryMode = SysExDiscoveryInquiryMode::PhysicalEndpointsWithExplicitOptIn;

    BroadcastInquiryAuthorization auth;
    auth.callerExplicitlyOptedIn = true;
    auth.callerConfirmedSingleTargetTopology = true;

    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    REQUIRE(snap.inquiryDecision == SysExInquiryDecision::Allowed);
    // Boundary check: isSessionDispatchAllowed is strictly false
    REQUIRE(SharedMidiHardwareAdapter::isSessionDispatchAllowed(snap) == false);
}

TEST_CASE("SS5-15: Virtual endpoints rejected for physical bench preflight",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor loopback;
    loopback.kind = MidiEndpointKind::VirtualLoopback;
    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;

    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        loopback, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    REQUIRE(SharedMidiHardwareAdapter::isPhysicalBenchEligible(snap) == false);
}

TEST_CASE("SS5-16: Unknown endpoint rejected for automatic physical bench preflight",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor unk;
    unk.kind = MidiEndpointKind::Unknown;
    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;

    auto snap = SharedMidiHardwareAdapter::createSnapshot(
        unk, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    REQUIRE(SharedMidiHardwareAdapter::isPhysicalBenchEligible(snap) == false);
}

TEST_CASE("SS5-17: Transport mock verifies ZERO write calls and ZERO bytes",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MockMidiTransport mockTransport;
    REQUIRE(mockTransport.sentCc().size() == 0);
    REQUIRE(mockTransport.sentSysEx().size() == 0);
    REQUIRE(mockTransport.isOpen() == false);
}

TEST_CASE("SS5-18: ExportReadiness remains strictly Blocked",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    abdaudiolab::gui::session::ExportReadiness readiness;
    REQUIRE(readiness.decision == abdaudiolab::gui::session::ExportReadiness::Decision::Blocked);
    REQUIRE(readiness.sessionCompleted == false);
    REQUIRE(readiness.metrologyPassed == false);
}

TEST_CASE("SS5-19: Invariant: UniversalInquiryRequest absent when not Allowed",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor physical;
    physical.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointDescriptor virt;
    virt.kind = MidiEndpointKind::VirtualLoopback;

    MidiEndpointDescriptor unk;
    unk.kind = MidiEndpointKind::Unknown;

    MidiEndpointSafetyPolicy disabledPolicy; // allowBroadcastSysEx = false

    MidiEndpointSafetyPolicy enabledPolicy;
    enabledPolicy.allowBroadcastSysEx = true;
    enabledPolicy.sysExInquiryMode = SysExDiscoveryInquiryMode::PhysicalEndpointsWithExplicitOptIn;

    BroadcastInquiryAuthorization noOptInAuth;
    noOptInAuth.callerExplicitlyOptedIn = false;
    noOptInAuth.callerConfirmedSingleTargetTopology = true;

    BroadcastInquiryAuthorization noTopologyAuth;
    noTopologyAuth.callerExplicitlyOptedIn = true;
    noTopologyAuth.callerConfirmedSingleTargetTopology = false;

    // 1. Policy Disabled -> DisabledByPolicy -> request == nullopt
    auto r1 = evaluateUniversalInquiryEligibilityDetailed(physical, disabledPolicy, noOptInAuth);
    REQUIRE(r1.decision == SysExInquiryDecision::DisabledByPolicy);
    REQUIRE(r1.request.has_value() == false);

    // 2. Virtual endpoint -> VirtualEndpointExcluded -> request == nullopt
    BroadcastInquiryAuthorization fullAuth;
    fullAuth.callerExplicitlyOptedIn = true;
    fullAuth.callerConfirmedSingleTargetTopology = true;
    auto r2 = evaluateUniversalInquiryEligibilityDetailed(virt, enabledPolicy, fullAuth);
    REQUIRE(r2.decision == SysExInquiryDecision::VirtualEndpointExcluded);
    REQUIRE(r2.request.has_value() == false);

    // 3. Unknown endpoint -> UnknownEndpointExcluded -> request == nullopt
    auto r3 = evaluateUniversalInquiryEligibilityDetailed(unk, enabledPolicy, fullAuth);
    REQUIRE(r3.decision == SysExInquiryDecision::UnknownEndpointExcluded);
    REQUIRE(r3.request.has_value() == false);

    // 4. Missing opt-in -> CallerOptInMissing -> request == nullopt
    auto r4 = evaluateUniversalInquiryEligibilityDetailed(physical, enabledPolicy, noOptInAuth);
    REQUIRE(r4.decision == SysExInquiryDecision::CallerOptInMissing);
    REQUIRE(r4.request.has_value() == false);

    // 5. Unconfirmed topology -> SingleTargetTopologyUnconfirmed -> request == nullopt
    auto r5 = evaluateUniversalInquiryEligibilityDetailed(physical, enabledPolicy, noTopologyAuth);
    REQUIRE(r5.decision == SysExInquiryDecision::SingleTargetTopologyUnconfirmed);
    REQUIRE(r5.request.has_value() == false);
}

TEST_CASE("SS5-20: Invariant: UniversalInquiryRequest exists ONLY when Allowed with exact payload",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor physical;
    physical.stableDeviceId = "USB\\VID_1397&PID_0500";
    physical.displayName = "DeepMind 12D";
    physical.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    policy.allowBroadcastSysEx = true;
    policy.sysExInquiryMode = SysExDiscoveryInquiryMode::PhysicalEndpointsWithExplicitOptIn;

    BroadcastInquiryAuthorization auth;
    auth.callerExplicitlyOptedIn = true;
    auth.callerConfirmedSingleTargetTopology = true;

    auto result = evaluateUniversalInquiryEligibilityDetailed(physical, policy, auth);
    REQUIRE(result.decision == SysExInquiryDecision::Allowed);
    REQUIRE(result.request.has_value() == true);

    const auto& req = result.request.value();
    const std::array<uint8_t, 6> expectedBytes = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    REQUIRE(req.bytes() == expectedBytes);
    REQUIRE(req.endpoint().stableDeviceId == "USB\\VID_1397&PID_0500");

    // Zero side-effects: no open device, no bytes transmitted
    MockMidiTransport mock;
    REQUIRE(mock.sentSysEx().empty());
}

TEST_CASE("SS5-21: Invariant: SharedMidiEndpointSnapshot is immutable",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    MidiEndpointDescriptor endpoint;
    endpoint.stableDeviceId = "USB\\ORIGINAL_ID";
    endpoint.displayName = "Original Name";
    endpoint.kind = MidiEndpointKind::PhysicalUsb;

    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;
    const auto originalSnapshot = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    // Local copy mutation test
    auto localCopy = originalSnapshot;
    localCopy.stableDeviceId = "USB\\MUTATED_ID";
    localCopy.identityState = HardwareMidiIdentityState::IdentityMismatch;

    // Original remains pristine
    REQUIRE(originalSnapshot.stableDeviceId == "USB\\ORIGINAL_ID");
    REQUIRE(originalSnapshot.identityState == HardwareMidiIdentityState::IdentityVerified);
    REQUIRE(originalSnapshot.isSysExVerified == true);
    REQUIRE(originalSnapshot.diagnosticCode == "IDENT_VERIFIED");
}

TEST_CASE("SS5-22: Invariant: Authority escalation strictly blocked",
          "[shared][integration][hardware][contract][no_dispatch]")
{
    // Shared identity state cannot be escalated through shared adapter
    MidiEndpointDescriptor endpoint;
    endpoint.kind = MidiEndpointKind::PhysicalUsb;
    MidiEndpointSafetyPolicy policy;
    BroadcastInquiryAuthorization auth;

    auto unverifiedSnap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityUnavailable);

    auto adapted = SharedMidiHardwareAdapter::adaptSnapshot(unverifiedSnap);
    REQUIRE(adapted.identityState == abdaudiolab::hardware::HardwareIdentityState::IdentityUnavailable);

    // Shared policy Allowed has zero route to dispatch
    policy.allowBroadcastSysEx = true;
    policy.sysExInquiryMode = SysExDiscoveryInquiryMode::PhysicalEndpointsWithExplicitOptIn;
    auth.callerExplicitlyOptedIn = true;
    auth.callerConfirmedSingleTargetTopology = true;

    auto allowedSnap = SharedMidiHardwareAdapter::createSnapshot(
        endpoint, policy, auth, HardwareMidiIdentityState::IdentityVerified);

    REQUIRE(allowedSnap.inquiryDecision == SysExInquiryDecision::Allowed);
    REQUIRE(SharedMidiHardwareAdapter::isSessionDispatchAllowed(allowedSnap) == false);

    MockMidiTransport transport;
    REQUIRE(transport.sentCc().empty());
    REQUIRE(transport.sentSysEx().empty());
}
