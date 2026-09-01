#include "LabAudioEngine.h"
#include <cmath>
#include <numbers>

namespace abdaudiolab::audio
{

LabAudioEngine::LabAudioEngine()
{
}

LabAudioEngine::~LabAudioEngine()
{
    deviceManager.removeAudioCallback(this);
}

bool LabAudioEngine::initializeAudioDevices(const juce::File& settingsFile)
{
    // Hierarchical 3-step fallback initialization chain (RNF-16)
    std::unique_ptr<juce::XmlElement> savedXml;
    if (settingsFile.existsAsFile())
    {
        savedXml = juce::XmlDocument::parse(settingsFile);
    }

    // Step 1: Initialize with saved XML state
    juce::String err = deviceManager.initialise(2, 2, savedXml.get(), true);

    // Step 2: Fallback to default system devices if step 1 failed
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("AudioDeviceManager: Step 1 failed (" + err + "). Attempting Step 2 (Default devices)...");
        err = deviceManager.initialiseWithDefaultDevices(2, 2);
    }

    // Step 3: Ultimate fallback to generic stereo setup
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("AudioDeviceManager: Step 2 failed (" + err + "). Attempting Step 3 (Generic fallback)...");
        err = deviceManager.initialise(2, 2, nullptr, true);
    }

    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("AudioDeviceManager: All 3 initialization steps failed: " + err);
        return false;
    }

    deviceManager.addAudioCallback(this);
    return true;
}

void LabAudioEngine::saveAudioSettings(const juce::File& settingsFile)
{
    auto stateXml = deviceManager.createStateXml();
    if (stateXml != nullptr)
    {
        settingsFile.getParentDirectory().createDirectory();
        stateXml->writeTo(settingsFile);
    }
}

void LabAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        int bufferSize = device->getCurrentBufferSizeSamples();
        tempProcessBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
    }
    else
    {
        currentSampleRate = 96000.0;
    }

    generator.prepare(currentSampleRate);
    receiver.prepare(currentSampleRate);
    if (mockHardware != nullptr)
        mockHardware->resetDsp(currentSampleRate);
}

void LabAudioEngine::audioDeviceStopped()
{
    generator.reset();
    receiver.reset();
}

void LabAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // RNF-14: ScopedNoDenormals mandatory at the very start of every audio callback
    juce::ScopedNoDenormals noDenormals;

    // Clear output channels
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            std::fill_n(outputChannelData[ch], numSamples, 0.0f);
    }

    // Check if diagnostic test tone is active (RNF-17)
    if (diagnosticToneActive.load(std::memory_order_relaxed))
    {
        double phaseIncrement = (2.0 * std::numbers::pi * diagnosticToneFreq) / currentSampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            float toneVal = static_cast<float>(std::sin(diagnosticTonePhase)) * diagnosticToneLevel;
            diagnosticTonePhase += phaseIncrement;
            if (diagnosticTonePhase >= 2.0 * std::numbers::pi)
                diagnosticTonePhase -= 2.0 * std::numbers::pi;

            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                    outputChannelData[ch][i] = toneVal;
            }
        }
        return;
    }

    // Generate stimulus signal into temporary buffer
    if (static_cast<int>(tempProcessBuffer.size()) < numSamples)
        tempProcessBuffer.assign(static_cast<size_t>(numSamples), 0.0f);

    generator.processBlock(tempProcessBuffer.data(), numSamples);

    // Route generator output to physical DAC output channels (e.g. Left/Right channel 0 and 1)
    if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
        std::copy_n(tempProcessBuffer.data(), numSamples, outputChannelData[0]);
    if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
        std::copy_n(tempProcessBuffer.data(), numSamples, outputChannelData[1]);

    // If Mock Hardware is active, process signal through simulated DSP loopback
    if (mockHardware != nullptr)
    {
        mockHardware->processAudioBlock(tempProcessBuffer.data(), tempProcessBuffer.data(), numSamples);
        receiver.processBlock(tempProcessBuffer.data(), numSamples);
    }
    else
    {
        // Receive from physical ADC input (Channel 0 / Return line)
        if (numInputChannels > 0 && inputChannelData[0] != nullptr)
        {
            receiver.processBlock(inputChannelData[0], numSamples);
        }
    }
}

} // namespace abdaudiolab::audio
