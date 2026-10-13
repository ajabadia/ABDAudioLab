/**
 * @file test_LoopbackProfileStore.cpp
 * @brief Hermetic unit tests for LoopbackProfile serialization, store persistence, and async loading.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "calibration/LoopbackProfile.h"
#include "calibration/LoopbackProfileStore.h"
#include "calibration/AsyncLoopbackProfileLoader.h"

using namespace abdaudiolab;
using namespace abdaudiolab::calibration;

namespace
{
    juce::File createTempTestDirectory()
    {
        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("ABDAudioLab_Tests_Loopback_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()));
        tempDir.createDirectory();
        return tempDir;
    }

    LoopbackProfile createSampleProfile(const std::string& devName, double sRate, int bufSize)
    {
        LoopbackProfile p;
        p.schemaVersion = LoopbackProfile::kCurrentSchemaVersion;
        p.deviceName = devName;
        p.driverType = "ASIO";
        p.sampleRate = sRate;
        p.bufferSizeSamples = bufSize;
        p.inputChannelIndex = 0;
        p.inputChannelLabel = "Input 1";
        p.outputChannelIndex = 0;
        p.outputChannelLabel = "Output 1";
        p.profileId = "loopback-" + p.getHardwareKey();
        p.createdAt = "2026-10-13T16:00:00Z";

        p.isCalibrated = true;
        p.peakInDbfs = -3.2f;
        p.recommendedTrimGain = 1.05f;
        p.targetHeadroomDbfs = -3.0f;
        p.roundTripLatencyMs = 5.333f;
        p.latencySamples = 256;
        p.thdPlusNoisePercent = 0.002f;
        p.snrDb = 98.5f;
        p.frequencyFlatnessDb = 0.15f;
        p.phaseInversionDetected = false;
        p.clippingDetected = false;

        p.freqsHz = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
        p.magnitudeDb = { -0.1f, 0.0f, 0.05f, -0.05f, -0.2f };
        p.inverseCorrectionDb = { 0.1f, 0.0f, -0.05f, 0.05f, 0.2f };

        p.sha256Hash = p.computeSha256();
        return p;
    }
}

TEST_CASE("LoopbackProfile: Serializacion y Deserializacion .abdloop", "[calibration][abdloop]")
{
    const auto original = createSampleProfile("Universal Audio Apollo Twin", 48000.0, 256);

    SECTION("Serializacion a JSON y parseo simetrico")
    {
        const auto json = original.toJson();
        const auto parsedOpt = LoopbackProfile::fromJson(json);

        REQUIRE(parsedOpt.has_value());
        const auto& parsed = *parsedOpt;

        CHECK(parsed.schemaVersion == original.schemaVersion);
        CHECK(parsed.profileId == original.profileId);
        CHECK(parsed.deviceName == original.deviceName);
        CHECK(parsed.driverType == original.driverType);
        CHECK(parsed.sampleRate == Catch::Approx(original.sampleRate));
        CHECK(parsed.bufferSizeSamples == original.bufferSizeSamples);
        CHECK(parsed.isCalibrated == original.isCalibrated);
        CHECK(parsed.latencySamples == original.latencySamples);
        CHECK(parsed.snrDb == Catch::Approx(original.snrDb));
        CHECK(parsed.freqsHz.size() == original.freqsHz.size());
        CHECK(parsed.inverseCorrectionDb.size() == original.inverseCorrectionDb.size());
        CHECK(parsed.verifyHash() == true);
    }

    SECTION("Calculo determinista de Hardware Key")
    {
        const auto key1 = LoopbackProfile::generateHardwareKey("Universal Audio Apollo Twin", 48000.0, 256);
        const auto key2 = LoopbackProfile::generateHardwareKey("universal audio apollo twin", 48000.0, 256);
        CHECK(key1 == key2);
        CHECK(key1.find("apollo") != std::string::npos);
        CHECK(key1.find("48000") != std::string::npos);
        CHECK(key1.find("256") != std::string::npos);
    }

    SECTION("Deteccion de manipulacion / hash corrupto")
    {
        auto modified = original;
        REQUIRE(modified.verifyHash() == true);

        // Modificamos una metrica sin recomputar hash
        modified.snrDb = 50.0f;
        CHECK(modified.verifyHash() == false);
    }
}

TEST_CASE("LoopbackProfileStore: Persistencia atomica en disco y tolerancia a fallos", "[calibration][store]")
{
    const auto tempDir = createTempTestDirectory();
    LoopbackProfileStore store(tempDir);

    SECTION("Guardado atomico en dos fases y lectura")
    {
        const auto profile = createSampleProfile("RME Babyface Pro", 96000.0, 128);
        const auto saveResult = store.saveProfile(profile, true);

        REQUIRE(saveResult.success == true);
        REQUIRE(!saveResult.filePath.empty());

        const juce::File targetFile(saveResult.filePath);
        CHECK(targetFile.existsAsFile() == true);
        CHECK(targetFile.getFileExtension() == ".abdloop");

        // El archivo .tmp debe haberse limpiado
        const juce::File tmpFile = tempDir.getChildFile(targetFile.getFileName() + ".tmp");
        CHECK(tmpFile.existsAsFile() == false);

        // Carga directa desde archivo
        const auto loadedOpt = store.loadProfile(targetFile);
        REQUIRE(loadedOpt.has_value());
        CHECK(loadedOpt->deviceName == "RME Babyface Pro");
        CHECK(loadedOpt->sampleRate == Catch::Approx(96000.0));
        CHECK(loadedOpt->bufferSizeSamples == 128);
    }

    SECTION("Tolerancia ante archivos corruptos o truncados")
    {
        const juce::File corruptFile = tempDir.getChildFile("corrupted_profile.abdloop");
        corruptFile.replaceWithText("{ \"schemaVersion\": 1, \"invalid_json\": [", false, false, "utf-8");

        const auto loaded = store.loadProfile(corruptFile);
        CHECK(loaded.has_value() == false);

        // listProfiles ignora los corruptos limpiamente
        const auto list = store.listProfiles();
        CHECK(list.empty());
    }

    SECTION("Autodeteccion y matching de perfiles por hardware")
    {
        const auto profFocusrite = createSampleProfile("Focusrite Scarlett 2i2 USB", 48000.0, 256);
        const auto profMotu = createSampleProfile("MOTU M4", 44100.0, 512);

        REQUIRE(store.saveProfile(profFocusrite).success == true);
        REQUIRE(store.saveProfile(profMotu).success == true);

        // 1. Busqueda exacta
        auto match = store.findMatchingProfile("Focusrite Scarlett 2i2 USB", 48000.0, 256);
        REQUIRE(match.has_value());
        CHECK(match->deviceName == "Focusrite Scarlett 2i2 USB");
        CHECK(match->bufferSizeSamples == 256);

        // 2. Busqueda insensible a mayusculas/minusculas
        auto matchLower = store.findMatchingProfile("focusrite scarlett 2i2 usb", 48000.0, 256);
        REQUIRE(matchLower.has_value());
        CHECK(matchLower->deviceName == "Focusrite Scarlett 2i2 USB");

        // 3. Tolerancia de sample rate (±1.0 Hz)
        auto matchSrTol = store.findMatchingProfile("MOTU M4", 44100.3, 512);
        REQUIRE(matchSrTol.has_value());
        CHECK(matchSrTol->deviceName == "MOTU M4");

        // 4. Fallback si el buffer size es diferente pero la interfaz y sample rate coinciden
        auto matchBufFallback = store.findMatchingProfile("MOTU M4", 44100.0, 1024);
        REQUIRE(matchBufFallback.has_value());
        CHECK(matchBufFallback->deviceName == "MOTU M4");

        // 5. Interfaz no existente
        auto noMatch = store.findMatchingProfile("Unknown Device", 48000.0, 256);
        CHECK(noMatch.has_value() == false);
    }

    tempDir.deleteRecursively();
}

TEST_CASE("AsyncLoopbackProfileLoader: Carga asincrona no bloqueante", "[calibration][async]")
{
    const auto tempDir = createTempTestDirectory();
    LoopbackProfileStore store(tempDir);

    const auto profile = createSampleProfile("Solid State Logic SSL 2+", 48000.0, 256);
    REQUIRE(store.saveProfile(profile).success == true);

    std::optional<LoopbackProfile> loadedResult;
    bool callbackFired = false;

    // Ejecucion directa del ThreadPoolJob para validar comportamiento hermetico
    AsyncLoopbackProfileLoader loader(
        store,
        "Solid State Logic SSL 2+",
        48000.0,
        256,
        [&](std::optional<LoopbackProfile> res) {
            loadedResult = std::move(res);
            callbackFired = true;
        });

    auto status = loader.runJob();
    CHECK(status == juce::ThreadPoolJob::jobHasFinished);

    // Procesamos la cola de mensajes de JUCE para despachar el callAsync
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    CHECK(callbackFired == true);
    REQUIRE(loadedResult.has_value());
    CHECK(loadedResult->deviceName == "Solid State Logic SSL 2+");

    tempDir.deleteRecursively();
}
