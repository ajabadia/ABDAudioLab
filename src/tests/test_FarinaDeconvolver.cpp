#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/FarinaDeconvolver.h"
#include <cmath>

TEST_CASE("FarinaDeconvolver Log Sweep Generation", "[dsp][farina]")
{
    abdaudiolab::math::FarinaDeconvolver deconvolver;
    
    double sampleRate = 96000.0;
    double durationSec = 1.0;
    float startFreq = 20.0f;
    float endFreq = 20000.0f;

    auto sweep = deconvolver.generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);

    size_t expectedSamples = static_cast<size_t>(sampleRate * durationSec);
    REQUIRE(sweep.size() == expectedSamples);

    // Check non-zero amplitude within peak bounds
    float maxAmp = 0.0f;
    for (float sample : sweep)
    {
        maxAmp = std::max(maxAmp, std::abs(sample));
    }

    REQUIRE(maxAmp > 0.5f);
    REQUIRE(maxAmp <= 1.0f);
}

TEST_CASE("FarinaDeconvolver Impulse Response Extraction", "[dsp][farina]")
{
    abdaudiolab::math::FarinaDeconvolver deconvolver;
    
    double sampleRate = 48000.0;
    double durationSec = 0.5;
    float startFreq = 20.0f;
    float endFreq = 20000.0f;

    auto sweep = deconvolver.generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);
    
    // Simulate ideal identity system (response == sweep)
    auto ir = deconvolver.extractImpulseResponse(sweep, sweep, sampleRate, durationSec, startFreq, endFreq);

    REQUIRE_FALSE(ir.empty());

    // Identity system should produce an impulse peak
    float maxPeak = 0.0f;
    for (float sample : ir)
    {
        maxPeak = std::max(maxPeak, std::abs(sample));
    }

    REQUIRE(maxPeak > 0.0001f);
}
