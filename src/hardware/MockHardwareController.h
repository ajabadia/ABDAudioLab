#pragma once

#include "HardwareController.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace abdaudiolab::hardware
{

/**
 * @brief In-memory mock hardware with integrated Virtual Analog DSP simulation.
 * 
 * Used for automated self-tests, CI/CD validation, and software loopback verification.
 */
class MockHardwareController : public IHardwareController
{
public:
    MockHardwareController()
    {
        // Default to a 24dB resonant low-pass filter
        resetDsp(96000.0);
    }

    void resetDsp(double newSampleRate)
    {
        sampleRate = newSampleRate;
        cutoffNormalized = 0.5f;
        resonanceNormalized = 0.3f;
        driveNormalized = 0.2f;
        filterState.fill(0.0f);
        updateCoefficients();
    }

    [[nodiscard]] bool isAutomatic() const noexcept override { return true; }

    [[nodiscard]] juce::String getHardwareName() const override
    {
        return "Internal Mock Virtual-Analog Filter (Self-Test)";
    }

    bool connect() override
    {
        connected = true;
        return true;
    }

    void disconnect() override
    {
        connected = false;
    }

    [[nodiscard]] bool isConnected() const noexcept override
    {
        return connected;
    }

    bool setParameter(int paramIndex, float normalizedValue) override
    {
        float norm = std::clamp(normalizedValue, 0.0f, 1.0f);
        switch (paramIndex)
        {
            case 1: // Cutoff
                cutoffNormalized = norm;
                updateCoefficients();
                break;
            case 2: // Resonance
                resonanceNormalized = norm;
                updateCoefficients();
                break;
            case 3: // Drive / Saturation
                driveNormalized = norm;
                break;
            default:
                break;
        }
        return true;
    }

    bool setParameterRaw(int paramIndex, int rawValue) override
    {
        return setParameter(paramIndex, static_cast<float>(std::clamp(rawValue, 0, 127)) / 127.0f);
    }

    bool setupSubmodule(int /*slotIndex*/, uint8_t /*typeId*/) override { return true; }
    bool setPatchCable(uint8_t /*sourceId*/, uint8_t /*destId*/, bool /*isConnected*/) override { return true; }
    void requestStateDump() override {}

    /**
     * @brief Real-time DSP process block simulating analog hardware with subtle thermal drift.
     */
    void processAudioBlock(const float* input, float* output, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        // Simple TPT 4-pole style ladder simulation
        for (int i = 0; i < numSamples; ++i)
        {
            float in = input[i];

            // Nonlinear saturation stage
            float driven = std::tanh(in * (1.0f + driveNormalized * 4.0f));

            // Feedback with resonance
            float fb = filterState[3] * resonanceAmount;
            float u = driven - fb;

            // 4-stage 1-pole cascade
            filterState[0] += g * (std::tanh(u) - filterState[0]);
            filterState[1] += g * (filterState[0] - filterState[1]);
            filterState[2] += g * (filterState[1] - filterState[2]);
            filterState[3] += g * (filterState[2] - filterState[3]);

            // Subtle simulated analog noise floor (-90 dBfs)
            noiseSeed = (noiseSeed * 196314165 + 907633515);
            float thermalNoise = (static_cast<float>(noiseSeed) / 2147483648.0f - 1.0f) * 0.00003f;

            output[i] = filterState[3] + thermalNoise;
        }
    }

private:
    void updateCoefficients()
    {
        // Exponential mapping 20 Hz to 20 kHz
        float cutoffHz = 20.0f * std::pow(1000.0f, cutoffNormalized);
        float wd = 2.0f * 3.14159265358979323846f * cutoffHz;
        float wa = (2.0f * static_cast<float>(sampleRate)) * std::tan(wd / (2.0f * static_cast<float>(sampleRate)));
        g = wa / (2.0f * static_cast<float>(sampleRate));
        g = std::clamp(g, 0.0001f, 0.9999f);

        resonanceAmount = resonanceNormalized * 3.8f;
    }

    bool connected { true };
    double sampleRate { 96000.0 };
    float cutoffNormalized { 0.5f };
    float resonanceNormalized { 0.3f };
    float driveNormalized { 0.2f };

    float g { 0.1f };
    float resonanceAmount { 1.0f };
    std::array<float, 4> filterState { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t noiseSeed { 0x12345678 };
};

} // namespace abdaudiolab::hardware
