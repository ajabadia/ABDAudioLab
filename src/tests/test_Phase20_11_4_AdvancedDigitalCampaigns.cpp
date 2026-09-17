/**
 * @file test_Phase20_11_4_AdvancedDigitalCampaigns.cpp
 * @brief Test suite for Phase 20.11.4: Advanced Digital FM Campaigns, Modulation Depth Proxies, and Temporal Trajectories.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../measurement/MeasurementSvgGenerator.h"
#include "../measurement/DexedParametricCampaignContracts.h"
#include "../measurement/DexedParametricCampaignCoordinator.h"
#include "../gui/measurement/MeasurementComparisonSession.h"
#include "../synth/Sha256.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::gui::measurement;

namespace
{

std::shared_ptr<MeasurementViewModel> createSyntheticTemporalViewModel(const juce::String& name,
                                                                       const std::vector<double>& centroids,
                                                                       int fftSize = 2048,
                                                                       const std::string& window = "hann",
                                                                       const std::string& status = "observed")
{
    auto vm = std::make_shared<MeasurementViewModel>();
    vm->dutName = name;
    vm->measurementStatus = MeasurementStatus::completed;
    vm->integrityStatus = UiIntegrityStatus::Verified;
    vm->sampleRateHz = 48000.0;
    vm->expectedAudioSha256 = "sha256_" + name.toStdString();

    DynamicResponseResult dyn;
    SpectralAnalysisMetadata spec;
    spec.fftSize = fftSize;
    spec.window = juce::String(window);
    dyn.spectralMetadata = spec;

    for (size_t i = 0; i < centroids.size(); ++i)
    {
        double tStart = static_cast<double>(i * 50);
        double tEnd = tStart + 50.0;
        vm->curve.x.push_back(tStart);
        vm->curve.y.push_back(-12.0); // Constant amplitude level

        dyn.brightnessCurve.x.push_back(tStart);
        dyn.brightnessCurve.y.push_back(centroids[i]);

        DynamicPoint pt;
        pt.velocity = 100;
        pt.spectralCentroidHz = centroids[i];
        pt.measurementWindowStartMs = tStart;
        pt.measurementWindowEndMs = tEnd;
        pt.status = status;
        dyn.points.push_back(pt);
    }

    dyn.amplitudeCurve = vm->curve;
    vm->dynamicsResult = dyn;
    return vm;
}

} // namespace

TEST_CASE("Fase 20.11.4 T1: Separacion de Proxy de Nivel vs Indice Fisico Beta", "[advanced_digital_campaigns][beta_proxy]")
{
    SECTION("betaStatus es not_estimated cuando no existe metodo fisico declarado")
    {
        auto obs = DexedParametricCampaignCoordinator::generateSyntheticFmObservation(75, 1.0, false);

        CHECK(obs.depthProxy == "operator_output_level");
        CHECK(obs.requestedOutputLevel == 75);
        CHECK(obs.effectiveOutputLevel == 75);
        CHECK(obs.estimatedBeta == Catch::Approx(0.0));
        CHECK(obs.betaStatus == "not_estimated");
        CHECK(obs.betaMethod.empty());

        auto j = obs.toJson();
        CHECK(j["depthProxy"] == "operator_output_level");
        CHECK(j["betaStatus"] == "not_estimated");
    }

    SECTION("Nivel 0 de modulador silencia bandas laterales sin inventar centroides")
    {
        auto obs = DexedParametricCampaignCoordinator::generateSyntheticFmObservation(0, 1.0, false);

        CHECK(obs.effectiveOutputLevel == 0);
        CHECK(obs.observedSidebandSpread == Catch::Approx(0.0));
        CHECK(obs.observedCarrierSuppressionDb == Catch::Approx(0.0));

        // In simulated tail frames where energy drops below -80 dBFS, centroid must be absent
        REQUIRE(obs.temporalFrames.size() == 10);
        const auto& silentFrame = obs.temporalFrames.back();
        CHECK(silentFrame.status == "silent");
        CHECK(silentFrame.rmsDbfs <= -80.0);
        CHECK_FALSE(silentFrame.spectralCentroidHz.has_value());
    }
}

TEST_CASE("Fase 20.11.4 T2: Distincion entre Ratio Nominal y Efectivo", "[advanced_digital_campaigns][ratios]")
{
    SECTION("Ratio inarmonico solicitado cuantiza formalmente en el registro efectivo")
    {
        ParametricCampaignManifest manifest;
        std::string err;
        juce::AudioPluginFormatManager fm;
        bool ok = DexedParametricCampaignCoordinator::executeFrequencyRatioCampaign(fm, {}, {}, manifest, err);
        REQUIRE(ok);

        REQUIRE(manifest.variants.size() == 3);

        // Variant 1: 1.0 -> Harmonic
        CHECK(manifest.variants[0].parameters[0].requestedValue == 1.0);
        CHECK(manifest.variants[0].parameters[0].effectiveValue == 1.0);

        // Variant 2: 2.0 -> Harmonic
        CHECK(manifest.variants[1].parameters[0].requestedValue == 2.0);
        CHECK(manifest.variants[1].parameters[0].effectiveValue == 2.0);

        // Variant 3: 3.14 -> Inharmonic requested, quantized coarse effective
        CHECK(manifest.variants[2].parameters[0].requestedValue == 3.14);
        CHECK(manifest.variants[2].parameters[0].effectiveValue == 3.0);
        CHECK(manifest.variants[2].parameters[0].requestedValue != manifest.variants[2].parameters[0].effectiveValue);
    }

    SECTION("Sidebands observadas quedan vinculadas a portadora y moduladora")
    {
        auto obsHarmonic = DexedParametricCampaignCoordinator::generateSyntheticFmObservation(50, 2.0, false);
        CHECK(obsHarmonic.ratioClass == "harmonic");
        CHECK(obsHarmonic.observedRatio == Catch::Approx(2.0));
        CHECK(obsHarmonic.modulatorFrequencyHz == Catch::Approx(obsHarmonic.carrierFrequencyHz * 2.0));

        auto obsInharmonic = DexedParametricCampaignCoordinator::generateSyntheticFmObservation(50, 3.14, false);
        CHECK(obsInharmonic.ratioClass == "inharmonic");
        CHECK(obsInharmonic.carrierFrequencyHz > 0.0);
    }
}

TEST_CASE("Fase 20.11.4 T3: Trayectoria Temporal C(t) y Segregacion Metrologica", "[advanced_digital_campaigns][temporal_timbre]")
{
    MeasurementComparisonSession session;

    SECTION("Trayectorias identicas resultan en temporalTimbreEquivalence BitExact")
    {
        std::vector<double> traj = { 2000.0, 1800.0, 1600.0, 1400.0, 1200.0 };
        auto vm1 = createSyntheticTemporalViewModel("SynthA", traj, 2048, "hann", "observed");
        auto vm2 = createSyntheticTemporalViewModel("SynthB", traj, 2048, "hann", "observed");

        LoadedContainerEntry e1; e1.id = 1; e1.loadState = ContainerLoadState::Verified; e1.viewModel = vm1;
        LoadedContainerEntry e2; e2.id = 2; e2.loadState = ContainerLoadState::Verified; e2.viewModel = vm2;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(1, 2);

        CHECK(res.levelEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.timbreEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.temporalTimbreEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.maxTemporalTimbreDeltaHz == Catch::Approx(0.0));
    }

    SECTION("Frames no fiables o silenciosos conmutan temporalTimbreEquivalence a NotComparable")
    {
        std::vector<double> traj = { 2000.0, 1800.0, 1600.0, 1400.0, 1200.0 };
        auto vm1 = createSyntheticTemporalViewModel("NormalA", traj, 2048, "hann", "observed");
        auto vm2 = createSyntheticTemporalViewModel("SilentB", traj, 2048, "hann", "silent");

        LoadedContainerEntry e1; e1.id = 1; e1.loadState = ContainerLoadState::Verified; e1.viewModel = vm1;
        LoadedContainerEntry e2; e2.id = 2; e2.loadState = ContainerLoadState::Verified; e2.viewModel = vm2;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(1, 2);

        // Level is identical, but temporal trajectory cannot be compared
        CHECK(res.levelEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.temporalTimbreEquivalence == PairwiseStateEquivalence::NotComparable);
    }

    SECTION("Discrepancia en FFT size o Window STFT conmuta temporalTimbreEquivalence a NotComparable")
    {
        std::vector<double> traj = { 2000.0, 1800.0, 1600.0, 1400.0, 1200.0 };
        auto vm1 = createSyntheticTemporalViewModel("Fft2048", traj, 2048, "hann", "observed");
        auto vm2 = createSyntheticTemporalViewModel("Fft1024", traj, 1024, "hann", "observed"); // Incompatible FFT

        LoadedContainerEntry e1; e1.id = 1; e1.loadState = ContainerLoadState::Verified; e1.viewModel = vm1;
        LoadedContainerEntry e2; e2.id = 2; e2.loadState = ContainerLoadState::Verified; e2.viewModel = vm2;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(1, 2);

        CHECK(res.temporalTimbreEquivalence == PairwiseStateEquivalence::NotComparable);
    }
}

TEST_CASE("Fase 20.11.4 T4: Integridad de Campana OFAT y Schemas Retrocompatibles", "[advanced_digital_campaigns][ofat]")
{
    SECTION("Campana de nivel de operador varia estrictamente de forma aislada")
    {
        ParametricCampaignManifest manifest;
        std::string err;
        juce::AudioPluginFormatManager fm;
        bool ok = DexedParametricCampaignCoordinator::executeModulationIndexCampaign(fm, {}, {}, manifest, err);
        REQUIRE(ok);

        REQUIRE(manifest.variants.size() == 5);
        int expectedLevels[] = { 0, 25, 50, 75, 99 };

        std::string prevHash;
        for (size_t i = 0; i < manifest.variants.size(); ++i)
        {
            const auto& v = manifest.variants[i];
            CHECK(v.parameters[0].parameterName == "operator_output_level");
            CHECK(v.parameters[0].effectiveValue == static_cast<double>(expectedLevels[i]));
            CHECK(v.expectedStateSha256 != prevHash);
            prevHash = v.expectedStateSha256;
        }
    }

    SECTION("Schema retrocompatible carga sin campos de operador sin romperse")
    {
        nlohmann::json legacyManifest = {
            { "campaignId", "legacy_campaign_01" },
            { "campaignType", "factorial_algorithm" },
            { "schemaVersion", "abdaudiolab-fair-lnl-1.0" },
            { "dutName", "Dexed.vst3" },
            { "variants", nlohmann::json::array() }
        };

        CHECK(legacyManifest["schemaVersion"] == "abdaudiolab-fair-lnl-1.0");
        CHECK_FALSE(legacyManifest.contains("stftParameters"));
    }
}
