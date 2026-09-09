#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "export/LutExporter.h"
#include "core/SessionSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace abdaudiolab;

TEST_CASE("LutExporter Manifest Serialization of Wiener-Hammerstein Model", "[export][lnl][manifest]")
{
    exporting::SessionManifestData manifest;
    manifest.hardwareId = "test_hardware";
    manifest.hardwareName = "Test Synth";
    manifest.hasWienerHammersteinModel = true;
    manifest.whH1Taps = { 0.05f, 0.90f, 0.05f };
    manifest.whNonLinearCoeffA = 0.025f;
    manifest.whH2Taps = { 0.10f, 0.80f, 0.10f };
    manifest.whGoodnessOfFitR2 = 0.985f;
    manifest.whResidualErrorRms = 0.003f;
    manifest.whPreFilterCentroidHz = 1200.0f;
    manifest.whPostFilterCentroidHz = 3500.0f;

    juce::File tempManifest = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_manifest_lnl.json");
    tempManifest.deleteFile();

    std::vector<exporting::MeasuredPoint> pts;
    bool ok = exporting::LutExporter::exportSessionManifest(tempManifest.getFullPathName().toStdString(), manifest, pts);
    REQUIRE(ok == true);

    std::ifstream in(tempManifest.getFullPathName().toStdString());
    REQUIRE(in.is_open());
    nlohmann::json j;
    in >> j;

    REQUIRE(j.contains("wienerHammersteinModel"));
    const auto& wh = j["wienerHammersteinModel"];
    REQUIRE(wh["identified"] == true);
    REQUIRE_THAT(wh["goodnessOfFitR2"].get<float>(), Catch::Matchers::WithinAbs(0.985f, 1e-4f));
    REQUIRE_THAT(wh["residualErrorRms"].get<float>(), Catch::Matchers::WithinAbs(0.003f, 1e-4f));
    REQUIRE_THAT(wh["staticNonlinearity"]["coeffA"].get<float>(), Catch::Matchers::WithinAbs(0.025f, 1e-4f));
    REQUIRE_THAT(wh["preFilterH1"]["centroidHz"].get<float>(), Catch::Matchers::WithinAbs(1200.0f, 1e-2f));
    REQUIRE_THAT(wh["postFilterH2"]["centroidHz"].get<float>(), Catch::Matchers::WithinAbs(3500.0f, 1e-2f));

    tempManifest.deleteFile();
}

TEST_CASE("SessionSerializer Roundtrip of Wiener-Hammerstein Model Parameters", "[core][session][lnl]")
{
    core::SessionManifest sm;
    sm.sessionTitle = "LnlTestSession";
    sm.hasWienerHammersteinModel = true;
    sm.whH1Taps = { 0.01f, 0.98f, 0.01f };
    sm.whNonLinearCoeffA = -0.015f;
    sm.whH2Taps = { 0.02f, 0.96f, 0.02f };
    sm.whGoodnessOfFitR2 = 0.991f;
    sm.whResidualErrorRms = 0.0015f;
    sm.whPreFilterCentroidHz = 850.0f;
    sm.whPostFilterCentroidHz = 4200.0f;

    core::SessionSerializer serializer;
    juce::File tempPackage = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_lnl_roundtrip.abdlabtest");
    tempPackage.deleteFile();

    std::vector<exporting::MeasuredPoint> pts;
    bool saveOk = serializer.saveSessionToPackage(tempPackage, sm, pts);
    REQUIRE(saveOk == true);

    core::SessionManifest loaded;
    std::vector<exporting::MeasuredPoint> loadedPts;
    juce::String err;
    bool loadOk = serializer.loadSessionFromPackage(tempPackage, loaded, loadedPts, err);
    REQUIRE(loadOk == true);
    REQUIRE(loaded.hasWienerHammersteinModel == true);
    REQUIRE_THAT(loaded.whNonLinearCoeffA, Catch::Matchers::WithinAbs(-0.015f, 1e-5f));
    REQUIRE_THAT(loaded.whGoodnessOfFitR2, Catch::Matchers::WithinAbs(0.991f, 1e-4f));
    REQUIRE_THAT(loaded.whResidualErrorRms, Catch::Matchers::WithinAbs(0.0015f, 1e-5f));
    REQUIRE_THAT(loaded.whPreFilterCentroidHz, Catch::Matchers::WithinAbs(850.0f, 1e-2f));
    REQUIRE_THAT(loaded.whPostFilterCentroidHz, Catch::Matchers::WithinAbs(4200.0f, 1e-2f));
    REQUIRE(loaded.whH1Taps.size() == 3);
    REQUIRE(loaded.whH2Taps.size() == 3);

    tempPackage.deleteFile();
}
