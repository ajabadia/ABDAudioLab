#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>

namespace abdaudiolab::math
{

/**
 * @brief Represents a single noise floor analysis snapshot.
 */
struct NoiseSnapshot
{
    double timestampSec { 0.0 };
    float totalRmsDb { -100.0f };
    float peakDb { -100.0f };
    std::array<float, 32> spectralBands {};
};

/**
 * @brief Tracks and analyzes background noise floor and thermal drift over time.
 * 
 * Implements Section 18.1 (RF-27) for capturing noise floor spectral fingerprints
 * during periodic silence interludes and exporting Analogue_Noise_Timeline.h.
 */
class NoiseFloorTracker
{
public:
    NoiseFloorTracker();
    ~NoiseFloorTracker() = default;

    void reset();

    /**
     * @brief Analyzes a silence interlude buffer and records a noise snapshot.
     */
    void recordNoiseSnapshot(double timestampSec, const float* buffer, int numSamples, double sampleRate);

    [[nodiscard]] const std::vector<NoiseSnapshot>& getSnapshots() const noexcept { return snapshots; }
    [[nodiscard]] size_t getSnapshotCount() const noexcept { return snapshots.size(); }
    [[nodiscard]] const NoiseSnapshot* getLatestSnapshot() const noexcept
    {
        return snapshots.empty() ? nullptr : &snapshots.back();
    }

    /**
     * @brief Exports the noise timeline as a C++ header file.
     */
    bool exportNoiseTimelineHeader(const juce::File& outputFile, const juce::String& hardwareName) const;

private:
    std::vector<NoiseSnapshot> snapshots;
    static constexpr int fftOrder = 11; // 2048 points
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> windowBuffer {};
};

} // namespace abdaudiolab::math
