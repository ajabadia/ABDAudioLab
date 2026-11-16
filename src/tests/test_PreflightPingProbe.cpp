#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math/PreflightPingProbe.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace abdaudiolab::math;

TEST_CASE("PreflightPingProbe Stimulus Generation", "[PreflightPingProbe][DSP]")
{
    const double sampleRate = 48000.0;
    const float durationSec = 1.5f;
    const float freqHz = 1000.0f;
    const float targetDbfs = -12.0f;
    const float rampMs = 50.0f;

    auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, freqHz, targetDbfs, rampMs);

    REQUIRE(stimulus.size() == static_cast<size_t>(sampleRate * durationSec));

    // Check fade-in starts at 0
    REQUIRE(std::abs(stimulus.front()) == Catch::Approx(0.0f).margin(1e-4f));
    // Check fade-out ends at 0
    REQUIRE(std::abs(stimulus.back()) == Catch::Approx(0.0f).margin(1e-4f));

    // Peak amplitude check (~ -12 dBFS = 0.25118864)
    float peak = 0.0f;
    for (float s : stimulus)
    {
        if (std::abs(s) > peak)
            peak = std::abs(s);
    }

    float peakDbfs = 20.0f * std::log10(peak);
    REQUIRE(peakDbfs == Catch::Approx(targetDbfs).margin(0.15f));
}

TEST_CASE("PreflightPingProbe Metrological Analysis", "[PreflightPingProbe][Diagnostics]")
{
    const double sampleRate = 48000.0;
    const float durationSec = 1.5f;

    SECTION("Empty or silent buffer fails continuity check")
    {
        std::vector<float> silence(static_cast<size_t>(sampleRate * durationSec), 0.0f);
        auto metrics = PreflightPingProbe::analyzeResponse(silence, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Fail_NoSignal);
        REQUIRE(metrics.rmsDbfs <= -60.0f);
        REQUIRE(metrics.diagnosticMessage.find("No signal detected") != std::string::npos);
    }

    SECTION("Ideal loopback response passes metrological preflight")
    {
        // 1 kHz sine at -12 dBFS
        auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, 1000.0f, -12.0f, 50.0f);
        auto metrics = PreflightPingProbe::analyzeResponse(stimulus, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Pass);
        REQUIRE(metrics.peakDbfs == Catch::Approx(-12.0f).margin(0.2f));
        REQUIRE(metrics.rmsDbfs > -20.0f);
        REQUIRE(metrics.humEnergyRatioDbc < -40.0f);
        REQUIRE(metrics.diagnosticMessage.find("Preflight signal verified") != std::string::npos);
    }

    SECTION("Clipping response triggers Fail_Clipping")
    {
        // Signal reaching 0 dBFS (peak = 1.0f >= -0.5 dBFS)
        auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, 1000.0f, 0.0f, 50.0f);
        auto metrics = PreflightPingProbe::analyzeResponse(stimulus, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Fail_Clipping);
        REQUIRE(metrics.peakDbfs >= -0.5f);
        REQUIRE(metrics.diagnosticMessage.find("Clipping") != std::string::npos);
    }

    SECTION("Severe 50 Hz mains hum triggers Fail_GroundLoopHum")
    {
        // 1 kHz at -18 dBFS with strong 50 Hz hum at -10 dBFS
        const size_t n = static_cast<size_t>(sampleRate * durationSec);
        std::vector<float> contaminated(n, 0.0f);

        const float amp1k = std::pow(10.0f, -18.0f / 20.0f);
        const float ampHum = std::pow(10.0f, -10.0f / 20.0f); // 8 dB higher than 1 kHz!

        for (size_t i = 0; i < n; ++i)
        {
            double t = static_cast<double>(i) / sampleRate;
            contaminated[i] = static_cast<float>(amp1k * std::sin(2.0 * M_PI * 1000.0 * t) +
                                                 ampHum * std::sin(2.0 * M_PI * 50.0 * t));
        }

        auto metrics = PreflightPingProbe::analyzeResponse(contaminated, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Fail_GroundLoopHum);
        REQUIRE(metrics.humEnergyRatioDbc > -24.0f);
        REQUIRE(metrics.diagnosticMessage.find("ground loop") != std::string::npos);
    }

    SECTION("Sub-optimal but safe low-level signal issues Warning_LowLevel")
    {
        // 1 kHz at -32 dBFS (low SNR warning)
        auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, 1000.0f, -32.0f, 50.0f);
        auto metrics = PreflightPingProbe::analyzeResponse(stimulus, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Warning_LowLevel);
        REQUIRE(metrics.peakDbfs < -24.0f);
        REQUIRE(metrics.rmsDbfs > -60.0f);
        REQUIRE(metrics.diagnosticMessage.find("low") != std::string::npos);
    }

    SECTION("High level near headroom boundary issues Warning_HighLevel")
    {
        // 1 kHz at -3.0 dBFS (safe from clip but tight headroom)
        auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, 1000.0f, -3.0f, 50.0f);
        auto metrics = PreflightPingProbe::analyzeResponse(stimulus, sampleRate, 1000.0f);

        REQUIRE(metrics.status == PreflightStatus::Warning_HighLevel);
        REQUIRE(metrics.peakDbfs > -6.0f);
        REQUIRE(metrics.peakDbfs < -0.5f);
        REQUIRE(metrics.diagnosticMessage.find("high") != std::string::npos);
    }
}
