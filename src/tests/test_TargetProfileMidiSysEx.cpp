#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-10D1: TargetProfile MIDI SysEx Validation, Security Policy and Formatting", "[target_profile][hardware][sysex]")
{
    TargetProfileService service;
    juce::File profileFile = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("profiles/targets/yamaha_dx7.target.json");
    if (!profileFile.existsAsFile())
        profileFile = juce::File("D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/yamaha_dx7.target.json");

    REQUIRE(profileFile.existsAsFile());

    SECTION("1. Carga valida del perfil formal Yamaha DX7")
    {
        auto loadRes = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadRes.isSuccess());
        const auto& profile = loadRes.profile;

        CHECK(profile.targetProfileId == "hw-yamaha-dx7-canonical");
        CHECK(profile.displayName == "Yamaha DX7 (Mark I)");
        CHECK(profile.vendor == "Yamaha");
        CHECK(profile.targetKind == "HardwareDigital");

        REQUIRE(profile.capabilities.controlTransports.size() == 1);
        CHECK(profile.capabilities.controlTransports[0] == ControlTransportKind::MidiSysEx);

        const auto* op1 = profile.findMappingForSemanticId("operator_1_level");
        REQUIRE(op1 != nullptr);
        CHECK(op1->displayName == "Op 1 Output Level");
        CHECK(op1->confirmationStatus == "UserConfirmed");
        REQUIRE(std::holds_alternative<MidiSysExIdentifier>(op1->technicalIdentifier));

        const auto& sysexId = std::get<MidiSysExIdentifier>(op1->technicalIdentifier);
        CHECK(sysexId.messageTemplate == "F0 43 {deviceId} 09 10 {value7bit} F7");
        CHECK(sysexId.manufacturerId == "43");
        CHECK(sysexId.valueEncoding == "7bit");
        CHECK(sysexId.checksumPolicy == "none");
        CHECK_FALSE(sysexId.requiresExplicitConfirmation);
    }

    SECTION("2. Politica de seguridad: Delimitadores F0/F7 y bytes legales")
    {
        // 2a. Sin F0
        std::string jsonNoF0 = R"({
            "schemaVersion": "1.0", "kind": "abd.target-profile", "targetProfileId": "hw-bad-sysex",
            "displayName": "Bad SysEx", "vendor": "Test", "targetKind": "HardwareDigital", "revision": 1,
            "identity": { "canonicalTargetId": "bad", "acceptedUniqueIds": ["b"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": true, "supportsParameterAutomation": true, "controlTransports": ["MidiSysEx"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000], "blockSizes": [256], "supportsPolyphony": true, "midiChannels": [1], "midiNoteRange": [0, 127]
            },
            "parameters": [{
                "semanticId": "p1", "displayName": "P1",
                "technicalIdentifier": { "kind": "MidiSysEx", "messageTemplate": "43 00 09 10 7F F7" },
                "valueType": "continuous", "normalizedRange": [0.0, 1.0], "mappingCurve": { "kind": "linear" }, "confirmationStatus": "UserConfirmed"
            }],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 50, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";
        auto resNoF0 = service.loadAndValidateProfileJson(jsonNoF0);
        CHECK_FALSE(resNoF0.isSuccess());
        bool hasDelimErr = false;
        for (const auto& d : resNoF0.diagnostics)
            if (d.code == "ERR_SYSEX_INVALID_DELIMITERS") hasDelimErr = true;
        CHECK(hasDelimErr);

        // 2b. Byte ilegal > 0x7F dentro de la carga util
        std::string jsonIllegalByte = R"({
            "schemaVersion": "1.0", "kind": "abd.target-profile", "targetProfileId": "hw-bad-sysex",
            "displayName": "Bad SysEx", "vendor": "Test", "targetKind": "HardwareDigital", "revision": 1,
            "identity": { "canonicalTargetId": "bad", "acceptedUniqueIds": ["b"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": true, "supportsParameterAutomation": true, "controlTransports": ["MidiSysEx"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000], "blockSizes": [256], "supportsPolyphony": true, "midiChannels": [1], "midiNoteRange": [0, 127]
            },
            "parameters": [{
                "semanticId": "p1", "displayName": "P1",
                "technicalIdentifier": { "kind": "MidiSysEx", "messageTemplate": "F0 43 90 09 10 {value7bit} F7" },
                "valueType": "continuous", "normalizedRange": [0.0, 1.0], "mappingCurve": { "kind": "linear" }, "confirmationStatus": "UserConfirmed"
            }],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 50, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";
        auto resIllegal = service.loadAndValidateProfileJson(jsonIllegalByte);
        CHECK_FALSE(resIllegal.isSuccess());
        bool hasStatusByteErr = false;
        for (const auto& d : resIllegal.diagnostics)
            if (d.code == "ERR_SYSEX_ILLEGAL_STATUS_BYTE") hasStatusByteErr = true;
        CHECK(hasStatusByteErr);

        // 2c. Token desconocido
        std::string jsonUnknownToken = R"({
            "schemaVersion": "1.0", "kind": "abd.target-profile", "targetProfileId": "hw-bad-sysex",
            "displayName": "Bad SysEx", "vendor": "Test", "targetKind": "HardwareDigital", "revision": 1,
            "identity": { "canonicalTargetId": "bad", "acceptedUniqueIds": ["b"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": true, "supportsParameterAutomation": true, "controlTransports": ["MidiSysEx"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000], "blockSizes": [256], "supportsPolyphony": true, "midiChannels": [1], "midiNoteRange": [0, 127]
            },
            "parameters": [{
                "semanticId": "p1", "displayName": "P1",
                "technicalIdentifier": { "kind": "MidiSysEx", "messageTemplate": "F0 43 {maliciousCommand} 09 F7" },
                "valueType": "continuous", "normalizedRange": [0.0, 1.0], "mappingCurve": { "kind": "linear" }, "confirmationStatus": "UserConfirmed"
            }],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 50, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";
        auto resToken = service.loadAndValidateProfileJson(jsonUnknownToken);
        CHECK_FALSE(resToken.isSuccess());
        bool hasTokenErr = false;
        for (const auto& d : resToken.diagnostics)
            if (d.code == "ERR_SYSEX_UNKNOWN_TOKEN") hasTokenErr = true;
        CHECK(hasTokenErr);
    }

    SECTION("3. Formateo y codificacion hermetica de mensajes SysEx")
    {
        MidiSysExIdentifier dx7Id;
        dx7Id.messageTemplate = "F0 43 {deviceId} 09 10 {value7bit} F7";
        dx7Id.valueEncoding = "7bit";

        // Normalizado 0.0 -> 0x00
        auto msg0 = TargetProfileService::formatSysExMessage(dx7Id, 0, 0.0);
        std::vector<uint8_t> expected0 = { 0xF0, 0x43, 0x00, 0x09, 0x10, 0x00, 0xF7 };
        CHECK(msg0 == expected0);

        // Normalizado 1.0 -> 0x7F (127)
        auto msg1 = TargetProfileService::formatSysExMessage(dx7Id, 2, 1.0);
        std::vector<uint8_t> expected1 = { 0xF0, 0x43, 0x02, 0x09, 0x10, 0x7F, 0xF7 };
        CHECK(msg1 == expected1);

        // Formateo con Nibbles
        MidiSysExIdentifier nibbleId;
        nibbleId.messageTemplate = "F0 41 {deviceId} 20 {valuenibblemsb} {valuenibblelsb} F7";
        auto msgNibble = TargetProfileService::formatSysExMessage(nibbleId, 1, 0.5); // 0.5 * 127 = 64 (0x40) -> MSB=4, LSB=0
        std::vector<uint8_t> expectedNibble = { 0xF0, 0x41, 0x01, 0x20, 0x04, 0x00, 0xF7 };
        CHECK(msgNibble == expectedNibble);

        // Formateo con Checksum Yamaha
        MidiSysExIdentifier checksumId;
        checksumId.messageTemplate = "F0 43 00 09 10 20 {checksum} F7";
        checksumId.checksumPolicy = "yamaha-dx7";
        auto msgYamaha = TargetProfileService::formatSysExMessage(checksumId, 0, 0.0);
        REQUIRE(msgYamaha.size() == 8);
        CHECK(msgYamaha[0] == 0xF0);
        CHECK(msgYamaha[7] == 0xF7);
        // Suma: 0x10 + 0x20 = 0x30. Checksum = (-0x30) & 0x7F = 0x50
        CHECK(msgYamaha[6] == 0x50);
    }
}
