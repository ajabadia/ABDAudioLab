/**
 * @file test_TargetProfileValidation.cpp
 * @brief HITO-10A: Verificación Normativa de Reglas de Esquema, Rechazo de Propiedades Desconocidas y Diagnósticos.
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::profiling;
using json = nlohmann::json;

namespace
{

std::string getValidBaseJson()
{
    return R"({
      "schemaVersion": "1.0",
      "kind": "abd.target-profile",
      "targetProfileId": "org.abd.test-synth.v1",
      "displayName": "Test Synth",
      "vendor": "ABD Labs",
      "targetKind": "SyntheticFixture",
      "revision": 1,
      "identity": {
        "canonicalTargetId": "test-synth",
        "acceptedUniqueIds": ["test-synth-uid"],
        "binaryIdentityPolicy": "not-applicable"
      },
      "capabilities": {
        "midiInput": true,
        "supportsParameterAutomation": true,
        "controlTransports": ["InternalParameter"],
        "audioOutput": {
          "supportedChannelCounts": [2],
          "requiredChannelCount": 2,
          "channelLayout": "stereo",
          "supportedObservationLayouts": ["stereo"]
        },
        "sampleRatesHz": [48000],
        "blockSizes": [256],
        "supportsPolyphony": true,
        "midiChannels": [1],
        "midiNoteRange": [0, 127]
      },
      "parameters": [
        {
          "semanticId": "cutoff",
          "displayName": "Cutoff",
          "technicalIdentifier": {
            "kind": "InternalParameter",
            "parameterKey": "filter.cutoff"
          },
          "valueType": "continuous",
          "normalizedRange": [0.0, 1.0],
          "mappingCurve": { "kind": "linear" },
          "confirmationStatus": "Declared"
        }
      ],
      "measurementPolicies": {
        "warmupTimeMs": 0,
        "defaultSettlingTimeMs": 50,
        "recommendedCalibrationPolicy": "None",
        "requiresResetBetweenTrials": false
      }
    })";
}

} // namespace

TEST_CASE("TargetProfile Validation: Rechazo de JSON Inválido", "[target_profile][validation]")
{
    TargetProfileService service;
    auto res = service.loadAndValidateProfileJson("{ unquoted_key: broken }");
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());
    CHECK(res.diagnostics[0].code == "ERR_SYNTAX_INVALID_JSON");
}

TEST_CASE("TargetProfile Validation: Rechazo Estricto de Campos Desconocidos", "[target_profile][validation]")
{
    TargetProfileService service;
    json j = json::parse(getValidBaseJson());
    j["unauthorizedRootKey"] = "malicious_or_unknown_value";

    auto res = service.loadAndValidateProfileJson(j.dump());
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());

    bool foundUnknownField = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_SCHEMA_UNKNOWN_FIELD" && d.jsonPointer == "/unauthorizedRootKey")
            foundUnknownField = true;
    }
    CHECK(foundUnknownField);
}

TEST_CASE("TargetProfile Validation: Rechazo de schemaVersion Incompatible", "[target_profile][validation]")
{
    TargetProfileService service;
    json j = json::parse(getValidBaseJson());
    j["schemaVersion"] = "2.0";

    auto res = service.loadAndValidateProfileJson(j.dump());
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());
    CHECK(res.diagnostics[0].code == "ERR_SCHEMA_VERSION_UNSUPPORTED");
    CHECK(res.diagnostics[0].jsonPointer == "/schemaVersion");
}

TEST_CASE("TargetProfile Validation: Detección de semanticId Duplicado", "[target_profile][validation]")
{
    TargetProfileService service;
    json j = json::parse(getValidBaseJson());
    // Duplicar el parámetro 'cutoff'
    j["parameters"].push_back(j["parameters"][0]);

    auto res = service.loadAndValidateProfileJson(j.dump());
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());

    bool foundDuplicate = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_SEMANTICS_DUPLICATE_ID")
            foundDuplicate = true;
    }
    CHECK(foundDuplicate);
}

TEST_CASE("TargetProfile Validation: Detección de normalizedRange Inválido", "[target_profile][validation]")
{
    TargetProfileService service;
    json j = json::parse(getValidBaseJson());
    // Invertir rango [1.0, 0.0]
    j["parameters"][0]["normalizedRange"] = { 1.0, 0.0 };

    auto res = service.loadAndValidateProfileJson(j.dump());
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());

    bool foundInvalidRange = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_SEMANTICS_INVALID_RANGE")
            foundInvalidRange = true;
    }
    CHECK(foundInvalidRange);
}

TEST_CASE("TargetProfile Validation: Detección de midiNoteRange Invertido", "[target_profile][validation]")
{
    TargetProfileService service;
    json j = json::parse(getValidBaseJson());
    // Invertir rango MIDI [120, 20]
    j["capabilities"]["midiNoteRange"] = { 120, 20 };

    auto res = service.loadAndValidateProfileJson(j.dump());
    REQUIRE_FALSE(res.isSuccess());
    REQUIRE(res.hasErrors());

    bool foundInvalidMidiRange = false;
    for (const auto& d : res.diagnostics)
    {
        if (d.code == "ERR_SEMANTICS_INVALID_RANGE" && d.jsonPointer == "/capabilities/midiNoteRange")
            foundInvalidMidiRange = true;
    }
    CHECK(foundInvalidMidiRange);
}

TEST_CASE("TargetProfile Validation: Determinismo RFC 8785 de Hash Canónico", "[target_profile][validation][rfc8785]")
{
    // Construir dos cadenas JSON con claves ordenadas en diferente orden
    std::string jsonOrderA = R"({"schemaVersion":"1.0","kind":"abd.target-profile","targetProfileId":"org.abd.a","displayName":"A","vendor":"V","targetKind":"SyntheticFixture","revision":1,"identity":{"canonicalTargetId":"a","acceptedUniqueIds":["uid"],"binaryIdentityPolicy":"not-applicable"},"capabilities":{"midiInput":true,"supportsParameterAutomation":true,"controlTransports":["InternalParameter"],"audioOutput":{"supportedChannelCounts":[2],"requiredChannelCount":2,"channelLayout":"stereo","supportedObservationLayouts":["stereo"]},"sampleRatesHz":[48000],"blockSizes":[256],"supportsPolyphony":true,"midiChannels":[1],"midiNoteRange":[0,127]},"parameters":[],"measurementPolicies":{"warmupTimeMs":0,"defaultSettlingTimeMs":0,"recommendedCalibrationPolicy":"None","requiresResetBetweenTrials":false}})";

    std::string jsonOrderB = R"({"kind":"abd.target-profile","schemaVersion":"1.0","vendor":"V","displayName":"A","targetProfileId":"org.abd.a","revision":1,"targetKind":"SyntheticFixture","measurementPolicies":{"warmupTimeMs":0,"requiresResetBetweenTrials":false,"recommendedCalibrationPolicy":"None","defaultSettlingTimeMs":0},"parameters":[],"identity":{"binaryIdentityPolicy":"not-applicable","canonicalTargetId":"a","acceptedUniqueIds":["uid"]},"capabilities":{"midiNoteRange":[0,127],"midiChannels":[1],"supportsPolyphony":true,"blockSizes":[256],"sampleRatesHz":[48000],"audioOutput":{"supportedObservationLayouts":["stereo"],"channelLayout":"stereo","requiredChannelCount":2,"supportedChannelCounts":[2]},"controlTransports":["InternalParameter"],"supportsParameterAutomation":true,"midiInput":true}})";

    std::string hashA = TargetProfileService::computeCanonicalProfileHash(jsonOrderA);
    std::string hashB = TargetProfileService::computeCanonicalProfileHash(jsonOrderB);

    // Deben coincidir exactamente bit a bit
    CHECK_FALSE(hashA.empty());
    CHECK(hashA == hashB);
}
