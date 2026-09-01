#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>

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
    AmplitudeRamp
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

    void setStimulus(StimulusType type, double durationSeconds = 2.0, float startFreqHz = 20.0f, float endFreqHz = 20000.0f);

    [[nodiscard]] bool isPlaying() const noexcept { return playing; }
    [[nodiscard]] bool hasFinished() const noexcept { return finished; }
    [[nodiscard]] int64_t getCurrentSampleIndex() const noexcept { return currentSampleIndex; }
    [[nodiscard]] int64_t getTotalSamples() const noexcept { return totalSamples; }

    void processBlock(float* outputBuffer, int numSamples) noexcept;

private:
    double sampleRate { 96000.0 };
    StimulusType currentType { StimulusType::Silence };
    bool playing { false };
    bool finished { false };

    int64_t currentSampleIndex { 0 };
    int64_t totalSamples { 0 };

    float startFreq { 20.0f };
    float endFreq { 20000.0f };
    double sweepDurationSec { 2.0 };

    // Deterministic random seed for White/Pink noise (entropy safety)
    uint32_t randomSeed { 0x48271983 };

    // Pink noise Voss-McCartney filter state
    float b0 { 0.0f }, b1 { 0.0f }, b2 { 0.0f }, b3 { 0.0f }, b4 { 0.0f }, b5 { 0.0f }, b6 { 0.0f };

    inline float getNextWhiteNoise() noexcept
    {
        randomSeed = randomSeed * 1664525u + 1013904223u;
        return (static_cast<float>(randomSeed) / 2147483648.0f) - 1.0f;
    }

    inline float getNextPinkNoise() noexcept
    {
        float white = getNextWhiteNoise();
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        return pink * 0.11f;
    }
};

} // namespace abdaudiolab::audio
