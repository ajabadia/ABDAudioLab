/**
 * @file test_Phase20_11_3_DigitalExpansion.cpp
 * @brief Test suite for Phase 20.11.3: Digital Expansion, Parametric Campaigns, and Timbre/Level Segregation.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../measurement/MeasurementSvgGenerator.h"
#include "../measurement/MeasurementContracts.h"
#include "../measurement/DexedParametricCampaignContracts.h"
#include "../measurement/DexedParametricCampaignCoordinator.h"
#include "../measurement/MeasurementComparisonReportGenerator.h"
#include "../gui/measurement/MeasurementComparisonSession.h"
#include "../synth/Sha256.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::gui::measurement;

namespace
{

struct TempFolder
{
    juce::File dir;
    TempFolder(const juce::String& prefix)
    {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile(prefix + "_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();
    }
    ~TempFolder()
    {
        dir.deleteRecursively();
    }
};

std::shared_ptr<MeasurementViewModel> createSyntheticViewModel(const juce::String& name,
                                                               const std::vector<double>& levels,
                                                               const std::vector<double>& centroids = {},
                                                               double sampleRate = 48000.0,
                                                               const std::string& pointStatus = "observed",
                                                               double winStart = 0.0,
                                                               double winEnd = 50.0)
{
    auto vm = std::make_shared<MeasurementViewModel>();
    vm->dutName = name;
    vm->measurementStatus = MeasurementStatus::completed;
    vm->integrityStatus = UiIntegrityStatus::Verified;
    vm->sampleRateHz = sampleRate;
    vm->expectedAudioSha256 = "dummy_sha_" + name.toStdString();

    // Primary curve
    for (size_t i = 0; i < levels.size(); ++i)
    {
        vm->curve.x.push_back(static_cast<double>(i * 32));
        vm->curve.y.push_back(levels[i]);
    }

    DynamicResponseResult dyn;
    dyn.amplitudeCurve = vm->curve;

    SpectralAnalysisMetadata specMeta;
    specMeta.fftSize = 2048;
    specMeta.window = "hann";
    dyn.spectralMetadata = specMeta;

    if (!centroids.empty())
    {
        for (size_t i = 0; i < centroids.size(); ++i)
        {
            dyn.brightnessCurve.x.push_back(static_cast<double>(i * 32));
            dyn.brightnessCurve.y.push_back(centroids[i]);

            DynamicPoint pt;
            pt.velocity = static_cast<int>(i * 32);
            pt.peakDbfs = levels[i];
            pt.spectralCentroidHz = centroids[i];
            pt.status = pointStatus;
            pt.measurementWindowStartMs = winStart;
            pt.measurementWindowEndMs = winEnd;
            dyn.points.push_back(pt);
        }
    }

    vm->dynamicsResult = dyn;
    return vm;
}

} // namespace

TEST_CASE("Fase 20.11.3 T1: Contratos de Campana y Registro de Valores Efectivos", "[digital_expansion][contracts]")
{
    SECTION("effectiveValue registra cuantizacion real distinguiendose de requestedValue")
    {
        ParametricRecord rec;
        rec.parameterName = "feedback";
        rec.requestedValue = 7.42; // Floating requested value
        rec.effectiveValue = 7.0;  // Integer quantized effective value
        rec.parameterId = "param_feedback";
        rec.stateSha256 = "abc123state";
        rec.fixtureRole = FixtureRole::CanonicalExploratoryPair;

        CHECK(rec.requestedValue != rec.effectiveValue);
        CHECK(rec.effectiveValue == 7.0);
        CHECK(fixtureRoleToString(rec.fixtureRole) == "canonical_pair");

        auto j = rec.toJson();
        CHECK(j["requestedValue"] == 7.42);
        CHECK(j["effectiveValue"] == 7.0);
        CHECK(j["fixtureRole"] == "canonical_pair");

        auto restored = ParametricRecord::fromJson(j);
        CHECK(restored.requestedValue == 7.42);
        CHECK(restored.effectiveValue == 7.0);
        CHECK(restored.fixtureRole == FixtureRole::CanonicalExploratoryPair);
    }

    SECTION("Campana Factorial A aisla estrictamente Algoritmo (FB 0 constante)")
    {
        auto manifest = DexedParametricCampaignCoordinator::generateSyntheticFactorialManifest(ParametricCampaignType::FactorialAlgorithm);

        REQUIRE(manifest.variants.size() == 2);
        const auto& v1 = manifest.variants[0];
        const auto& v2 = manifest.variants[1];

        // Variant 1: Algo 1, FB 0
        CHECK(v1.parameters[0].parameterName == "algorithm");
        CHECK(v1.parameters[0].effectiveValue == 1.0);
        CHECK(v1.parameters[1].parameterName == "feedback");
        CHECK(v1.parameters[1].effectiveValue == 0.0);

        // Variant 2: Algo 32, FB 0
        CHECK(v2.parameters[0].parameterName == "algorithm");
        CHECK(v2.parameters[0].effectiveValue == 32.0);
        CHECK(v2.parameters[1].parameterName == "feedback");
        CHECK(v2.parameters[1].effectiveValue == 0.0);

        // Isolated OFAT guarantee: feedback is constant across variants
        CHECK(v1.parameters[1].effectiveValue == v2.parameters[1].effectiveValue);
        // But algorithm diverges
        CHECK(v1.parameters[0].effectiveValue != v2.parameters[0].effectiveValue);
    }

    SECTION("Campana Factorial B aisla estrictamente Feedback (Algoritmo 1 constante)")
    {
        auto manifest = DexedParametricCampaignCoordinator::generateSyntheticFactorialManifest(ParametricCampaignType::FactorialFeedback);

        REQUIRE(manifest.variants.size() == 2);
        const auto& v1 = manifest.variants[0];
        const auto& v2 = manifest.variants[1];

        // Both hold Algorithm = 1
        CHECK(v1.parameters[0].effectiveValue == 1.0);
        CHECK(v2.parameters[0].effectiveValue == 1.0);

        // Feedback varies 0 vs 7
        CHECK(v1.parameters[1].effectiveValue == 0.0);
        CHECK(v2.parameters[1].effectiveValue == 7.0);

        // Hashes differ per variant
        CHECK(v1.expectedStateSha256 != v2.expectedStateSha256);
    }

    SECTION("Derivacion de fixtures garantiza estados y hashes independientes")
    {
        DexedVerticalFixture baseFix;
        baseFix.presetName = "Dexed_Init";
        baseFix.presetBytes = { 0x01, 0x02, 0x03, 0x04 };
        baseFix.stateSha256 = abdaudiolab::synth::Sha256::computeHex(baseFix.presetBytes.data(), baseFix.presetBytes.size());

        auto fixAlgo1 = DexedParametricCampaignCoordinator::createControlledVariantFixture(baseFix, 1, 0);
        auto fixAlgo32 = DexedParametricCampaignCoordinator::createControlledVariantFixture(baseFix, 32, 0);
        auto fixFb7 = DexedParametricCampaignCoordinator::createControlledVariantFixture(baseFix, 1, 7);

        CHECK(fixAlgo1.stateSha256 != fixAlgo32.stateSha256);
        CHECK(fixAlgo1.stateSha256 != fixFb7.stateSha256);
        CHECK(fixAlgo32.stateSha256 != fixFb7.stateSha256);
    }
}

TEST_CASE("Fase 20.11.3 T2: DRY y Paridad Visual del Motor SVG Declarativo", "[digital_expansion][svg]")
{
    SECTION("generateMultiSeriesSvg reproduce las caracteristicas estructurales exactas de la fase anterior")
    {
        MeasurementSvgGenerator::SvgSeries s1;
        s1.id = "1";
        s1.label = "Dexed Real Algo 1";
        s1.points = { { 0.0, -96.0 }, { 64.0, -18.0 }, { 127.0, -3.0 } };
        s1.lineStyle = 0;
        s1.markerStyle = 0;

        MeasurementSvgGenerator::SvgSeries s2;
        s2.id = "2";
        s2.label = "Dexed Real Algo 32";
        s2.points = { { 0.0, -96.0 }, { 64.0, -22.0 }, { 127.0, -6.0 } };
        s2.lineStyle = 1; // Dashed
        s2.markerStyle = 1; // Square

        MeasurementSvgGenerator::SvgPlotSpec spec;
        spec.xMin = 0.0;
        spec.xMax = 127.0;
        spec.yMin = -96.0;
        spec.yMax = 0.0;
        spec.width = 800;
        spec.height = 320;
        spec.yUnit = "dB";

        std::string svg = MeasurementSvgGenerator::generateMultiSeriesSvg({ s1, s2 }, spec);

        CHECK(svg.find("viewBox=\"0 0 800 320\"") != std::string::npos);
        CHECK(svg.find("<rect x=\"50\" y=\"20\" width=\"550\" height=\"260\"") != std::string::npos);
        // Ticks verification
        CHECK(svg.find(">0 dB</text>") != std::string::npos);
        CHECK(svg.find(">-96 dB</text>") != std::string::npos);
        CHECK(svg.find(">127</text>") != std::string::npos);
        // Styles & Markers
        CHECK(svg.find("stroke-dasharray=\"6,3\"") != std::string::npos); // s2 dash
        CHECK(svg.find("<circle cx=") != std::string::npos); // s1 marker
        CHECK(svg.find("<rect x=") != std::string::npos); // s2 marker
        // Legend text
        CHECK(svg.find("Dexed Real Algo 1") != std::string::npos);
        CHECK(svg.find("Dexed Real Algo 32") != std::string::npos);
    }
}

TEST_CASE("Fase 20.11.3 T3: Comparacion Timbre/Nivel Segregada y Metrologica", "[digital_expansion][timbre_comparison]")
{
    MeasurementComparisonSession session;

    SECTION("Nivel bit-exact pero timbre divergente reporta adecuadamente sin fusionar dimensiones")
    {
        // Two instances with identical amplitude levels, but divergent centroids
        std::vector<double> levels = { -96.0, -24.0, -12.0, -6.0, 0.0 };
        std::vector<double> centroidsA = { 1000.0, 1500.0, 2000.0, 2500.0, 3000.0 };
        std::vector<double> centroidsB = { 1000.0, 1800.0, 2600.0, 3400.0, 4200.0 }; // Divergence up to 1200 Hz

        auto vm1 = createSyntheticViewModel("Instance_A", levels, centroidsA);
        auto vm2 = createSyntheticViewModel("Instance_B", levels, centroidsB);

        int id1 = 1;
        int id2 = 2;

        LoadedContainerEntry e1;
        e1.id = id1;
        e1.loadState = ContainerLoadState::Verified;
        e1.viewModel = vm1;

        LoadedContainerEntry e2;
        e2.id = id2;
        e2.loadState = ContainerLoadState::Verified;
        e2.viewModel = vm2;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(id1, id2);

        CHECK(res.levelEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.maxAudioDelta == Catch::Approx(0.0));

        CHECK(res.timbreEquivalence == PairwiseStateEquivalence::NotEquivalent);
        CHECK(res.maxTimbreDeltaHz == Catch::Approx(1200.0));
        CHECK(res.timbreMetric == "spectralCentroidHz");

        // Consolidated status reports timbre divergence explicitly
        CHECK(res.equivalence == PairwiseStateEquivalence::NotEquivalent);
        CHECK(res.reason.contains("timbre divergence observed"));
    }

    SECTION("Tomas con base incompatible o estado unreliable retornan NotComparable en timbre")
    {
        std::vector<double> levels = { -96.0, -24.0, -12.0 };
        std::vector<double> centroids = { 1000.0, 2000.0, 3000.0 };

        // Take A is normal, Take B has "silent" point
        auto vmA = createSyntheticViewModel("Normal_A", levels, centroids, 48000.0, "observed", 0.0, 50.0);
        auto vmB = createSyntheticViewModel("Silent_B", levels, centroids, 48000.0, "silent", 0.0, 50.0);

        int id1 = 1;
        int id2 = 2;

        LoadedContainerEntry e1;
        e1.id = id1;
        e1.loadState = ContainerLoadState::Verified;
        e1.viewModel = vmA;

        LoadedContainerEntry e2;
        e2.id = id2;
        e2.loadState = ContainerLoadState::Verified;
        e2.viewModel = vmB;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(id1, id2);

        CHECK(res.levelEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.timbreEquivalence == PairwiseStateEquivalence::NotComparable);
    }

    SECTION("Tomas con ventanas temporales dispares retornan NotComparable en timbre")
    {
        std::vector<double> levels = { -96.0, -24.0, -12.0 };
        std::vector<double> centroids = { 1000.0, 2000.0, 3000.0 };

        // Window mismatch: 0..50ms vs 100..150ms
        auto vmA = createSyntheticViewModel("WinA", levels, centroids, 48000.0, "observed", 0.0, 50.0);
        auto vmB = createSyntheticViewModel("WinB", levels, centroids, 48000.0, "observed", 100.0, 150.0);

        int id1 = 1;
        int id2 = 2;

        LoadedContainerEntry e1;
        e1.id = id1;
        e1.loadState = ContainerLoadState::Verified;
        e1.viewModel = vmA;

        LoadedContainerEntry e2;
        e2.id = id2;
        e2.loadState = ContainerLoadState::Verified;
        e2.viewModel = vmB;

        session.addLoadedContainerDirectlyForTesting(e1);
        session.addLoadedContainerDirectlyForTesting(e2);

        auto res = session.compareContainers(id1, id2);

        CHECK(res.levelEquivalence == PairwiseStateEquivalence::BitExact);
        CHECK(res.timbreEquivalence == PairwiseStateEquivalence::NotComparable);
    }
}
