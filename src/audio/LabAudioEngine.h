/**
 * @file LabAudioEngine.h
 * @brief Standalone low-latency audio device manager and I/O callback engine.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <memory>
#include "LabStimulusGenerator.h"
#include "LabAudioReceiver.h"
#include "../hardware/MockHardwareController.h"
#include <Core/ScopeTap.h>
#include <Core/ScopeDataCollector.h>
#include <Core/ScopeFrameSerializer.h>
#include "../dsp/AnalogLutFilterModule.h"

namespace abdaudiolab::audio
{

/**
 * @class LabAudioEngine
 * @brief High-performance standalone audio engine with real-time lock-free I/O dispatching.
 */
class LabAudioEngine : public juce::AudioIODeviceCallback
{
public:
    LabAudioEngine();
    ~LabAudioEngine() override;

    /**
     * @brief Initializes audio devices from XML settings file or fallback defaults (WASAPI/DirectSound).
     * @param settingsFile File object pointing to stored settings XML.
     * @return true on successful initialization, false on error.
     */
    bool initializeAudioDevices(const juce::File& settingsFile);

    /**
     * @brief Persists current audio device settings to XML file.
     * @param settingsFile Target file path.
     */
    void saveAudioSettings(const juce::File& settingsFile);

    /**
     * @brief Gets reference to JUCE AudioDeviceManager.
     */
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    /**
     * @brief Real-time audio I/O callback invoked by audio driver thread.
     */
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    /**
     * @brief Audio stream lifecycle callback when driver starts.
     */
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;

    /**
     * @brief Audio stream lifecycle callback when driver stops.
     */
    void audioDeviceStopped() override;

    // Direct accessors to generator and receiver
    LabStimulusGenerator& getStimulusGenerator() noexcept { return generator; }
    LabAudioReceiver& getResponseReceiver() noexcept { return receiver; }
    LabStimulusGenerator& getGenerator() noexcept { return getStimulusGenerator(); }
    LabAudioReceiver& getReceiver() noexcept { return getResponseReceiver(); }

    /**
     * @brief Attaches mock hardware controller for offline self-test loopback.
     */
    void setMockHardware(hardware::MockHardwareController* mock) noexcept { mockHardware = mock; }

    /**
     * @brief Attaches active software plugin instance for direct internal digital loopback (VST3, AU, LV2).
     */
    void setActivePluginInstance(juce::AudioPluginInstance* plugin, double sampleRate = 0.0, int blockSize = 0);
    [[nodiscard]] juce::AudioPluginInstance* getActivePluginInstance() const noexcept { return activePlugin.load(std::memory_order_acquire); }

    /**
     * @brief Gets reported internal latency of active software plugin in samples (getLatencySamples()).
     * Dynamically queried for sample-accurate latency compensation (zero allocations).
     */
    [[nodiscard]] int getPluginLatencySamples() const noexcept
    {
        if (auto* plugin = activePlugin.load(std::memory_order_acquire))
            return plugin->getLatencySamples();
        return 0;
    }

    /**
     * @brief Performs synchronous digital auto-trim calibration targeting -3.0 dBFS for active software plugin.
     * Generates a 1 kHz reference test tone, routes it through the active plugin,
     * computes the exact normalization factor and applies setInputAutoTrim(gain).
     * @param targetHeadroomDbfs Target peak headroom (default -3.0 dBFS = 0.7079458)
     * @param testDurationSec Duration in seconds for digital probe (default 0.2s)
     * @return Applied linear gain factor
     */
    float calibratePluginDigitalTrim(float targetHeadroomDbfs = -3.0f, double testDurationSec = 0.2);

    /**
     * @brief Enables or disables direct audio passthrough monitoring for active software plugins.
     */
    void setPluginMonitoringEnabled(bool enabled) noexcept { pluginMonitoringEnabled.store(enabled, std::memory_order_release); }
    [[nodiscard]] bool isPluginMonitoringEnabled() const noexcept { return pluginMonitoringEnabled.load(std::memory_order_acquire); }

    /**
     * @brief Posts live MIDI messages (from Virtual Keyboard or hardware MIDI) to active plugin in a thread-safe manner.
     */
    void postLiveMidiMessage(const juce::MidiMessage& message);

    // Plugin MIDI delivery & audio verification telemetry
    [[nodiscard]] int getPluginMidiEventsDelivered() const noexcept { return pluginMidiEventsDelivered.load(std::memory_order_relaxed); }
    [[nodiscard]] int getPluginNoteOnCount() const noexcept { return pluginNoteOnCount.load(std::memory_order_relaxed); }
    [[nodiscard]] int getPluginNoteOffCount() const noexcept { return pluginNoteOffCount.load(std::memory_order_relaxed); }
    [[nodiscard]] int getLastNoteOnNumber() const noexcept { return lastNoteOnNumber.load(std::memory_order_relaxed); }
    [[nodiscard]] float getLastNoteOnVelocity() const noexcept { return lastNoteOnVelocity.load(std::memory_order_relaxed); }
    [[nodiscard]] float getLastPluginOutputRms() const noexcept { return lastPluginOutputRms.load(std::memory_order_relaxed); }
    void resetPluginMidiTelemetry() noexcept
    {
        pluginMidiEventsDelivered.store(0, std::memory_order_relaxed);
        pluginNoteOnCount.store(0, std::memory_order_relaxed);
        pluginNoteOffCount.store(0, std::memory_order_relaxed);
        lastNoteOnNumber.store(-1, std::memory_order_relaxed);
        lastNoteOnVelocity.store(0.0f, std::memory_order_relaxed);
        lastPluginOutputRms.store(0.0f, std::memory_order_relaxed);
    }

    /**
     * @brief Enables or disables 1 kHz diagnostic reference test tone.
     */
    void enableDiagnosticTestTone(bool enable, float freqHz = 1000.0f, float levelLinear = 0.5f) noexcept
    {
        diagnosticToneFreq.store(freqHz, std::memory_order_relaxed);
        diagnosticToneLevel.store(levelLinear, std::memory_order_relaxed);
        diagnosticToneActive.store(enable, std::memory_order_release);
    }

    /**
     * @brief Returns true if diagnostic test tone is active.
     */
    [[nodiscard]] bool isDiagnosticTestToneActive() const noexcept { return diagnosticToneActive.load(std::memory_order_relaxed); }

    /**
     * @brief Enables or disables real-time audition mode ("Comprobar cómo sonaría").
     */
    void enableAuditionMode(bool enable) noexcept
    {
        auditionActive.store(enable, std::memory_order_release);
    }

    [[nodiscard]] bool isAuditionActive() const noexcept
    {
        return auditionActive.load(std::memory_order_relaxed);
    }

    void setAuditionParameters(float cutoff, float resonance) noexcept
    {
        auditionCutoff.store(juce::jlimit(0.0f, 1.0f, cutoff), std::memory_order_relaxed);
        auditionResonance.store(juce::jlimit(0.0f, 1.0f, resonance), std::memory_order_relaxed);
    }

    void setAuditionWaveform(int waveform) noexcept
    {
        auditionWaveform.store(juce::jlimit(0, 2, waveform), std::memory_order_relaxed);
    }

    void loadAuditionLut(const std::vector<dsp::AbdBatchedPoint>& lut, int gridSize)
    {
        auditionLutStorage = lut;
        auditionLutGridSize.store(gridSize, std::memory_order_release);
    }

    [[nodiscard]] bool hasAuditionLut() const noexcept
    {
        return auditionLutGridSize.load(std::memory_order_relaxed) > 0 && !auditionLutStorage.empty();
    }

    /**
     * @brief Gets current active sampling rate in Hz.
     */
    [[nodiscard]] double getSampleRate() const noexcept { return currentSampleRate; }
    [[nodiscard]] double getCurrentSampleRate() const noexcept { return getSampleRate(); }

    // Telemetry Multi-Tap Collector & Serializer for ABDScope
    [[nodiscard]] abd::scope::ScopeTap& getScopeTap() noexcept { return *tapHardwareIn; }
    [[nodiscard]] abd::scope::ScopeDataCollector& getScopeCollector() noexcept { return scopeCollector; }
    [[nodiscard]] abd::scope::ScopeFrameSerializer& getFrameSerializer() noexcept { return frameSerializer; }

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

    // Live FFT Spectrum Analysis (lock-free read from GUI timer)
    static constexpr int kFFTOrder = 11;           // 2048 points
    static constexpr int kFFTSize = 1 << kFFTOrder; // 2048
    static constexpr int kSpectrumBins = kFFTSize / 2; // 1024 usable bins

    /**
     * @brief Returns a snapshot of the current FFT magnitude spectrum.
     * Safe to call from the message thread. Values are in dBfs (-96..0).
     */
    void getSpectrumMagnitudes(std::array<float, kSpectrumBins>& outMagnitudes) const noexcept
    {
        (void)spectrumDataReady.load(std::memory_order_acquire);
        std::copy(spectrumMagnitudesDb.begin(), spectrumMagnitudesDb.end(), outMagnitudes.begin());
    }
    [[nodiscard]] bool isSpectrumReady() const noexcept { return spectrumDataReady.load(std::memory_order_acquire); }

    void performAutoGainTrim(float targetHeadroomDbfs = -3.0f) noexcept
    {
        if (activePlugin.load(std::memory_order_acquire) != nullptr)
        {
            calibratePluginDigitalTrim(targetHeadroomDbfs);
            return;
        }

        float inPeak = std::max(getInputPeakL(), getInputPeakR());
        if (inPeak > 1e-4f)
        {
            float targetLin = std::pow(10.0f, targetHeadroomDbfs / 20.0f); // e.g. -3 dBfs -> 0.707
            float calculatedGain = targetLin / inPeak;
            calculatedGain = juce::jlimit(0.1f, 10.0f, calculatedGain);
            setInputAutoTrim(calculatedGain);
        }
    }

    /**
     * @brief Triggers a gentle 800 Hz metronome tick (-24 dBFS, 15ms Hann window)
     *        to guide the operator during manual continuous sweeps.
     *        Thread-safe and real-time audio safe (zero allocations).
     */
    void triggerMetronomeTick() noexcept
    {
        metronomeCurrentSample.store(0, std::memory_order_relaxed);
        int totalSamples = static_cast<int>(std::lround(0.015 * currentSampleRate));
        metronomeTotalSamples.store(totalSamples > 0 ? totalSamples : 1440, std::memory_order_release);
    }

private:
    juce::AudioDeviceManager deviceManager;
    double currentSampleRate { 96000.0 };

    // Metronome pulse state for manual rhythm guide (real-time lock-free)
    std::atomic<int> metronomeCurrentSample { 0 };
    std::atomic<int> metronomeTotalSamples { 0 };

    LabStimulusGenerator generator;
    LabAudioReceiver receiver;
    hardware::MockHardwareController* mockHardware { nullptr };
    std::atomic<juce::AudioPluginInstance*> activePlugin { nullptr };
    std::atomic<bool> pluginMonitoringEnabled { false };
    juce::MidiBuffer pluginMidiMessages;
    juce::MidiMessageCollector liveMidiCollector;

    // Diagnostic tone state
    std::atomic<bool> diagnosticToneActive { false };
    std::atomic<float> diagnosticToneFreq { 1000.0f };
    std::atomic<float> diagnosticToneLevel { 0.5f };
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

    // Plugin MIDI delivery & audio verification telemetry
    std::atomic<int> pluginMidiEventsDelivered { 0 };
    std::atomic<int> pluginNoteOnCount { 0 };
    std::atomic<int> pluginNoteOffCount { 0 };
    std::atomic<int> lastNoteOnNumber { -1 };
    std::atomic<float> lastNoteOnVelocity { 0.0f };
    std::atomic<float> lastPluginOutputRms { 0.0f };

    std::vector<float> tempProcessBufferL;
    std::vector<float> tempProcessBufferR;
    abd::scope::ScopeDataCollector scopeCollector;
    abd::scope::ScopeTap* tapHardwareIn { nullptr };
    abd::scope::ScopeTap* tapStimulus   { nullptr };
    abd::scope::ScopeTap* tapDiagTone   { nullptr };
    double diagTonePhase { 0.0 };
    double diagToneSpreadPhase { 0.0 };
    abd::scope::ScopeFrameSerializer frameSerializer { 512 };

    // FFT Spectrum Pipeline (runs in audio thread, read from GUI)
    juce::dsp::FFT spectrumFFT { kFFTOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow { static_cast<size_t>(kFFTSize), juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kFFTSize> fftAccumBuffer {};
    int fftAccumPos { 0 };
    std::array<float, kFFTSize * 2> fftWorkBuffer {};  // interleaved real/imag for JUCE FFT
    std::array<float, kSpectrumBins> spectrumMagnitudesDb {};  // Published dBfs values
    std::atomic<bool> spectrumDataReady { false };

    // Audition preview state (real-time DSP preview of trained LUT)
    std::atomic<bool> auditionActive { false };
    std::atomic<float> auditionCutoff { 0.5f };
    std::atomic<float> auditionResonance { 0.5f };
    std::atomic<int> auditionWaveform { 0 }; // 0: Saw, 1: Square, 2: Noise
    std::atomic<int> auditionLutGridSize { 0 };
    std::vector<dsp::AbdBatchedPoint> auditionLutStorage;
    dsp::AnalogLutFilterModule auditionFilter;
    double auditionOscPhase { 0.0 };
    uint32_t auditionNoiseSeed { 0x12345678 };

    // Internal real-time safe audio subroutines (P2 Callback Modularization)
    void renderDiagnosticTone(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept;
    void renderAuditionPreview(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept;
    void renderStimulusAndRoute(float* const* outputChannelData, int numOutputChannels, int samplesToProcess) noexcept;
    std::pair<const float*, const float*> processInputAndMetrics(const float* const* inputChannelData, int numInputChannels, int samplesToProcess, float trim) noexcept;
    void accumulateFft(const float* sourceData, int sourceLen) noexcept;
    void updateTelemetryTaps(const float* inL, const float* inR, int samplesToProcess) noexcept;
};

} // namespace abdaudiolab::audio
