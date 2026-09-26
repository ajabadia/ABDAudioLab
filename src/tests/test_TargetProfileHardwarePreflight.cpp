#include <catch2/catch_test_macros.hpp>
#include "hardware/preflight/HardwareTransportPreflightService.h"
#include "hardware/preflight/IMidiIdentityProbe.h"
#include "tests/support/MockMidiTransport.h"
#include "tests/support/MockMidiIdentityProbe.h"
#include "profiling/TargetProfile.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::tests;

namespace {

TargetProfile makeTestProfile(std::string profileId, bool requiresVerified, bool allowUnverified)
{
    TargetProfile profile;
    profile.targetProfileId = std::move(profileId);
    profile.vendor = "Yamaha";
    profile.displayName = "DX7";
    profile.hasExplicitTransportPolicy = true;
    profile.transportPolicy.requiresVerifiedIdentity = requiresVerified;
    profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity = allowUnverified;
    profile.transportPolicy.minimumInterMessageDelayMs = 20;
    return profile;
}

MidiPortSelection makeTestPort(std::string deviceId = "midi_out_port_1", 
                              std::string displayName = "USB MIDI 1x1 Out",
                              std::string expectedProfileId = "yamaha_dx7")
{
    return MidiPortSelection {
        std::move(deviceId),
        std::move(displayName),
        std::move(expectedProfileId)
    };
}

} // namespace

TEST_CASE("HardwarePreflight - Port Opening Failures", "[hardware][preflight]")
{
    SECTION("Fails closed when transport fails to open")
    {
        MockMidiTransport transport;
        transport.failNextOpen(MidiTransportError::OpenFailed, "ERR_MIDI_OUTPUT_OPEN_FAILED");

        const auto profile = makeTestProfile("yamaha_dx7", false, true);
        const auto port = makeTestPort();

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, nullptr);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.diagnosticCode == "ERR_MIDI_OUTPUT_OPEN_FAILED");
        REQUIRE_FALSE(transport.isOpen());
    }

    SECTION("Fails closed when port selection is invalid")
    {
        MockMidiTransport transport;
        transport.failNextOpen(MidiTransportError::InvalidPortSelection, "ERR_INVALID_PORT_SELECTION");

        const auto profile = makeTestProfile("yamaha_dx7", false, true);
        const auto port = makeTestPort("", "", "");

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, nullptr);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.diagnosticCode == "ERR_INVALID_PORT_SELECTION");
    }
}

TEST_CASE("HardwarePreflight - Identity Probe Not Available (Unprobed Target)", "[hardware][preflight]")
{
    MockMidiTransport transport;
    const auto port = makeTestPort();

    SECTION("Unprobed target with allowUserConfirmedUnverified = true yields AwaitingUserConfirmation")
    {
        const auto profile = makeTestProfile("boss_ds1", false, true);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, nullptr);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.diagnosticCode == "AWAITING_USER_CONFIRMATION");
        REQUIRE(transport.isOpen());
    }

    SECTION("Unprobed target with requiresVerifiedIdentity = true is Blocked")
    {
        const auto profile = makeTestProfile("critical_synth", true, false);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, nullptr);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.diagnosticCode == "ERR_VERIFIED_IDENTITY_REQUIRED");
    }

    SECTION("Unprobed target with allowUserConfirmedUnverified = false is Blocked")
    {
        const auto profile = makeTestProfile("strict_synth", false, false);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, nullptr);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.diagnosticCode == "ERR_UNVERIFIED_IDENTITY_NOT_ALLOWED");
    }
}

TEST_CASE("HardwarePreflight - Active Identity Probe Verification", "[hardware][preflight]")
{
    MockMidiTransport transport;
    MockMidiIdentityProbe probe;
    const auto port = makeTestPort();
    const auto profile = makeTestProfile("yamaha_dx7", true, false);

    SECTION("Successful verification yields Ready and IdentityVerified")
    {
        probe.setNextOutcome(IdentityProbeOutcome::Verified, "43", "DX7", "1.0");

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Ready);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityVerified);
        REQUIRE(result.diagnosticCode == "OK");
        REQUIRE(probe.probeCallCount() == 1);
        REQUIRE(probe.lastProbedProfileId() == "yamaha_dx7");
    }

    SECTION("Identity mismatch yields Blocked and IdentityMismatch")
    {
        probe.setNextOutcome(IdentityProbeOutcome::Mismatch, "41", "D-50", "2.1");

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityMismatch);
        REQUIRE(result.diagnosticCode == "ERR_IDENTITY_MISMATCH");
    }

    SECTION("Probe transport failure yields Blocked and ERR_IDENTITY_PROBE_TRANSPORT_FAILED")
    {
        probe.setNextOutcome(IdentityProbeOutcome::TransportFailure);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.diagnosticCode == "ERR_IDENTITY_PROBE_TRANSPORT_FAILED");
    }
}

TEST_CASE("HardwarePreflight - Probe Timeout and Fallbacks", "[hardware][preflight]")
{
    MockMidiTransport transport;
    MockMidiIdentityProbe probe;
    const auto port = makeTestPort();

    SECTION("Probe timeout with requiresVerifiedIdentity = true is Blocked")
    {
        const auto profile = makeTestProfile("yamaha_dx7", true, false);
        probe.setNextOutcome(IdentityProbeOutcome::Timeout);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::Blocked);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.diagnosticCode == "ERR_VERIFIED_IDENTITY_REQUIRED");
    }

    SECTION("Probe timeout with allowUserConfirmedUnverified = true yields AwaitingUserConfirmation")
    {
        const auto profile = makeTestProfile("yamaha_dx7", false, true);
        probe.setNextOutcome(IdentityProbeOutcome::Timeout);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE_FALSE(result.readyForConsentOrDispatch);
        REQUIRE(result.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.diagnosticCode == "AWAITING_USER_CONFIRMATION");
    }
}

TEST_CASE("HardwarePreflight - Boundary Invariants: No Auto-Confirmation and Zero Traffic", "[hardware][preflight]")
{
    MockMidiTransport transport;
    MockMidiIdentityProbe probe;
    const auto port = makeTestPort();

    SECTION("D2.3 never emits UserConfirmedUnverified state directly")
    {
        // Even when unverified operation is allowed, preflight cannot produce UserConfirmedUnverified.
        // That transition is strictly governed by D2.4 OperatorConsentService.
        const auto profile = makeTestProfile("analog_synth", false, true);
        probe.setNextOutcome(IdentityProbeOutcome::Unavailable);

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE(result.identityState != HardwareIdentityState::UserConfirmedUnverified);
        REQUIRE(result.identityState == HardwareIdentityState::IdentityUnavailable);
        REQUIRE(result.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);
        REQUIRE_FALSE(result.readyForConsentOrDispatch);
    }

    SECTION("D2.3 preflight evaluation emits ZERO measurement CC or SysEx messages to transport")
    {
        const auto profile = makeTestProfile("yamaha_dx7", true, false);
        probe.setNextOutcome(IdentityProbeOutcome::Verified, "43", "DX7", "1.0");

        const auto result = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

        REQUIRE(result.readyForConsentOrDispatch);
        // Transport output queues must remain completely empty during preflight
        REQUIRE(transport.sentCc().empty());
        REQUIRE(transport.sentSysEx().empty());
    }
}
