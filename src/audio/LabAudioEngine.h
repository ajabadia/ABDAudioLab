#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include "LabStimulusGenerator.h"
#include "LabAudioReceiver.h"
#include "../hardware/MockHardwareController.h"

namespace abdaudiolab::audio
{

/**
 * @brief High-performance standalone audio engine with fallback device management.
 */
class LabAudioEngine : public juce::AudioIODeviceCallback
{
public:
    LabAudioEngine();
    ~LabAudioEngine() override;

    bool initializeAudioDevices(const juce::File& settingsFile);
    void saveAudioSettings(const juce::File& settingsFile);

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Direct access to generator and receiver
    LabStimulusGenerator& getGenerator() noexcept { return generator; }
    LabAudioReceiver& getReceiver() noexcept { return receiver; }
    LabStimulusGenerator& getStimulusGenerator() noexcept { return generator; }
    LabAudioReceiver& getResponseReceiver() noexcept { return receiver; }

    // Mock hardware attachment for self-test loopback
    void setMockHardware(hardware::MockHardwareController* mock) noexcept { mockHardware = mock; }

    // Diagnostic test tone (RNF-17)
    void enableDiagnosticTestTone(bool enable, float freqHz = 1000.0f, float levelLinear = 0.5f) noexcept
    {
        diagnosticToneFreq = freqHz;
        diagnosticToneLevel = levelLinear;
        diagnosticToneActive.store(enable, std::memory_order_release);
    }
    [[nodiscard]] bool isDiagnosticTestToneActive() const noexcept { return diagnosticToneActive.load(std::memory_order_relaxed); }

    [[nodiscard]] double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    [[nodiscard]] double getSampleRate() const noexcept { return currentSampleRate; }

    // Level Meters (RMS & Peak for GUI VU Meters)
    [[nodiscard]] float getInputPeakL() const noexcept { return inputPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getInputPeakR() const noexcept { return inputPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] float getInputRmsL() const noexcept { return inputRmsL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getInputRmsR() const noexcept { return inputRmsR.load(std::memory_order_relaxed); }

    [[nodiscard]] float getOutputPeakL() const noexcept { return outputPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakR() const noexcept { return outputPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputRmsL() const noexcept { return outputRmsL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputRmsR() const noexcept { return outputRmsR.load(std::memory_order_relaxed); }

    // Auto-Trim input gain (scaling to -3 dBfs)
    void setInputAutoTrim(float linearGain) noexcept { inputTrimGain.store(linearGain, std::memory_order_release); }
    [[nodiscard]] float getInputAutoTrim() const noexcept { return inputTrimGain.load(std::memory_order_relaxed); }

    void performAutoGainTrim(float targetHeadroomDbfs = -3.0f) noexcept
    {
        float inPeak = std::max(getInputPeakL(), getInputPeakR());
        if (inPeak > 1e-4f)
        {
            float targetLin = std::pow(10.0f, targetHeadroomDbfs / 20.0f); // e.g. -3 dBfs -> 0.707
            float calculatedGain = targetLin / inPeak;
            calculatedGain = juce::jlimit(0.1f, 10.0f, calculatedGain);
            setInputAutoTrim(calculatedGain);
        }
    }

private:
    juce::AudioDeviceManager deviceManager;
    double currentSampleRate { 96000.0 };

    LabStimulusGenerator generator;
    LabAudioReceiver receiver;
    hardware::MockHardwareController* mockHardware { nullptr };

    // Diagnostic tone state
    std::atomic<bool> diagnosticToneActive { false };
    float diagnosticToneFreq { 1000.0f };
    float diagnosticToneLevel { 0.5f };
    double diagnosticTonePhase { 0.0 };

    // Atomic meters
    std::atomic<float> inputPeakL { 0.0f };
    std::atomic<float> inputPeakR { 0.0f };
    std::atomic<float> inputRmsL { 0.0f };
    std::atomic<float> inputRmsR { 0.0f };

    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };
    std::atomic<float> outputRmsL { 0.0f };
    std::atomic<float> outputRmsR { 0.0f };

    std::atomic<float> inputTrimGain { 1.0f };

    std::vector<float> tempProcessBuffer;
};

} // namespace abdaudiolab::audio
