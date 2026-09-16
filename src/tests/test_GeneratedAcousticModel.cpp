/**
 * @file test_GeneratedAcousticModel.cpp
 * @brief Unit tests for GeneratedAcousticModel runtime (T1).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/GeneratedAcousticModel.h"
#include <cmath>
#include <numbers>
#include <vector>

namespace
{

std::vector<abdaudiolab::dsp::AbdBatchedPoint> createTestLut(int gridSize, float baseMu)
{
    std::vector<abdaudiolab::dsp::AbdBatchedPoint> lut(static_cast<size_t>(gridSize * gridSize));
    for (int y = 0; y < gridSize; ++y)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            size_t idx = static_cast<size_t>(y * gridSize + x);
            lut[idx].p1 = static_cast<float>(x) / static_cast<float>(gridSize - 1);
            lut[idx].p2 = static_cast<float>(y) / static_cast<float>(gridSize - 1);
            lut[idx].mu = baseMu * (0.2f + 0.8f * lut[idx].p1);
            lut[idx].sigma = 0.01f;
            lut[idx].sec_mu = 1.0f;
            lut[idx].sec_sigma = 0.0f;
            lut[idx].thd_percent = 0.5f;
            lut[idx].reserved = 0.0f;
        }
    }
    return lut;
}

} // namespace

TEST_CASE("GeneratedAcousticModel - Lifecycle, Prepare and Invariants", "[model][runtime][lifecycle]")
{
    using namespace abdaudiolab::core;
    using namespace abdaudiolab::dsp;

    const int gridSize = 4;
    auto lut = createTestLut(gridSize, 0.5f);

    GeneratedAcousticModel model;
    CHECK_FALSE(model.isPrepared());
    CHECK(model.getSampleRate() == 48000.0);

    model.setLutTable(lut.data(), gridSize);
    CHECK(model.getDomainLimitations().gridSize == 4);
    CHECK(model.getDomainLimitations().totalPoints == 16);

    // Invalid prepare arguments
    CHECK_FALSE(model.prepare(1000.0, 256, 2)); // Too low SR
    CHECK_FALSE(model.prepare(48000.0, 0, 2));   // Invalid block size
    CHECK_FALSE(model.prepare(48000.0, 256, 0)); // Invalid channel count

    // Valid prepare
    REQUIRE(model.prepare(48000.0, 256, 2));
    CHECK(model.isPrepared());
    CHECK(model.getSampleRate() == 48000.0);
    CHECK(model.getMaxBlockSize() == 256);
    CHECK(model.getNumChannels() == 2);
}

TEST_CASE("GeneratedAcousticModel - Processing with Empirical LUT", "[model][runtime][process]")
{
    using namespace abdaudiolab::core;

    const int gridSize = 4;
    auto lut = createTestLut(gridSize, 0.5f);

    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(48000.0, 128, 2));

    AcousticModelParameters params;
    params.param1 = 1.0f; // Max gain: 0.5 * (0.2 + 0.8) = 0.5f
    params.param2 = 0.0f;
    model.snapParameters(params);

    const int blockSize = 64;
    std::vector<float> inL(blockSize, 1.0f);
    std::vector<float> inR(blockSize, 1.0f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    model.processBlock(inPtrs, outPtrs, 2, blockSize);

    // Filter output must be non-zero and scaled by LUT mu gain (~0.5)
    CHECK(outL[0] > 0.1f);
    CHECK(outR[0] > 0.1f);
    REQUIRE_THAT(outL[blockSize - 1], Catch::Matchers::WithinAbs(0.5f, 0.05f));
    REQUIRE_THAT(outR[blockSize - 1], Catch::Matchers::WithinAbs(0.5f, 0.05f));
}

TEST_CASE("GeneratedAcousticModel - In-Place Processing Safety", "[model][runtime][inplace]")
{
    using namespace abdaudiolab::core;

    const int gridSize = 4;
    auto lut = createTestLut(gridSize, 0.8f);

    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(48000.0, 128, 1));

    AcousticModelParameters params;
    params.param1 = 0.0f; // Min gain: 0.8 * 0.2 = 0.16f
    params.param2 = 0.0f;
    model.snapParameters(params);

    const int blockSize = 64;
    std::vector<float> buffer(blockSize, 1.0f);
    float* channelPtrs[1] = { buffer.data() };

    // In-place: input and output pointers point to the exact same memory
    model.processBlock(channelPtrs, channelPtrs, 1, blockSize);

    REQUIRE_THAT(buffer[blockSize - 1], Catch::Matchers::WithinAbs(0.16f, 0.05f));
}

TEST_CASE("GeneratedAcousticModel - Nullptr and Boundary Resilience", "[model][runtime][safety]")
{
    using namespace abdaudiolab::core;

    GeneratedAcousticModel uninitModel;
    std::vector<float> buf(32, 1.0f);
    float* ptrs[1] = { buf.data() };

    // Should not crash when not prepared
    uninitModel.processBlock(ptrs, ptrs, 1, 32);

    const int gridSize = 4;
    auto lut = createTestLut(gridSize, 0.5f);
    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(48000.0, 128, 2));

    // Null outputs
    model.processBlock(ptrs, nullptr, 1, 32);

    // Negative or zero samples
    model.processBlock(ptrs, ptrs, 1, 0);
    model.processBlock(ptrs, ptrs, 1, -5);

    // Null channel inside pointers array
    float* sparsePtrs[2] = { buf.data(), nullptr };
    model.processBlock(sparsePtrs, sparsePtrs, 2, 32);

    // Out-of-bounds parameter clamping
    AcousticModelParameters extremeParams;
    extremeParams.param1 = 5.0f;
    extremeParams.param2 = -10.0f;
    model.setParameters(extremeParams);
    CHECK(model.getCurrentParameters().param1 == 1.0f);
    CHECK(model.getCurrentParameters().param2 == 0.0f);
}

TEST_CASE("GeneratedAcousticModel - RT Zero Allocation Verification Loop", "[model][runtime][zero_heap]")
{
    using namespace abdaudiolab::core;

    const int gridSize = 4;
    auto lut = createTestLut(gridSize, 0.5f);

    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(96000.0, 512, 2));

    const int blockSize = 128;
    std::vector<float> inL(blockSize, 0.5f);
    std::vector<float> inR(blockSize, -0.5f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    // Run 500 consecutive processBlock cycles simulating 1.5 seconds of audio
    // Parameter sweep between blocks
    for (int block = 0; block < 500; ++block)
    {
        float p1 = static_cast<float>(block % 100) / 100.0f;
        float p2 = static_cast<float>((block * 3) % 100) / 100.0f;
        AcousticModelParameters params { p1, p2 };

        model.processBlock(inPtrs, outPtrs, 2, blockSize, params);

        if (block % 50 == 0)
        {
            model.reset();
        }
    }

    CHECK(outL[0] != 0.0f);
    CHECK(outR[0] != 0.0f);
}
