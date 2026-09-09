#include <catch2/catch_test_macros.hpp>
#include "../core/ProfilingSession.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("ProfilingSession: Casio CZ Live Scan Suite Generation", "[core][session][cz]")
{
    SECTION("Default resolution generation (100 DCW steps, 8 envelope stages, 12 DCO notes)")
    {
        auto session = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 100, 8);

        // Verify session-level metadata
        CHECK(session.getMetadata().hardwareName == "casio_cz101_mame_ves");
        CHECK(session.getMetadata().sampleRate == 44100.0);
        CHECK(session.getMetadata().operatorMode == "VIRTUAL_LOOPBACK_ASIO");
        CHECK(session.getMetadata().targetModule == "CASIO_CZ_PD_ENGINE");

        // Total test cases: 100 (DCW) + 8 (ENV) + 12 (DCO) = 120
        REQUIRE(session.getTestCases().size() == 120);

        // Verify DCW Stage 0
        const auto& tcDcw0 = session.getTestCases()[0];
        CHECK(tcDcw0.testId == "TC_CZ_DCW_00");
        CHECK(tcDcw0.functionalBlockType == "WaveShaper");
        CHECK(tcDcw0.queueItemIndex == 0);
        CHECK(tcDcw0.pointIndexInTest == 0);
        CHECK(tcDcw0.stimulusType == audio::StimulusType::SineWave1kHz);
        // Setup actions should be populated from the preset recipe
        CHECK_FALSE(tcDcw0.presetRecipe.setupActions.empty());
        CHECK(tcDcw0.presetRecipe.recipeType == "DIRECT_AUDIO_IN");

        // Verify DCW Stage 99
        const auto& tcDcw99 = session.getTestCases()[99];
        CHECK(tcDcw99.testId == "TC_CZ_DCW_99");
        CHECK(tcDcw99.functionalBlockType == "WaveShaper");
        CHECK(tcDcw99.queueItemIndex == 0);
        CHECK(tcDcw99.pointIndexInTest == 99);

        // Verify ENV Stage 1
        const auto& tcEnv1 = session.getTestCases()[100];
        CHECK(tcEnv1.testId == "TC_CZ_ENV_STEP_1");
        CHECK(tcEnv1.functionalBlockType == "TimeDynamic");
        CHECK(tcEnv1.queueItemIndex == 1);
        CHECK(tcEnv1.pointIndexInTest == 0);
        CHECK(tcEnv1.stimulusType == audio::StimulusType::SyncPulses3);

        // Verify ENV Stage 8
        const auto& tcEnv8 = session.getTestCases()[107];
        CHECK(tcEnv8.testId == "TC_CZ_ENV_STEP_8");
        CHECK(tcEnv8.functionalBlockType == "TimeDynamic");
        CHECK(tcEnv8.queueItemIndex == 1);
        CHECK(tcEnv8.pointIndexInTest == 7);

        // Verify DCO Note 60 (Middle C)
        const auto& tcDco0 = session.getTestCases()[108];
        CHECK(tcDco0.testId == "TC_CZ_DCO_NOTE_60");
        CHECK(tcDco0.functionalBlockType == "NoiseFloor");
        CHECK(tcDco0.queueItemIndex == 2);
        CHECK(tcDco0.pointIndexInTest == 0);
        CHECK(tcDco0.stimulusType == audio::StimulusType::Silence);
        CHECK(tcDco0.isAutonomousSynth);
        CHECK(tcDco0.midiNoteNumber == 60);

        // Verify DCO Note 71 (B4)
        const auto& tcDco11 = session.getTestCases()[119];
        CHECK(tcDco11.testId == "TC_CZ_DCO_NOTE_71");
        CHECK(tcDco11.functionalBlockType == "NoiseFloor");
        CHECK(tcDco11.queueItemIndex == 2);
        CHECK(tcDco11.pointIndexInTest == 11);
        CHECK(tcDco11.isAutonomousSynth);
        CHECK(tcDco11.midiNoteNumber == 71);
    }

    SECTION("Custom and clamped resolution steps")
    {
        // Request steps below minimum limits (DCW < 4, ENV < 1)
        auto sessionMin = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "ASIO_DEVICE", 1, 1);
        // DCW clamped to 4, ENV clamped to 1, DCO always 12 -> 4 + 1 + 12 = 17
        REQUIRE(sessionMin.getTestCases().size() == 17);

        // Request steps above maximum limits (DCW > 100, ENV > 8)
        auto sessionMax = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "ASIO_DEVICE", 200, 50);
        // DCW clamped to 100, ENV clamped to 8, DCO always 12 -> 100 + 8 + 12 = 120
        REQUIRE(sessionMax.getTestCases().size() == 120);
    }

    SECTION("Dynamic serialization to juce::var (toDynamicVar)")
    {
        auto session = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 10, 2);
        // 10 + 2 + 12 = 24
        REQUIRE(session.getTestCases().size() == 24);

        juce::var sessionVar = session.toDynamicVar();
        REQUIRE(sessionVar.isObject());

        auto* obj = sessionVar.getDynamicObject();
        REQUIRE(obj != nullptr);

        CHECK(obj->getProperty("hardwareName").toString() == "casio_cz101_mame_ves");
        CHECK(static_cast<double>(obj->getProperty("sampleRate")) == 44100.0);
        CHECK(obj->getProperty("operatorMode").toString() == "VIRTUAL_LOOPBACK_ASIO");

        auto tcArray = obj->getProperty("testCases");
        REQUIRE(tcArray.isArray());
        CHECK(tcArray.size() == 24);

        // Verify first test case in serialized tree
        auto firstTc = tcArray[0];
        CHECK(firstTc["testId"].toString() == "TC_CZ_DCW_00");
        CHECK(firstTc["blockType"].toString() == "WaveShaper");
        CHECK(static_cast<int>(firstTc["queueIndex"]) == 0);
    }

    SECTION("JSON string and file export (saveProfileToJson & exportSessionToJsonFile)")
    {
        auto session = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 10, 2);
        std::string jsonStr = session.saveProfileToJson();
        REQUIRE_FALSE(jsonStr.empty());

        // Test roundtrip loading from generated JSON
        ProfilingSession loadedSession;
        bool loadedOk = loadedSession.loadProfileFromJson(jsonStr);
        REQUIRE(loadedOk);
        CHECK(loadedSession.getMetadata().hardwareName == "casio_cz101_mame_ves");
        CHECK(loadedSession.getTestCases().size() == 24);

        // Test exporting to temporary session file
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        juce::File tempJson = tempDir.getChildFile("casio_cz101_mame_ves_session_test.json");
        if (tempJson.existsAsFile())
            tempJson.deleteFile();

        bool exported = session.exportSessionToJsonFile(tempJson.getFullPathName().toStdString());
        REQUIRE(exported);
        REQUIRE(tempJson.existsAsFile());
        REQUIRE(tempJson.getSize() > 100);

        ProfilingSession fileLoadedSession;
        REQUIRE(fileLoadedSession.loadProfileFromFile(tempJson.getFullPathName().toStdString()));
        CHECK(fileLoadedSession.getTestCases().size() == 24);

        tempJson.deleteFile();
    }

    SECTION("Assets presets directory initialization (casio_cz101_mame_ves_session.json)")
    {
        juce::File repoDir = juce::File::getCurrentWorkingDirectory();
        juce::File presetsDir = repoDir.getChildFile("assets").getChildFile("presets");
        presetsDir.createDirectory();
        juce::File czSessionFile = presetsDir.getChildFile("casio_cz101_mame_ves_session.json");

        if (!czSessionFile.existsAsFile())
        {
            auto czSuite = ProfilingSession::createCasioCzSuite("casio_cz101_mame_ves", "VIRTUAL_LOOPBACK_ASIO", 100, 8);
            REQUIRE(czSuite.exportSessionToJsonFile(czSessionFile.getFullPathName().toStdString()));
        }

        REQUIRE(czSessionFile.existsAsFile());
        REQUIRE(czSessionFile.getSize() > 1000);

        ProfilingSession loaded;
        REQUIRE(loaded.loadProfileFromFile(czSessionFile.getFullPathName().toStdString()));
        CHECK(loaded.getTestCases().size() == 120);
        CHECK(loaded.getMetadata().hardwareName == "casio_cz101_mame_ves");
    }
}


