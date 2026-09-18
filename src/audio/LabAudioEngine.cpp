#include "LabAudioEngine.h"
#include <cmath>
#include <numbers>

namespace abdaudiolab::audio
{

LabAudioEngine::LabAudioEngine()
{
    tempProcessBufferL.assign(16384, 0.0f);
    tempProcessBufferR.assign(16384, 0.0f);

    tapHardwareIn = scopeCollector.registerTap("Hardware In (DUT)", abd::scope::ScopeTapType::StereoAudio, 8192, "hardware_in");
    tapStimulus   = scopeCollector.registerTap("Stimulus Generator", abd::scope::ScopeTapType::StereoAudio, 8192, "stimulus");
    tapDiagTone   = scopeCollector.registerTap("Diagnostic 1kHz", abd::scope::ScopeTapType::StereoAudio, 8192, "diag_tone");

    if (tapHardwareIn != nullptr) tapHardwareIn->setActive(true);
    if (tapStimulus != nullptr)   tapStimulus->setActive(true);
    if (tapDiagTone != nullptr)   tapDiagTone->setActive(true);
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

void LabAudioEngine::setActivePluginInstance(juce::AudioPluginInstance* plugin, double sampleRate, int blockSize)
{
    auto* oldPlugin = activePlugin.exchange(nullptr, std::memory_order_acq_rel);
    if (oldPlugin != nullptr)
    {
        juce::Logger::writeToLog("[AudioEngine] Releasing resources for previous plugin: " + oldPlugin->getName());
        oldPlugin->releaseResources();
    }

    if (plugin != nullptr)
    {
        double sr = sampleRate > 0.0 ? sampleRate : currentSampleRate;
        if (sr <= 0.0) sr = 44100.0;
        int bs = blockSize > 0 ? blockSize : static_cast<int>(tempProcessBufferL.size());
        if (bs <= 0) bs = 512;

        juce::Logger::writeToLog("[AudioEngine] Preparing plugin '" + plugin->getName()
            + "' with SR: " + juce::String(sr) + ", BS: " + juce::String(bs));
        plugin->prepareToPlay(sr, bs);
        juce::Logger::writeToLog("[AudioEngine] Plugin prepareToPlay completed successfully.");

        int latency = plugin->getLatencySamples();
        receiver.setLatencyCompensationSamples(latency);
        if (latency > 0)
        {
            juce::Logger::writeToLog("[AudioEngine] Plugin internal latency reported: "
                + juce::String(latency) + " samples. Latency compensation armed.");
        }

        activePlugin.store(plugin, std::memory_order_release);
    }
    else
    {
        receiver.setLatencyCompensationSamples(0);
    }
}

void LabAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        int bufferSize = device->getCurrentBufferSizeSamples();
        tempProcessBufferL.assign(static_cast<size_t>(std::max(bufferSize, 16384)), 0.0f);
        tempProcessBufferR.assign(static_cast<size_t>(std::max(bufferSize, 16384)), 0.0f);
    }
    else
    {
        currentSampleRate = 96000.0;
        tempProcessBufferL.assign(16384, 0.0f);
        tempProcessBufferR.assign(16384, 0.0f);
    }

    generator.prepare(currentSampleRate);
    receiver.prepare(currentSampleRate);
    liveMidiCollector.reset(currentSampleRate);
    if (mockHardware != nullptr)
        mockHardware->resetDsp(currentSampleRate);

    if (auto* plugin = activePlugin.load(std::memory_order_acquire))
    {
        int bufferSize = (device != nullptr) ? device->getCurrentBufferSizeSamples() : 512;
        juce::Logger::writeToLog("[AudioEngine] Audio device starting: preparing plugin '" + plugin->getName()
            + "' for SR: " + juce::String(currentSampleRate) + ", BS: " + juce::String(bufferSize));
        plugin->prepareToPlay(currentSampleRate, bufferSize);
    }
}

void LabAudioEngine::audioDeviceStopped()
{
    if (auto* plugin = activePlugin.load(std::memory_order_acquire))
    {
        juce::Logger::writeToLog("[AudioEngine] Audio device stopped: releasing plugin resources for '" + plugin->getName() + "'");
        plugin->releaseResources();
    }
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

    // 1. Dual-Buffer bound check (P1 stereo safety guarantee)
    const int maxCapacity = static_cast<int>(std::min(tempProcessBufferL.size(), tempProcessBufferR.size()));
    const int samplesToProcess = std::min(numSamples, maxCapacity);
    if (samplesToProcess <= 0)
        return;

    // If host delivers more samples than our pre-allocated buffer, safely zero the excess output
    if (numSamples > samplesToProcess)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                std::fill_n(outputChannelData[ch] + samplesToProcess, numSamples - samplesToProcess, 0.0f);
        }
    }

    // 2. Diagnostic test tone mode (RNF-17 physical DAC test)
    if (diagnosticToneActive.load(std::memory_order_relaxed))
    {
        renderDiagnosticTone(outputChannelData, numOutputChannels, samplesToProcess);
        return;
    }

    // 2b. Audition preview mode ("Comprobar cómo sonaría")
    if (auditionActive.load(std::memory_order_relaxed) && auditionLutGridSize.load(std::memory_order_relaxed) > 0)
    {
        renderAuditionPreview(outputChannelData, numOutputChannels, samplesToProcess);
        if (outputChannelData[0] != nullptr)
        {
            accumulateFft(outputChannelData[0], samplesToProcess);
            const float* outR = (numOutputChannels > 1 && outputChannelData[1] != nullptr) ? outputChannelData[1] : outputChannelData[0];
            updateTelemetryTaps(outputChannelData[0], outR, samplesToProcess);
        }
        return;
    }

    // 3. Stimulus generation and DAC routing
    renderStimulusAndRoute(outputChannelData, numOutputChannels, samplesToProcess);

    // 4. Input processing, metering, and balanced trim
    float trim = inputTrimGain.load(std::memory_order_relaxed);
    auto [fftSource, inR] = processInputAndMetrics(inputChannelData, numInputChannels, samplesToProcess, trim);

    // 4b. Direct Monitoring Passthrough for active software plugins (audition/manual interaction)
    if (activePlugin.load(std::memory_order_acquire) != nullptr
        && pluginMonitoringEnabled.load(std::memory_order_acquire)
        && !generator.isPlaying())
    {
        if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
            std::copy_n(tempProcessBufferL.data(), static_cast<size_t>(samplesToProcess), outputChannelData[0]);
        if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
            std::copy_n(tempProcessBufferR.data(), static_cast<size_t>(samplesToProcess), outputChannelData[1]);

        outputPeakL.store(inputPeakL.load(std::memory_order_relaxed), std::memory_order_relaxed);
        outputPeakR.store(inputPeakR.load(std::memory_order_relaxed), std::memory_order_relaxed);
        outputRmsL.store(inputRmsL.load(std::memory_order_relaxed), std::memory_order_relaxed);
        outputRmsR.store(inputRmsR.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // 5. Live FFT Spectrum Analysis
    if (fftSource != nullptr)
        accumulateFft(fftSource, samplesToProcess);

    // 6. Telemetry Taps (Hardware In & Diagnostic Tone Tap)
    updateTelemetryTaps(fftSource, inR, samplesToProcess);
}

void LabAudioEngine::renderDiagnosticTone(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept
{
    float freq = diagnosticToneFreq.load(std::memory_order_relaxed);
    float level = diagnosticToneLevel.load(std::memory_order_relaxed);
    double phaseIncrement = (2.0 * std::numbers::pi * freq) / currentSampleRate;
    for (int i = 0; i < samplesToProcess; ++i)
    {
        float toneVal = static_cast<float>(std::sin(diagnosticTonePhase)) * level;
        diagnosticTonePhase += phaseIncrement;
        if (diagnosticTonePhase >= 2.0 * std::numbers::pi)
            diagnosticTonePhase -= 2.0 * std::numbers::pi;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                outputChannelData[ch][i] = toneVal;
        }
    }

    if (tapDiagTone != nullptr && tapDiagTone->isActive() && numOutputChannels > 0 && outputChannelData[0] != nullptr)
    {
        const float* outR = (numOutputChannels > 1 && outputChannelData[1] != nullptr) ? outputChannelData[1] : outputChannelData[0];
        tapDiagTone->writeStereo(outputChannelData[0], outR, static_cast<size_t>(samplesToProcess));
    }
}

void LabAudioEngine::renderAuditionPreview(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept
{
    const int gridSize = auditionLutGridSize.load(std::memory_order_acquire);
    if (gridSize <= 0 || auditionLutStorage.empty())
        return;

    const float cutoff = auditionCutoff.load(std::memory_order_relaxed);
    const float reso = auditionResonance.load(std::memory_order_relaxed);
    const int wf = auditionWaveform.load(std::memory_order_relaxed);

    auditionFilter.setVoiceParameters(0, cutoff, reso);

    const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 96000.0;
    const double oscFreq = 130.8128; // C3
    const double twoPi = 2.0 * std::numbers::pi;
    const double phaseInc = (twoPi * oscFreq) / sr;

    for (int i = 0; i < samplesToProcess; ++i)
    {
        float s = 0.0f;
        if (wf == 0) // Sawtooth
        {
            s = static_cast<float>((auditionOscPhase / std::numbers::pi) - 1.0);
            auditionOscPhase += phaseInc;
            if (auditionOscPhase >= twoPi)
                auditionOscPhase -= twoPi;
        }
        else if (wf == 1) // Square / Pulse
        {
            s = (auditionOscPhase < std::numbers::pi) ? 0.6f : -0.6f;
            auditionOscPhase += phaseInc;
            if (auditionOscPhase >= twoPi)
                auditionOscPhase -= twoPi;
        }
        else // White Noise
        {
            auditionNoiseSeed = auditionNoiseSeed * 1664525u + 1013904223u;
            s = (static_cast<float>(auditionNoiseSeed) / 2147483648.0f - 1.0f) * 0.4f;
        }
        tempProcessBufferL[static_cast<size_t>(i)] = s;
    }

    std::array<float*, 8> voicePtrs = { tempProcessBufferL.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    auditionFilter.processPolyphonicBlock(voicePtrs.data(), samplesToProcess, auditionLutStorage.data(), gridSize);

    const float outGain = 0.5f;
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                outputChannelData[ch][i] = tempProcessBufferL[static_cast<size_t>(i)] * outGain;
            }
        }
    }
}

void LabAudioEngine::renderStimulusAndRoute(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept
{
    // Zero-allocation stimulus buffer clearing for current block
    std::fill_n(tempProcessBufferL.data(), static_cast<size_t>(samplesToProcess), 0.0f);

    generator.processBlock(tempProcessBufferL.data(), samplesToProcess);

    // Mix rhythm metronome click if active (800 Hz, -24 dBFS, 15ms Hann window)
    int totalMetSamples = metronomeTotalSamples.load(std::memory_order_acquire);
    int currentMetSample = metronomeCurrentSample.load(std::memory_order_relaxed);
    if (totalMetSamples > 0 && currentMetSample < totalMetSamples)
    {
        const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 96000.0;
        const double twoPi = 2.0 * std::numbers::pi;
        const float gainLinear = 0.0630957f; // -24 dBFS
        int samplesToRender = std::min(samplesToProcess, totalMetSamples - currentMetSample);

        for (int i = 0; i < samplesToRender; ++i)
        {
            int s = currentMetSample + i;
            double t = static_cast<double>(s) / sr;
            double phase = twoPi * 800.0 * t;
            double hann = 0.5 * (1.0 - std::cos(twoPi * (static_cast<double>(s) / static_cast<double>(totalMetSamples))));
            float tickVal = static_cast<float>(std::sin(phase) * hann * static_cast<double>(gainLinear));

            tempProcessBufferL[static_cast<size_t>(i)] += tickVal;
        }
        metronomeCurrentSample.fetch_add(samplesToRender, std::memory_order_relaxed);
    }

    // Route generator output to physical DAC output channels (e.g. Left/Right channel 0 and 1)
    if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
        std::copy_n(tempProcessBufferL.data(), samplesToProcess, outputChannelData[0]);
    if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
        std::copy_n(tempProcessBufferL.data(), samplesToProcess, outputChannelData[1]);

    if (tapStimulus != nullptr && tapStimulus->isActive() && samplesToProcess > 0)
        tapStimulus->writeStereo(tempProcessBufferL.data(), tempProcessBufferL.data(), static_cast<size_t>(samplesToProcess));

    // Measure Output Levels (Peak & RMS)
    float outSumSqL = 0.0f, outPeakValL = 0.0f;
    float outSumSqR = 0.0f, outPeakValR = 0.0f;
    if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
    {
        for (int i = 0; i < samplesToProcess; ++i)
        {
            float s = std::abs(outputChannelData[0][i]);
            if (s > outPeakValL) outPeakValL = s;
            outSumSqL += s * s;
        }
    }
    if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
    {
        for (int i = 0; i < samplesToProcess; ++i)
        {
            float s = std::abs(outputChannelData[1][i]);
            if (s > outPeakValR) outPeakValR = s;
            outSumSqR += s * s;
        }
    }
    outputPeakL.store(outPeakValL, std::memory_order_relaxed);
    outputPeakR.store(outPeakValR, std::memory_order_relaxed);
    outputRmsL.store(std::sqrt(outSumSqL / static_cast<float>(samplesToProcess)), std::memory_order_relaxed);
    outputRmsR.store(std::sqrt(outSumSqR / static_cast<float>(samplesToProcess)), std::memory_order_relaxed);
}

std::pair<const float*, const float*> LabAudioEngine::processInputAndMetrics(
    const float* const* inputChannelData,
    int numInputChannels,
    int samplesToProcess,
    float trim) noexcept
{
    // If an active software plugin is hosted, route stimulus block directly through its processBlock
    if (auto* plugin = activePlugin.load(std::memory_order_acquire))
    {
        int currentLat = plugin->getLatencySamples();
        if (currentLat != receiver.getLatencyCompensationSamples())
        {
            receiver.setLatencyCompensationSamples(currentLat);
        }

        liveMidiCollector.removeNextBlockOfMessages(pluginMidiMessages, samplesToProcess);

        if (!pluginMidiMessages.isEmpty())
        {
            pluginMidiEventsDelivered.fetch_add(pluginMidiMessages.getNumEvents(), std::memory_order_relaxed);
            for (const auto meta : pluginMidiMessages)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn())
                {
                    pluginNoteOnCount.fetch_add(1, std::memory_order_relaxed);
                    lastNoteOnNumber.store(msg.getNoteNumber(), std::memory_order_relaxed);
                    lastNoteOnVelocity.store(msg.getFloatVelocity(), std::memory_order_relaxed);
                }
                else if (msg.isNoteOff())
                {
                    pluginNoteOffCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        float* channels[2] = { tempProcessBufferL.data(), tempProcessBufferR.data() };
        juce::AudioBuffer<float> pluginBuf(channels, 2, samplesToProcess);
        
        // Zero channel R if mono stimulus was generated
        std::copy_n(tempProcessBufferL.data(), samplesToProcess, tempProcessBufferR.data());

        plugin->processBlock(pluginBuf, pluginMidiMessages);
        pluginMidiMessages.clear();

        if (std::abs(trim - 1.0f) > 0.001f)
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                tempProcessBufferL[static_cast<size_t>(i)] *= trim;
                tempProcessBufferR[static_cast<size_t>(i)] *= trim;
            }
        }
        receiver.processBlock(tempProcessBufferL.data(), samplesToProcess);

        // Measure plugin output levels
        float inSumSqL = 0.0f, inPeakL = 0.0f;
        float inSumSqR = 0.0f, inPeakR = 0.0f;
        for (int i = 0; i < samplesToProcess; ++i)
        {
            float sL = std::abs(tempProcessBufferL[static_cast<size_t>(i)]);
            if (sL > inPeakL) inPeakL = sL;
            inSumSqL += sL * sL;

            float sR = std::abs(tempProcessBufferR[static_cast<size_t>(i)]);
            if (sR > inPeakR) inPeakR = sR;
            inSumSqR += sR * sR;
        }
        float computedRmsL = std::sqrt(inSumSqL / static_cast<float>(samplesToProcess));
        inputPeakL.store(inPeakL, std::memory_order_relaxed);
        inputPeakR.store(inPeakR, std::memory_order_relaxed);
        inputRmsL.store(computedRmsL, std::memory_order_relaxed);
        inputRmsR.store(std::sqrt(inSumSqR / static_cast<float>(samplesToProcess)), std::memory_order_relaxed);
        lastPluginOutputRms.store(computedRmsL, std::memory_order_relaxed);

        return { tempProcessBufferL.data(), tempProcessBufferR.data() };
    }

    // If Mock Hardware is active, process signal through simulated DSP loopback
    if (mockHardware != nullptr)
    {
        mockHardware->processAudioBlock(tempProcessBufferL.data(), tempProcessBufferL.data(), samplesToProcess);
        if (std::abs(trim - 1.0f) > 0.001f)
        {
            for (int i = 0; i < samplesToProcess; ++i)
                tempProcessBufferL[static_cast<size_t>(i)] *= trim;
        }
        receiver.processBlock(tempProcessBufferL.data(), samplesToProcess);

        // Measure mock input levels
        float inSumSq = 0.0f, inPeakVal = 0.0f;
        for (int i = 0; i < samplesToProcess; ++i)
        {
            float s = std::abs(tempProcessBufferL[static_cast<size_t>(i)]);
            if (s > inPeakVal) inPeakVal = s;
            inSumSq += s * s;
        }
        inputPeakL.store(inPeakVal, std::memory_order_relaxed);
        inputPeakR.store(inPeakVal, std::memory_order_relaxed);
        float rmsVal = std::sqrt(inSumSq / static_cast<float>(samplesToProcess));
        inputRmsL.store(rmsVal, std::memory_order_relaxed);
        inputRmsR.store(rmsVal, std::memory_order_relaxed);

        return { tempProcessBufferL.data(), tempProcessBufferL.data() };
    }

    // Receive from physical ADC input (Channel 0 / Return line) with dedicated L/R buffers
    float inSumSqL = 0.0f, inPeakValL = 0.0f;
    float inSumSqR = 0.0f, inPeakValR = 0.0f;
    const float* srcL = nullptr;
    const float* srcR = nullptr;

    if (numInputChannels > 0 && inputChannelData[0] != nullptr)
    {
        if (std::abs(trim - 1.0f) > 0.001f)
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                float s = inputChannelData[0][i] * trim;
                tempProcessBufferL[static_cast<size_t>(i)] = s;
                float absS = std::abs(s);
                if (absS > inPeakValL) inPeakValL = absS;
                inSumSqL += absS * absS;
            }
            receiver.processBlock(tempProcessBufferL.data(), samplesToProcess);
            srcL = tempProcessBufferL.data();
        }
        else
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                float absS = std::abs(inputChannelData[0][i]);
                if (absS > inPeakValL) inPeakValL = absS;
                inSumSqL += absS * absS;
            }
            receiver.processBlock(inputChannelData[0], samplesToProcess);
            srcL = inputChannelData[0];
        }
    }

    if (numInputChannels > 1 && inputChannelData[1] != nullptr)
    {
        if (std::abs(trim - 1.0f) > 0.001f)
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                float s = inputChannelData[1][i] * trim;
                tempProcessBufferR[static_cast<size_t>(i)] = s;
                float absS = std::abs(s);
                if (absS > inPeakValR) inPeakValR = absS;
                inSumSqR += absS * absS;
            }
            srcR = tempProcessBufferR.data();
        }
        else
        {
            for (int i = 0; i < samplesToProcess; ++i)
            {
                float absS = std::abs(inputChannelData[1][i]);
                if (absS > inPeakValR) inPeakValR = absS;
                inSumSqR += absS * absS;
            }
            srcR = inputChannelData[1];
        }
    }
    else
    {
        srcR = srcL;
    }

    inputPeakL.store(inPeakValL, std::memory_order_relaxed);
    inputPeakR.store(inPeakValR, std::memory_order_relaxed);
    inputRmsL.store(std::sqrt(inSumSqL / static_cast<float>(samplesToProcess)), std::memory_order_relaxed);
    inputRmsR.store(std::sqrt(inSumSqR / static_cast<float>(samplesToProcess)), std::memory_order_relaxed);

    return { srcL, srcR };
}

void LabAudioEngine::accumulateFft(const float* sourceData, int sourceLen) noexcept
{
    for (int i = 0; i < sourceLen; ++i)
    {
        fftAccumBuffer[static_cast<size_t>(fftAccumPos++)] = sourceData[i];

        if (fftAccumPos >= kFFTSize)
        {
            fftAccumPos = 0;

            // Copy accumulated samples and apply Hann window
            std::copy(fftAccumBuffer.begin(), fftAccumBuffer.end(), fftWorkBuffer.begin());
            spectrumWindow.multiplyWithWindowingTable(fftWorkBuffer.data(), static_cast<size_t>(kFFTSize));

            // Clear imaginary parts before in-place real forward transform
            std::fill(fftWorkBuffer.begin() + kFFTSize, fftWorkBuffer.end(), 0.0f);

            spectrumFFT.performRealOnlyForwardTransform(fftWorkBuffer.data());

            for (size_t bin = 0; bin < kSpectrumBins; ++bin)
            {
                float real = fftWorkBuffer[bin * 2];
                float imag = fftWorkBuffer[bin * 2 + 1];
                float mag = std::sqrt(real * real + imag * imag) / static_cast<float>(kFFTSize);
                float db = juce::Decibels::gainToDecibels(mag, -120.0f);
                spectrumMagnitudesDb[bin] = db;
            }

            spectrumDataReady.store(true, std::memory_order_release);
        }
    }
}

void LabAudioEngine::updateTelemetryTaps(const float* inL, const float* inR, int samplesToProcess) noexcept
{
    if (tapHardwareIn != nullptr && tapHardwareIn->isActive() && samplesToProcess > 0)
    {
        if (inL != nullptr)
        {
            const float* rightChannel = (inR != nullptr) ? inR : inL;
            tapHardwareIn->writeStereo(inL, rightChannel, static_cast<size_t>(samplesToProcess));
        }
        else
        {
            // If no physical input device is present or active, stream zero baseline so scope draws 0V reticle
            tapHardwareIn->writeStereo(tempProcessBufferL.data(), tempProcessBufferR.data(), static_cast<size_t>(samplesToProcess));
        }
    }

    // Diagnostic 1kHz reference tone generator with stereo spread (for dynamic Lissajous and Phase meter)
    if (tapDiagTone != nullptr && tapDiagTone->isActive() && samplesToProcess > 0)
    {
        double rate = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
        double phaseIncr = (2.0 * std::numbers::pi * 1000.0) / rate;
        double spreadIncr = (2.0 * std::numbers::pi * 0.5) / rate; // 0.5 Hz continuous rotation

        for (int i = 0; i < samplesToProcess; ++i)
        {
            float sL = 0.5f * static_cast<float>(std::sin(diagTonePhase));
            float sR = 0.5f * static_cast<float>(std::sin(diagTonePhase + diagToneSpreadPhase));

            tempProcessBufferL[static_cast<size_t>(i)] = sL;
            tempProcessBufferR[static_cast<size_t>(i)] = sR;

            diagTonePhase += phaseIncr;
            if (diagTonePhase >= 2.0 * std::numbers::pi)
                diagTonePhase -= 2.0 * std::numbers::pi;

            diagToneSpreadPhase += spreadIncr;
            if (diagToneSpreadPhase >= 2.0 * std::numbers::pi)
                diagToneSpreadPhase -= 2.0 * std::numbers::pi;
        }
        tapDiagTone->writeStereo(tempProcessBufferL.data(), tempProcessBufferR.data(), static_cast<size_t>(samplesToProcess));
    }
}

void LabAudioEngine::postLiveMidiMessage(const juce::MidiMessage& message)
{
    liveMidiCollector.addMessageToQueue(message);
}

float LabAudioEngine::calibratePluginDigitalTrim(float targetHeadroomDbfs, double testDurationSec)
{
    auto* plugin = activePlugin.load(std::memory_order_acquire);
    if (plugin == nullptr)
        return 1.0f;

    double sr = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
    int totalSamples = static_cast<int>(std::lround(testDurationSec * sr));
    if (totalSamples <= 0)
        totalSamples = 2048;

    // Pre-allocate temporary calibration buffers (runs synchronously on non-audio thread)
    std::vector<float> calBufL(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> calBufR(static_cast<size_t>(totalSamples), 0.0f);

    // Render 1 kHz reference sine test tone at 0 dBFS (peak 1.0)
    double twoPi = 2.0 * std::numbers::pi;
    double phaseIncr = (twoPi * 1000.0) / sr;
    double currentPhase = 0.0;
    for (int i = 0; i < totalSamples; ++i)
    {
        float s = static_cast<float>(std::sin(currentPhase));
        calBufL[static_cast<size_t>(i)] = s;
        calBufR[static_cast<size_t>(i)] = s;
        currentPhase += phaseIncr;
        if (currentPhase >= twoPi)
            currentPhase -= twoPi;
    }

    // Process through plugin in blocks of 512 samples
    constexpr int blockSize = 512;
    juce::MidiBuffer emptyMidi;
    float maxObservedPeak = 0.0f;

    int samplesProcessed = 0;
    while (samplesProcessed < totalSamples)
    {
        int chunk = std::min(blockSize, totalSamples - samplesProcessed);
        float* channels[2] = { calBufL.data() + samplesProcessed, calBufR.data() + samplesProcessed };
        juce::AudioBuffer<float> chunkBuf(channels, 2, chunk);

        plugin->processBlock(chunkBuf, emptyMidi);

        for (int i = 0; i < chunk; ++i)
        {
            maxObservedPeak = std::max(maxObservedPeak, std::abs(channels[0][i]));
            maxObservedPeak = std::max(maxObservedPeak, std::abs(channels[1][i]));
        }

        samplesProcessed += chunk;
    }

    if (maxObservedPeak > 1e-4f)
    {
        float targetLin = std::pow(10.0f, targetHeadroomDbfs / 20.0f); // -3 dBFS -> 0.7079458
        float calculatedGain = targetLin / maxObservedPeak;
        calculatedGain = juce::jlimit(0.01f, 100.0f, calculatedGain); // Safe +-40 dB digital trim range
        setInputAutoTrim(calculatedGain);

        float gainDb = 20.0f * std::log10(calculatedGain);
        juce::Logger::writeToLog("[AudioEngine] Plugin Digital Auto-Trim: peak="
            + juce::String(maxObservedPeak, 4) + " -> calibrated gain="
            + juce::String(calculatedGain, 4) + " (" + (gainDb >= 0.0f ? "+" : "")
            + juce::String(gainDb, 2) + " dB to " + juce::String(targetHeadroomDbfs, 1) + " dBFS)");

        return calculatedGain;
    }

    return 1.0f;
}

} // namespace abdaudiolab::audio
