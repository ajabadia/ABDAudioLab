#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/SessionSerializer.h"
#include "core/ProfilingSession.h"
#include <juce_core/juce_core.h>
#include "support/LabTestScratch.h"

TEST_CASE("SessionSerializer Serialization Roundtrip", "[core][serializer]")
{
    abdaudiolab::core::SessionSerializer serializer;
    
    abdaudiolab::core::SessionManifest manifest;
    manifest.sessionTitle = "UnitTest_Session";
    manifest.hardwareName = "MOCK_SYNTH_UNITTEST";
    manifest.targetModule = "FILTER_SCAN";
    manifest.appVersion = "1.1.0";
    manifest.totalPointsMeasured = 2;
    manifest.operatorNotes = "Warmup 30m, room 21.5C, pristine test signal.";
    manifest.ambientTemperatureC = 21.5f;
    manifest.warmupTimeMinutes = 30;

    std::vector<abdaudiolab::exporting::MeasuredPoint> points;
    
    abdaudiolab::exporting::MeasuredPoint p1;
    p1.pointId = "P_001";
    p1.testId = "TC_FLT_001";
    p1.blockType = "SpectrumFilter";
    p1.stimulusType = "LogFarinaSweep";
    p1.thdPercent = 0.05f;
    p1.snrDb = 85.4f;
    p1.irSamples = { 0.0f, 1.0f, 0.0f };
    points.push_back(p1);

    abdaudiolab::exporting::MeasuredPoint p2;
    p2.pointId = "P_002";
    p2.testId = "TC_FLT_002";
    p2.blockType = "SpectrumFilter";
    p2.stimulusType = "LogFarinaSweep";
    p2.thdPercent = 0.12f;
    p2.snrDb = 82.1f;
    p2.irSamples = { 0.0f, 0.8f, -0.1f };
    points.push_back(p2);

    juce::File tempPackageFile = abdaudiolab::test::scratchDir ("SessionSerializer")
            .getChildFile ("unittest_session_package.abdlabtest");
    if (tempPackageFile.existsAsFile())
        tempPackageFile.deleteFile();

    bool saveOk = serializer.saveSessionToPackage(tempPackageFile, manifest, points);
    REQUIRE(saveOk);
    REQUIRE(tempPackageFile.existsAsFile());

    // Load back and verify roundtrip
    abdaudiolab::core::SessionManifest loadedManifest;
    std::vector<abdaudiolab::exporting::MeasuredPoint> loadedPoints;
    juce::String loadedError;

    bool loadOk = serializer.loadSessionFromPackage(tempPackageFile, loadedManifest, loadedPoints, loadedError);
    REQUIRE(loadOk);
    REQUIRE(loadedManifest.sessionTitle == manifest.sessionTitle);
    REQUIRE(loadedManifest.hardwareName == manifest.hardwareName);
    REQUIRE(loadedManifest.operatorNotes == manifest.operatorNotes);
    REQUIRE(std::abs(loadedManifest.ambientTemperatureC - 21.5f) < 0.001f);
    REQUIRE(loadedManifest.warmupTimeMinutes == 30);
    REQUIRE(loadedPoints.size() == 2);
    REQUIRE(loadedPoints[0].pointId == "P_001");
    REQUIRE(loadedPoints[1].pointId == "P_002");

    // Clean up
    tempPackageFile.deleteFile();
}

TEST_CASE("SessionSerializer: Physical Noise Baseline and Policy Decoupling Roundtrip", "[core][serializer][noise_baseline]")
{
    abdaudiolab::core::SessionSerializer serializer;

    abdaudiolab::core::SessionManifest manifest;
    manifest.sessionTitle = "NoiseBaseline_Test_Session";
    manifest.hardwareName = "PHYSICAL_CONVERTER_LAB";
    manifest.appVersion = "1.1.0";
    manifest.sampleRate = 48000.0;
    
    // Policy acceptance threshold: -85.0 dBFS (must never be overwritten by measured noise)
    manifest.noiseFloorThresholdDb = -85.0f;

    // Physical noise baseline measured in Step 2A
    manifest.measuredNoiseFloorRmsDbfs = -89.5f;
    manifest.measuredNoiseFloorPeakDbfs = -75.2f;
    manifest.noiseBaselineStatus = abdaudiolab::calibration::NoiseBaselineStatus::Valid;
    manifest.hasPhysicalNoiseBaseline = true;
    manifest.snrMeasurementMethod = abdaudiolab::math::SnrMeasurementMethod::PhysicalNoiseBaseline;

    std::vector<abdaudiolab::exporting::MeasuredPoint> points;
    abdaudiolab::exporting::MeasuredPoint p1;
    p1.pointId = "P_001";
    p1.snrDb = 92.5f;
    points.push_back(p1);

    juce::File tempPackageFile = abdaudiolab::test::scratchDir("SessionSerializer")
                                     .getChildFile("noise_baseline_roundtrip.abdlabtest");
    if (tempPackageFile.existsAsFile())
        tempPackageFile.deleteFile();

    bool saveOk = serializer.saveSessionToPackage(tempPackageFile, manifest, points);
    REQUIRE(saveOk);

    abdaudiolab::core::SessionManifest loaded;
    std::vector<abdaudiolab::exporting::MeasuredPoint> loadedPoints;
    juce::String err;

    bool loadOk = serializer.loadSessionFromPackage(tempPackageFile, loaded, loadedPoints, err);
    REQUIRE(loadOk);

    // Invariant: Acceptance policy threshold remains unchanged
    CHECK(loaded.noiseFloorThresholdDb == Catch::Approx(-85.0f));

    // Invariant: Physical baseline measurements are faithfully preserved
    REQUIRE(loaded.hasPhysicalNoiseBaseline);
    REQUIRE(loaded.measuredNoiseFloorRmsDbfs.has_value());
    CHECK(*loaded.measuredNoiseFloorRmsDbfs == Catch::Approx(-89.5f));
    REQUIRE(loaded.measuredNoiseFloorPeakDbfs.has_value());
    CHECK(*loaded.measuredNoiseFloorPeakDbfs == Catch::Approx(-75.2f));
    CHECK(loaded.noiseBaselineStatus == abdaudiolab::calibration::NoiseBaselineStatus::Valid);
    CHECK(loaded.snrMeasurementMethod == abdaudiolab::math::SnrMeasurementMethod::PhysicalNoiseBaseline);

    tempPackageFile.deleteFile();
}

TEST_CASE("SessionSerializer: Legacy Session Decoupling Without Physical Baseline", "[core][serializer][legacy]")
{
    abdaudiolab::core::SessionSerializer serializer;

    abdaudiolab::core::SessionManifest manifest;
    manifest.sessionTitle = "Legacy_Session";
    manifest.hardwareName = "LEGACY_SYNTH";
    manifest.appVersion = "1.0.0";
    manifest.noiseFloorThresholdDb = -85.0f;
    manifest.hasPhysicalNoiseBaseline = false;
    manifest.measuredNoiseFloorRmsDbfs = std::nullopt;
    manifest.measuredNoiseFloorPeakDbfs = std::nullopt;
    manifest.noiseBaselineStatus = abdaudiolab::calibration::NoiseBaselineStatus::NotMeasured;
    manifest.snrMeasurementMethod = abdaudiolab::math::SnrMeasurementMethod::LegacyAssumedNoiseFloor;

    juce::File tempPackageFile = abdaudiolab::test::scratchDir("SessionSerializer")
                                     .getChildFile("legacy_session_roundtrip.abdlabtest");
    if (tempPackageFile.existsAsFile())
        tempPackageFile.deleteFile();

    bool saveOk = serializer.saveSessionToPackage(tempPackageFile, manifest, {});
    REQUIRE(saveOk);

    abdaudiolab::core::SessionManifest loaded;
    std::vector<abdaudiolab::exporting::MeasuredPoint> loadedPoints;
    juce::String err;

    bool loadOk = serializer.loadSessionFromPackage(tempPackageFile, loaded, loadedPoints, err);
    REQUIRE(loadOk);

    CHECK_FALSE(loaded.hasPhysicalNoiseBaseline);
    CHECK_FALSE(loaded.measuredNoiseFloorRmsDbfs.has_value());
    CHECK_FALSE(loaded.measuredNoiseFloorPeakDbfs.has_value());
    CHECK(loaded.noiseBaselineStatus == abdaudiolab::calibration::NoiseBaselineStatus::NotMeasured);
    CHECK(loaded.snrMeasurementMethod == abdaudiolab::math::SnrMeasurementMethod::LegacyAssumedNoiseFloor);
    CHECK(loaded.noiseFloorThresholdDb == Catch::Approx(-85.0f));

    tempPackageFile.deleteFile();
}
