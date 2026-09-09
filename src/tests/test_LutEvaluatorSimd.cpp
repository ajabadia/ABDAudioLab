#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/LutEvaluatorSimd.h"
#include <vector>
#include <cmath>

TEST_CASE("LutEvaluatorSimd Precision and Scalar Identity", "[dsp][simd]")
{
    using namespace abdaudiolab::dsp;

    const int gridSize = 16;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);

    // Populate a test grid with non-linear synthetic values
    for (int y = 0; y < gridSize; ++y)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            int idx = y * gridSize + x;
            float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
            float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);

            lut[idx].p1 = normX;
            lut[idx].p2 = normY;
            lut[idx].mu = std::sin(normX * 3.14159265f) * std::cos(normY * 3.14159265f) * 1000.0f;
            lut[idx].sigma = normX * 0.05f;
            lut[idx].sec_mu = normY * 12.0f;
            lut[idx].sec_sigma = 0.01f;
            lut[idx].thd_percent = (normX + normY) * 2.5f;
            lut[idx].reserved = 0.0f;
        }
    }

    // Test 4 distinct voices with fractional coordinates
    alignas(16) float p1[4] = { 0.125f, 0.450f, 0.780f, 0.950f };
    alignas(16) float p2[4] = { 0.333f, 0.666f, 0.210f, 0.888f };

    __m128 simdRes = LutEvaluatorSimd::evaluateBilinear4Voices(p1, p2, lut.data(), gridSize, LutMetric::PrimaryMean);
    __m128 scalarRes = LutEvaluatorSimd::evaluateBilinear4VoicesScalar(p1, p2, lut.data(), gridSize, LutMetric::PrimaryMean);

    alignas(16) float simdOut[4];
    alignas(16) float scalarOut[4];
    _mm_store_ps(simdOut, simdRes);
    _mm_store_ps(scalarOut, scalarRes);

    for (int v = 0; v < 4; ++v)
    {
        // SIMD and scalar fallback must be identical within floating point epsilon
        REQUIRE_THAT(simdOut[v], Catch::Matchers::WithinAbs(scalarOut[v], 1e-4f));
    }
}

TEST_CASE("LutEvaluatorSimd Boundary Clamping and Out-of-Range Handling", "[dsp][simd]")
{
    using namespace abdaudiolab::dsp;

    const int gridSize = 8;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);

    for (int i = 0; i < gridSize * gridSize; ++i)
    {
        lut[i].mu = 42.0f;
        lut[i].thd_percent = 1.25f;
    }

    // Inputs with negative and out-of-range bounds
    alignas(16) float p1[4] = { -0.5f, 1.5f, 0.0f, 1.0f };
    alignas(16) float p2[4] = { -0.2f, 2.0f, 1.0f, 0.0f };

    __m128 simdRes = LutEvaluatorSimd::evaluateBilinear4Voices(p1, p2, lut.data(), gridSize, LutMetric::PrimaryMean);

    alignas(16) float out[4];
    _mm_store_ps(out, simdRes);

    for (int v = 0; v < 4; ++v)
    {
        REQUIRE_FALSE(std::isnan(out[v]));
        REQUIRE_FALSE(std::isinf(out[v]));
        REQUIRE_THAT(out[v], Catch::Matchers::WithinAbs(42.0f, 1e-4f));
    }
}

TEST_CASE("LutEvaluatorSimd Multi-Metric Evaluation", "[dsp][simd]")
{
    using namespace abdaudiolab::dsp;

    const int gridSize = 4;
    std::vector<AbdBatchedPoint> lut(gridSize * gridSize);

    for (int i = 0; i < gridSize * gridSize; ++i)
    {
        lut[i].mu = 10.0f;
        lut[i].sigma = 0.5f;
        lut[i].sec_mu = 20.0f;
        lut[i].sec_sigma = 0.02f;
        lut[i].thd_percent = 3.5f;
    }

    alignas(16) float p1[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    alignas(16) float p2[4] = { 0.5f, 0.5f, 0.5f, 0.5f };

    alignas(16) float outMu[4], outSigma[4], outThd[4];

    _mm_store_ps(outMu, LutEvaluatorSimd::evaluateBilinear4Voices(p1, p2, lut.data(), gridSize, LutMetric::PrimaryMean));
    _mm_store_ps(outSigma, LutEvaluatorSimd::evaluateBilinear4Voices(p1, p2, lut.data(), gridSize, LutMetric::PrimaryStdDev));
    _mm_store_ps(outThd, LutEvaluatorSimd::evaluateBilinear4Voices(p1, p2, lut.data(), gridSize, LutMetric::ThdPercent));

    for (int v = 0; v < 4; ++v)
    {
        REQUIRE_THAT(outMu[v], Catch::Matchers::WithinAbs(10.0f, 1e-4f));
        REQUIRE_THAT(outSigma[v], Catch::Matchers::WithinAbs(0.5f, 1e-4f));
        REQUIRE_THAT(outThd[v], Catch::Matchers::WithinAbs(3.5f, 1e-4f));
    }
}

TEST_CASE("LutEvaluator1DSimd Casio CZ DCW 100-Point Curve Evaluation", "[shared][dsp][simd]")
{
    using namespace abdaudiolab::dsp;

    LutEvaluator1DSimd evaluator;
    REQUIRE(evaluator.isEmpty());

    // 100 points simulating the non-linear Phase Distortion transfer function
    const int numPoints = 100;
    std::vector<float> dcwCurve(numPoints);
    for (int i = 0; i < numPoints; ++i)
    {
        float x = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        // Non-linear knee curve resembling CZ-101 PD response
        dcwCurve[i] = std::sin(x * 1.57079632f) * 2.0f;
    }

    evaluator.loadLutData(dcwCurve);
    REQUIRE(evaluator.getLutSize() == 100);
    REQUIRE_THAT(evaluator.getScaleFactor(), Catch::Matchers::WithinAbs(99.0f, 1e-5f));

    // Evaluate known boundary points
    REQUIRE_THAT(evaluator.evaluateSingle(0.0f), Catch::Matchers::WithinAbs(dcwCurve.front(), 1e-5f));
    REQUIRE_THAT(evaluator.evaluateSingle(1.0f), Catch::Matchers::WithinAbs(dcwCurve.back(), 1e-5f));

    // Test SIMD 4-lane vector against scalar
    alignas(16) float inVals[4] = { 0.0f, 0.25f, 0.50f, 1.0f };
    __m128 inVec = _mm_load_ps(inVals);
    __m128 outVec = evaluator.evaluateSingleValueSimd(inVec);

    alignas(16) float outVals[4];
    _mm_store_ps(outVals, outVec);

    for (int i = 0; i < 4; ++i)
    {
        float expectedScalar = evaluator.evaluateSingle(inVals[i]);
        REQUIRE_THAT(outVals[i], Catch::Matchers::WithinAbs(expectedScalar, 1e-5f));
    }

    // Out-of-bounds clamping tests
    alignas(16) float clampedInputs[4] = { -0.5f, 1.5f, -100.0f, 2.0f };
    __m128 clampedVec = evaluator.evaluateSingleValueSimd(_mm_load_ps(clampedInputs));
    alignas(16) float clampedOutputs[4];
    _mm_store_ps(clampedOutputs, clampedVec);

    REQUIRE_THAT(clampedOutputs[0], Catch::Matchers::WithinAbs(dcwCurve.front(), 1e-5f));
    REQUIRE_THAT(clampedOutputs[1], Catch::Matchers::WithinAbs(dcwCurve.back(), 1e-5f));
    REQUIRE_THAT(clampedOutputs[2], Catch::Matchers::WithinAbs(dcwCurve.front(), 1e-5f));
    REQUIRE_THAT(clampedOutputs[3], Catch::Matchers::WithinAbs(dcwCurve.back(), 1e-5f));
}

TEST_CASE("LutEvaluator1DSimd Block Audio & Stereo Processing", "[shared][dsp][simd]")
{
    using namespace abdaudiolab::dsp;

    LutEvaluator1DSimd evaluator;
    std::vector<float> linearRamp = { 0.0f, 10.0f, 20.0f, 30.0f, 40.0f };
    evaluator.loadLutData(linearRamp);

    const int bufferSize = 10;
    std::vector<float> audioL = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
    std::vector<float> audioR = audioL;

    evaluator.processStereoBlockSimd(audioL.data(), audioR.data(), bufferSize);

    for (int i = 0; i < bufferSize; ++i)
    {
        REQUIRE_THAT(audioL[i], Catch::Matchers::WithinAbs(audioR[i], 1e-5f));
        REQUIRE(audioL[i] >= 0.0f);
        REQUIRE(audioL[i] <= 40.0f);
    }
}

