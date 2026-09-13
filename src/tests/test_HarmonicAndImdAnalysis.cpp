/**
 * @file test_HarmonicAndImdAnalysis.cpp
 * @brief Catch2 unit tests for Farina H2/H3 harmonic extraction and SMPTE/CCIF IMD analysis.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../math/FarinaDeconvolver.h"
#include "../math/IntermodulationAnalyzer.h"
#include <cmath>

using namespace abdaudiolab::math;

TEST_CASE("FarinaDeconvolver - H2..H5 Harmonic Extraction and THD", "[harmonic][farina]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const float startFreq = 40.0f;
    const float endFreq = 16000.0f;

    auto stimulus = FarinaDeconvolver::generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);
    auto invFilter = FarinaDeconvolver::generateInverseFilter(sampleRate, durationSec, startFreq, endFreq);

    SECTION("Linear loopback has negligible harmonic distortion")
    {
        auto result = FarinaDeconvolver::deconvolve(stimulus, invFilter, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE_FALSE(result.linearIR.empty());
        REQUIRE_FALSE(result.frequenciesHz.empty());
        REQUIRE(result.thdPercent < 0.6f);
        REQUIRE(result.h2Percent < 0.3f);
        REQUIRE(result.h3Percent < 0.3f);
        REQUIRE(result.h4Percent < 0.2f);
        REQUIRE(result.h5Percent < 0.2f);
        REQUIRE(result.thdVsFreqPercent.size() == result.frequenciesHz.size());
    }

    SECTION("Cubic non-linearity produces prominent H3 harmonic peak")
    {
        // Apply cubic soft saturation: y = x - 0.15 * x^3 (generates odd harmonics, predominantly H3)
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            distorted[i] = x - 0.15f * x * x * x;
        }

        auto result = FarinaDeconvolver::deconvolve(distorted, invFilter, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE_FALSE(result.h3IR.empty());
        REQUIRE(result.h3Percent > 0.5f);
        REQUIRE(result.thdPercent > 0.5f);
        REQUIRE_FALSE(result.h3MagnitudeDb.empty());
    }

    SECTION("Quadratic asymmetry produces prominent H2 harmonic peak")
    {
        // Apply asymmetric quadratic non-linearity: y = x + 0.1 * x^2 (generates even harmonics, predominantly H2)
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            distorted[i] = x + 0.1f * x * x;
        }

        auto result = FarinaDeconvolver::deconvolve(distorted, invFilter, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE_FALSE(result.h2IR.empty());
        REQUIRE(result.h2Percent > 0.5f);
        REQUIRE(result.thdPercent > 0.5f);
        REQUIRE_FALSE(result.h2MagnitudeDb.empty());
    }

    SECTION("Quartic non-linearity produces prominent H4 harmonic peak")
    {
        // Apply 4th-order non-linearity: y = x + 0.08 * x^4
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            float x2 = x * x;
            distorted[i] = x + 0.08f * x2 * x2;
        }

        auto result = FarinaDeconvolver::deconvolve(distorted, invFilter, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE_FALSE(result.h4IR.empty());
        REQUIRE(result.h4Percent > 0.3f);
        REQUIRE(result.thdPercent > 0.3f);
        REQUIRE_FALSE(result.h4MagnitudeDb.empty());
    }

    SECTION("Quintic non-linearity produces prominent H5 harmonic peak")
    {
        // Apply 5th-order odd non-linearity: y = x - 0.06 * x^5
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            float x2 = x * x;
            distorted[i] = x - 0.06f * x2 * x2 * x;
        }

        auto result = FarinaDeconvolver::deconvolve(distorted, invFilter, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE_FALSE(result.h5IR.empty());
        REQUIRE(result.h5Percent > 0.3f);
        REQUIRE(result.thdPercent > 0.3f);
        REQUIRE_FALSE(result.h5MagnitudeDb.empty());
    }
}

TEST_CASE("IntermodulationAnalyzer - SMPTE Analysis", "[imd][smpte]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.5;

    auto stimulus = IntermodulationAnalyzer::generateSmpteStimulus(sampleRate, durationSec, 60.0f, 7000.0f, 4.0f);
    REQUIRE(stimulus.size() == static_cast<size_t>(sampleRate * durationSec));

    SECTION("Linear transmission has negligible SMPTE IMD")
    {
        auto result = IntermodulationAnalyzer::analyzeSmpte(stimulus, sampleRate, 60.0f, 7000.0f);
        REQUIRE(result.totalImdPercent < 0.6f);
        REQUIRE(result.d2Percent < 0.6f);
        REQUIRE(result.d3Percent < 0.4f);
    }

    SECTION("Non-linear soft clipping produces measurable SMPTE sidebands")
    {
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            distorted[i] = std::tanh(1.5f * x);
        }

        auto result = IntermodulationAnalyzer::analyzeSmpte(distorted, sampleRate, 60.0f, 7000.0f);
        REQUIRE(result.totalImdPercent > 1.0f);
        REQUIRE((result.d2Percent + result.d3Percent) > 0.8f);
    }
}

TEST_CASE("IntermodulationAnalyzer - CCIF Twin-Tone Analysis", "[imd][ccif]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.5;

    auto stimulus = IntermodulationAnalyzer::generateCcifStimulus(sampleRate, durationSec, 19000.0f, 20000.0f);
    REQUIRE(stimulus.size() == static_cast<size_t>(sampleRate * durationSec));

    SECTION("Linear transmission has negligible CCIF IMD")
    {
        auto result = IntermodulationAnalyzer::analyzeCcif(stimulus, sampleRate, 19000.0f, 20000.0f);
        REQUIRE(result.totalImdPercent < 0.05f);
    }

    SECTION("Asymmetric distortion generates 2nd-order difference frequency (1 kHz)")
    {
        std::vector<float> distorted(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            float x = stimulus[i];
            distorted[i] = x + 0.2f * x * x; // Quadratic term produces (f2 - f1)
        }

        auto result = IntermodulationAnalyzer::analyzeCcif(distorted, sampleRate, 19000.0f, 20000.0f);
        REQUIRE(result.d2Percent > 1.0f);
        REQUIRE(result.totalImdPercent > 1.0f);
    }
}
