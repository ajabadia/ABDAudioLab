#include <catch2/catch_test_macros.hpp>
#include "hardware/consent/OperatorConsentService.h"
#include "hardware/consent/OperatorConsentRequest.h"
#include "tests/support/MockMidiTransport.h"
#include "profiling/TargetProfile.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::tests;

namespace {

TargetProfile makeTestProfile(std::string profileId, bool explicitConfirm = false)
{
    TargetProfile profile;
    profile.targetProfileId = std::move(profileId);
    profile.vendor = "Yamaha";
    profile.displayName = "DX7";
    profile.hasExplicitTransportPolicy = true;
    profile.transportPolicy.requiresExplicitConfirmation = explicitConfirm;
    profile.transportPolicy.minimumInterMessageDelayMs = 20;
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

HardwarePreflightResult makePreflightResult(HardwareIdentityState idState, HardwarePreflightDisposition disposition)
{
    HardwarePreflightResult res;
    res.identityState = idState;
    res.disposition = disposition;
    res.portSelection = makeTestPort();
    res.targetProfileId = "yamaha_dx7";
    res.readyForConsentOrDispatch = (disposition == HardwarePreflightDisposition::Ready);
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

TEST_CASE("OperatorConsent - SysEx Mandates Explicit Consent", "[hardware][consent]")
{
    const auto profile = makeTestProfile("yamaha_dx7", false);
    const auto preflight = makePreflightResult(HardwareIdentityState::IdentityVerified, HardwarePreflightDisposition::Ready);
    const auto sysEx = makeSysExMessage(95);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.75,
        std::nullopt,
        sysEx);

    REQUIRE(req.containsSysEx);
    REQUIRE(OperatorConsentService::requiresExplicitConsent(req));
    REQUIRE_FALSE(req.commandDigest.empty());
}

TEST_CASE("OperatorConsent - Untrusted Imported Profile Mandates Explicit Consent", "[hardware][consent]")
{
    const auto profile = makeTestProfile("custom_synth", false);
    const auto preflight = makePreflightResult(HardwareIdentityState::IdentityVerified, HardwarePreflightDisposition::Ready);
    const auto cc = makeCcMessage(19, 64);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::ImportedUntrusted,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.5,
        cc,
        std::nullopt);

    REQUIRE_FALSE(req.containsSysEx);
    REQUIRE(req.profileOrigin == ProfileProvenanceOrigin::ImportedUntrusted);
    REQUIRE(OperatorConsentService::requiresExplicitConsent(req));
}

TEST_CASE("OperatorConsent - BuiltIn CC With Verified Identity Policy", "[hardware][consent]")
{
    const auto profile = makeTestProfile("behringer_pro800", false);
    const auto preflight = makePreflightResult(HardwareIdentityState::IdentityVerified, HardwarePreflightDisposition::Ready);
    const auto cc = makeCcMessage(19, 64);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.5,
        cc,
        std::nullopt);

    REQUIRE_FALSE(OperatorConsentService::requiresExplicitConsent(req));
}

TEST_CASE("OperatorConsent - Operator Denial Guarantees Zero Dispatch", "[hardware][consent]")
{
    const auto profile = makeTestProfile("yamaha_dx7", false);
    const auto preflight = makePreflightResult(HardwareIdentityState::IdentityVerified, HardwarePreflightDisposition::Ready);
    const auto sysEx = makeSysExMessage(95);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.75,
        std::nullopt,
        sysEx);

    const auto consent = OperatorConsentService::denyConsent(req, "Operator clicked Cancel");

    REQUIRE(consent.decision == OperatorConsentDecision::Denied);
    REQUIRE_FALSE(consent.readyForDispatch);
    REQUIRE(consent.diagnosticCode == "ERR_OPERATOR_DENIED");
}

TEST_CASE("OperatorConsent - Identity Transition for Unverified Target", "[hardware][consent]")
{
    const auto profile = makeTestProfile("yamaha_dx7", false);
    const auto preflight = makePreflightResult(
        HardwareIdentityState::IdentityUnavailable, 
        HardwarePreflightDisposition::AwaitingUserConfirmation);
    const auto sysEx = makeSysExMessage(95);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.75,
        std::nullopt,
        sysEx);

    REQUIRE(req.requiresIdentityConfirmation);

    const auto consent = OperatorConsentService::grantConsent(req, "Operator confirmed unverified hardware");

    REQUIRE(consent.decision == OperatorConsentDecision::Granted);
    REQUIRE(consent.readyForDispatch);
    REQUIRE(consent.resultingIdentityState == HardwareIdentityState::UserConfirmedUnverified);
    REQUIRE(consent.diagnosticCode == "OK");
}

TEST_CASE("OperatorConsent - Identity Mismatch Is Strictly Blocked", "[hardware][consent]")
{
    const auto profile = makeTestProfile("yamaha_dx7", false);
    const auto preflight = makePreflightResult(
        HardwareIdentityState::IdentityMismatch, 
        HardwarePreflightDisposition::Blocked);
    const auto sysEx = makeSysExMessage(95);

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        "plan_hash_456",
        "cutoff",
        0.75,
        std::nullopt,
        sysEx);

    const auto consent = OperatorConsentService::grantConsent(req);

    REQUIRE(consent.decision == OperatorConsentDecision::BlockedByIdentityMismatch);
    REQUIRE_FALSE(consent.readyForDispatch);
    REQUIRE(consent.resultingIdentityState == HardwareIdentityState::IdentityMismatch);
    REQUIRE(consent.diagnosticCode == "ERR_IDENTITY_MISMATCH_BLOCKED");
}

TEST_CASE("OperatorConsent - Anti-TOCTOU Invalidation Gate", "[hardware][consent]")
{
    const auto profile = makeTestProfile("yamaha_dx7", false);
    const auto preflight = makePreflightResult(HardwareIdentityState::IdentityVerified, HardwarePreflightDisposition::Ready);
    const auto sysEx = makeSysExMessage(95);
    const std::string planHash = "plan_hash_original_456";
    const auto port = makeTestPort("midi_out_1");

    const auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_hash_123",
        planHash,
        "cutoff",
        0.75,
        std::nullopt,
        sysEx);

    const auto granted = OperatorConsentService::grantConsent(req);
    REQUIRE(granted.decision == OperatorConsentDecision::Granted);
    REQUIRE(granted.readyForDispatch);

    SECTION("Valid token passes when environment and plan match perfectly")
    {
        const auto validated = OperatorConsentService::validateConsentToken(
            req, granted, planHash, port, std::nullopt, sysEx);

        REQUIRE(validated.decision == OperatorConsentDecision::Granted);
        REQUIRE(validated.readyForDispatch);
    }

    SECTION("Altered execution plan hash invalidates consent")
    {
        const std::string alteredPlanHash = "plan_hash_tampered_789";

        const auto validated = OperatorConsentService::validateConsentToken(
            req, granted, alteredPlanHash, port, std::nullopt, sysEx);

        REQUIRE(validated.decision == OperatorConsentDecision::Invalidated);
        REQUIRE_FALSE(validated.readyForDispatch);
        REQUIRE(validated.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }

    SECTION("Altered MIDI port selection invalidates consent")
    {
        const auto alteredPort = makeTestPort("midi_out_hijacked_2");

        const auto validated = OperatorConsentService::validateConsentToken(
            req, granted, planHash, alteredPort, std::nullopt, sysEx);

        REQUIRE(validated.decision == OperatorConsentDecision::Invalidated);
        REQUIRE_FALSE(validated.readyForDispatch);
        REQUIRE(validated.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }

    SECTION("Altered SysEx payload bytes invalidate consent")
    {
        auto alteredSysEx = makeSysExMessage(96); // 95 -> 96

        const auto validated = OperatorConsentService::validateConsentToken(
            req, granted, planHash, port, std::nullopt, alteredSysEx);

        REQUIRE(validated.decision == OperatorConsentDecision::Invalidated);
        REQUIRE_FALSE(validated.readyForDispatch);
        REQUIRE(validated.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }
}
