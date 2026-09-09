#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/AnalogLutFilterModule.h"
#include <vector>
#include <cmath>

TEST_CASE("AnalogLutFilterModule Ballistic Exponential Smoothing Convergence", "[dsp][simd][smoothing]")
{
    using namespace abdaudiolab::dsp;

    AnalogLutFilterModule module;
    const double sampleRate = 44100.0;
    const double timeMs = 10.0; // 10ms time constant (tau = 441 samples)
    module.setSmoothingTime(timeMs, sampleRate);

    // Initial state: Voice 0 starts at 0.0, target set to 1.0
    module.setVoiceParameters(0, 1.0f, 0.8f);

    REQUIRE(module.getVoiceParam1Current(0) == 0.0f);
    REQUIRE(module.getVoiceParam2Current(0) == 0.0f);

    // Create a dummy LUT (gridSize = 4)
    const int gridSize = 4;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);
    for (auto& pt : lut)
    {
        pt.mu = 1.0f;
    }

    const int blockSize = 64;
    std::vector<float> voiceBuffer(blockSize, 1.0f);
    std::array<float*, 8> voicePtrs = { voiceBuffer.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    // Process blocks until 5*tau (~2205 samples)
    int totalSamplesProcessed = 0;
    while (totalSamplesProcessed < 2500)
    {
        std::fill(voiceBuffer.begin(), voiceBuffer.end(), 1.0f);
        module.processPolyphonicBlock(voicePtrs.data(), blockSize, lut.data(), gridSize);
        totalSamplesProcessed += blockSize;
    }

    // After 5*tau, exponential decay must have converged to > 99.3% of target (1 - e^-5 ≈ 0.99326)
    REQUIRE_THAT(module.getVoiceParam1Current(0), Catch::Matchers::WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(module.getVoiceParam2Current(0), Catch::Matchers::WithinAbs(0.8f, 0.01f));
}

TEST_CASE("AnalogLutFilterModule 8-Voice Simultaneous Modulation Accuracy", "[dsp][simd][polyphony]")
{
    using namespace abdaudiolab::dsp;

    AnalogLutFilterModule module;
    const int gridSize = 8;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);

    // Populate LUT with known analytic linear plane: f(x, y) = 0.20 + 0.50*x + 0.30*y
    for (int y = 0; y < gridSize; ++y)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            int idx = y * gridSize + x;
            float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
            float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);
            lut[idx].p1 = normX;
            lut[idx].p2 = normY;
            lut[idx].mu = 0.20f + (0.50f * normX) + (0.30f * normY);
        }
    }

    // Assign 8 distinct voice coordinates
    const float testP1[8] = { 0.10f, 0.25f, 0.40f, 0.55f, 0.70f, 0.85f, 0.0f, 1.0f };
    const float testP2[8] = { 0.90f, 0.75f, 0.60f, 0.45f, 0.30f, 0.15f, 1.0f, 0.0f };

    for (int v = 0; v < 8; ++v)
    {
        module.snapVoiceParameters(v, testP1[v], testP2[v]);
    }

    // Prepare 8 voice audio buffers with unit impulse / constant 1.0f
    const int blockSize = 32;
    std::vector<std::vector<float>> voiceBuffers(8, std::vector<float>(blockSize, 1.0f));
    std::array<float*, 8> voicePtrs;
    for (int v = 0; v < 8; ++v)
        voicePtrs[v] = voiceBuffers[v].data();

    // Process single block
    module.processPolyphonicBlock(voicePtrs.data(), blockSize, lut.data(), gridSize);

    // Verify each voice audio output matches f(p1, p2)
    for (int v = 0; v < 8; ++v)
    {
        float expectedGain = 0.20f + (0.50f * testP1[v]) + (0.30f * testP2[v]);

        for (int s = 0; s < blockSize; ++s)
        {
            REQUIRE_THAT(voiceBuffers[v][s], Catch::Matchers::WithinAbs(expectedGain, 1e-4f));
        }
    }
}

TEST_CASE("AnalogLutFilterModule Nullptr and Bounds Safety", "[dsp][simd][safety]")
{
    using namespace abdaudiolab::dsp;

    AnalogLutFilterModule module;
    const int gridSize = 4;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);
    for (auto& pt : lut) pt.mu = 0.5f;

    std::vector<float> v0(16, 1.0f);
    std::vector<float> v7(16, 2.0f);

    // Sparse voice array: only voice 0 and 7 active, others nullptr
    std::array<float*, 8> sparsePtrs = { v0.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, v7.data() };

    module.snapVoiceParameters(0, 0.5f, 0.5f);
    module.snapVoiceParameters(7, 0.5f, 0.5f);

    // Must execute safely without crashing on null channels
    REQUIRE_NOTHROW(module.processPolyphonicBlock(sparsePtrs.data(), 16, lut.data(), gridSize));
    REQUIRE_NOTHROW(module.processPolyphonicBlock(nullptr, 16, lut.data(), gridSize));
    REQUIRE_NOTHROW(module.processPolyphonicBlock(sparsePtrs.data(), 0, lut.data(), gridSize));
    REQUIRE_NOTHROW(module.processPolyphonicBlock(sparsePtrs.data(), 16, nullptr, gridSize));

    REQUIRE_THAT(v0[0], Catch::Matchers::WithinAbs(0.5f, 1e-4f));
    REQUIRE_THAT(v7[0], Catch::Matchers::WithinAbs(1.0f, 1e-4f)); // 2.0f * 0.5f = 1.0f
}

#include "dsp/OfficialLutModels.h"

TEST_CASE("OfficialLutModels Canonical Shared Tables Verification", "[dsp][simd][official_lut]")
{
    using namespace abdaudiolab::dsp;
    namespace models = abdaudiolab::dsp::models;

    // Verify sizes and alignment of canonical LUTs
    STATIC_REQUIRE(models::behringer_pro800_cem3320_vcf_SIZE == 32);
    STATIC_REQUIRE(models::roland_juno106_ir3109_vcf_SIZE == 32);
    STATIC_REQUIRE(models::mock_va_synth_moog_ladder_SIZE == 32);
    STATIC_REQUIRE(models::casio_cz101_phase_distortion_resonant_SIZE == 32);
    STATIC_REQUIRE(models::manual_eurorack_vcf_diode_ladder_SIZE == 32);
    STATIC_REQUIRE(models::roland_aira_bitrazer_filter_SIZE == 32);

    // Verify 16-byte alignment
    REQUIRE(reinterpret_cast<uintptr_t>(models::behringer_pro800_cem3320_vcf) % 16 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(models::roland_juno106_ir3109_vcf) % 16 == 0);

    // Verify cutoff at corners using evaluateBilinear4Voices
    alignas(16) float p1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
    alignas(16) float p2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    __m128 results = LutEvaluatorSimd::evaluateBilinear4Voices(
        p1, p2, models::behringer_pro800_cem3320_vcf, 8, LutMetric::PrimaryMean
    );
    alignas(16) float out[4];
    _mm_store_ps(out, results);

    // Cutoff at (0.0, 0.0) -> 20 Hz
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(20.0f, 0.1f));
    // Cutoff at (1.0, 0.0) -> 20000 Hz
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(20000.0f, 0.1f));

    // Process polyphonic block using Juno-106 IR3109 table
    AnalogLutFilterModule module;
    std::vector<float> voiceBuffer(32, 1.0f);
    std::array<float*, 8> voicePtrs = { voiceBuffer.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    module.snapVoiceParameters(0, 0.5f, 0.5f);
    REQUIRE_NOTHROW(module.processPolyphonicBlock(voicePtrs.data(), 32, models::roland_juno106_ir3109_vcf, 8));
}
