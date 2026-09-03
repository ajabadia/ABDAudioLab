#include <catch2/catch_test_macros.hpp>
#include "core/SessionSerializer.h"
#include "core/ProfilingSession.h"
#include <juce_core/juce_core.h>

TEST_CASE("SessionSerializer Serialization Roundtrip", "[core][serializer]")
{
    abdaudiolab::core::SessionSerializer serializer;
    
    abdaudiolab::core::SessionManifest manifest;
    manifest.sessionTitle = "UnitTest_Session";
    manifest.hardwareName = "MOCK_SYNTH_UNITTEST";
    manifest.targetModule = "FILTER_SCAN";
    manifest.appVersion = "1.1.0";
    manifest.buildNumber = 157;
    manifest.totalPointsMeasured = 2;

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

    juce::File tempPackageFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                     .getChildFile("unittest_session_package.abdlabtest");
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
    REQUIRE(loadedPoints.size() == 2);
    REQUIRE(loadedPoints[0].pointId == "P_001");
    REQUIRE(loadedPoints[1].pointId == "P_002");

    // Clean up
    tempPackageFile.deleteFile();
}
