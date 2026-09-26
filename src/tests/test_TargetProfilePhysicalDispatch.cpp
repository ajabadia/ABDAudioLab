#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numeric>
#include "hardware/transport/HardwareDispatchScheduler.h"
#include "hardware/consent/OperatorConsentService.h"
#include "tests/support/MockMidiTransport.h"
#include "tests/support/MockMonotonicClock.h"
#include "tests/support/MockMidiResponseAwaiter.h"
#include "profiling/TargetProfile.h"
#include "synth/SysExContracts.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::tests;
using namespace abdaudiolab::synth;

namespace {

TargetProfile makePro800Profile()
{
    TargetProfile p;
    p.targetProfileId = "hw-behringer-pro800-canonical";
    p.displayName = "Behringer PRO-800";
    p.vendor = "Behringer";
    p.targetKind = "HardwareDigital";
    p.hasExplicitTransportPolicy = true;
    p.transportPolicy.minimumInterMessageDelayMs = 0;
    p.transportPolicy.maximumMessagesPerSecond = 0;
    p.transportPolicy.requiresResponseAck = false;
    return p;
}

TargetProfile makeDx7Profile(int delayMs = 20)
{
    TargetProfile p;
    p.targetProfileId = "hw-yamaha-dx7-canonical";
    p.displayName = "Yamaha DX7 (Mark I)";
    p.vendor = "Yamaha";
    p.targetKind = "HardwareDigital";
    p.hasExplicitTransportPolicy = true;
    p.transportPolicy.minimumInterMessageDelayMs = delayMs;
    p.transportPolicy.maximumMessagesPerSecond = 0;
    p.transportPolicy.requiresResponseAck = false;
    return p;
}

MidiPortSelection makePort(std::string devId = "midi_port_1", std::string targetId = "hw-target")
{
    return MidiPortSelection {
        std::move(devId),
        "Hardware MIDI USB",
        std::move(targetId)
    };
}

HardwarePreflightResult makePreflight(std::string targetId,
                                      const MidiPortSelection& port,
                                      HardwareIdentityState state = HardwareIdentityState::IdentityVerified)
{
    HardwarePreflightResult r;
    r.targetProfileId = targetId;
    r.identityState = state;
    r.disposition = (state == HardwareIdentityState::IdentityMismatch) 
        ? HardwarePreflightDisposition::Blocked 
        : HardwarePreflightDisposition::Ready;
    r.portSelection = port;
    r.readyForConsentOrDispatch = (state != HardwareIdentityState::IdentityMismatch);
    return r;
}

uint8_t calculateYamahaChecksum(const std::vector<uint8_t>& data)
{
    int sum = 0;
    for (auto b : data)
    {
        sum += (b & 0x7F);
    }
    return static_cast<uint8_t>((128 - (sum % 128)) & 0x7F);
}

} // namespace

TEST_CASE("D2.6: TargetProfile Physical Dispatch - PRO-800 Quantization and CC Exactness", "[hardware][dispatch]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makePort("pro800_out", "hw-behringer-pro800-canonical");
    transport.open(port);

    const auto profile = makePro800Profile();
    const auto preflight = makePreflight("hw-behringer-pro800-canonical", port);
    const std::string planHash = "plan_pro800_alpha";

    SECTION("PRO-800 filter_cutoff -> CC 19, channel 1, round() quantization")
    {
        const double normalizedCutoff = 0.503937; // 0.503937 * 127 = 64.000000 -> round -> 64
        const uint8_t expectedVal = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(normalizedCutoff * 127.0)), 0, 127));
        REQUIRE(expectedVal == 64);

        MidiCcMessage cc;
        cc.channel = 1;
        cc.controllerNumber = 19;
        cc.value = expectedVal;
        cc.targetProfileId = profile.targetProfileId;
        cc.semanticId = "filter_cutoff";

        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_cutoff", planHash, "filter_cutoff", normalizedCutoff, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE_FALSE(evidence.exportBlocked);
        REQUIRE(evidence.diagnosticCode == "OK");
        REQUIRE(evidence.consentWasGranted);
        REQUIRE(evidence.transportError == MidiTransportError::None);
        REQUIRE(evidence.targetProfileId == "hw-behringer-pro800-canonical");
        REQUIRE(evidence.semanticId == "filter_cutoff");
        REQUIRE(evidence.commandDigest == req.commandDigest);

        REQUIRE(transport.sentCc().size() == 1);
        const auto& sent = transport.sentCc().front();
        CHECK(sent.channel == 1);
        CHECK(sent.controllerNumber == 19);
        CHECK(sent.value == 64);
    }

    SECTION("PRO-800 filter_resonance -> CC 21, channel 1, exact value")
    {
        const double normalizedResonance = 0.8; // 0.8 * 127 = 101.6 -> round -> 102
        const uint8_t expectedVal = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(normalizedResonance * 127.0)), 0, 127));
        REQUIRE(expectedVal == 102);

        MidiCcMessage cc;
        cc.channel = 1;
        cc.controllerNumber = 21;
        cc.value = expectedVal;
        cc.targetProfileId = profile.targetProfileId;
        cc.semanticId = "filter_resonance";

        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_res", planHash, "filter_resonance", normalizedResonance, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE_FALSE(evidence.exportBlocked);
        REQUIRE(evidence.diagnosticCode == "OK");
        REQUIRE(transport.sentCc().size() == 1);
        const auto& sent = transport.sentCc().front();
        CHECK(sent.channel == 1);
        CHECK(sent.controllerNumber == 21);
        CHECK(sent.value == 102);
    }
}

TEST_CASE("D2.6: TargetProfile Physical Dispatch - DX7 SysEx, Checksum and 20ms Pacing", "[hardware][dispatch]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makePort("dx7_out", "hw-yamaha-dx7-canonical");
    transport.open(port);

    const auto profile = makeDx7Profile(20); // 20 ms minimum delay
    const auto preflight = makePreflight("hw-yamaha-dx7-canonical", port);
    const std::string planHash = "plan_dx7_hex";

    // Build DX7 parameter change SysEx: F0 43 00 00 13 [val] [chk] F7
    const uint8_t val1 = 99;
    std::vector<uint8_t> dataPayload1 = { 0x00, 0x00, 0x13, val1 };
    uint8_t chk1 = calculateYamahaChecksum(dataPayload1);

    MidiSysExMessage sysex1;
    sysex1.bytes = { 0xF0, 0x43, dataPayload1[0], dataPayload1[1], dataPayload1[2], dataPayload1[3], chk1, 0xF7 };
    sysex1.targetProfileId = profile.targetProfileId;
    sysex1.semanticId = "operator_1_level";

    const auto req1 = OperatorConsentService::buildRequest(
        profile, ProfileProvenanceOrigin::BuiltIn, preflight,
        "recipe_dx7", planHash, "operator_1_level", 0.78, std::nullopt, sysex1);
    const auto consent1 = OperatorConsentService::grantConsent(req1);

    SECTION("First SysEx dispatches at t=1000 with exact bytes, checksum and commandDigest")
    {
        const auto ev1 = scheduler.executeDispatchSysEx(
            transport, nullptr, req1, consent1, planHash, port, sysex1, profile.transportPolicy);

        REQUIRE_FALSE(ev1.exportBlocked);
        REQUIRE(ev1.diagnosticCode == "OK");
        REQUIRE(ev1.commandDigest == req1.commandDigest);
        REQUIRE(ev1.monotonicTimestampMs == 1000);
        REQUIRE(transport.sentSysEx().size() == 1);

        const auto& sent1 = transport.sentSysEx().front();
        REQUIRE(sent1.bytes.size() == 8);
        CHECK(sent1.bytes.front() == 0xF0);
        CHECK(sent1.bytes.back() == 0xF7);
        CHECK(sent1.bytes[1] == 0x43); // Yamaha
        CHECK(sent1.bytes[6] == chk1); // Checksum matches

        // Second SysEx attempted at t=1010 ms (only 10 ms elapsed, requires 20 ms)
        clock.advanceMs(10); // t = 1010
        const uint8_t val2 = 100;
        std::vector<uint8_t> dataPayload2 = { 0x00, 0x00, 0x13, val2 };
        uint8_t chk2 = calculateYamahaChecksum(dataPayload2);

        MidiSysExMessage sysex2;
        sysex2.bytes = { 0xF0, 0x43, dataPayload2[0], dataPayload2[1], dataPayload2[2], dataPayload2[3], chk2, 0xF7 };
        sysex2.targetProfileId = profile.targetProfileId;
        sysex2.semanticId = "operator_1_level";

        const auto req2 = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_dx7", planHash, "operator_1_level", 0.79, std::nullopt, sysex2);
        const auto consent2 = OperatorConsentService::grantConsent(req2);

        const auto ev2 = scheduler.executeDispatchSysEx(
            transport, nullptr, req2, consent2, planHash, port, sysex2, profile.transportPolicy);

        // Pacing pending -> 0 new messages dispatched
        REQUIRE_FALSE(ev2.exportBlocked);
        REQUIRE(ev2.diagnosticCode == "ERR_DISPATCH_PACING_PENDING");
        REQUIRE(transport.sentSysEx().size() == 1);

        // Advance clock by 10 ms to reach t=1020 ms (20 ms elapsed)
        clock.advanceMs(10); // t = 1020

        const auto ev3 = scheduler.executeDispatchSysEx(
            transport, nullptr, req2, consent2, planHash, port, sysex2, profile.transportPolicy);

        REQUIRE_FALSE(ev3.exportBlocked);
        REQUIRE(ev3.diagnosticCode == "OK");
        REQUIRE(transport.sentSysEx().size() == 2);
        CHECK(transport.sentSysEx().back().bytes[5] == val2);
        CHECK(transport.sentSysEx().back().bytes[6] == chk2);
    }
}

TEST_CASE("D2.6: TargetProfile Physical Dispatch - Zero Messages on Guard Violations", "[hardware][dispatch]")
{
    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    const auto port = makePort("pro800_out", "hw-behringer-pro800-canonical");
    transport.open(port);

    const auto profile = makePro800Profile();
    const std::string planHash = "plan_alpha";

    MidiCcMessage cc;
    cc.channel = 1;
    cc.controllerNumber = 19;
    cc.value = 64;
    cc.targetProfileId = profile.targetProfileId;
    cc.semanticId = "filter_cutoff";

    SECTION("Consent denied -> 0 messages transmitted")
    {
        const auto preflight = makePreflight("hw-behringer-pro800-canonical", port, HardwareIdentityState::IdentityVerified);
        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_cutoff", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto deniedConsent = OperatorConsentService::denyConsent(req, "Operator rejected parameter move");

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, deniedConsent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE_FALSE(evidence.consentWasGranted);
        REQUIRE(evidence.diagnosticCode == "ERR_CONSENT_NOT_GRANTED");
        REQUIRE(transport.sentCc().empty());
        REQUIRE(transport.sentSysEx().empty());
    }

    SECTION("Preflight blocked by IdentityMismatch -> 0 messages transmitted")
    {
        const auto preflightMismatch = makePreflight("hw-behringer-pro800-canonical", port, HardwareIdentityState::IdentityMismatch);
        const auto req = OperatorConsentService::buildRequest(
            profile, ProfileProvenanceOrigin::BuiltIn, preflightMismatch,
            "recipe_cutoff", planHash, "filter_cutoff", 0.5, cc, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        const auto evidence = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, port, cc, profile.transportPolicy);

        REQUIRE(evidence.exportBlocked);
        REQUIRE(evidence.diagnosticCode == "ERR_IDENTITY_MISMATCH_BLOCKED");
        REQUIRE(transport.sentCc().empty());
        REQUIRE(transport.sentSysEx().empty());
    }

    SECTION("Pacing pending -> 0 messages transmitted until clock advances")
    {
        const auto portDx7 = makePort("dx7_out", "hw-yamaha-dx7-canonical");
        transport.open(portDx7);
        const auto profilePaced = makeDx7Profile(50); // 50 ms delay
        const auto preflight = makePreflight("hw-yamaha-dx7-canonical", portDx7, HardwareIdentityState::IdentityVerified);

        MidiCcMessage ccDx7;
        ccDx7.channel = 1;
        ccDx7.controllerNumber = 19;
        ccDx7.value = 64;
        ccDx7.targetProfileId = profilePaced.targetProfileId;
        ccDx7.semanticId = "filter_cutoff";

        const auto req = OperatorConsentService::buildRequest(
            profilePaced, ProfileProvenanceOrigin::BuiltIn, preflight,
            "recipe_cutoff", planHash, "filter_cutoff", 0.5, ccDx7, std::nullopt);
        const auto consent = OperatorConsentService::grantConsent(req);

        // 1st transmission
        const auto ev1 = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, portDx7, ccDx7, profilePaced.transportPolicy);
        REQUIRE(ev1.diagnosticCode == "OK");
        REQUIRE(transport.sentCc().size() == 1);

        // 2nd transmission at t+10ms -> pending pacing window
        clock.advanceMs(10);
        const auto ev2 = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, portDx7, ccDx7, profilePaced.transportPolicy);
        REQUIRE(ev2.diagnosticCode == "ERR_DISPATCH_PACING_PENDING");
        REQUIRE(transport.sentCc().size() == 1); // No new message

        // Advance to t+50ms -> allowed
        clock.advanceMs(40);
        const auto ev3 = scheduler.executeDispatchCc(
            transport, nullptr, req, consent, planHash, portDx7, ccDx7, profilePaced.transportPolicy);
        REQUIRE(ev3.diagnosticCode == "OK");
        REQUIRE(transport.sentCc().size() == 2);
    }
}
