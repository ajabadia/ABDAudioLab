#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "core/LabResourcePaths.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-10D1: TargetProfile MIDI CC Validation and Resolution", "[target_profile][hardware][midi_cc]")
{
    TargetProfileService service;
    juce::File profileFile = abdaudiolab::core::canonicalTargetsDir()
                                .getChildFile("behringer_pro800.target.json");

    REQUIRE(profileFile.existsAsFile());

    SECTION("1. Carga valida del perfil formal Behringer PRO-800")
    {
        auto loadRes = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadRes.isSuccess());
        const auto& profile = loadRes.profile;

        CHECK(profile.targetProfileId == "hw-behringer-pro800-canonical");
        CHECK(profile.displayName == "Behringer PRO-800");
        CHECK(profile.vendor == "Behringer");
        CHECK(profile.targetKind == "HardwareDigital");
        CHECK(profile.identity.canonicalTargetId == "hw-behringer-pro800");

        REQUIRE(profile.capabilities.controlTransports.size() == 1);
        CHECK(profile.capabilities.controlTransports[0] == ControlTransportKind::MidiContinuousController);

        // Parametros confirmados
        const auto* cutoff = profile.findMappingForSemanticId("filter_cutoff");
        REQUIRE(cutoff != nullptr);
        CHECK(cutoff->displayName == "Filter Cutoff");
        CHECK(cutoff->confirmationStatus == "UserConfirmed");
        REQUIRE(std::holds_alternative<MidiCcIdentifier>(cutoff->technicalIdentifier));

        const auto& ccId = std::get<MidiCcIdentifier>(cutoff->technicalIdentifier);
        CHECK(ccId.channel == 1);
        CHECK(ccId.controllerNumber == 19);

        const auto* res = profile.findMappingForSemanticId("filter_resonance");
        REQUIRE(res != nullptr);
        REQUIRE(std::holds_alternative<MidiCcIdentifier>(res->technicalIdentifier));
        CHECK(std::get<MidiCcIdentifier>(res->technicalIdentifier).controllerNumber == 21);
    }

    SECTION("2. Rechazo estricto de canal y controller number fuera de rango")
    {
        std::string jsonInvalidChannel = R"({
            "schemaVersion": "1.0",
            "kind": "abd.target-profile",
            "targetProfileId": "hw-invalid-cc",
            "displayName": "Invalid CC",
            "vendor": "Test",
            "targetKind": "HardwareDigital",
            "revision": 1,
            "identity": { "canonicalTargetId": "inv-cc", "acceptedUniqueIds": ["inv"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": true,
                "supportsParameterAutomation": true,
                "controlTransports": ["MidiContinuousController"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000],
                "blockSizes": [256],
                "supportsPolyphony": true,
                "midiChannels": [1],
                "midiNoteRange": [0, 127]
            },
            "parameters": [
                {
                    "semanticId": "filter_cutoff",
                    "displayName": "Cutoff",
                    "technicalIdentifier": {
                        "kind": "MidiContinuousController",
                        "channel": 17,
                        "controllerNumber": 19
                    },
                    "valueType": "continuous",
                    "normalizedRange": [0.0, 1.0],
                    "mappingCurve": { "kind": "linear" },
                    "confirmationStatus": "UserConfirmed"
                }
            ],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 50, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";

        auto resChannel = service.loadAndValidateProfileJson(jsonInvalidChannel);
        CHECK_FALSE(resChannel.isSuccess());
        bool hasRangeError = false;
        for (const auto& d : resChannel.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_RANGE" && d.jsonPointer.find("channel") != std::string::npos)
                hasRangeError = true;
        CHECK(hasRangeError);

        std::string jsonInvalidCC = R"({
            "schemaVersion": "1.0",
            "kind": "abd.target-profile",
            "targetProfileId": "hw-invalid-cc",
            "displayName": "Invalid CC",
            "vendor": "Test",
            "targetKind": "HardwareDigital",
            "revision": 1,
            "identity": { "canonicalTargetId": "inv-cc", "acceptedUniqueIds": ["inv"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": true,
                "supportsParameterAutomation": true,
                "controlTransports": ["MidiContinuousController"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000],
                "blockSizes": [256],
                "supportsPolyphony": true,
                "midiChannels": [1],
                "midiNoteRange": [0, 127]
            },
            "parameters": [
                {
                    "semanticId": "filter_cutoff",
                    "displayName": "Cutoff",
                    "technicalIdentifier": {
                        "kind": "MidiContinuousController",
                        "channel": 1,
                        "controllerNumber": 128
                    },
                    "valueType": "continuous",
                    "normalizedRange": [0.0, 1.0],
                    "mappingCurve": { "kind": "linear" },
                    "confirmationStatus": "UserConfirmed"
                }
            ],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 50, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";

        auto resCC = service.loadAndValidateProfileJson(jsonInvalidCC);
        CHECK_FALSE(resCC.isSuccess());
        bool hasCcRangeError = false;
        for (const auto& d : resCC.diagnostics)
            if (d.code == "ERR_SEMANTICS_INVALID_RANGE" && d.jsonPointer.find("controllerNumber") != std::string::npos)
                hasCcRangeError = true;
        CHECK(hasCcRangeError);
    }

    SECTION("3. Resolucion hermetica de receta y enriquecimiento de eventos CC")
    {
        auto loadRes = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadRes.isSuccess());

        juce::File recipeFile = abdaudiolab::core::profilingPresetsDir()
                                    .getChildFile("quick_vcf_3pts.json");
        REQUIRE(recipeFile.existsAsFile());

        MeasurementRecipeService recipeService;
        auto recLoad = recipeService.loadAndValidate(recipeFile);
        REQUIRE(recLoad.isSuccess());

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.blockSize = 256;
        env.channels = 1;

        auto resolvedRes = ExperimentPlanCompiler::resolveExecutionPlan(recLoad.recipe, loadRes.profile, env);
        REQUIRE(resolvedRes.succeeded());
        REQUIRE(resolvedRes.resolvedPlan.has_value());

        const auto& plan = resolvedRes.resolvedPlan->experimentPlan;
        bool checkedCcEvent = false;
        for (const auto& ev : plan.events)
        {
            if (ev.eventType == abdaudiolab::synth::TargetEventType::Parameter)
            {
                CHECK((ev.parameter.normalizedParameterId == "filter_cutoff" || ev.parameter.normalizedParameterId == "vcf.cutoff"));
                CHECK(ev.parameter.nativeParameterId == "CC_19");
                CHECK(ev.parameter.transportAccuracy == abdaudiolab::synth::TransportAccuracy::Timestamped);
                checkedCcEvent = true;
            }
        }
        CHECK(checkedCcEvent);

        // Generacion de ProfilingSession enriquecida con TargetProfile
        abdaudiolab::gui::session::TargetSelectionState targetState;
        targetState.targetName = loadRes.profile.displayName;
        targetState.manufacturer = loadRes.profile.vendor;

        auto session = ExperimentPlanCompiler::createProfilingSession(*resolvedRes.resolvedPlan, targetState, &loadRes.profile);
        CHECK(session.getMetadata().operatorMode == "AUTOMATED_MIDI_CC");
        CHECK(session.getMetadata().hardwareName == "Behringer PRO-800");

        const auto& testCases = session.getTestCases();
        REQUIRE(!testCases.empty());
        for (const auto& tc : testCases)
        {
            REQUIRE(!tc.parameterSteps.empty());
            const auto& step = tc.parameterSteps[0];
            CHECK(step.paramIndex == 19);
            CHECK(step.controlType == "MidiCC");
            CHECK(step.rawValue >= 0);
            CHECK(step.rawValue <= 127);
        }
    }
}
