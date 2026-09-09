#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LoopbackCalibrator.h"
#include <vector>
#include <cmath>

using namespace abdaudiolab::math;

TEST_CASE("LoopbackCalibrator De-Coloring Active Inverse Compensation", "[math][loopback][decolor]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 4096;

    // Create a mock calibration profile with known attenuation (-3 dB at high frequencies)
    LoopbackCalibrationData calData;
    calData.isCalibrated = true;
    calData.sampleRate = sampleRate;

    const int numBins = 1024;
    calData.freqsHz.resize(numBins);
    calData.magnitudeDb.resize(numBins);
    calData.inverseCorrectionDb.resize(numBins);

    for (int i = 0; i < numBins; ++i)
    {
        float f = static_cast<float>(i) * (static_cast<float>(sampleRate) / 2048.0f);
        calData.freqsHz[i] = f;
        // Simulate high-frequency roll-off (-3dB at 10kHz)
        float rollOff = (f > 1000.0f) ? -3.0f * (f / 10000.0f) : 0.0f;
        calData.magnitudeDb[i] = rollOff;
        calData.inverseCorrectionDb[i] = -rollOff; // Exact inverse
    }

    // Audio signal: 10 kHz pure sine wave
    std::vector<float> audio(numSamples, 0.0f);
    const float testFreq = 10000.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        audio[i] = 0.5f * std::sin(2.0f * 3.14159265f * testFreq * static_cast<float>(i) / static_cast<float>(sampleRate));
    }

    // Apply inverse compensation
    LoopbackCalibrator::applyInverseCompensation(audio, calData, sampleRate);

    // Peak amplitude of compensated 10 kHz wave should have increased by ~+3 dB (factor ~1.41)
    float maxCompensated = 0.0f;
    for (size_t i = 1000; i < 3000; ++i)
    {
        float a = std::abs(audio[i]);
        if (a > maxCompensated) maxCompensated = a;
    }

    // Original peak was 0.5, with +3dB compensation it should be near 0.707f
    REQUIRE(maxCompensated > 0.55f);
}
