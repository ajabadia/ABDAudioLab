#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "synth/ExternalPluginFixture.h"
#include "synth/TargetContractDiscovery.h"
#include "synth/ExperimentPlan.h"
#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "core/LabResourcePaths.h"

using namespace abdaudiolab::synth;
using namespace abdaudiolab::profiling;

namespace
{

juce::File resolveDexedBinary()
{
    auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (defaultWin.exists())
        return defaultWin;

    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("../Local/Programs/Common/VST3/Dexed.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

} // namespace

TEST_CASE("HITO-10C: TargetProfile Dexed Real Hosting and Verification", "[target_profile][external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    TargetProfileService profileService;
    const auto profileFile = abdaudiolab::core::repoResource("profiles/targets/dexed.target.json");

    REQUIRE(profileFile.existsAsFile());
    auto profileRes = profileService.loadAndValidateProfile(profileFile);
    REQUIRE(profileRes.isSuccess());
    const auto& targetProfile = profileRes.profile;

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    bool loaded = fixture.loadPluginFromDisk(dexedFile, 48000.0, 512, loadErr);
    REQUIRE(loaded);
    REQUIRE(loadErr.empty());
    REQUIRE(fixture.getPluginInstance() != nullptr);

    SECTION("1. Verificacion de identidad real y auditoria binaria")
    {
        const auto& identity = fixture.getIdentity();
        CHECK(identity.format == "VST3");
        CHECK_FALSE(identity.binaryHash.empty());

        // Auditoría binaria formal frente a la política warn-on-mismatch del TargetProfile
        auto auditRes = profileService.auditBinaryFixity(targetProfile, identity.binaryHash);
        CHECK(auditRes.passed);
        if (auditRes.isWarning)
        {
            WARN("Dexed binary hash differs from reference; warn-on-mismatch permitted continuation.");
        }
        else
        {
            CHECK(identity.binaryHash == targetProfile.identity.expectedBinarySha256);
        }
    }

    SECTION("2. Introspeccion y generacion de TargetProfileDraft desde contrato real")
    {
        TargetContract contract = fixture.discoverContract();
        REQUIRE(contract.parameters.size() > 0);

        TargetProfileDraft draft = profileService.generateDraftFromContract(contract, fixture.getIdentity().binaryHash);
        CHECK_FALSE(draft.isExecutable());
        CHECK(draft.parameters.size() == contract.parameters.size());

        // Comprobar que Cutoff se descubrió honestamente
        const auto* cutoffDraft = draft.findParameterBySuggestedSemanticId("filter_cutoff");
        REQUIRE(cutoffDraft != nullptr);
        CHECK(cutoffDraft->semanticStatus == DraftSemanticStatus::Inferred);
        CHECK(cutoffDraft->inferenceReason == "name_contains_cutoff");

        // Comprobar correspondencia con el TargetProfile formal confirmado
        const auto* cutoffProfile = targetProfile.findMappingForSemanticId("filter_cutoff");
        REQUIRE(cutoffProfile != nullptr);
        CHECK(cutoffProfile->confirmationStatus == "UserConfirmed");
    }

    SECTION("3. Resolucion declarativa de receta formal con el perfil de Dexed")
    {
        const auto recipeFile = abdaudiolab::core::repoResource("presets/profiling/quick_vcf_3pts.json");
        REQUIRE(recipeFile.existsAsFile());

        MeasurementRecipeService recipeService;
        auto recipeResult = recipeService.loadAndValidate(recipeFile);
        REQUIRE(recipeResult.isSuccess());

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.blockSize = 512;
        env.channels = 2;

        auto resolvedPlanRes = ExperimentPlanCompiler::resolveExecutionPlan(
            recipeResult.recipe,
            targetProfile,
            env
        );

        REQUIRE(resolvedPlanRes.succeeded());
        REQUIRE(resolvedPlanRes.resolvedPlan.has_value());
        const auto& resolved = *resolvedPlanRes.resolvedPlan;

        CHECK(resolved.experimentPlan.windows.size() == 9);
        CHECK_FALSE(resolved.resolvedExecutionPlanHash.empty());

        // Verificación de eventos de parámetro VST3
        bool hasParameterEvents = false;
        for (const auto& ev : resolved.experimentPlan.events)
        {
            if (ev.eventType == abdaudiolab::synth::TargetEventType::Parameter)
            {
                hasParameterEvents = true;
                CHECK((ev.parameter.normalizedParameterId == "filter_cutoff" || ev.parameter.normalizedParameterId == "vcf.cutoff"));
            }
        }
        CHECK(hasParameterEvents);
    }

    SECTION("4. Preservacion de estado y reset entre trials")
    {
        std::vector<uint8_t> baseState;
        auto getRes = fixture.getState(baseState);
        REQUIRE(getRes.succeeded);
        REQUIRE(!baseState.empty());

        // Alterar un parámetro
        auto* instance = fixture.getPluginInstance();
        auto params = instance->getParameters();
        REQUIRE(params.size() > 0);
        float origVal = params[0]->getValue();
        params[0]->setValueNotifyingHost((origVal < 0.5f) ? 0.9f : 0.1f);

        // Restaurar estado
        auto setRes = fixture.setState(baseState);
        REQUIRE(setRes.succeeded);

        std::vector<uint8_t> restoredState;
        fixture.getState(restoredState);
        CHECK(restoredState == baseState);
    }
}
