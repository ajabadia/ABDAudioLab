/**
 * @file GeneratedAcousticModel.cpp
 * @brief Implementation of GeneratedAcousticModel empirical LUT runtime.
 * @author ABDSynths
 * @date 2026
 */

#include "GeneratedAcousticModel.h"
#include <cstring>

namespace abdaudiolab::core
{

GeneratedAcousticModel::GeneratedAcousticModel()
{
    domain_.gridSize = 0;
    domain_.totalPoints = 0;
}

GeneratedAcousticModel::GeneratedAcousticModel(const dsp::AbdBatchedPoint* lutData,
                                               int gridSize,
                                               ModelDomainLimitations domain)
    : lutData_(lutData), gridSize_(gridSize), domain_(domain)
{
    if (gridSize_ > 0)
    {
        domain_.gridSize = gridSize_;
        domain_.totalPoints = static_cast<size_t>(gridSize_ * gridSize_);
    }
}

void GeneratedAcousticModel::setLutTable(const dsp::AbdBatchedPoint* lutData,
                                         int gridSize,
                                         ModelDomainLimitations domain) noexcept
{
    lutData_ = lutData;
    gridSize_ = gridSize;
    domain_ = domain;
    if (gridSize_ > 0)
    {
        domain_.gridSize = gridSize_;
        domain_.totalPoints = static_cast<size_t>(gridSize_ * gridSize_);
    }
}

bool GeneratedAcousticModel::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    if (sampleRate < 8000.0 || maxBlockSize <= 0 || numChannels <= 0)
        return false;

    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    numChannels_ = std::clamp(numChannels, 1, kMaxInternalChannels);

    for (int ch = 0; ch < kMaxInternalChannels; ++ch)
    {
        scratchBuffers_[static_cast<size_t>(ch)].assign(static_cast<size_t>(maxBlockSize_), 0.0f);
    }

    filterEngine_.setSmoothingTime(5.0, sampleRate_);
    reset();

    prepared_ = true;
    return true;
}

void GeneratedAcousticModel::reset() noexcept
{
    snapParameters(currentParams_);
}

void GeneratedAcousticModel::setParameters(const AcousticModelParameters& params) noexcept
{
    currentParams_.param1 = std::clamp(params.param1, domain_.param1Min, domain_.param1Max);
    currentParams_.param2 = std::clamp(params.param2, domain_.param2Min, domain_.param2Max);

    for (int ch = 0; ch < kMaxInternalChannels; ++ch)
    {
        filterEngine_.setVoiceParameters(ch, currentParams_.param1, currentParams_.param2);
    }
}

void GeneratedAcousticModel::snapParameters(const AcousticModelParameters& params) noexcept
{
    currentParams_.param1 = std::clamp(params.param1, domain_.param1Min, domain_.param1Max);
    currentParams_.param2 = std::clamp(params.param2, domain_.param2Min, domain_.param2Max);

    for (int ch = 0; ch < kMaxInternalChannels; ++ch)
    {
        filterEngine_.snapVoiceParameters(ch, currentParams_.param1, currentParams_.param2);
    }
}

void GeneratedAcousticModel::processBlock(const float* const* input,
                                          float* const* output,
                                          int numChannels,
                                          int numSamples,
                                          const AcousticModelParameters& parameters) noexcept
{
    setParameters(parameters);
    processBlock(input, output, numChannels, numSamples);
}

void GeneratedAcousticModel::processBlock(const float* const* input,
                                          float* const* output,
                                          int numChannels,
                                          int numSamples) noexcept
{
    if (output == nullptr || numSamples <= 0 || numChannels <= 0)
        return;

    // Safety fallback: if not prepared or missing LUT, perform safe pass-through or silence
    if (!prepared_ || lutData_ == nullptr || gridSize_ <= 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (output[ch] == nullptr)
                continue;

            if (input != nullptr && input[ch] != nullptr)
            {
                if (input[ch] != output[ch])
                    std::copy_n(input[ch], static_cast<size_t>(numSamples), output[ch]);
            }
            else
            {
                std::fill_n(output[ch], static_cast<size_t>(numSamples), 0.0f);
            }
        }
        return;
    }

    const int samplesToRender = std::min(numSamples, maxBlockSize_);
    const int activeChannels = std::min({ numChannels, kMaxInternalChannels, numChannels_ });

    std::array<float*, kMaxInternalChannels> voicePtrs = { nullptr };

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        if (output[ch] == nullptr)
            continue;

        float* scratch = scratchBuffers_[static_cast<size_t>(ch)].data();
        if (input != nullptr && input[ch] != nullptr)
        {
            std::copy_n(input[ch], static_cast<size_t>(samplesToRender), scratch);
        }
        else
        {
            std::fill_n(scratch, static_cast<size_t>(samplesToRender), 0.0f);
        }

        voicePtrs[static_cast<size_t>(ch)] = scratch;
    }

    // Execute zero-allocation SIMD polyphonic LUT filter engine
    filterEngine_.processPolyphonicBlock(voicePtrs.data(), samplesToRender, lutData_, gridSize_);

    // Output delivery
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        if (output[ch] == nullptr)
            continue;

        const float* scratch = scratchBuffers_[static_cast<size_t>(ch)].data();
        std::copy_n(scratch, static_cast<size_t>(samplesToRender), output[ch]);

        if (numSamples > samplesToRender)
        {
            std::fill(output[ch] + samplesToRender, output[ch] + numSamples, 0.0f);
        }
    }

    // Pass-through any extra channels beyond kMaxInternalChannels
    for (int ch = activeChannels; ch < numChannels; ++ch)
    {
        if (output[ch] == nullptr)
            continue;

        if (input != nullptr && input[ch] != nullptr)
        {
            if (input[ch] != output[ch])
                std::copy_n(input[ch], static_cast<size_t>(numSamples), output[ch]);
        }
        else
        {
            std::fill_n(output[ch], static_cast<size_t>(numSamples), 0.0f);
        }
    }
}

} // namespace abdaudiolab::core
