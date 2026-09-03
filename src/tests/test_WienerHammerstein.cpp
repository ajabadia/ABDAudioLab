#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/WienerHammersteinFitter.h"
#include "math/LabAnalyticEngine.h"
#include <cmath>
#include <numbers>

TEST_CASE("WienerHammersteinFitter LNL System Identification", "[dsp][wiener_hammerstein]")
{
    using namespace abdaudiolab::math;

    const double sampleRate = 48000.0;
    const size_t N = 1024;

    // Generate input test signal: mix of sinusoids
    std::vector<float> inputSignal(N);
    for (size_t n = 0; n < N; ++n)
    {
        double t = static_cast<double>(n) / sampleRate;
        inputSignal[n] = static_cast<float>(
            0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * t) +
            0.3 * std::sin(2.0 * std::numbers::pi * 1200.0 * t)
        );
    }

    // Ground truth LNL system
    WienerHammersteinModel trueModel;
    trueModel.h1Taps = { 0.15f, 0.7f, 0.15f };
    trueModel.nonLinearCoeffA = 0.25f;
    trueModel.h2Taps = { 0.1f, 0.8f, 0.1f };

    std::vector<float> targetSignal;
    WienerHammersteinFitter::process(trueModel, inputSignal, targetSignal);

    // Fit model using WienerHammersteinFitter
    WienerHammersteinConfig cfg;
    cfg.numTapsH1 = 7;
    cfg.numTapsH2 = 7;
    cfg.maxEpochs = 150;
    cfg.learningRate = 0.03f;

    WienerHammersteinFitter fitter(cfg);
    auto model = fitter.fit(inputSignal, targetSignal, sampleRate);

    // Verify model identified system with high fidelity
    REQUIRE(model.iterationsRun > 0);
    REQUIRE(model.goodnessOfFitR2 > 0.85f); // High explained variance
    REQUIRE(model.residualErrorRms < 0.05f); // Low reconstruction error
    REQUIRE(std::isfinite(model.nonLinearCoeffA));

    // Test process inference
    std::vector<float> predicted;
    WienerHammersteinFitter::process(model, inputSignal, predicted);
    REQUIRE(predicted.size() == inputSignal.size());

    // Test LabAnalyticEngine integration across multiple passes
    std::vector<std::vector<float>> recordedPasses = { targetSignal, targetSignal, targetSignal };
    auto analyticRes = LabAnalyticEngine::analyzeWienerHammerstein(recordedPasses, inputSignal, sampleRate);

    REQUIRE(analyticRes.goodnessOfFitR2.mean > 0.85f);
    REQUIRE(std::isfinite(analyticRes.nonLinearCoeffA.mean));
    REQUIRE(analyticRes.goodnessOfFitR2.stdDev >= 0.0f);
    REQUIRE_FALSE(analyticRes.representativeH1.empty());
    REQUIRE_FALSE(analyticRes.representativeH2.empty());
}
