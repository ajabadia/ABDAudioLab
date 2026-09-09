#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/FarinaDeconvolver.h"
#include <vector>
#include <cmath>

using namespace abdaudiolab::math;

TEST_CASE("FarinaDeconvolver Phase and Group Delay Computation on Pure Delay", "[math][farina][phase]")
{
    const double sampleRate = 48000.0;
    const int delaySamples = 10;
    const int irSize = 256;

    std::vector<float> ir(irSize, 0.0f);
    ir[delaySamples] = 1.0f; // Pure delay impulse

    std::vector<float> freqs;
    std::vector<float> phaseRad;
    std::vector<float> groupDelay;

    FarinaDeconvolver::computePhaseAndGroupDelay(ir, sampleRate, freqs, phaseRad, groupDelay);

    REQUIRE(!freqs.empty());
    REQUIRE(freqs.size() == phaseRad.size());
    REQUIRE(freqs.size() == groupDelay.size());

    // Phase at DC (0 Hz) should be near 0
    REQUIRE_THAT(phaseRad[0], Catch::Matchers::WithinAbs(0.0f, 1e-3f));

    // For a pure delay of D samples, group delay is constant: tau_g = D samples
    // Check in mid-frequency bins
    for (size_t i = 10; i < 50; ++i)
    {
        REQUIRE_THAT(groupDelay[i], Catch::Matchers::WithinAbs(static_cast<float>(delaySamples), 0.25f));
    }
}

TEST_CASE("FarinaDeconvolver Phase Response Linear Slope for Known Latency", "[math][farina][groupdelay]")
{
    const double sampleRate = 44100.0;
    const int delaySamples = 5;
    const int irSize = 512;

    std::vector<float> ir(irSize, 0.0f);
    ir[delaySamples] = 1.0f;

    std::vector<float> freqs;
    std::vector<float> phaseRad;
    std::vector<float> groupDelay;

    FarinaDeconvolver::computePhaseAndGroupDelay(ir, sampleRate, freqs, phaseRad, groupDelay);

    // Phase slope d(phi)/df = - 2 * pi * D / sampleRate
    const float expectedSlope = -2.0f * 3.14159265f * static_cast<float>(delaySamples) / static_cast<float>(sampleRate);

    // Verify slope between two audible bins
    size_t bin1 = 20;
    size_t bin2 = 40;
    float deltaF = freqs[bin2] - freqs[bin1];
    float deltaPhi = phaseRad[bin2] - phaseRad[bin1];
    float measuredSlope = deltaPhi / deltaF;

    REQUIRE_THAT(measuredSlope, Catch::Matchers::WithinAbs(expectedSlope, 0.01f * std::abs(expectedSlope) + 1e-4f));
}
