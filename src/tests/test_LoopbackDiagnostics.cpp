#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LoopbackCalibrator.h"
#include <vector>
#include <cmath>

using namespace abdaudiolab::math;

TEST_CASE("LoopbackCalibrator Clipping Detection", "[math][loopback][diagnostics]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 2000;
    std::vector<float> recorded(numSamples, 0.2f);

    // Inject clipped samples (>= 0.999f)
    recorded[500] = 1.0f;
    recorded[501] = 1.0f;
    recorded[502] = -1.0f;

    auto data = LoopbackCalibrator::analyzeLoopback(recorded, sampleRate, 0.05);

    REQUIRE(data.clippingDetected == true);
    REQUIRE(data.clippedSamplesCount == 3);
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
    const double sampleRate = 48000.0;
    LoopbackCalibrationData data;
    data.phaseInversionDetected = true;
    data.phaseInversionCorrelation = -0.95f;
    data.clippingDetected = true;
    data.clippedSamplesCount = 12;
    data.dcOffsetVolts = 0.045f;
    data.isCalibrated = true;

    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_loopback_diag.json");
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
