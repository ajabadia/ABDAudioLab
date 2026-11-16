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
        sampleRate.store(newSampleRate > 0.0 ? newSampleRate : 96000.0, std::memory_order_release);
        cutoffNormalized.store(0.5f, std::memory_order_release);
        resonanceNormalized.store(0.3f, std::memory_order_release);
        driveNormalized.store(0.2f, std::memory_order_release);
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
        connected.store(true, std::memory_order_release);
        return true;
    }

    void disconnect() override
    {
        connected.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool isConnected() const noexcept override
    {
        return connected.load(std::memory_order_relaxed);
    }

    bool setParameter(int paramIndex, float normalizedValue) override
    {
        float norm = std::clamp(normalizedValue, 0.0f, 1.0f);
        switch (paramIndex)
        {
            case 1: // Cutoff
                cutoffNormalized.store(norm, std::memory_order_release);
                updateCoefficients();
                break;
            case 2: // Resonance
                resonanceNormalized.store(norm, std::memory_order_release);
                updateCoefficients();
                break;
            case 3: // Drive / Saturation
                driveNormalized.store(norm, std::memory_order_release);
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
     *        Guaranteed lock-free with zero allocations and zero mutexes.
     */
    void processAudioBlock(const float* input, float* output, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        const float currentDrive = driveNormalized.load(std::memory_order_relaxed);
        const float currentG = g.load(std::memory_order_relaxed);
        const float currentRes = resonanceAmount.load(std::memory_order_relaxed);

        // Simple TPT 4-pole style ladder simulation
        for (int i = 0; i < numSamples; ++i)
        {
            float in = input[i];

            // Nonlinear saturation stage
            float driven = std::tanh(in * (1.0f + currentDrive * 4.0f));

            // Feedback with resonance
            float fb = filterState[3] * currentRes;
            float u = driven - fb;

            // 4-stage 1-pole cascade
            filterState[0] += currentG * (std::tanh(u) - filterState[0]);
            filterState[1] += currentG * (filterState[0] - filterState[1]);
            filterState[2] += currentG * (filterState[1] - filterState[2]);
            filterState[3] += currentG * (filterState[2] - filterState[3]);

            // Subtle simulated analog noise floor (-90 dBfs)
            noiseSeed = (noiseSeed * 196314165 + 907633515);
            float thermalNoise = (static_cast<float>(noiseSeed) / 2147483648.0f - 1.0f) * 0.00003f;

            output[i] = filterState[3] + thermalNoise;
        }
    }

    bool sendMidiMessage(const juce::MidiMessage& msg) override
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        sentMessages.push_back(msg);
        return true;
    }

    [[nodiscard]] std::vector<juce::MidiMessage> getSentMessages() const
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        return sentMessages;
    }

    void clearSentMessages() noexcept
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        sentMessages.clear();
    }

    [[nodiscard]] bool hasReceivedNoteOn(int noteNumber = -1) const
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        for (const auto& m : sentMessages)
        {
            if (m.isNoteOn())
            {
                if (noteNumber < 0 || m.getNoteNumber() == noteNumber)
                    return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool hasReceivedNoteOff(int noteNumber = -1) const
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        for (const auto& m : sentMessages)
        {
            if (m.isNoteOff())
            {
                if (noteNumber < 0 || m.getNoteNumber() == noteNumber)
                    return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool hasReceivedAllNotesOff() const
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        for (const auto& m : sentMessages)
        {
            if (m.isAllNotesOff())
                return true;
        }
        return false;
    }

    [[nodiscard]] int getActiveNoteCount() const
    {
        const std::lock_guard<std::mutex> lock(midiMutex);
        int count = 0;
        for (const auto& m : sentMessages)
        {
            if (m.isNoteOn() && m.getVelocity() > 0)
                count++;
            else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                count = std::max(0, count - 1);
            else if (m.isAllNotesOff())
                count = 0;
        }
        return count;
    }

private:
    void updateCoefficients()
    {
        // Exponential mapping 20 Hz to 20 kHz
        float cutoff = cutoffNormalized.load(std::memory_order_relaxed);
        double sr = sampleRate.load(std::memory_order_relaxed);
        if (sr <= 0.0) sr = 96000.0;
        float cutoffHz = 20.0f * std::pow(1000.0f, cutoff);
        float wd = 2.0f * 3.14159265358979323846f * cutoffHz;
        float wa = (2.0f * static_cast<float>(sr)) * std::tan(wd / (2.0f * static_cast<float>(sr)));
        float newG = wa / (2.0f * static_cast<float>(sr));
        newG = std::clamp(newG, 0.0001f, 0.9999f);
        g.store(newG, std::memory_order_release);

        float res = resonanceNormalized.load(std::memory_order_relaxed);
        resonanceAmount.store(res * 3.8f, std::memory_order_release);
    }

    std::atomic<bool> connected { true };
    std::atomic<double> sampleRate { 96000.0 };
    std::atomic<float> cutoffNormalized { 0.5f };
    std::atomic<float> resonanceNormalized { 0.3f };
    std::atomic<float> driveNormalized { 0.2f };

    std::atomic<float> g { 0.1f };
    std::atomic<float> resonanceAmount { 1.0f };
    std::array<float, 4> filterState { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t noiseSeed { 0x12345678 };
    mutable std::mutex midiMutex;
    std::vector<juce::MidiMessage> sentMessages;
};

} // namespace abdaudiolab::hardware
