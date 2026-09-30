#include <catch2/catch_test_macros.hpp>
#include "core/LabResourcePaths.h"
#include "profiling/TargetProfileService.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::profiling;

namespace
{

std::string createBaseProfileJson(const std::string& transportPolicySnippet = "")
{
    std::string jsonStr = R"({
  "schemaVersion": "1.0",
  "kind": "abd.target-profile",
  "targetProfileId": "test.hardware.policy",
  "displayName": "Test Hardware Policy Target",
  "vendor": "ABD Testing",
  "targetKind": "HardwareDigital",
  "revision": 1,
  "identity": {
    "canonicalTargetId": "test_hw_policy",
    "acceptedUniqueIds": ["test-hw-uid"],
    "binaryIdentityPolicy": "not-applicable"
  },
  "capabilities": {
    "midiInput": true,
    "supportsParameterAutomation": true,
    "controlTransports": ["MidiContinuousController"],
    "audioOutput": {
      "supportedChannelCounts": [1, 2],
      "requiredChannelCount": 2,
      "channelLayout": "stereo",
      "supportedObservationLayouts": ["stereo"]
    },
    "sampleRatesHz": [44100, 48000],
    "blockSizes": [128, 256],
    "supportsPolyphony": true,
    "midiChannels": [1],
    "midiNoteRange": [0, 127]
  },
  "parameters": [
    {
      "semanticId": "filter_cutoff",
      "displayName": "Cutoff Frequency",
      "technicalIdentifier": {
        "kind": "MidiContinuousController",
        "channel": 1,
        "controllerNumber": 19
      },
      "valueType": "continuous",
      "normalizedRange": [0.0, 1.0],
      "mappingCurve": { "kind": "linear" },
      "confirmationStatus": "UserConfirmed"
    }
  ],
  "measurementPolicies": {
    "warmupTimeMs": 20,
    "defaultSettlingTimeMs": 50,
    "recommendedCalibrationPolicy": "None",
    "requiresResetBetweenTrials": false
  }
)";

    if (!transportPolicySnippet.empty())
    {
        jsonStr += ",\n  \"transportPolicy\": " + transportPolicySnippet;
    }
    jsonStr += "\n}";
    return jsonStr;
}

} // namespace

TEST_CASE("TargetProfile TransportPolicy: Defaults When Omitted", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;
    std::string jsonStr = createBaseProfileJson();

    auto res = service.loadAndValidateProfileJson(jsonStr);
    REQUIRE(res.isSuccess());
    REQUIRE(res.diagnostics.empty());

    const auto& tp = res.profile.transportPolicy;
    CHECK(tp.minimumInterMessageDelayMs == 0);
    CHECK(tp.maximumMessagesPerSecond == 0);
    CHECK_FALSE(tp.requiresResponseAck);
    CHECK(tp.responseTimeoutMs == 0);
    CHECK(tp.retryPolicy == RetryPolicy::None);
    CHECK(tp.maxRetries == 0);
    CHECK_FALSE(tp.requiresExplicitConfirmation);
    CHECK_FALSE(tp.allowsBulkDump);
    CHECK_FALSE(tp.requiresVerifiedIdentity);
    CHECK(tp.allowsUserConfirmedUnverifiedIdentity);
}

TEST_CASE("TargetProfile TransportPolicy: Valid Policy Loading and Deterministic Hash", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;
    std::string policySnippet = R"({
    "minimumInterMessageDelayMs": 25,
    "maximumMessagesPerSecond": 40,
    "requiresResponseAck": true,
    "responseTimeoutMs": 400,
    "retryPolicy": "linear",
    "maxRetries": 2,
    "requiresExplicitConfirmation": true,
    "allowsBulkDump": false,
    "requiresVerifiedIdentity": true,
    "allowsUserConfirmedUnverifiedIdentity": false
  })";

    std::string jsonStr = createBaseProfileJson(policySnippet);
    auto res1 = service.loadAndValidateProfileJson(jsonStr);
    REQUIRE(res1.isSuccess());
    REQUIRE(res1.diagnostics.empty());

    const auto& tp = res1.profile.transportPolicy;
    CHECK(tp.minimumInterMessageDelayMs == 25);
    CHECK(tp.maximumMessagesPerSecond == 40);
    CHECK(tp.requiresResponseAck);
    CHECK(tp.responseTimeoutMs == 400);
    CHECK(tp.retryPolicy == RetryPolicy::Linear);
    CHECK(tp.maxRetries == 2);
    CHECK(tp.requiresExplicitConfirmation);
    CHECK_FALSE(tp.allowsBulkDump);
    CHECK(tp.requiresVerifiedIdentity);
    CHECK_FALSE(tp.allowsUserConfirmedUnverifiedIdentity);

    // Repetibilidad determinista del hash
    auto res2 = service.loadAndValidateProfileJson(jsonStr);
    REQUIRE(res2.isSuccess());
    CHECK(res1.canonicalProfileHash == res2.canonicalProfileHash);
    CHECK_FALSE(res1.canonicalProfileHash.empty());

    // Round-trip de serialización
    std::string serialized = service.serializeProfileToJson(res1.profile);
    auto resRoundTrip = service.loadAndValidateProfileJson(serialized);
    REQUIRE(resRoundTrip.isSuccess());
    CHECK(resRoundTrip.profile.transportPolicy == res1.profile.transportPolicy);
}

TEST_CASE("TargetProfile TransportPolicy: Unknown Field Rejection", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;
    std::string policySnippet = R"({
    "minimumInterMessageDelayMs": 10,
    "unknownTransportOption": 999
  })";

    std::string jsonStr = createBaseProfileJson(policySnippet);
    auto res = service.loadAndValidateProfileJson(jsonStr);
    REQUIRE_FALSE(res.isSuccess());

    bool hasUnknownFieldDiag = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_SCHEMA_UNKNOWN_FIELD" && d.jsonPointer.find("unknownTransportOption") != std::string::npos)
            hasUnknownFieldDiag = true;
    }
    CHECK(hasUnknownFieldDiag);
}

TEST_CASE("TargetProfile TransportPolicy: Range Validation", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;

    SECTION("minimumInterMessageDelayMs negativo rechazado")
    {
        std::string jsonStr = createBaseProfileJson(R"({ "minimumInterMessageDelayMs": -1 })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        CHECK(res.diagnostics.front().code == "ERR_SEMANTICS_INVALID_RANGE");
    }

    SECTION("minimumInterMessageDelayMs mayor a 60000 ms rechazado")
    {
        std::string jsonStr = createBaseProfileJson(R"({ "minimumInterMessageDelayMs": 60001 })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        CHECK(res.diagnostics.front().code == "ERR_SEMANTICS_INVALID_RANGE");
    }

    SECTION("maximumMessagesPerSecond negativo rechazado")
    {
        std::string jsonStr = createBaseProfileJson(R"({ "maximumMessagesPerSecond": -5 })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        CHECK(res.diagnostics.front().code == "ERR_SEMANTICS_INVALID_RANGE");
    }

    SECTION("responseTimeoutMs negativo rechazado")
    {
        std::string jsonStr = createBaseProfileJson(R"({ "responseTimeoutMs": -10 })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        CHECK(res.diagnostics.front().code == "ERR_SEMANTICS_INVALID_RANGE");
    }

    SECTION("maxRetries negativo rechazado")
    {
        std::string jsonStr = createBaseProfileJson(R"({ "maxRetries": -2 })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        CHECK(res.diagnostics.front().code == "ERR_SEMANTICS_INVALID_RANGE");
    }
}

TEST_CASE("TargetProfile TransportPolicy: Cross-Field Consistency and Semantic Rules", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;

    SECTION("requiresResponseAck = true exige responseTimeoutMs > 0")
    {
        std::string jsonStr = createBaseProfileJson(R"({
            "requiresResponseAck": true,
            "responseTimeoutMs": 0
        })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        bool hasDiag = false;
        for (const auto& d : res.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_POLICY") hasDiag = true;
        CHECK(hasDiag);
    }

    SECTION("retryPolicy = none exige maxRetries == 0")
    {
        std::string jsonStr = createBaseProfileJson(R"({
            "retryPolicy": "none",
            "maxRetries": 3
        })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        bool hasDiag = false;
        for (const auto& d : res.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_POLICY") hasDiag = true;
        CHECK(hasDiag);
    }

    SECTION("requiresResponseAck = false exige retryPolicy == none")
    {
        std::string jsonStr = createBaseProfileJson(R"({
            "requiresResponseAck": false,
            "retryPolicy": "linear",
            "maxRetries": 1
        })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        bool hasDiag = false;
        for (const auto& d : res.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_POLICY") hasDiag = true;
        CHECK(hasDiag);
    }

    SECTION("requiresVerifiedIdentity = true y allowsUserConfirmedUnverifiedIdentity = true son incompatibles")
    {
        std::string jsonStr = createBaseProfileJson(R"({
            "requiresVerifiedIdentity": true,
            "allowsUserConfirmedUnverifiedIdentity": true
        })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        bool hasDiag = false;
        for (const auto& d : res.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_POLICY") hasDiag = true;
        CHECK(hasDiag);
    }

    SECTION("allowsBulkDump = true es rechazado en D2.1")
    {
        std::string jsonStr = createBaseProfileJson(R"({
            "allowsBulkDump": true
        })");
        auto res = service.loadAndValidateProfileJson(jsonStr);
        REQUIRE_FALSE(res.isSuccess());
        bool hasDiag = false;
        for (const auto& d : res.diagnostics)
            if (d.code == "ERR_TRANSPORT_BULK_DUMP_UNSUPPORTED") hasDiag = true;
        CHECK(hasDiag);
    }
}

TEST_CASE("TargetProfile TransportPolicy: Canonical Hash Sensitivity", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;
    std::string jsonA = createBaseProfileJson(R"({ "minimumInterMessageDelayMs": 20 })");
    std::string jsonB = createBaseProfileJson(R"({ "minimumInterMessageDelayMs": 50 })");
    std::string jsonC = createBaseProfileJson(R"({ "minimumInterMessageDelayMs": 20, "requiresExplicitConfirmation": true })");

    auto resA = service.loadAndValidateProfileJson(jsonA);
    auto resB = service.loadAndValidateProfileJson(jsonB);
    auto resC = service.loadAndValidateProfileJson(jsonC);

    REQUIRE(resA.isSuccess());
    REQUIRE(resB.isSuccess());
    REQUIRE(resC.isSuccess());

    CHECK(resA.canonicalProfileHash != resB.canonicalProfileHash);
    CHECK(resA.canonicalProfileHash != resC.canonicalProfileHash);
    CHECK(resB.canonicalProfileHash != resC.canonicalProfileHash);
}

TEST_CASE("TargetProfile TransportPolicy: Existing Hardware Profiles Compliance", "[target_profile][transport_policy][d2]")
{
    TargetProfileService service;

    const auto targetsDir = abdaudiolab::core::canonicalTargetsDir();
    REQUIRE(targetsDir.exists());

    // 1. Yamaha DX7
    juce::File dx7File = targetsDir.getChildFile("yamaha_dx7.target.json");
    REQUIRE(dx7File.existsAsFile());
    auto dx7Res = service.loadAndValidateProfile(dx7File);
    REQUIRE(dx7Res.isSuccess());
    CHECK(dx7Res.profile.transportPolicy.minimumInterMessageDelayMs == 20);
    CHECK(dx7Res.profile.transportPolicy.requiresExplicitConfirmation);
    CHECK_FALSE(dx7Res.profile.transportPolicy.allowsBulkDump);

    // 2. Behringer PRO-800
    juce::File pro800File = targetsDir.getChildFile("behringer_pro800.target.json");
    REQUIRE(pro800File.existsAsFile());
    auto proRes = service.loadAndValidateProfile(pro800File);
    REQUIRE(proRes.isSuccess());
    CHECK(proRes.profile.transportPolicy.minimumInterMessageDelayMs == 5);
    CHECK_FALSE(proRes.profile.transportPolicy.requiresExplicitConfirmation);

    // 3. BOSS DS-1
    juce::File ds1File = targetsDir.getChildFile("boss_ds1_distortion.target.json");
    REQUIRE(ds1File.existsAsFile());
    auto ds1Res = service.loadAndValidateProfile(ds1File);
    REQUIRE(ds1Res.isSuccess());
    CHECK(ds1Res.profile.transportPolicy.minimumInterMessageDelayMs >= 500);
    CHECK(ds1Res.profile.transportPolicy.requiresExplicitConfirmation);
}
