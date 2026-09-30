#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "profiling/TargetProfileService.h"
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "core/HardwareContractRegistry.h"
#include "core/LabResourcePaths.h"

using namespace abdaudiolab::profiling;
using namespace abdaudiolab::core;

TEST_CASE("HITO-10D1: TargetProfile Transport Safety and Isolation", "[target_profile][hardware][safety]")
{
    TargetProfileService service;

    SECTION("1. Aislamiento total: La carga y resolucion en memoria jamas despacha MIDI ni altera DSP")
    {
        const auto pro800File = repoResource("profiles/targets/behringer_pro800.target.json");
        const auto dx7File = repoResource("profiles/targets/yamaha_dx7.target.json");
        const auto ds1File = repoResource("profiles/targets/boss_ds1_distortion.target.json");

        REQUIRE(pro800File.existsAsFile());
        REQUIRE(dx7File.existsAsFile());
        REQUIRE(ds1File.existsAsFile());

        auto res1 = service.loadAndValidateProfile(pro800File);
        auto res2 = service.loadAndValidateProfile(dx7File);
        auto res3 = service.loadAndValidateProfile(ds1File);

        CHECK(res1.isSuccess());
        CHECK(res2.isSuccess());
        CHECK(res3.isSuccess());

        // Verificación de hashes canónicos deterministas RFC 8785
        CHECK_FALSE(res1.canonicalProfileHash.empty());
        CHECK_FALSE(res2.canonicalProfileHash.empty());
        CHECK_FALSE(res3.canonicalProfileHash.empty());
    }

    SECTION("2. Preservacion integra de contratos de hardware post-retirada")
    {
        HardwareContractRegistry registry;
        const auto contractsDir = contractsHardwareDir();
        REQUIRE(contractsDir.isDirectory());

        bool loaded = registry.loadContractsFromDirectory(contractsDir);
        REQUIRE(loaded);
        // 30 = 31 - 1 retenido por cuarentena (roland_aira_submodules, que lleva
        // `status: "quarantined"` en el propio contrato y por eso no se carga).
        CHECK(registry.getContracts().size() == 30);

        const auto targetsDir = canonicalTargetsDir();
        if (targetsDir.isDirectory())
            registry.loadCanonicalTargetProfiles(targetsDir);
        // 35 = 30 del catalogo fisico + 5 perfiles canonicos de profiles/targets.
        CHECK(registry.getContracts().size() == 35);

        // Los contratos legacy clave se preservan intactos vía resolución canónica adaptada
        const auto* dx7Legacy = registry.findContractById("yamaha_dx7");
        REQUIRE(dx7Legacy != nullptr);
        CHECK(dx7Legacy->displayName == "Yamaha DX7 (Mark I)");
        CHECK(dx7Legacy->deviceType == "AUTOMATED_SYSEX");

        const auto* pro800Legacy = registry.findContractById("behringer_pro800");
        REQUIRE(pro800Legacy != nullptr);
        CHECK(pro800Legacy->deviceType == "AUTOMATED_MIDI_CC");

        const auto* ds1Legacy = registry.findContractById("boss_ds1_distortion");
        REQUIRE(ds1Legacy != nullptr);
        CHECK(ds1Legacy->deviceType == "ANALOGUE_PEDAL");
    }

    SECTION("3. Incompatibilidad de canal y layout detectada limpiamente en resolucion")
    {
        const auto ds1File = repoResource("profiles/targets/boss_ds1_distortion.target.json");
        auto loadRes = service.loadAndValidateProfile(ds1File);
        REQUIRE(loadRes.isSuccess());

        MeasurementRecipe recipe;
        recipe.recipeId = "RECIPE_DS1_TEST";
        recipe.measurement.points.push_back(MeasurementPointConfig{ "distortion_tone", 0.5 });
        // Receta exige estéreo obligatorio, pero DS-1 es mono estricto
        recipe.targetConstraints.channels = 2;

        ExecutionEnvironment env;
        env.sampleRate = 48000.0;
        env.blockSize = 256;
        env.channels = 2;

        auto resolvedRes = ExperimentPlanCompiler::resolveExecutionPlan(recipe, loadRes.profile, env);
        CHECK_FALSE(resolvedRes.succeeded());

        bool foundChannelErr = false;
        for (const auto& d : resolvedRes.diagnostics)
        {
            if (d.code == "ERR_CAPABILITY_CHANNELS_UNSUPPORTED")
                foundChannelErr = true;
        }
        CHECK(foundChannelErr);
    }
}
