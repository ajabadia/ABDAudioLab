#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-10D1: TargetProfile Manual Operator (BOSS DS-1 Distortion)", "[target_profile][hardware][manual_operator]")
{
    TargetProfileService service;
    juce::File profileFile = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("profiles/targets/boss_ds1_distortion.target.json");
    if (!profileFile.existsAsFile())
        profileFile = juce::File("D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/boss_ds1_distortion.target.json");

    REQUIRE(profileFile.existsAsFile());

    SECTION("1. Carga valida del perfil formal BOSS DS-1 (HardwareAnalogue)")
    {
        auto loadRes = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadRes.isSuccess());
        const auto& profile = loadRes.profile;

        CHECK(profile.targetProfileId == "hw-boss-ds1-canonical");
        CHECK(profile.displayName == "BOSS DS-1 Distortion (Analogue Pedal)");
        CHECK(profile.vendor == "BOSS");
        CHECK(profile.targetKind == "HardwareAnalogue");

        // Canal mono
        CHECK(profile.capabilities.audioOutput.requiredChannelCount == 1);
        CHECK(profile.capabilities.audioOutput.channelLayout == "mono");

        REQUIRE(profile.capabilities.controlTransports.size() == 1);
        CHECK(profile.capabilities.controlTransports[0] == ControlTransportKind::ManualOperator);

        // Politica de reposo humano >= 500 ms
        CHECK(profile.measurementPolicies.defaultSettlingTimeMs >= 500);

        // Parametros manuales guiados
        const auto* tone = profile.findMappingForSemanticId("distortion_tone");
        REQUIRE(tone != nullptr);
        CHECK(tone->displayName == "TONE Potentiometer");
        CHECK(tone->confirmationStatus == "UserConfirmed");
        REQUIRE(std::holds_alternative<ManualOperatorIdentifier>(tone->technicalIdentifier));

        const auto& manualId = std::get<ManualOperatorIdentifier>(tone->technicalIdentifier);
        CHECK(manualId.instructionId == "adjust_tone_pot");
        CHECK(manualId.controlWidget == "Knob");
        CHECK_FALSE(manualId.confirmationPrompt.empty());
    }

    SECTION("2. Rechazo de instructionId vacio")
    {
        std::string jsonEmptyInstruction = R"({
            "schemaVersion": "1.0", "kind": "abd.target-profile", "targetProfileId": "hw-bad-manual",
            "displayName": "Bad Manual", "vendor": "Test", "targetKind": "HardwareAnalogue", "revision": 1,
            "identity": { "canonicalTargetId": "bad", "acceptedUniqueIds": ["b"], "binaryIdentityPolicy": "not-applicable" },
            "capabilities": {
                "midiInput": false, "supportsParameterAutomation": false, "controlTransports": ["ManualOperator"],
                "audioOutput": { "supportedChannelCounts": [1], "requiredChannelCount": 1, "channelLayout": "mono", "supportedObservationLayouts": ["mono"] },
                "sampleRatesHz": [48000], "blockSizes": [256], "supportsPolyphony": false, "midiChannels": [1], "midiNoteRange": [0, 127]
            },
            "parameters": [{
                "semanticId": "p1", "displayName": "P1",
                "technicalIdentifier": { "kind": "ManualOperator", "instructionId": "", "confirmationPrompt": "Prompt" },
                "valueType": "continuous", "normalizedRange": [0.0, 1.0], "mappingCurve": { "kind": "linear" }, "confirmationStatus": "UserConfirmed"
            }],
            "measurementPolicies": { "warmupTimeMs": 0, "defaultSettlingTimeMs": 500, "recommendedCalibrationPolicy": "None", "requiresResetBetweenTrials": false }
        })";

        auto resEmpty = service.loadAndValidateProfileJson(jsonEmptyInstruction);
        CHECK_FALSE(resEmpty.isSuccess());
        bool hasMissingErr = false;
        for (const auto& d : resEmpty.diagnostics)
            if (d.code == "ERR_SCHEMA_MISSING_REQUIRED_FIELD" && d.jsonPointer.find("instructionId") != std::string::npos)
                hasMissingErr = true;
        CHECK(hasMissingErr);
    }

    SECTION("3. Resolucion hermetica de receta manual y generacion de tarjetas de operador")
    {
        auto loadRes = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadRes.isSuccess());

        MeasurementRecipe recipe;
        recipe.recipeId = "RECIPE_DS1_TONE_TEST";
        recipe.displayName = "DS-1 Tone Potentiometer Characterization";
        recipe.assistanceLevel = AssistanceLevel::Quick;
        recipe.measurement.points.push_back(MeasurementPointConfig{ "distortion_tone", 0.5 });
        recipe.targetConstraints.channels = 1;
        recipe.targetConstraints.requiredCapabilities = { "ManualOperator" };

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.blockSize = 256;
        env.channels = 1;
        env.discoveredCapabilities = { "ManualOperator" };

        auto resolvedRes = ExperimentPlanCompiler::resolveExecutionPlan(recipe, loadRes.profile, env);
        REQUIRE(resolvedRes.succeeded());
        REQUIRE(resolvedRes.resolvedPlan.has_value());

        const auto& plan = resolvedRes.resolvedPlan->experimentPlan;
        REQUIRE(!plan.events.empty());
        bool foundManualEvent = false;
        for (const auto& ev : plan.events)
        {
            if (ev.eventType == abdaudiolab::synth::TargetEventType::Parameter)
            {
                CHECK(ev.parameter.normalizedParameterId == "distortion_tone");
                CHECK(ev.parameter.nativeParameterId == "adjust_tone_pot");
                CHECK(ev.parameter.transportAccuracy == abdaudiolab::synth::TransportAccuracy::BestEffort);
                foundManualEvent = true;
            }
        }
        CHECK(foundManualEvent);

        // Generacion de sesión de perfilado para tarjetas de operador
        abdaudiolab::gui::session::TargetSelectionState targetState;
        targetState.targetName = loadRes.profile.displayName;
        targetState.manufacturer = loadRes.profile.vendor;
        targetState.kind = abdaudiolab::gui::session::TargetKind::HardwareAnalogue;

        auto session = ExperimentPlanCompiler::createProfilingSession(*resolvedRes.resolvedPlan, targetState, &loadRes.profile);
        CHECK(session.getMetadata().operatorMode == "MANUAL_OPERATOR");
        CHECK(session.getMetadata().hardwareName == "BOSS DS-1 Distortion (Analogue Pedal)");

        const auto& testCases = session.getTestCases();
        REQUIRE(!testCases.empty());
        for (const auto& tc : testCases)
        {
            CHECK(tc.excitationMode == abdaudiolab::core::ExcitationMode::ManualCapture);
            CHECK(tc.presetRecipe.recipeType == "MANUAL_PATCH");
            CHECK(tc.stabilizationWaitMs >= 500.0);
            REQUIRE(!tc.parameterSteps.empty());
            const auto& step = tc.parameterSteps[0];
            CHECK(step.controlType == "Knob");
            CHECK(step.id == "adjust_tone_pot");
        }
    }
}
