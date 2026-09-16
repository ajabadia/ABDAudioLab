/**
 * @file GeneratedAcousticModel.h
 * @brief Executable runtime for empirical acoustic models based on AbdBatchedPoint LUTs.
 * @author ABDSynths
 * @date 2026
 *
 * Implements an out-of-the-box C++20 real-time audio processor executing the
 * empirical Look-Up Table (LUT) with SIMD bilinear interpolation and ballistic
 * exponential smoothing via AnalogLutFilterModule.
 *
 * Concurrency & DSP Safety:
 * - Real-time zero-heap allocation in processBlock(), reset() and setParameters().
 * - All scratch buffers pre-allocated in prepare().
 * - Bounded input domain clamping and robust nullptr protection.
 */

#pragma once

#include <array>
#include <vector>
#include <algorithm>
#include <cstddef>
#include "../dsp/AnalogLutFilterModule.h"

namespace abdaudiolab::core
{

/**
 * @struct AcousticModelParameters
 * @brief Runtime control parameters for the acoustic model.
 */
struct AcousticModelParameters
{
    float param1 { 0.5f }; /**< Parameter 1 normalizado [0.0, 1.0] (ej. Cutoff / Frecuencia) */
    float param2 { 0.5f }; /**< Parameter 2 normalizado [0.0, 1.0] (ej. Resonancia / Drive) */
};

/**
 * @struct ModelDomainLimitations
 * @brief Formal specifications and bounded operating domain for the model.
 */
struct ModelDomainLimitations
{
    float param1Min { 0.0f };
    float param1Max { 1.0f };
    float param2Min { 0.0f };
    float param2Max { 1.0f };
    double minSampleRate { 22050.0 };
    double maxSampleRate { 192000.0 };
    int maxChannels { 8 };
    int gridSize { 8 };
    size_t totalPoints { 64 };
    const char* interpolationMode { "Bilinear SIMD with Ballistic Smoothing (5 ms)" };
    const char* indexOrder { "Row-major: idx = y * gridSize + x (x=param1, y=param2)" };
};

/**
 * @class GeneratedAcousticModel
 * @brief High-performance executable runtime evaluating empirical LUTs.
 */
class GeneratedAcousticModel
{
public:
    GeneratedAcousticModel();
    GeneratedAcousticModel(const dsp::AbdBatchedPoint* lutData,
                           int gridSize,
                           ModelDomainLimitations domain = {});
    ~GeneratedAcousticModel() = default;

    /**
     * @brief Assigns or updates the underlying empirical LUT table.
     * @param lutData Pointer to 16-byte aligned AbdBatchedPoint array.
     * @param gridSize Dimension of N x N square grid.
     * @param domain Domain specifications and limits.
     */
    void setLutTable(const dsp::AbdBatchedPoint* lutData, int gridSize, ModelDomainLimitations domain = {}) noexcept;

    /**
     * @brief Pre-allocates scratch buffers and configures sample rate and block size.
     * Memory allocation occurs strictly in this method, NEVER during processBlock.
     * @param sampleRate Sampling rate in Hz (e.g. 44100, 48000, 96000).
     * @param maxBlockSize Maximum number of samples in any processBlock call.
     * @param numChannels Number of channels to allocate (clamped to max 8).
     * @return true on valid configuration, false on invalid parameters.
     */
    bool prepare(double sampleRate, int maxBlockSize, int numChannels = 2);

    /**
     * @brief Resets filter internal states and smoothing ballistic memory.
     * RT-Safe: zero allocation.
     */
    void reset() noexcept;

    /**
     * @brief Updates parameters with smooth ballistic interpolation.
     * RT-Safe: zero allocation.
     */
    void setParameters(const AcousticModelParameters& params) noexcept;

    /**
     * @brief Instantly snaps parameters bypassing ballistic smoothing.
     * RT-Safe: zero allocation.
     */
    void snapParameters(const AcousticModelParameters& params) noexcept;

    /**
     * @brief Real-time audio block rendering.
     * Strict RT-safety: zero dynamic allocations, bounds checked, handles in-place and nullptr.
     *
     * @param input Array of pointers to input channel buffers (can be nullptr for silence).
     * @param output Array of pointers to output channel buffers (must not be nullptr).
     * @param numChannels Number of channels in input/output arrays.
     * @param numSamples Number of audio samples to render (must be <= maxBlockSize).
     * @param parameters Operating parameters for this audio block.
     */
    void processBlock(const float* const* input,
                      float* const* output,
                      int numChannels,
                      int numSamples,
                      const AcousticModelParameters& parameters) noexcept;

    /**
     * @brief Overload using current internal parameter state.
     */
    void processBlock(const float* const* input,
                      float* const* output,
                      int numChannels,
                      int numSamples) noexcept;

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int getMaxBlockSize() const noexcept { return maxBlockSize_; }
    [[nodiscard]] int getNumChannels() const noexcept { return numChannels_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] const ModelDomainLimitations& getDomainLimitations() const noexcept { return domain_; }
    [[nodiscard]] const AcousticModelParameters& getCurrentParameters() const noexcept { return currentParams_; }

private:
    const dsp::AbdBatchedPoint* lutData_ { nullptr };
    int gridSize_ { 0 };
    ModelDomainLimitations domain_;

    double sampleRate_ { 48000.0 };
    int maxBlockSize_ { 256 };
    int numChannels_ { 2 };
    bool prepared_ { false };

    AcousticModelParameters currentParams_;
    dsp::AnalogLutFilterModule filterEngine_;

    // Scratch buffers pre-allocated in prepare() to ensure zero-heap in RT thread
    static constexpr int kMaxInternalChannels = 8;
    std::array<std::vector<float>, kMaxInternalChannels> scratchBuffers_;
};

} // namespace abdaudiolab::core
