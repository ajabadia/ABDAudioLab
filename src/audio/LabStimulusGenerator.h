#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../math/PinkNoise.h"
#include <vector>
#include <cstdint>
#include <atomic>

namespace abdaudiolab::audio
{

enum class StimulusType
{
    Silence,
    DiracDelta,
    SyncPulses3,       // 3-Tone / 3-Pulse Sync Marker & Pre-Roll Calibration (NAM / ALEX pattern)
    WhiteNoise,
    PinkNoise,
    SineWave1kHz,
    SquareWave1kHz,
    LogFarinaSweep,
    AmplitudeRamp,
    NamCalibration,    // Full multi-stage Neural Calibration sequence (Sync, dynamic sweep, noise bursts, multitone transients)
    MetronomeTick     // Rhythmic 800 Hz click for continuous manual calibration sweeps (-24 dBFS)
};

/**
 * @brief Real-time safe stimulus signal generator.
 * 
 * Generates sample-accurate test signals for the hardware profiler.
 */
class LabStimulusGenerator
{
public:
    LabStimulusGenerator();
    ~LabStimulusGenerator() = default;

    void prepare(double newSampleRate);
    void reset();
    void stop() noexcept { reset(); }

    void setStimulus(StimulusType type, double durationSeconds = 2.0, float startFreqHz = 20.0f, float endFreqHz = 20000.0f);

    /**
     * @brief Generates a complete standalone NAM calibration sequence buffer.
     * @param sampleRate Target audio sampling rate (e.g. 48000 or 96000).
     * @param durationSeconds Total length of the sequence (default 9.0s).
     */
    static juce::AudioBuffer<float> generateNamCalibrationBuffer(double sampleRate, double durationSeconds = 9.0);

    /**
     * @brief Generates the ideal SyncPulses3 reference pattern (300ms, 3x 1kHz bursts with Hann windows).
     * @param sampleRate Target sampling rate.
     */
    static std::vector<float> generateSyncPulses3Pattern(double sampleRate);

    /**
     * @brief Renders a clean, low-priority 800 Hz metronome click at -24 dBFS with a smooth Hann envelope.
     *        Designed for the Manual Phase rhythm guide to avoid measurement corruption.
     * @param destinationBuffer Buffer where the click is rendered (or mixed).
     * @param numSamples Number of samples in the block.
     * @param sampleRate The operating sample rate.
     * @param clickDurationSec Duration of the click pulse (default 15ms).
     * @param clickFreqHz Pitch of the tick (default 800 Hz).
     * @param gainLinear Linear gain (default 0.063f ≈ -24 dBFS).
     */
    static void renderMetronomeTick(float* destinationBuffer, 
                                    int numSamples, 
                                    double sampleRate, 
                                    double clickDurationSec = 0.015,
                                    float clickFreqHz = 800.0f,
                                    float gainLinear = 0.0630957f) noexcept;

    [[nodiscard]] bool isPlaying() const noexcept { return playing; }
    [[nodiscard]] bool hasFinished() const noexcept { return finished; }
    [[nodiscard]] int64_t getCurrentSampleIndex() const noexcept { return currentSampleIndex; }
    [[nodiscard]] int64_t getTotalSamples() const noexcept { return totalSamples; }

    void processBlock(float* outputBuffer, int numSamples) noexcept;

private:
    double sampleRate { 96000.0 };
    StimulusType currentType { StimulusType::Silence };
    std::atomic<bool> playing { false };
    std::atomic<bool> finished { false };

    std::atomic<int64_t> currentSampleIndex { 0 };
    std::atomic<int64_t> totalSamples { 0 };

    float startFreq { 20.0f };
    float endFreq { 20000.0f };
    double sweepDurationSec { 2.0 };

    // Los coeficientes del ruido rosa viven en `math/PinkNoise.h`, no aqui.
    // La matematica estaba duplicada byte a byte con el coordinador de
    // estimulacion de medicion, y dos copias de la misma regla numerica
    // divergen sin ruido: el dia que una se toca y la otra no, los dos caminos
    // dan series distintas y el fallo aparece en un hash, a kilometros del sitio
    // del cambio. Ver el comentario de ese header para el resto del porque.
    math::PinkNoiseGenerator pinkNoise;
};

} // namespace abdaudiolab::audio
