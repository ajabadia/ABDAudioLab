#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LoopbackCalibrator.h"
#include "math/FarinaDeconvolver.h"
#include <vector>
#include <cmath>
#include "support/LabTestScratch.h"

using namespace abdaudiolab::math;

TEST_CASE("LoopbackCalibrator Clipping Detection and Invalidation", "[math][loopback][diagnostics]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const float startFreq = 10.0f;
    const float endFreq = 24000.0f;

    auto cleanSweep = FarinaDeconvolver::generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);

    // Añadir cola para ventana IR (4096 muestras) como en captura real
    cleanSweep.resize(cleanSweep.size() + 4096, 0.0f);

    // Escalar a nivel nominal (-3.1 dBFS aprox)
    for (auto& s : cleanSweep)
        s *= 0.7f;

    SECTION("Caso sano: Sin clipping, nivel y planitud correctos -> isCalibrated = true")
    {
        auto healthyData = LoopbackCalibrator::analyzeLoopback(cleanSweep, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE(healthyData.clippingDetected == false);
        REQUIRE(healthyData.clippedSamplesCount == 0);
        REQUIRE(healthyData.peakInDbfs > -40.0f);
        REQUIRE(healthyData.frequencyFlatnessDb < 6.0f);
        REQUIRE(healthyData.isCalibrated == true);
    }

    SECTION("Caso saturado: Con clipping pero nivel y planitud validos -> isCalibrated = false")
    {
        auto clippedSweep = cleanSweep;
        clippedSweep[500] = 1.0f;
        clippedSweep[501] = 1.0f;
        clippedSweep[502] = -1.0f;

        auto clippedData = LoopbackCalibrator::analyzeLoopback(clippedSweep, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE(clippedData.clippingDetected == true);
        REQUIRE(clippedData.clippedSamplesCount == 3);
        REQUIRE(clippedData.peakInDbfs > -40.0f);
        REQUIRE(clippedData.frequencyFlatnessDb < 6.0f);
        REQUIRE_FALSE(clippedData.isCalibrated);
    }
}

TEST_CASE("LoopbackCalibrator DC Offset Measurement", "[math][loopback][diagnostics]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 2000;
    const float expectedDc = 0.08f;
    std::vector<float> recorded(numSamples, expectedDc);

    // Add AC component
    for (int i = 0; i < numSamples; ++i)
    {
        recorded[i] += 0.2f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
    }

    auto data = LoopbackCalibrator::analyzeLoopback(recorded, sampleRate, 0.05);

    REQUIRE_THAT(data.dcOffsetVolts, Catch::Matchers::WithinAbs(expectedDc, 0.01f));
}

TEST_CASE("LoopbackCalibrator Phase Inversion Diagnostic Flag", "[math][loopback][diagnostics]")
{
    LoopbackCalibrationData data;
    data.phaseInversionDetected = true;
    data.phaseInversionCorrelation = -0.95f;
    data.clippingDetected = true;
    data.clippedSamplesCount = 12;
    data.dcOffsetVolts = 0.045f;
    data.isCalibrated = true;

    juce::File tempFile = abdaudiolab::test::scratchDir ("LoopbackDiagnostics")
            .getChildFile ("test_loopback_diag.json");
    tempFile.deleteFile();

    bool saved = LoopbackCalibrator::saveCalibrationToJson(data, tempFile);
    REQUIRE(saved == true);

    auto loaded = LoopbackCalibrator::loadCalibrationFromJson(tempFile);
    REQUIRE(loaded.phaseInversionDetected == true);
    REQUIRE_THAT(loaded.phaseInversionCorrelation, Catch::Matchers::WithinAbs(-0.95f, 1e-4f));
    REQUIRE(loaded.clippingDetected == true);
    REQUIRE(loaded.clippedSamplesCount == 12);
    REQUIRE_THAT(loaded.dcOffsetVolts, Catch::Matchers::WithinAbs(0.045f, 1e-4f));

    tempFile.deleteFile();
}
