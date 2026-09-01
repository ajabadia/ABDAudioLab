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

    std::vector<float> tempProcessBuffer;
};

} // namespace abdaudiolab::audio
