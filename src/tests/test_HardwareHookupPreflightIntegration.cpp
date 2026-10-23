#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/HardwareContractRegistry.h"
#include "core/LabResourcePaths.h"
#include "math/PreflightPingProbe.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::core;
using namespace abdaudiolab::math;

TEST_CASE("Hardware Hookup & Preflight End-to-End Integration", "[HardwareHookup][Preflight][Integration]")
{
    HardwareContractRegistry registry;

    SECTION("Pedal profile loads physical hookup steps and passes acoustic ping preflight")
    {
        const auto pedalFile = repoResource("contracts/hardware/boss_ds1_distortion.json");

        if (pedalFile.existsAsFile())
        {
            HardwareContract contract;
            juce::String warning;
            bool ok = registry.loadProfileResilient(pedalFile, contract, warning);

            REQUIRE(ok);
            REQUIRE(contract.id == "boss_ds1_distortion");

            // Verify Physical Routing Steps
            REQUIRE(contract.routingGuide.steps.size() >= 4);
            REQUIRE(contract.routingGuide.stimulusOutput.find("DS-1") != std::string::npos);
            REQUIRE(contract.routingGuide.responseInput.find("DS-1") != std::string::npos);

            // Verify Lifecycle Init Patch / Pot Alignment
            REQUIRE(contract.lifecycle.preSessionSetup.size() == 3);
            REQUIRE(contract.lifecycle.preSessionSetup[0].method == HardwareMethod::MANUAL_PROMPT);
            REQUIRE(contract.lifecycle.preSessionSetup[0].normalizedValue == 0.5f);

            // Run Acoustic Preflight Ping Probe simulation
            const double sampleRate = 48000.0;
            const float durationSec = 1.5f;

            // Generate nominal 1 kHz stimulus at -12 dBFS
            auto stimulus = PreflightPingProbe::generateStimulus(sampleRate, durationSec, 1000.0f, -12.0f, 50.0f);
            REQUIRE(stimulus.size() == static_cast<size_t>(sampleRate * durationSec));

            // Simulate DUT unity gain return with clean SNR
            auto metrics = PreflightPingProbe::analyzeResponse(stimulus, sampleRate, 1000.0f);
            REQUIRE(metrics.status == PreflightStatus::Pass);
            REQUIRE(metrics.peakDbfs == Catch::Approx(-12.0f).margin(0.2f));
            REQUIRE(metrics.rmsDbfs > -20.0f);
            REQUIRE(metrics.humEnergyRatioDbc < -40.0f);
        }
    }

    SECTION("Synthesizer profile loads automated MIDI/SysEx actions and fails preflight when disconnected")
    {
        const auto synthFile = repoResource("contracts/hardware/behringer_deepmind12.json");

        if (synthFile.existsAsFile())
        {
            HardwareContract contract;
            juce::String warning;
            bool ok = registry.loadProfileResilient(synthFile, contract, warning);

            REQUIRE(ok);
            REQUIRE(contract.id == "behringer_deepmind12");

            // Verify Automated Setup Actions
            REQUIRE(contract.lifecycle.preSessionSetup.size() >= 3);
            REQUIRE(contract.lifecycle.preSessionSetup[0].method == HardwareMethod::MIDI_CC);
            REQUIRE(contract.lifecycle.preSessionSetup[1].controlNumber == 121); // Reset all controllers

            // Disconnected return simulation (silence)
            std::vector<float> disconnectedBuffer(static_cast<size_t>(48000.0 * 1.5), 0.0f);
            auto metrics = PreflightPingProbe::analyzeResponse(disconnectedBuffer, 48000.0, 1000.0f);

            REQUIRE(metrics.status == PreflightStatus::Fail_NoSignal);
            REQUIRE(metrics.rmsDbfs <= -60.0f);
        }
    }

    SECTION("Eurorack profile loads cabling steps and detects ground loop hum")
    {
        const auto eurorackFile = repoResource("contracts/hardware/manual_eurorack_vcf.json");

        if (eurorackFile.existsAsFile())
        {
            HardwareContract contract;
            juce::String warning;
            bool ok = registry.loadProfileResilient(eurorackFile, contract, warning);

            REQUIRE(ok);
            REQUIRE(contract.id == "manual_eurorack_vcf");
            REQUIRE(contract.routingGuide.steps.size() >= 4);

            // Ground loop contaminated return simulation
            const double sampleRate = 48000.0;
            const size_t n = static_cast<size_t>(sampleRate * 1.5);
            std::vector<float> humBuffer(n, 0.0f);

            for (size_t i = 0; i < n; ++i)
            {
                double t = static_cast<double>(i) / sampleRate;
                // 1 kHz fundamental at -20 dBFS + 50 Hz hum at -12 dBFS (8 dB stronger!)
                humBuffer[i] = static_cast<float>(0.1 * std::sin(2.0 * 3.141592653589793 * 1000.0 * t) +
                                                  0.25 * std::sin(2.0 * 3.141592653589793 * 50.0 * t));
            }

            auto metrics = PreflightPingProbe::analyzeResponse(humBuffer, sampleRate, 1000.0f);
            REQUIRE(metrics.status == PreflightStatus::Fail_GroundLoopHum);
            REQUIRE(metrics.humEnergyRatioDbc > -24.0f);
        }
    }
}
