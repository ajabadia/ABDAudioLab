#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LoopbackCalibrator.h"
#include "math/FarinaDeconvolver.h"
#include <vector>
#include <cmath>
#include <limits>

using namespace abdaudiolab::math;

TEST_CASE("LoopbackCalibrator Sample Rate Limit Helper Policies", "[math][loopback][adaptation]")
{
    SECTION("computeSafeSweepMaxHz respects Nyquist margin and 20 kHz ceiling")
    {
        // 44.1 kHz -> 0.45 * 44100 = 19845.0 Hz (< 20 kHz, < 22050 Hz)
        REQUIRE_THAT(LoopbackCalibrator::computeSafeSweepMaxHz(44100.0),
                     Catch::Matchers::WithinAbs(19845.0f, 0.1f));

        // 48.0 kHz -> 0.45 * 48000 = 21600.0 Hz, capped at 20000.0 Hz
        REQUIRE_THAT(LoopbackCalibrator::computeSafeSweepMaxHz(48000.0),
                     Catch::Matchers::WithinAbs(20000.0f, 0.1f));

        // 96.0 kHz -> 0.45 * 96000 = 43200.0 Hz, capped at 20000.0 Hz
        REQUIRE_THAT(LoopbackCalibrator::computeSafeSweepMaxHz(96000.0),
                     Catch::Matchers::WithinAbs(20000.0f, 0.1f));

        // Edge cases: invalid or negative sample rates fallback safely
        REQUIRE_THAT(LoopbackCalibrator::computeSafeSweepMaxHz(0.0),
                     Catch::Matchers::WithinAbs(20000.0f, 0.1f));
        REQUIRE_THAT(LoopbackCalibrator::computeSafeSweepMaxHz(-44100.0),
                     Catch::Matchers::WithinAbs(20000.0f, 0.1f));
    }

    SECTION("computeFlatnessBandHz produces conservative passband [40 Hz, f_high]")
    {
        // 44.1 kHz -> [40 Hz, 0.40 * 44100 = 17640 Hz]
        auto [low44, high44] = LoopbackCalibrator::computeFlatnessBandHz(44100.0);
        REQUIRE_THAT(low44, Catch::Matchers::WithinAbs(40.0f, 0.01f));
        REQUIRE_THAT(high44, Catch::Matchers::WithinAbs(17640.0f, 0.1f));

        // 48.0 kHz -> [40 Hz, min(18000, 0.40 * 48000 = 19200) = 18000 Hz]
        auto [low48, high48] = LoopbackCalibrator::computeFlatnessBandHz(48000.0);
        REQUIRE_THAT(low48, Catch::Matchers::WithinAbs(40.0f, 0.01f));
        REQUIRE_THAT(high48, Catch::Matchers::WithinAbs(18000.0f, 0.1f));

        // 96.0 kHz -> [40 Hz, min(18000, 0.40 * 96000 = 38400) = 18000 Hz]
        auto [low96, high96] = LoopbackCalibrator::computeFlatnessBandHz(96000.0);
        REQUIRE_THAT(low96, Catch::Matchers::WithinAbs(40.0f, 0.01f));
        REQUIRE_THAT(high96, Catch::Matchers::WithinAbs(18000.0f, 0.1f));

        // Edge case: invalid sample rate
        auto [lowZero, highZero] = LoopbackCalibrator::computeFlatnessBandHz(0.0);
        REQUIRE_THAT(lowZero, Catch::Matchers::WithinAbs(40.0f, 0.01f));
        REQUIRE_THAT(highZero, Catch::Matchers::WithinAbs(18000.0f, 0.1f));
    }
}

TEST_CASE("LoopbackCalibrator Pure Flatness Evaluator", "[math][loopback][adaptation]")
{
    // Generate linear grid of frequencies 0 Hz to 22050 Hz (step 10 Hz)
    std::vector<float> freqs;
    freqs.reserve(2206);
    for (int f = 0; f <= 22050; f += 10)
        freqs.push_back(static_cast<float>(f));

    SECTION("Flat response inside passband yields 0.0 dB delta and hasValidBins=true")
    {
        std::vector<float> mags(freqs.size(), 0.0f);
        auto eval = LoopbackCalibrator::evaluateFlatnessInBand(freqs, mags, 40.0f, 17640.0f);

        REQUIRE(eval.hasValidBins == true);
        REQUIRE(eval.validBinCount > 1000);
        REQUIRE_THAT(eval.deltaDb, Catch::Matchers::WithinAbs(0.0f, 1e-4f));
    }

    SECTION("Sharp roll-off above 18 kHz does not penalize passband flatness at 44.1 kHz")
    {
        std::vector<float> mags(freqs.size(), 0.0f);
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] > 18000.0f)
            {
                // Simulate steep 50 dB attenuation in Nyquist transition band
                float over = freqs[i] - 18000.0f;
                mags[i] = -std::min(50.0f, over * 0.025f);
            }
        }

        // Passband [40 Hz, 17640 Hz] is flat
        auto eval = LoopbackCalibrator::evaluateFlatnessInBand(freqs, mags, 40.0f, 17640.0f);
        REQUIRE(eval.hasValidBins == true);
        REQUIRE_THAT(eval.deltaDb, Catch::Matchers::WithinAbs(0.0f, 1e-3f));
    }

    SECTION("Drop within passband (e.g. at 5 kHz) correctly triggers excessive deltaDb")
    {
        std::vector<float> mags(freqs.size(), 0.0f);
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 4800.0f && freqs[i] <= 5200.0f)
                mags[i] = -8.5f; // 8.5 dB notch inside audible band
        }

        auto eval = LoopbackCalibrator::evaluateFlatnessInBand(freqs, mags, 40.0f, 17640.0f);
        REQUIRE(eval.hasValidBins == true);
        REQUIRE(eval.deltaDb > 6.0f);
    }

    SECTION("Defensive contracts: empty vectors, mismatched sizes, or empty band fail securely")
    {
        std::vector<float> emptyVec;
        std::vector<float> singleVal { 0.0f };

        // Empty vector
        auto evalEmpty = LoopbackCalibrator::evaluateFlatnessInBand(emptyVec, emptyVec, 40.0f, 17640.0f);
        REQUIRE(evalEmpty.hasValidBins == false);
        REQUIRE(std::isinf(evalEmpty.deltaDb));
        REQUIRE(evalEmpty.validBinCount == 0);

        // Mismatched vector sizes
        auto evalMismatch = LoopbackCalibrator::evaluateFlatnessInBand(freqs, singleVal, 40.0f, 17640.0f);
        REQUIRE(evalMismatch.hasValidBins == false);
        REQUIRE(std::isinf(evalMismatch.deltaDb));

        // Inverted or empty frequency band
        auto evalInverted = LoopbackCalibrator::evaluateFlatnessInBand(freqs, std::vector<float>(freqs.size(), 0.0f), 20000.0f, 1000.0f);
        REQUIRE(evalInverted.hasValidBins == false);
        REQUIRE(std::isinf(evalInverted.deltaDb));

        // Band outside all bins (e.g. 30000 Hz to 40000 Hz when max freq is 22050 Hz)
        auto evalNoIntersection = LoopbackCalibrator::evaluateFlatnessInBand(freqs, std::vector<float>(freqs.size(), 0.0f), 30000.0f, 40000.0f);
        REQUIRE(evalNoIntersection.hasValidBins == false);
        REQUIRE(std::isinf(evalNoIntersection.deltaDb));
        REQUIRE(evalNoIntersection.validBinCount == 0);
    }
}

TEST_CASE("LoopbackCalibrator Full Farina Sweep Integration with Adaptive Limits", "[math][loopback][adaptation]")
{
    const double sampleRate = 44100.0;
    const double durationSec = 1.0;
    const float startFreq = 20.0f;
    const float endFreq = LoopbackCalibrator::computeSafeSweepMaxHz(sampleRate); // 19845 Hz

    REQUIRE_THAT(endFreq, Catch::Matchers::WithinAbs(19845.0f, 0.1f));

    auto cleanSweep = FarinaDeconvolver::generateLogFarinaSweep(sampleRate, durationSec, startFreq, endFreq);
    cleanSweep.resize(cleanSweep.size() + 4096, 0.0f);

    // Scale to nominal -3.1 dBFS
    for (auto& s : cleanSweep)
        s *= 0.7f;

    SECTION("Synthetic sweep at 44.1 kHz with safe endFreq passes calibration")
    {
        auto data = LoopbackCalibrator::analyzeLoopback(cleanSweep, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE(data.clippingDetected == false);
        REQUIRE(data.peakInDbfs > -40.0f);
        REQUIRE(data.frequencyFlatnessDb < 6.0f);
        REQUIRE(data.isCalibrated == true);
    }

    SECTION("Clipping remains fatal even if frequency response is flat")
    {
        auto clippedSweep = cleanSweep;
        clippedSweep[600] = 1.0f;
        clippedSweep[601] = -1.0f;

        auto data = LoopbackCalibrator::analyzeLoopback(clippedSweep, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE(data.clippingDetected == true);
        REQUIRE_FALSE(data.isCalibrated);
    }

    SECTION("Signal below -40 dBFS threshold remains fatal")
    {
        auto quietSweep = cleanSweep;
        for (auto& s : quietSweep)
            s *= 0.001f; // ~ -60 dBFS

        auto data = LoopbackCalibrator::analyzeLoopback(quietSweep, sampleRate, durationSec, startFreq, endFreq);

        REQUIRE(data.peakInDbfs <= -40.0f);
        REQUIRE_FALSE(data.isCalibrated);
    }
}
