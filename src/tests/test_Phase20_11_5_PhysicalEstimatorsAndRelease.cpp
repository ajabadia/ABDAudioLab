/**
 * @file test_Phase20_11_5_PhysicalEstimatorsAndRelease.cpp
 * @brief Test suite for Phase 20.11.5: Bessel carrier null physical beta estimator,
 *        keyboard scaling segregation and deterministic batch manifest export.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "measurement/DexedParametricCampaignContracts.h"
#include "measurement/DexedParametricCampaignCoordinator.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::measurement;
using Catch::Approx;

TEST_CASE("Phase 20.11.5: Carrier Null Bessel J0 physical beta estimator", "[physical_estimators]")
{
    CarrierNullEstimationConfig config;
    config.carrierNullThresholdDb = 24.0;
    config.carrierNullSearchToleranceDb = 3.0;
    config.targetNullOrder = 1;
    config.baselineCarrierDbfs = -6.0;

    SECTION("Supresion de 24 dB sin minimo local produce not_estimated con suppression_without_local_null")
    {
        // Carrier levels monotonically decrease beyond 24 dB of suppression (-6 to -32)
        // without bouncing back up (no 3-point local minimum where k <= k-1 and k <= k+1)
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 25, -12.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 50, -20.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 75, -31.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 99, -35.0, true, 261.63, true, false, false, false }); // Monotonic decrease

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "not_estimated");
        REQUIRE(obs.method == BetaEstimationMethod::CarrierNullBesselJ0);
        REQUIRE(obs.reason == "suppression_without_local_null");
        REQUIRE(obs.estimatedBeta == 0.0);
        REQUIRE_FALSE(obs.betaEstimatedFromObservedNull);
    }

    SECTION("Minimo local valido con supresion suficiente detecta el primer nulo de Bessel con beta ~ 2.4048")
    {
        // Baseline: -6 dBFS
        // Trough at level 78: -32.5 dBFS (suppression = 26.5 dB >= 24 dB threshold)
        // Levels: 70 -> -18 dB, 78 -> -32.5 dB, 85 -> -20 dB (clear 3-point trough)
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 50, -12.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 70, -18.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 78, -32.5, true, 261.63, true, false, false, false }); // Null order 1
        sweep.push_back({ 85, -20.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 99, -15.0, true, 261.63, true, false, false, false });

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "estimated");
        REQUIRE(obs.method == BetaEstimationMethod::CarrierNullBesselJ0);
        REQUIRE(obs.betaEstimatedFromObservedNull == true);
        REQUIRE(obs.nullOrder == 1);
        REQUIRE(obs.controlValueAtNull == 78);
        REQUIRE(obs.estimatedBeta == Approx(2.4048255577).epsilon(1e-5));
        REQUIRE(obs.carrierSuppressionDb == Approx(26.5));
        REQUIRE(obs.carrierLevelBeforeDbfs == Approx(-18.0));
        REQUIRE(obs.carrierLevelAtNullDbfs == Approx(-32.5));
        REQUIRE(obs.carrierLevelAfterDbfs == Approx(-20.0));
        REQUIRE(obs.nullConfidence > 0.8);
        REQUIRE(obs.reason.empty());

        // Verify 3-point neighborhood persistence for offline audit
        REQUIRE(obs.localNeighborhood.controlBefore == Approx(70.0));
        REQUIRE(obs.localNeighborhood.carrierBeforeDbfs == Approx(-18.0));
        REQUIRE(obs.localNeighborhood.controlAtNull == Approx(78.0));
        REQUIRE(obs.localNeighborhood.carrierAtNullDbfs == Approx(-32.5));
        REQUIRE(obs.localNeighborhood.controlAfter == Approx(85.0));
        REQUIRE(obs.localNeighborhood.carrierAfterDbfs == Approx(-20.0));

        // Verify JSON serialization conformity
        auto j = obs.toJson();
        REQUIRE(j["measurand"] == "modulation_index_beta");
        REQUIRE(j["referenceModel"] == "bessel_J0_carrier_null");
        REQUIRE(j["betaStatus"] == "estimated");
        REQUIRE(j["betaMethod"] == "carrier_null_bessel_j0");
        REQUIRE(j["estimatedBeta"].get<double>() == Approx(2.4048255577).epsilon(1e-5));
        REQUIRE(j["nullControlValue"] == 78);
        REQUIRE(j["nullOrder"] == 1);
        REQUIRE(j["betaEstimatedFromObservedNull"] == true);
        REQUIRE(j["carrierNullThresholdDb"] == 24.0);
        REQUIRE(j.contains("localNeighborhood"));
        REQUIRE(j["localNeighborhood"]["carrierAtNullDbfs"].get<double>() == Approx(-32.5));
    }

    SECTION("Minimo local pero con bandas laterales ausentes produce not_estimated")
    {
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 70, -18.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 78, -32.5, false, 261.63, true, false, false, false }); // Sidebands NOT observable
        sweep.push_back({ 85, -20.0, true, 261.63, true, false, false, false });

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "not_estimated");
        REQUIRE(obs.reason == "sidebands_not_observable");
        REQUIRE(obs.estimatedBeta == 0.0);
    }

    SECTION("Frecuencia moduladora (FM) no observable produce not_estimated")
    {
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 70, -18.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 78, -32.5, true, 0.0, true, false, false, false }); // fmHz == 0.0
        sweep.push_back({ 85, -20.0, true, 261.63, true, false, false, false });

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "not_estimated");
        REQUIRE(obs.reason == "fm_not_observable");
        REQUIRE(obs.estimatedBeta == 0.0);
    }

    SECTION("Ratio incompatible produce not_estimated con ratio_incompatible")
    {
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 70, -18.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 78, -32.5, true, 261.63, false, false, false, false }); // ratioCompatible = false
        sweep.push_back({ 85, -20.0, true, 261.63, true, false, false, false });

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "not_estimated");
        REQUIRE(obs.reason == "ratio_incompatible");
    }

    SECTION("Segundo nulo no se clasifica como primero y asigna beta ~ 5.5201")
    {
        // Sweep exhibiting two distinct carrier nulls:
        // Null 1 at control 60: -31.0 dBFS (suppression 25 dB)
        // Null 2 at control 90: -35.0 dBFS (suppression 29 dB)
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 50, -18.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 60, -31.0, true, 261.63, true, false, false, false }); // 1st null (beta ~ 2.4048)
        sweep.push_back({ 70, -16.0, true, 261.63, true, false, false, false }); // Rebound
        sweep.push_back({ 80, -19.0, true, 261.63, true, false, false, false });
        sweep.push_back({ 90, -35.0, true, 261.63, true, false, false, false }); // 2nd null (beta ~ 5.5201)
        sweep.push_back({ 99, -21.0, true, 261.63, true, false, false, false });

        CarrierNullEstimationConfig cfgOrder2 = config;
        cfgOrder2.targetNullOrder = 2;

        auto obs2 = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, cfgOrder2);

        REQUIRE(obs2.status == "estimated");
        REQUIRE(obs2.nullOrder == 2);
        REQUIRE(obs2.controlValueAtNull == 90);
        REQUIRE(obs2.estimatedBeta == Approx(5.5200781103).epsilon(1e-5));
        REQUIRE(obs2.estimatedBeta != Approx(2.4048255577).epsilon(1e-3)); // NOT classified as 1st null
    }

    SECTION("Clipping durante el barrido clasifica la medicion como unreliable")
    {
        std::vector<CarrierSweepPoint> sweep;
        sweep.push_back({ 0,  -6.0,  true, 261.63, true, false, false, false });
        sweep.push_back({ 50,  0.5,  true, 261.63, true, true, false, false }); // Clipping!
        sweep.push_back({ 78, -32.5, true, 261.63, true, false, false, false });
        sweep.push_back({ 85, -20.0, true, 261.63, true, false, false, false });

        auto obs = DexedParametricCampaignCoordinator::estimateBetaFromCarrierNull(sweep, config);

        REQUIRE(obs.status == "unreliable");
        REQUIRE(obs.reason == "clipping_detected");
        REQUIRE(obs.estimatedBeta == 0.0);
    }
}

TEST_CASE("Phase 20.11.5: Segregacion de Keyboard Level Scaling vs Rate Scaling", "[physical_estimators]")
{
    SECTION("Level scaling aislado con rate scaling desactivado (rateScaling = 0)")
    {
        // Across notes C1 (36) to C6 (96) with rateScaling = 0, envelope times are constant
        auto pC1 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(36, 60, 60, 60, 0);
        auto pC3 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(60, 60, 60, 60, 0);
        auto pC5 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(84, 60, 60, 60, 0);

        // Rate scaling remains invariant
        REQUIRE(pC1.attackTimeMs == Approx(pC3.attackTimeMs));
        REQUIRE(pC1.releaseTimeMs == Approx(pC3.releaseTimeMs));
        REQUIRE(pC5.attackTimeMs == Approx(pC3.attackTimeMs));
        REQUIRE(pC5.releaseTimeMs == Approx(pC3.releaseTimeMs));

        // Level scaling changes effective output level
        REQUIRE(pC1.effectiveOutputLevel < pC3.effectiveOutputLevel);
        REQUIRE(pC5.effectiveOutputLevel < pC3.effectiveOutputLevel);

        // Segregated records are intact
        REQUIRE(pC1.keyboardRateScaling.rateScaling == 0);
        REQUIRE(pC1.keyboardLevelScaling.leftDepth == 60);
        REQUIRE(pC1.keyboardLevelScaling.effectiveOutputLevel == pC1.effectiveOutputLevel);
    }

    SECTION("Rate scaling aislado con level scaling plano (leftDepth = 0, rightDepth = 0)")
    {
        // Across notes C1 (36) to C6 (96) with depths = 0, output level is invariant, but times scale
        auto pC1 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(36, 60, 0, 0, 5);
        auto pC3 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(60, 60, 0, 0, 5);
        auto pC5 = DexedParametricCampaignCoordinator::generateSyntheticKeyboardScalingPoint(84, 60, 0, 0, 5);

        // Effective output level remains completely flat
        REQUIRE(pC1.effectiveOutputLevel == pC3.effectiveOutputLevel);
        REQUIRE(pC5.effectiveOutputLevel == pC3.effectiveOutputLevel);

        // Rate scaling shortens attack and release times for higher notes
        REQUIRE(pC1.attackTimeMs > pC3.attackTimeMs);
        REQUIRE(pC3.attackTimeMs > pC5.attackTimeMs);
        REQUIRE(pC1.releaseTimeMs > pC3.releaseTimeMs);
        REQUIRE(pC3.releaseTimeMs > pC5.releaseTimeMs);
    }

    SECTION("Campana completa C1 a C6 con segregacion y persistencia JSON")
    {
        KeyboardScalingCampaignResult result;
        std::string err;
        juce::AudioPluginFormatManager dummyFmt;
        bool ok = DexedParametricCampaignCoordinator::executeKeyboardScalingCampaign(
            dummyFmt, juce::File(), juce::File(), result, err);

        REQUIRE(ok);
        REQUIRE(result.points.size() == 6); // C1, C2, C3, C4, C5, C6

        auto j = result.toJson();
        REQUIRE(j["campaignId"] == "campaign_keyboard_scaling");
        REQUIRE(j["points"].size() == 6);
        REQUIRE(j["points"][0]["note"] == 36);
        REQUIRE(j["points"][5]["note"] == 96);
        REQUIRE(j["points"][0].contains("keyboardLevelScaling"));
        REQUIRE(j["points"][0].contains("keyboardRateScaling"));
    }
}

TEST_CASE("Phase 20.11.5: Batch Manifest determinista y hashes de variante", "[physical_estimators]")
{
    std::vector<BatchVariantItem> items;
    // Push items deliberately out-of-order
    items.push_back({ "var_note84_lvl50_ratio1",  84, 50, 1.0, "sha_state_c", "sha_stim_c", "containers/var_c" });
    items.push_back({ "var_note36_lvl75_ratio1",  36, 75, 1.0, "sha_state_b", "sha_stim_b", "containers/var_b" });
    items.push_back({ "var_note36_lvl0_ratio1",   36,  0, 1.0, "sha_state_a", "sha_stim_a", "containers/var_a" });
    items.push_back({ "var_note60_lvl50_ratio2",  60, 50, 2.0, "sha_state_e", "sha_stim_e", "containers/var_e" });
    items.push_back({ "var_note60_lvl50_ratio1",  60, 50, 1.0, "sha_state_d", "sha_stim_d", "containers/var_d" });

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("abdaudiolab_batch_manifest_test_" + juce::String(juce::Random::getSystemRandom().nextInt()));

    BatchCampaignManifest manifest;
    std::string err;
    bool ok = DexedParametricCampaignCoordinator::exportBatchManifest(
        tempDir, "campaign_batch_release", "multi_campaign", items, manifest, err);

    REQUIRE(ok);
    REQUIRE(manifest.orderedVariantIds.size() == 5);

    // Expected canonical order:
    // 1. note 36, lvl 0, ratio 1.0 -> var_note36_lvl0_ratio1
    // 2. note 36, lvl 75, ratio 1.0 -> var_note36_lvl75_ratio1
    // 3. note 60, lvl 50, ratio 1.0 -> var_note60_lvl50_ratio1
    // 4. note 60, lvl 50, ratio 2.0 -> var_note60_lvl50_ratio2
    // 5. note 84, lvl 50, ratio 1.0 -> var_note84_lvl50_ratio1
    CHECK(manifest.orderedVariantIds[0] == "var_note36_lvl0_ratio1");
    CHECK(manifest.orderedVariantIds[1] == "var_note36_lvl75_ratio1");
    CHECK(manifest.orderedVariantIds[2] == "var_note60_lvl50_ratio1");
    CHECK(manifest.orderedVariantIds[3] == "var_note60_lvl50_ratio2");
    CHECK(manifest.orderedVariantIds[4] == "var_note84_lvl50_ratio1");

    // Check variant hashes are preserved in lockstep with canonical ordering
    CHECK(manifest.variantStateHashes[0] == "sha_state_a");
    CHECK(manifest.variantStateHashes[1] == "sha_state_b");
    CHECK(manifest.variantStateHashes[2] == "sha_state_d");
    CHECK(manifest.variantStateHashes[3] == "sha_state_e");
    CHECK(manifest.variantStateHashes[4] == "sha_state_c");

    // Check disk manifest was written
    auto manifestFile = tempDir.getChildFile("batch_manifest.json");
    REQUIRE(manifestFile.existsAsFile());

    // Clean up
    tempDir.deleteRecursively();
}

TEST_CASE("Phase 20.11.5: Agnostic DUT Identity y Parameter Bindings", "[physical_estimators]")
{
    DutIdentity dut;
    dut.dutType = "vst3";
    dut.vendor = "DigitalSuburban";
    dut.model = "Dexed";
    dut.version = "1.0.0";
    dut.instanceId = "dut_inst_001";
    dut.stateSha256 = "state_hash_123";
    dut.firmwareSha256 = "fw_hash_abc";
    dut.interfaceId = "audio_bus_0";

    REQUIRE(dut.dutType == "vst3");
    REQUIRE(dut.vendor == "DigitalSuburban");
    REQUIRE(dut.model == "Dexed");
    REQUIRE(dut.stateSha256 == "state_hash_123");
    REQUIRE(dut.firmwareSha256 == "fw_hash_abc");

    auto jDut = dutIdentityToJson(dut);
    REQUIRE(jDut["dutType"].get<std::string>() == "vst3");
    REQUIRE(jDut["vendor"].get<std::string>() == "DigitalSuburban");
    REQUIRE(jDut["model"].get<std::string>() == "Dexed");
    REQUIRE(jDut["stateSha256"].get<std::string>() == "state_hash_123");
    REQUIRE(jDut["firmwareSha256"].get<std::string>() == "fw_hash_abc");

    TargetParameterBinding binding;
    binding.logicalName = "operator_output_level";
    binding.nativeId = "p_op2_level";
    binding.unit = "index_0_99";
    binding.mappingVersion = "1.0";

    auto jBind = binding.toJson();
    REQUIRE(jBind["logicalName"].get<std::string>() == "operator_output_level");
    REQUIRE(jBind["nativeId"].get<std::string>() == "p_op2_level");
}

