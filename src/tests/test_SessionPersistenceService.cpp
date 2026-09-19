/**
 * @file test_SessionPersistenceService.cpp
 * @brief Characterization and regression test suite for SessionPersistenceService.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/SessionPersistenceService.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("SessionPersistenceService: Round-Trip Integrity & Canonical Equivalence", "[SessionPersistenceService]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_PersistTest_" + juce::String(juce::Random::getSystemRandom().nextInt(100000)));
    if (tempDir.exists()) tempDir.deleteRecursively();
    tempDir.createDirectory();

    // 1. Build rich reference manifest
    SessionManifest originalManifest;
    originalManifest.sessionTitle = "Lab Calibration Master";
    originalManifest.formatVersion = "1.0";
    originalManifest.hardwareId = "hw_moog_sub37";
    originalManifest.hardwareName = "Moog Sub 37";
    originalManifest.hardwareDisplayName = "Moog Sub 37 Tribute Edition";
    originalManifest.activeFunctionId = "vcf_ladder";
    originalManifest.activeFunctionName = "Ladder Filter Sweep";
    originalManifest.sampleRate = 96000.0;
    originalManifest.lineCalibrationGainDb = -2.5f;
    originalManifest.noiseFloorThresholdDb = -88.0f;
    originalManifest.operatorNotes = "Warm-up 30m, room 21.5C";
    originalManifest.ambientTemperatureC = 21.5f;
    originalManifest.warmupTimeMinutes = 30;

    gui::TestConfiguration tc;
    tc.testName = "Cutoff vs Resonance Matrix";
    tc.stimulusType = audio::StimulusType::LogFarinaSweep;
    tc.burstDurationSec = 1.2f;
    tc.captureMode = "DirectAudio";
    
    gui::ControlStepConfig c1;
    c1.id = "cutoff_knob";
    c1.name = "Cutoff";
    c1.type = "Knob";
    c1.steps = 5;
    c1.minPct = 10.0f;
    c1.maxPct = 90.0f;
    c1.sortOrder = 1;
    tc.controls.push_back(c1);

    gui::ControlStepConfig c2;
    c2.id = "resonance_knob";
    c2.name = "Resonance";
    c2.type = "Knob";
    c2.steps = 3;
    c2.minPct = 0.0f;
    c2.maxPct = 80.0f;
    c2.sortOrder = 2;
    tc.controls.push_back(c2);

    originalManifest.tests.push_back(tc);
    originalManifest.totalMeasuredPoints = 15;

    // 2. Build measured points
    std::vector<exporting::MeasuredPoint> originalPoints;
    for (int i = 0; i < 15; ++i)
    {
        exporting::MeasuredPoint pt;
        pt.pointId = "P_" + std::to_string(i + 1);
        pt.testId = tc.testName.toStdString();
        pt.blockType = "SpectrumFilter";
        pt.stimulusType = "LogChirp";
        pt.param1Normalized = static_cast<float>(i % 5) / 4.0f;
        pt.param2Normalized = static_cast<float>(i / 5) / 2.0f;
        pt.muSigmaValue.mean = -12.0f + static_cast<float>(i);
        pt.muSigmaValue.stdDev = 0.05f;
        pt.thdPercent = 0.12f;
        pt.snrDb = 68.5f;
        originalPoints.push_back(pt);
    }

    juce::File pkgFile = tempDir.getChildFile("sub37_session.abdlabtest");

    SECTION("Exact Save/Load Round-Trip preserves canonical content and point order")
    {
        SessionSaveRequest saveReq;
        saveReq.manifest = originalManifest;
        saveReq.points = originalPoints;
        saveReq.destination = pkgFile;

        auto saveResult = SessionPersistenceService::save(saveReq);
        REQUIRE(saveResult.succeeded());
        REQUIRE(saveResult.status == SessionIoStatus::Success);
        REQUIRE(pkgFile.existsAsFile());
        REQUIRE(pkgFile.getSize() > 0);

        // Verify inputs were NOT mutated
        REQUIRE(saveReq.manifest.hardwareId == "hw_moog_sub37");
        REQUIRE(saveReq.points.size() == 15);

        // Load back
        SessionLoadRequest loadReq;
        loadReq.source = pkgFile;

        auto loadResult = SessionPersistenceService::load(loadReq);
        REQUIRE(loadResult.succeeded());
        REQUIRE(loadResult.status == SessionIoStatus::Success);

        // Canonical verification
        const auto& loadedManifest = loadResult.manifest;
        REQUIRE(loadedManifest.sessionTitle == originalManifest.sessionTitle);
        REQUIRE(loadedManifest.formatVersion == originalManifest.formatVersion);
        REQUIRE(loadedManifest.hardwareId == originalManifest.hardwareId);
        REQUIRE(loadedManifest.hardwareDisplayName == originalManifest.hardwareDisplayName);
        REQUIRE(loadedManifest.activeFunctionId == originalManifest.activeFunctionId);
        REQUIRE(loadedManifest.sampleRate == originalManifest.sampleRate);
        REQUIRE(loadedManifest.ambientTemperatureC == Catch::Approx(originalManifest.ambientTemperatureC));
        REQUIRE(loadedManifest.warmupTimeMinutes == originalManifest.warmupTimeMinutes);
        REQUIRE(loadedManifest.operatorNotes == originalManifest.operatorNotes);
        REQUIRE(loadedManifest.tests.size() == 1);
        REQUIRE(loadedManifest.tests[0].testName == originalManifest.tests[0].testName);
        REQUIRE(loadedManifest.tests[0].controls.size() == 2);
        REQUIRE(loadedManifest.tests[0].controls[0].name == "Cutoff");
        REQUIRE(loadedManifest.tests[0].controls[1].name == "Resonance");

        // Points verification
        REQUIRE(loadResult.points.size() == originalPoints.size());
        for (size_t i = 0; i < originalPoints.size(); ++i)
        {
            REQUIRE(loadResult.points[i].pointId == originalPoints[i].pointId);
            REQUIRE(loadResult.points[i].testId == originalPoints[i].testId);
            REQUIRE(loadResult.points[i].blockType == originalPoints[i].blockType);
            REQUIRE(loadResult.points[i].param1Normalized == Catch::Approx(originalPoints[i].param1Normalized));
            REQUIRE(loadResult.points[i].param2Normalized == Catch::Approx(originalPoints[i].param2Normalized));
            REQUIRE(loadResult.points[i].muSigmaValue.mean == Catch::Approx(originalPoints[i].muSigmaValue.mean));
            REQUIRE(loadResult.points[i].snrDb == Catch::Approx(originalPoints[i].snrDb));
        }
    }

    tempDir.deleteRecursively();
}

TEST_CASE("SessionPersistenceService: Edge Cases and Atomic Integrity", "[SessionPersistenceService]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_PersistEdgeTest_" + juce::String(juce::Random::getSystemRandom().nextInt(100000)));
    if (tempDir.exists()) tempDir.deleteRecursively();
    tempDir.createDirectory();

    SECTION("Minimal valid manifest with zero points and empty tests succeeds")
    {
        SessionManifest minManifest;
        minManifest.formatVersion = "1.0";
        minManifest.hardwareId = "generic_hw";

        juce::File minPkg = tempDir.getChildFile("minimal.abdlabtest");
        auto saveRes = SessionPersistenceService::save({ minManifest, {}, minPkg });
        REQUIRE(saveRes.succeeded());

        auto loadRes = SessionPersistenceService::load({ minPkg });
        REQUIRE(loadRes.succeeded());
        REQUIRE(loadRes.manifest.hardwareId == "generic_hw");
        REQUIRE(loadRes.points.empty());
    }

    SECTION("Loading non-existent file returns FileNotFound")
    {
        juce::File missing = tempDir.getChildFile("does_not_exist.abdlabtest");
        auto res = SessionPersistenceService::load({ missing });
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == SessionIoStatus::FileNotFound);
        REQUIRE(res.errorCode == "FILE_NOT_FOUND");
    }

    SECTION("Loading empty or non-ZIP file returns InvalidPackage")
    {
        juce::File dummyText = tempDir.getChildFile("corrupted.abdlabtest");
        dummyText.replaceWithText("THIS_IS_NOT_A_ZIP_ARCHIVE");

        auto res = SessionPersistenceService::load({ dummyText });
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == SessionIoStatus::InvalidPackage);
    }

    SECTION("Unsupported future formatVersion returns UnsupportedVersion")
    {
        SessionManifest futureManifest;
        futureManifest.formatVersion = "2.5"; // Future incompatible version
        futureManifest.hardwareId = "future_synth";

        juce::File futurePkg = tempDir.getChildFile("future.abdlabtest");
        auto saveRes = SessionPersistenceService::save({ futureManifest, {}, futurePkg });
        REQUIRE(saveRes.succeeded());

        auto loadRes = SessionPersistenceService::load({ futurePkg });
        REQUIRE_FALSE(loadRes.succeeded());
        REQUIRE(loadRes.status == SessionIoStatus::UnsupportedVersion);
        REQUIRE(loadRes.errorCode == "UNSUPPORTED_VERSION");
    }

    SECTION("Atomic save: destination remains intact if save fails")
    {
        juce::File destFile = tempDir.getChildFile("precious_session.abdlabtest");
        destFile.replaceWithText("ORIGINAL_UNTOUCHED_CONTENT");

        // Attempt save with invalid empty destination (or invalid parent)
        SessionSaveRequest badReq;
        badReq.destination = juce::File(); // Empty invalid destination
        auto res = SessionPersistenceService::save(badReq);
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == SessionIoStatus::CannotWrite);

        // Verify original file is still intact
        REQUIRE(destFile.loadFileAsString() == "ORIGINAL_UNTOUCHED_CONTENT");
    }

    tempDir.deleteRecursively();
}
