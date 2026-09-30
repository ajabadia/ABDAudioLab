#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "core/LabResourcePaths.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-10C: TargetProfile Dexed Formal Contract Validation", "[target_profile][dexed][contract]")
{
    TargetProfileService service;
    juce::File profileFile = abdaudiolab::core::canonicalTargetsDir()
                                .getChildFile("dexed.target.json");

    REQUIRE(profileFile.existsAsFile());

    REQUIRE(profileFile.existsAsFile());

    SECTION("1. Parsing y validacion estricta de schema")
    {
        auto loadResult = service.loadAndValidateProfile(profileFile);
        for (const auto& diag : loadResult.diagnostics)
        {
            INFO(diag.jsonPointer + ": [" + diag.code + "] " + diag.message);
        }
        REQUIRE(loadResult.isSuccess());
        REQUIRE_FALSE(loadResult.hasErrors());

        const auto& p = loadResult.profile;
        CHECK(p.schemaVersion == "1.0");
        CHECK(p.kind == "abd.target-profile");
        CHECK(p.targetProfileId == "vst3-dexed-canonical");
        CHECK(p.displayName == "Dexed FM Synthesizer");
        CHECK(p.vendor == "Digital Suburban");
        CHECK(p.targetKind == "PluginVST3");
        CHECK(p.revision == 1);
    }

    SECTION("2. Identidad canónica y política de fixity binaria")
    {
        auto loadResult = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadResult.isSuccess());
        const auto& p = loadResult.profile;

        CHECK(p.identity.canonicalTargetId == "vst3-dexed-3f015740-d7709eec");
        REQUIRE(p.identity.acceptedUniqueIds.size() >= 2);
        CHECK(p.identity.binaryIdentityPolicy == "warn-on-mismatch");
        CHECK(p.identity.expectedBinarySha256 == "e8b3b00a53bb0aa1ef082b0c1b5cb66bdf1af5eddf787b8f47397df6d2411a40");
    }

    SECTION("3. Capacidades de audio y transporte VST3")
    {
        auto loadResult = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadResult.isSuccess());
        const auto& p = loadResult.profile;

        CHECK(p.capabilities.midiInput == true);
        CHECK(p.capabilities.supportsParameterAutomation == true);
        REQUIRE(p.capabilities.controlTransports.size() == 1);
        CHECK(p.capabilities.controlTransports[0] == ControlTransportKind::VST3Parameter);

        CHECK(p.capabilities.audioOutput.requiredChannelCount == 2);
        CHECK(p.capabilities.audioOutput.channelLayout == "stereo");
        REQUIRE(p.capabilities.sampleRatesHz.size() == 3);
        REQUIRE(p.capabilities.blockSizes.size() >= 4);
    }

    SECTION("4. Mapeo tipado de parámetros VST3ParameterIdentifier")
    {
        auto loadResult = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadResult.isSuccess());
        const auto& p = loadResult.profile;

        REQUIRE(p.parameters.size() >= 3);

        const auto* cutoff = p.findMappingForSemanticId("filter_cutoff");
        REQUIRE(cutoff != nullptr);
        CHECK(cutoff->displayName == "Cutoff Frequency");
        CHECK(cutoff->getTransportKind() == ControlTransportKind::VST3Parameter);
        REQUIRE(std::holds_alternative<Vst3ParameterIdentifier>(cutoff->technicalIdentifier));
        const auto& vstCutoff = std::get<Vst3ParameterIdentifier>(cutoff->technicalIdentifier);
        CHECK(vstCutoff.parameterId == "Cutoff");
        CHECK(cutoff->confirmationStatus == "UserConfirmed");

        const auto* res = p.findMappingForSemanticId("filter_resonance");
        REQUIRE(res != nullptr);
        CHECK(res->displayName == "Resonance");
        CHECK(res->getTransportKind() == ControlTransportKind::VST3Parameter);
        REQUIRE(std::holds_alternative<Vst3ParameterIdentifier>(res->technicalIdentifier));
        const auto& vstRes = std::get<Vst3ParameterIdentifier>(res->technicalIdentifier);
        CHECK(vstRes.parameterId == "Resonance");
        CHECK(res->confirmationStatus == "UserConfirmed");

        const auto* vol = p.findMappingForSemanticId("master_volume");
        REQUIRE(vol != nullptr);
        CHECK(vol->displayName == "Master Volume");
        CHECK(vol->getTransportKind() == ControlTransportKind::VST3Parameter);
        REQUIRE(std::holds_alternative<Vst3ParameterIdentifier>(vol->technicalIdentifier));
        const auto& vstVol = std::get<Vst3ParameterIdentifier>(vol->technicalIdentifier);
        CHECK(vstVol.parameterId == "Master");
        CHECK(vol->confirmationStatus == "UserConfirmed");
    }

    SECTION("5. Estabilidad y reproducibilidad del hash canónico RFC 8785")
    {
        auto r1 = service.loadAndValidateProfile(profileFile);
        auto r2 = service.loadAndValidateProfile(profileFile);
        REQUIRE(r1.isSuccess());
        REQUIRE(r2.isSuccess());

        CHECK_FALSE(r1.canonicalProfileHash.empty());
        CHECK(r1.canonicalProfileHash == r2.canonicalProfileHash);
    }

    SECTION("6. Resolucion declarativa de receta con Dexed TargetProfile")
    {
        auto loadResult = service.loadAndValidateProfile(profileFile);
        REQUIRE(loadResult.isSuccess());

        juce::File recipeFile = abdaudiolab::core::profilingPresetsDir()
                                    .getChildFile("quick_vcf_3pts.json");

        REQUIRE(recipeFile.existsAsFile());

        MeasurementRecipeService recipeService;
        auto recipeResult = recipeService.loadAndValidate(recipeFile);
        REQUIRE(recipeResult.isSuccess());

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.blockSize = 512;
        env.channels = 2;

        auto resPlan = ExperimentPlanCompiler::resolveExecutionPlan(
            recipeResult.recipe,
            loadResult.profile,
            env
        );

        CHECK(resPlan.diagnostics.empty());
        REQUIRE(resPlan.succeeded());
        REQUIRE(resPlan.resolvedPlan.has_value());

        const auto& resolved = *resPlan.resolvedPlan;
        CHECK(resolved.environment.sampleRate == Catch::Approx(48000.0));
        CHECK(resolved.experimentPlan.windows.size() == 9);
        CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());
    }
}
