#pragma once

#include <vector>
#include <atomic>
#include <juce_core/juce_core.h>
#include "../audio/LabAudioReceiver.h"
#include "../audio/LabStimulusGenerator.h"

namespace abdaudiolab::core
{

/**
 * @class ProfilingAudioCapture
 * @brief Manages synchronous closed-loop stimulus generation, buffer capture,
 *        settling delays, and overload protection.
 */
class ProfilingAudioCapture
{
public:
    ProfilingAudioCapture(audio::LabAudioReceiver& receiver,
                          audio::LabStimulusGenerator& generator);
    ~ProfilingAudioCapture() = default;

    void executeSettlingWait(int delayMs, const juce::Thread& callingThread);

    /**
     * @brief Query or configure plugin algorithmic latency compensation on the receiver.
     */
    [[nodiscard]] int getLatencyCompensationSamples() const noexcept { return audioReceiver.getLatencyCompensationSamples(); }
    void setLatencyCompensationSamples(int samples) noexcept { audioReceiver.setLatencyCompensationSamples(samples); }

    /**
     * Captura síncrona para pruebas de estímulo estándar (Farina sweep, seno, etc.)
     */
    bool captureStimulusSynchronous(audio::StimulusType stimulus,
                                    float durationSec,
                                    float startFreqHz,
                                    float endFreqHz,
                                    double sampleRate,
                                    double timeoutMs,
                                    const juce::Thread& callingThread,
                                    std::vector<float>& outRecordedAudio,
                                    std::atomic<bool>& safetyAborted);

    /**
     * Captura continua para verificación de calibración inicial de línea (Auto-Trim).
     */
    bool captureLineCalibrationSynchronous(float durationSec,
                                           double sampleRate,
                                           double timeoutMs,
                                           const juce::Thread& callingThread,
                                           std::vector<float>& outRecordedAudio,
                                           std::atomic<bool>& safetyAborted);

    /**
     * Captura síncrona ligera para ráfagas discretas de comprobación de modulación.
     */
    bool captureModulationBurstSynchronous(float probeDurationSec,
                                           double sampleRate,
                                           const juce::Thread& callingThread,
                                           std::vector<float>& outRecordedAudio);

private:
    audio::LabAudioReceiver& audioReceiver;
    audio::LabStimulusGenerator& stimulusGenerator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfilingAudioCapture)
};

} // namespace abdaudiolab::core
