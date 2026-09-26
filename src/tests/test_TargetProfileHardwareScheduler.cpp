#include <catch2/catch_test_macros.hpp>
#include "hardware/transport/HardwareDispatchScheduler.h"
#include "hardware/consent/OperatorConsentService.h"
#include "tests/support/MockMidiTransport.h"
#include "tests/support/MockMonotonicClock.h"
#include "profiling/TargetProfile.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::tests;

namespace {

TargetProfile makeTestProfile(std::string profileId, int delayMs = 20, int rateLimit = 0)
{
    TargetProfile profile;
    profile.targetProfileId = std::move(profileId);
    profile.vendor = "Yamaha";
    profile.displayName = "DX7";
    profile.hasExplicitTransportPolicy = true;
    profile.transportPolicy.minimumInterMessageDelayMs = delayMs;
    profile.transportPolicy.maximumMessagesPerSecond = rateLimit;
    return profile;
}

MidiPortSelection makeTestPort(std::string deviceId = "midi_out_1")
{
    return MidiPortSelection {
        std::move(deviceId),
        "USB MIDI 1x1 Out",
        "yamaha_dx7"
    };
}

HardwarePreflightResult makePreflightResult()
{
    HardwarePreflightResult res;
    res.identityState = HardwareIdentityState::IdentityVerified;
    res.disposition = HardwarePreflightDisposition::Ready;
    res.portSelection = makeTestPort();
    res.targetProfileId = "yamaha_dx7";
    res.readyForConsentOrDispatch = true;
    return res;
}

MidiCcMessage makeCcMessage(uint8_t ctrl = 19, uint8_t val = 64)
{
    MidiCcMessage msg;
    msg.channel = 1;
    msg.controllerNumber = ctrl;
    msg.value = val;
    msg.targetProfileId = "yamaha_dx7";
    msg.semanticId = "cutoff";
    msg.sequenceNumber = 1;
    return msg;
}

MidiSysExMessage makeSysExMessage(uint8_t val = 95)
{
    MidiSysExMessage msg;
    msg.bytes = { 0xF0, 0x43, 0x00, 0x09, 0x10, val, 0xF7 };
    msg.targetProfileId = "yamaha_dx7";
    msg.semanticId = "cutoff";
    msg.sequenceNumber = 1;
    return msg;
}

} // namespace

TEST_CASE("HardwareScheduler - Anti-TOCTOU and Consent Guard", "[hardware][scheduler]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(makeTestPort());

    const auto profile = makeTestProfile("yamaha_dx7", 20);
    const auto preflight = makePreflightResult();
    const auto port = makeTestPort("midi_out_1");
    const std::string planHash = "plan_hash_alpha";
    const auto cc = makeCcMessage(19, 64);

    const auto req = OperatorConsentService::buildRequest(
        profile, ProfileProvenanceOrigin::BuiltIn, preflight,
        "recipe_hash_1", planHash, "cutoff", 0.5, cc, std::nullopt);

    SECTION("Blocked when consent was not granted")
    {
        const auto deniedConsent = OperatorConsentService::denyConsent(req);

        const auto decision = scheduler.dispatchCc(
            transport, req, deniedConsent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::BlockedByConsent);
        REQUIRE(decision.diagnosticCode == "ERR_CONSENT_NOT_GRANTED");
        REQUIRE(transport.sentCc().empty());
    }

    SECTION("Blocked with ERR_CONSENT_INVALIDATED_TOCTOU if plan changed after consent")
    {
        const auto grantedConsent = OperatorConsentService::grantConsent(req);
        const std::string alteredPlanHash = "plan_hash_tampered_beta";

        const auto decision = scheduler.dispatchCc(
            transport, req, grantedConsent, alteredPlanHash, port, cc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::BlockedByConsent);
        REQUIRE(decision.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
        REQUIRE(transport.sentCc().empty());
    }

    SECTION("Blocked with ERR_CONSENT_INVALIDATED_TOCTOU if port changed after consent")
    {
        const auto grantedConsent = OperatorConsentService::grantConsent(req);
        const auto alteredPort = makeTestPort("midi_out_hijacked");

        const auto decision = scheduler.dispatchCc(
            transport, req, grantedConsent, planHash, alteredPort, cc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::BlockedByConsent);
        REQUIRE(decision.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
        REQUIRE(transport.sentCc().empty());
    }

    SECTION("Blocked with ERR_CONSENT_INVALIDATED_TOCTOU if payload changed after consent")
    {
        const auto grantedConsent = OperatorConsentService::grantConsent(req);
        const auto alteredCc = makeCcMessage(19, 65); // value 64 -> 65

        const auto decision = scheduler.dispatchCc(
            transport, req, grantedConsent, planHash, port, alteredCc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::BlockedByConsent);
        REQUIRE(decision.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
        REQUIRE(transport.sentCc().empty());
    }
}

TEST_CASE("HardwareScheduler - Inter-Message Delay Pacing", "[hardware][scheduler]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(makeTestPort());

    const auto profile = makeTestProfile("yamaha_dx7", 20); // 20 ms min delay
    const auto preflight = makePreflightResult();
    const auto port = makeTestPort();
    const std::string planHash = "plan_hash_alpha";

    const auto sysEx1 = makeSysExMessage(95);
    const auto req1 = OperatorConsentService::buildRequest(
        profile, ProfileProvenanceOrigin::BuiltIn, preflight,
        "recipe_hash_1", planHash, "cutoff", 0.75, std::nullopt, sysEx1);
    const auto consent1 = OperatorConsentService::grantConsent(req1);

    SECTION("First message dispatches immediately at t=1000")
    {
        const auto d1 = scheduler.dispatchSysEx(
            transport, req1, consent1, planHash, port, sysEx1, profile.transportPolicy);

        REQUIRE(d1.decision == DispatchDecision::DispatchNow);
        REQUIRE(d1.diagnosticCode == "OK");
        REQUIRE(transport.sentSysEx().size() == 1);
        REQUIRE(scheduler.getLastTransmissionMonotonicMs() == 1000);

        // Attempt second message at t=1005 (only 5 ms elapsed, requires 20 ms)
        clock.advanceMs(5); // t = 1005
        const auto sysEx2 = makeSysExMessage(96);
        const auto req2 = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_hash_1", planHash, "cutoff", 0.76, std::nullopt, sysEx2);
        const auto consent2 = OperatorConsentService::grantConsent(req2);

        const auto d2 = scheduler.dispatchSysEx(
            transport, req2, consent2, planHash, port, sysEx2, profile.transportPolicy);

        REQUIRE(d2.decision == DispatchDecision::PendingPacingWindow);
        REQUIRE(d2.earliestAllowedMonotonicTimeMs == 1020);
        REQUIRE(d2.diagnosticCode == "ERR_DISPATCH_PACING_PENDING");
        // Transport must still only contain 1 message
        REQUIRE(transport.sentSysEx().size() == 1);

        // Advance clock to t=1020 ms (delay satisfied)
        clock.setTimeMs(1020);
        const auto d3 = scheduler.dispatchSysEx(
            transport, req2, consent2, planHash, port, sysEx2, profile.transportPolicy);

        REQUIRE(d3.decision == DispatchDecision::DispatchNow);
        REQUIRE(transport.sentSysEx().size() == 2);
        REQUIRE(scheduler.getLastTransmissionMonotonicMs() == 1020);
    }
}

TEST_CASE("HardwareScheduler - Rate Limiting Per Second", "[hardware][scheduler]")
{
    MockMonotonicClock clock(2000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(makeTestPort());

    // 0 ms min delay, max 2 messages per second
    const auto profile = makeTestProfile("behringer_pro800", 0, 2);
    const auto preflight = makePreflightResult();
    const auto port = makeTestPort();
    const std::string planHash = "plan_hash_rate";

    auto sendCcAt = [&](uint8_t val) {
        const auto cc = makeCcMessage(19, val);
        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_hash_1", planHash, "cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);
        return scheduler.dispatchCc(transport, req, consent, planHash, port, cc, profile.transportPolicy);
    };

    // First two messages in same second succeed
    REQUIRE(sendCcAt(10).decision == DispatchDecision::DispatchNow);
    REQUIRE(sendCcAt(20).decision == DispatchDecision::DispatchNow);
    REQUIRE(transport.sentCc().size() == 2);

    // Third message in same second is rate-limited
    const auto d3 = sendCcAt(30);
    REQUIRE(d3.decision == DispatchDecision::PendingPacingWindow);
    REQUIRE(d3.diagnosticCode == "ERR_DISPATCH_RATE_LIMIT_EXCEEDED");
    REQUIRE(transport.sentCc().size() == 2);

    // Advance clock to next second (t=3000) -> rate limit resets
    clock.setTimeMs(3000);
    const auto d4 = sendCcAt(30);
    REQUIRE(d4.decision == DispatchDecision::DispatchNow);
    REQUIRE(transport.sentCc().size() == 3);
}

TEST_CASE("HardwareScheduler - Transport Failures Propagation", "[hardware][scheduler]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(makeTestPort());

    const auto profile = makeTestProfile("yamaha_dx7", 0);
    const auto preflight = makePreflightResult();
    const auto port = makeTestPort();
    const std::string planHash = "plan_hash_err";
    const auto cc = makeCcMessage(19, 64);

    const auto req = OperatorConsentService::buildRequest(
        profile, ProfileProvenanceOrigin::BuiltIn, preflight,
        "recipe_hash_1", planHash, "cutoff", 0.5, cc, std::nullopt);
    const auto consent = OperatorConsentService::grantConsent(req);

    SECTION("Transport write failure propagates as Failed decision")
    {
        transport.failNextSend(MidiTransportError::WriteFailed, "ERR_MIDI_TRANSPORT_WRITE_FAILED");

        const auto decision = scheduler.dispatchCc(
            transport, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::Failed);
        REQUIRE(decision.diagnosticCode == "ERR_MIDI_TRANSPORT_WRITE_FAILED");
    }

    SECTION("Transport disconnect propagates as Failed decision")
    {
        transport.simulateDisconnect();

        const auto decision = scheduler.dispatchCc(
            transport, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(decision.decision == DispatchDecision::Failed);
        REQUIRE(decision.diagnosticCode == "ERR_MIDI_TRANSPORT_DISCONNECTED");
    }
}
