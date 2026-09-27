// ==============================================================================
// ABDAudioLab - HITO-10E: HardwareContractRegistry Canonical Resolution Tests (E4)
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::core;
using namespace abdaudiolab::profiling;

namespace
{

juce::File getCanonicalTargetsDir()
{
    juce::File current = juce::File::getCurrentWorkingDirectory();
    auto dir = current.getChildFile("profiles").getChildFile("targets");
    if (dir.isDirectory()) return dir;

    dir = current.getParentDirectory().getChildFile("profiles").getChildFile("targets");
    if (dir.isDirectory()) return dir;

    return {};
}

juce::File getContractsHardwareDir()
{
    juce::File current = juce::File::getCurrentWorkingDirectory();
    auto dir = current.getChildFile("contracts").getChildFile("hardware");
    if (dir.isDirectory()) return dir;

    dir = current.getParentDirectory().getChildFile("contracts").getChildFile("hardware");
    if (dir.isDirectory()) return dir;

    return {};
}

} // namespace

TEST_CASE("HITO-10E / E4 - 1. Resolucion de PRO-800 por ID canonico y alias",
          "[targetprofile][legacy][registry][resolution]")
{
    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));
    auto loadResult = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(loadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // 1. Por ID canonico
    auto resCan = registry.resolveContractById("hw-behringer-pro800-canonical");
    CHECK(resCan.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    CHECK((resCan.diagnosticCode == "INFO_CANONICAL_TARGET_PROFILE_ADAPTED" ||
           resCan.diagnosticCode == "INFO_CANONICAL_LEGACY_PARITY_CERTIFIED"));
    REQUIRE(resCan.contract.has_value());
    CHECK(resCan.contract->id == "hw-behringer-pro800-canonical");

    // 2. Por acceptedUniqueId / alias
    auto resAlias = registry.resolveContractById("behringer_pro800");
    CHECK(resAlias.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    CHECK((resAlias.diagnosticCode == "INFO_CANONICAL_TARGET_PROFILE_ADAPTED" ||
           resAlias.diagnosticCode == "INFO_CANONICAL_LEGACY_PARITY_CERTIFIED"));
    REQUIRE(resAlias.contract.has_value());
    CHECK(resAlias.contract->id == "hw-behringer-pro800-canonical");

    // Wrapper historico findContractById
    const auto* found = registry.findContractById("behringer_pro800");
    REQUIRE(found != nullptr);
    CHECK(found->id == "hw-behringer-pro800-canonical");
}

TEST_CASE("HITO-10E / E4 - 2. Resolucion de DX7 y DS-1 por ID canonico",
          "[targetprofile][legacy][registry][resolution]")
{
    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));
    auto loadResult = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(loadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // 3. DX7 por ID canonico
    auto resDx7 = registry.resolveContractById("hw-yamaha-dx7-canonical");
    CHECK(resDx7.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    CHECK((resDx7.diagnosticCode == "INFO_CANONICAL_TARGET_PROFILE_ADAPTED" ||
           resDx7.diagnosticCode == "INFO_CANONICAL_LEGACY_PARITY_CERTIFIED"));
    REQUIRE(resDx7.contract.has_value());
    CHECK(resDx7.contract->displayName == "Yamaha DX7 (Mark I)");

    // 4. BOSS DS-1 por ID canonico
    auto resDs1 = registry.resolveContractById("hw-boss-ds1-canonical");
    CHECK(resDs1.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted);
    CHECK((resDs1.diagnosticCode == "INFO_CANONICAL_TARGET_PROFILE_ADAPTED" ||
           resDs1.diagnosticCode == "INFO_CANONICAL_LEGACY_PARITY_CERTIFIED"));
    REQUIRE(resDs1.contract.has_value());
    CHECK(resDs1.contract->displayName == "BOSS DS-1 Distortion (Analogue Pedal)");
}

TEST_CASE("HITO-10E / E4 - 3. Perfiles no migrados resuelven como NativeLegacyContract",
          "[targetprofile][legacy][registry][resolution]")
{
    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));
    auto loadResult = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(loadResult.outcome == CanonicalTargetProfileLoadOutcome::Loaded);

    // 5. Casio CZ-101 no migrado
    auto resCz = registry.resolveContractById("casio_cz101");
    CHECK(resCz.source == HardwareContractResolutionSource::NativeLegacyContract);
    CHECK(resCz.diagnosticCode == "INFO_NATIVE_LEGACY_CONTRACT");
    REQUIRE(resCz.contract.has_value());
    CHECK(resCz.contract->id == "casio_cz101");

    // 6. Roland Juno-106 no migrado
    auto resJuno = registry.resolveContractById("roland_juno106");
    CHECK(resJuno.source == HardwareContractResolutionSource::NativeLegacyContract);
    CHECK(resJuno.diagnosticCode == "INFO_NATIVE_LEGACY_CONTRACT");
    REQUIRE(resJuno.contract.has_value());
    CHECK(resJuno.contract->id == "roland_juno106");

    // 7. ID inexistente
    auto resNone = registry.resolveContractById("non_existent_synth_999");
    CHECK(resNone.source == HardwareContractResolutionSource::NotFound);
    CHECK(resNone.diagnosticCode == "ERR_HARDWARE_CONTRACT_NOT_FOUND");
    CHECK_FALSE(resNone.contract.has_value());

    const auto* foundNone = registry.findContractById("non_existent_synth_999");
    CHECK(foundNone == nullptr);
}

TEST_CASE("HITO-10E / E4 - 4. Colision no certificada bloquea resolucion (Fail-Closed)",
          "[targetprofile][legacy][registry][resolution]")
{
    // 8. Crear directorio temporal con un perfil que colisiona con un contrato legacy no certificado (ej. casio_cz101)
    juce::File tempDir = juce::File::createTempFile("e4_collision_test_dir");
    tempDir.deleteFile();
    tempDir.createDirectory();

    auto mockFile = tempDir.getChildFile("casio_cz101.target.json");
    std::string mockContent = R"({
      "$schema": "https://json-schema.org/draft/2020-12/schema",
      "schemaVersion": "1.0",
      "kind": "abd.target-profile",
      "targetProfileId": "hw-casio-cz101-unproven",
      "displayName": "Casio CZ-101 Unproven",
      "vendor": "Casio",
      "targetKind": "HardwareDigital",
      "revision": 1,
      "identity": {
        "canonicalTargetId": "cz101-unproven",
        "acceptedUniqueIds": ["casio_cz101"],
        "binaryIdentityPolicy": "not-applicable"
      },
      "capabilities": {
        "midiInput": true,
        "supportsParameterAutomation": false,
        "controlTransports": ["MidiSysEx"],
        "audioOutput": {
          "supportedChannelCounts": [1],
          "requiredChannelCount": 1,
          "channelLayout": "mono",
          "supportedObservationLayouts": ["mono"]
        },
        "sampleRatesHz": [44100],
        "blockSizes": [256],
        "supportsPolyphony": true,
        "midiChannels": [1],
        "midiNoteRange": [0, 127]
      },
      "parameters": [],
      "measurementPolicies": {
        "warmupTimeMs": 10,
        "defaultSettlingTimeMs": 10,
        "recommendedCalibrationPolicy": "None",
        "requiresResetBetweenTrials": false
      },
      "transportPolicy": {
        "minimumInterMessageDelayMs": 10,
        "maximumMessagesPerSecond": 10,
        "requiresResponseAck": false,
        "responseTimeoutMs": 100,
        "retryPolicy": "none",
        "maxRetries": 0,
        "requiresExplicitConfirmation": false,
        "allowsBulkDump": false,
        "requiresVerifiedIdentity": false,
        "allowsUserConfirmedUnverifiedIdentity": true
      }
    })";
    mockFile.replaceWithText(mockContent);

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));

    // Intentar cargar colision unproven
    auto loadResult = registry.loadCanonicalTargetProfiles(tempDir);
    CHECK(loadResult.outcome == CanonicalTargetProfileLoadOutcome::CanonicalLegacyParityUnproven);
    CHECK(registry.getCanonicalAdaptedContractCount() == 0); // RegistryUnchanged

    tempDir.deleteRecursively();
}

TEST_CASE("HITO-10E / E4 - 5. Carga atomica rechaza alias duplicado o perfil invalido",
          "[targetprofile][legacy][registry][resolution]")
{
    juce::File tempDir = juce::File::createTempFile("e4_atomic_test_dir");
    tempDir.deleteFile();
    tempDir.createDirectory();

    // 9. Crear dos perfiles con el mismo alias
    auto p1 = tempDir.getChildFile("p1.target.json");
    auto p2 = tempDir.getChildFile("p2.target.json");

    std::string baseJson = R"({
      "$schema": "https://json-schema.org/draft/2020-12/schema",
      "schemaVersion": "1.0",
      "kind": "abd.target-profile",
      "targetProfileId": "ID_REPLACE",
      "displayName": "Test Synth",
      "vendor": "Test",
      "targetKind": "SyntheticFixture",
      "revision": 1,
      "identity": {
        "canonicalTargetId": "ID_REPLACE",
        "acceptedUniqueIds": ["duplicate_alias_xyz"],
        "binaryIdentityPolicy": "not-applicable"
      },
      "capabilities": {
        "midiInput": true,
        "supportsParameterAutomation": false,
        "controlTransports": ["InternalParameter"],
        "audioOutput": {
          "supportedChannelCounts": [1],
          "requiredChannelCount": 1,
          "channelLayout": "mono",
          "supportedObservationLayouts": ["mono"]
        },
        "sampleRatesHz": [44100],
        "blockSizes": [256],
        "supportsPolyphony": true,
        "midiChannels": [1],
        "midiNoteRange": [0, 127]
      },
      "parameters": [],
      "measurementPolicies": {
        "warmupTimeMs": 10,
        "defaultSettlingTimeMs": 10,
        "recommendedCalibrationPolicy": "None",
        "requiresResetBetweenTrials": false
      },
      "transportPolicy": {
        "minimumInterMessageDelayMs": 10,
        "maximumMessagesPerSecond": 10,
        "requiresResponseAck": false,
        "responseTimeoutMs": 100,
        "retryPolicy": "none",
        "maxRetries": 0,
        "requiresExplicitConfirmation": false,
        "allowsBulkDump": false,
        "requiresVerifiedIdentity": false,
        "allowsUserConfirmedUnverifiedIdentity": true
      }
    })";

    std::string s1 = baseJson;
    s1.replace(s1.find("ID_REPLACE"), 10, "profile_one");
    s1.replace(s1.find("ID_REPLACE"), 10, "profile_one");
    p1.replaceWithText(s1);

    std::string s2 = baseJson;
    s2.replace(s2.find("ID_REPLACE"), 10, "profile_two");
    s2.replace(s2.find("ID_REPLACE"), 10, "profile_two");
    p2.replaceWithText(s2);

    HardwareContractRegistry registry;
    auto resDuplicate = registry.loadCanonicalTargetProfiles(tempDir);
    CHECK(resDuplicate.outcome == CanonicalTargetProfileLoadOutcome::CanonicalAliasCollision);
    CHECK(registry.getCanonicalAdaptedContractCount() == 0); // RegistryUnchanged

    // 10 & 11. Error en uno de varios perfiles invalida todo el lote
    p2.deleteFile();
    auto pInvalid = tempDir.getChildFile("invalid_profile.target.json");
    pInvalid.replaceWithText("{ \"schemaVersion\": \"1.0\", \"kind\": \"broken\" }"); // JSON sin campos obligatorios

    auto resInvalid = registry.loadCanonicalTargetProfiles(tempDir);
    CHECK(resInvalid.outcome == CanonicalTargetProfileLoadOutcome::TargetProfileInvalid);
    CHECK(registry.getCanonicalAdaptedContractCount() == 0); // RegistryUnchanged: lote completo rechazado

    tempDir.deleteRecursively();
}

TEST_CASE("HITO-10E / E4 - 6. Idempotencia y preservacion de invariantes de transporte",
          "[targetprofile][legacy][registry][resolution]")
{
    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(getContractsHardwareDir()));

    // 12. Carga repetida idempotente
    auto load1 = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(load1.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    std::size_t initialCount = registry.getCanonicalAdaptedContractCount();

    auto load2 = registry.loadCanonicalTargetProfiles(getCanonicalTargetsDir());
    REQUIRE(load2.outcome == CanonicalTargetProfileLoadOutcome::Loaded);
    CHECK(registry.getCanonicalAdaptedContractCount() == initialCount); // No se duplican

    // 13. PRO-800 adaptado preserva CC 19
    auto resPro = registry.resolveContractById("hw-behringer-pro800-canonical");
    REQUIRE(resPro.contract.has_value());
    CHECK(resPro.contract->deviceType == "AUTOMATED_MIDI_CC");
    bool foundCutoff = false;
    for (const auto& ctrl : resPro.contract->functions[0].controls)
    {
        if (ctrl.name == "Filter Cutoff")
        {
            foundCutoff = true;
            CHECK(ctrl.controlMethod == "MIDI_CC");
            CHECK(ctrl.ccNumber == 19);
        }
    }
    CHECK(foundCutoff);

    // 14. DX7 adaptado preserva SysEx framing
    auto resDx7 = registry.resolveContractById("hw-yamaha-dx7-canonical");
    REQUIRE(resDx7.contract.has_value());
    CHECK(resDx7.contract->deviceType == "AUTOMATED_SYSEX");
    bool foundAlgorithm = false;
    for (const auto& ctrl : resDx7.contract->functions[0].controls)
    {
        if (ctrl.name == "Algorithm Select")
        {
            foundAlgorithm = true;
            CHECK(ctrl.controlMethod == "SYSEX");
            CHECK(ctrl.sysexAddress.find("F0 43") != std::string::npos);
            CHECK(ctrl.sysexAddress.find("F7") != std::string::npos);
        }
    }
    CHECK(foundAlgorithm);

    // 15. DS-1 adaptado preserva ruta manual y semantica de operador
    auto resDs1 = registry.resolveContractById("hw-boss-ds1-canonical");
    REQUIRE(resDs1.contract.has_value());
    CHECK(resDs1.contract->deviceType == "ANALOGUE_PEDAL");
    bool foundDist = false;
    for (const auto& ctrl : resDs1.contract->functions[0].controls)
    {
        if (ctrl.name == "DIST Potentiometer")
        {
            foundDist = true;
            CHECK(ctrl.controlMethod == "MANUAL");
            CHECK(ctrl.type == "Knob");
        }
    }
    CHECK(foundDist);
}
