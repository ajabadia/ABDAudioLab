/**
 * @file test_Phase20_12_FineLatency_T1.cpp
 * @brief Unit and integration tests for Phase 20.12 Increment 1 (T20.12-1):
 *        Fine Sub-Sample Latency, Clock Drift Modeling, Multichannel Skew & Compensation Views.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "measurement/FineLatencyContracts.h"
#include "measurement/FineLatencyAnalyzer.h"
#include <vector>
#include <cmath>
#include <algorithm>

using namespace abdaudiolab::measurement;

namespace
{
    // Helper to generate a bandlimited sinc-interpolated delayed impulse or chirp
    std::vector<float> generateSincDelayedSignal(
        const std::vector<float>& source,
        double delaySamples,
        size_t outputLength)
    {
        std::vector<float> out(outputLength, 0.0f);
        const int filterRadius = 16;

        for (size_t n = 0; n < outputLength; ++n)
        {
            const double srcIndex = static_cast<double>(n) - delaySamples;
            const int centerK = static_cast<int>(std::floor(srcIndex));

            double sum = 0.0;
            for (int k = centerK - filterRadius; k <= centerK + filterRadius; ++k)
            {
                if (k >= 0 && k < static_cast<int>(source.size()))
                {
                    const double diff = srcIndex - static_cast<double>(k);
                    // Blackman-windowed sinc
                    double sincVal = 1.0;
                    if (std::abs(diff) > 1e-9)
                    {
                        const double piDiff = 3.14159265358979323846 * diff;
                        sincVal = std::sin(piDiff) / piDiff;
                    }

                    const double w = 0.42 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * (diff + filterRadius) / (2.0 * filterRadius))
                                   + 0.08 * std::cos(4.0 * 3.14159265358979323846 * (diff + filterRadius) / (2.0 * filterRadius));
                    sum += static_cast<double>(source[static_cast<size_t>(k)]) * sincVal * w;
                }
            }
            out[n] = static_cast<float>(sum);
        }
        return out;
    }

    // Helper to generate a bandlimited test stimulus (log sine chirp)
    std::vector<float> generateChirpStimulus(double sampleRate, double durationSec)
    {
        const size_t numSamples = static_cast<size_t>(sampleRate * durationSec);
        std::vector<float> stimulus(numSamples, 0.0f);
        const double f0 = 100.0;
        const double f1 = 10000.0;
        const double T = durationSec;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const double t = static_cast<double>(i) / sampleRate;
            const double phase = 2.0 * 3.14159265358979323846 * f0 * (std::pow(f1 / f0, t / T) - 1.0) / std::log(f1 / f0);
            stimulus[i] = static_cast<float>(0.5 * std::sin(phase));
        }
        return stimulus;
    }
}

TEST_CASE("FineLatency: 3-Point Parabolic Sub-sample Refinement with Bounded Uncertainty", "[fine_latency][subsample]")
{
    SECTION("Symmetric peak yields exact zero delta and high confidence")
    {
        const double rPrev = 0.80;
        const double rPeak = 0.95;
        const double rNext = 0.80;

        const auto res = FineLatencyAnalyzer::refineParabolicThreePoint(rPrev, rPeak, rNext, 60.0);
        REQUIRE(res.valid);
        CHECK(res.delta == Catch::Approx(0.0).margin(1e-6));
        CHECK(res.interpolatedPeak == Catch::Approx(0.95).margin(1e-6));
        CHECK(res.estimatedErrorBound < 0.05);
    }

    SECTION("Asymmetric peak calculates expected fractional shift within [-0.5, 0.5]")
    {
        // Peak shifted toward Next
        const double rPrev = 0.70;
        const double rPeak = 0.92;
        const double rNext = 0.85;

        const auto res = FineLatencyAnalyzer::refineParabolicThreePoint(rPrev, rPeak, rNext, 60.0);
        REQUIRE(res.valid);
        CHECK(res.delta > 0.0);
        CHECK(res.delta <= 0.5);
        CHECK(res.interpolatedPeak > rPeak);
    }

    SECTION("Flat peak or plateau is rejected safely without divide-by-zero")
    {
        const double rPrev = 0.90;
        const double rPeak = 0.90;
        const double rNext = 0.90;

        const auto res = FineLatencyAnalyzer::refineParabolicThreePoint(rPrev, rPeak, rNext, 60.0);
        CHECK_FALSE(res.valid);
        CHECK(res.estimatedErrorBound == 0.5);
    }

    SECTION("Low SNR widens the estimated error bound")
    {
        const double rPrev = 0.75;
        const double rPeak = 0.90;
        const double rNext = 0.82;

        const auto resHighSnr = FineLatencyAnalyzer::refineParabolicThreePoint(rPrev, rPeak, rNext, 70.0);
        const auto resLowSnr = FineLatencyAnalyzer::refineParabolicThreePoint(rPrev, rPeak, rNext, 15.0);

        REQUIRE(resHighSnr.valid);
        REQUIRE(resLowSnr.valid);
        CHECK(resLowSnr.estimatedErrorBound > resHighSnr.estimatedErrorBound);
    }
}

TEST_CASE("FineLatency: Sub-sample Latency Retrieval with Synthetic Known Offsets", "[fine_latency][accuracy]")
{
    const double sampleRate = 48000.0;
    const auto stimulus = generateChirpStimulus(sampleRate, 0.25); // 250 ms chirp

    SECTION("Integer delay 128.0 is recovered with near-zero fractional residual")
    {
        const double trueDelay = 128.0;
        const auto capture = generateSincDelayedSignal(stimulus, trueDelay, stimulus.size() + 256);

        FineLatencyConfig cfg;
        cfg.clockTopology = ClockTopology::SharedClock;
        cfg.refinementMethod = SubsampleRefinementMethod::ParabolicThreePoint;

        const auto record = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(stimulus, capture, sampleRate, cfg, "calib_int_128");

        CHECK(record.status == "resolved");
        CHECK(record.integerLatencySamples == 128);
        CHECK(std::abs(record.fractionalLatencySamples) < record.estimatedErrorBoundSamples);
        CHECK(record.totalLatencySamples == Catch::Approx(128.0).margin(0.05));
    }

    SECTION("Fractional delay 256.35 is recovered within bounded uncertainty")
    {
        const double trueDelay = 256.35;
        const auto capture = generateSincDelayedSignal(stimulus, trueDelay, stimulus.size() + 512);

        FineLatencyConfig cfg;
        cfg.refinementMethod = SubsampleRefinementMethod::ParabolicThreePoint;

        const auto record = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(stimulus, capture, sampleRate, cfg, "calib_frac_256_35");

        CHECK(record.status == "resolved");
        CHECK(record.integerLatencySamples == 256);
        CHECK(record.totalLatencySamples == Catch::Approx(256.35).margin(0.15));
        CHECK(record.estimatedErrorBoundSamples > 0.0);
    }
}

TEST_CASE("FineLatency: Clock Drift Modeling and Topology Discrimination", "[fine_latency][drift]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;
    const auto stimulus = generateChirpStimulus(sampleRate, durationSec);

    SECTION("Independent clocks with +25.0 ppm drift: linear fit and slope recovered")
    {
        const double targetPpm = 25.0;
        const double baseLatencySamples = 200.0;
        const size_t totalSamples = stimulus.size();
        std::vector<float> driftingCapture(totalSamples + 500, 0.0f);

        auto evaluateChirp = [](double t, double duration) -> float {
            if (t < 0.0 || t > duration) return 0.0f;
            const double f0 = 100.0;
            const double f1 = 10000.0;
            const double phase = 2.0 * 3.14159265358979323846 * f0 * (std::pow(f1 / f0, t / duration) - 1.0) / std::log(f1 / f0);
            return static_cast<float>(0.5 * std::sin(phase));
        };

        const double driftRatio = targetPpm * 1e-6;
        for (size_t n = 0; n < driftingCapture.size(); ++n)
        {
            // Physical time at receiver: t_rx = n / Fs.
            // Under clock drift (receiver clock slightly slower/faster), the corresponding emitter time is:
            const double t_tx = (static_cast<double>(n) - baseLatencySamples) / (sampleRate * (1.0 + driftRatio));
            driftingCapture[n] = evaluateChirp(t_tx, durationSec);
        }

        FineLatencyConfig cfg;
        cfg.clockTopology = ClockTopology::IndependentClocks;
        cfg.analysisWindowSizeSamples = 4096;
        cfg.analysisHopSizeSamples = 2048;

        const auto record = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(stimulus, driftingCapture, sampleRate, cfg);

        CHECK(record.clockTopology == ClockTopology::IndependentClocks);
        CHECK(record.driftInterpretation == DriftInterpretation::RelativeClockDrift);
        CHECK(record.fitStatus == LinearFitStatus::LinearFit);
        CHECK(record.driftRSquared >= 0.90);
        CHECK(record.fittedDriftRatePpm == Catch::Approx(targetPpm).margin(2.0));
        CHECK(record.offsetUnit == "samples");
        CHECK(record.timeUnit == "seconds");
        CHECK(record.slopeUnit == "samples_per_second");
        CHECK(record.discontinuities.empty());
    }

    SECTION("Shared clock loopback with ~0.0 ppm drift: classified as residual jitter")
    {
        const double baseLatencySamples = 150.0;
        const auto capture = generateSincDelayedSignal(stimulus, baseLatencySamples, stimulus.size() + 300);

        FineLatencyConfig cfg;
        cfg.clockTopology = ClockTopology::SharedClock;
        cfg.analysisWindowSizeSamples = 4096;
        cfg.analysisHopSizeSamples = 2048;

        const auto record = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(stimulus, capture, sampleRate, cfg);

        CHECK(record.clockTopology == ClockTopology::SharedClock);
        CHECK(record.driftInterpretation == DriftInterpretation::ResidualJitter);
        CHECK(std::abs(record.fittedDriftRatePpm) < 1.0);
        CHECK(record.shortTermJitterSamples < 0.1);
    }

    SECTION("Dropout discontinuity is detected and flagged as degraded or invalid")
    {
        std::vector<ClockDriftObservation> obs;
        for (int i = 0; i < 8; ++i)
        {
            ClockDriftObservation o;
            o.elapsedTimeSeconds = 0.1 * i;
            // Inject a 10-sample dropout jump at index 4
            o.localOffsetSamples = 100.0 + (i >= 4 ? 10.0 : 0.0);
            o.correlationConfidence = 0.95;
            obs.push_back(o);
        }

        FineLatencyCalibrationRecord rec;
        FineLatencyAnalyzer::fitLinearDrift(obs, sampleRate, ClockTopology::IndependentClocks, 4.0, rec);

        REQUIRE_FALSE(rec.discontinuities.empty());
        CHECK(rec.discontinuities[0].type == "dropout");
        CHECK(rec.discontinuities[0].jumpSamples == Catch::Approx(10.0).margin(0.5));
        CHECK((rec.status == "degraded" || rec.status == "calibration_invalid"));
    }
}

TEST_CASE("FineLatency: Multichannel Inter-channel Skew and Phase Alignment", "[fine_latency][multichannel]")
{
    const double sampleRate = 48000.0;
    const auto stimulus = generateChirpStimulus(sampleRate, 0.2);

    // Channel 0: Reference loopback (unaltered)
    const auto& chan0 = stimulus;

    // Channel 1: Delayed by +8.35 samples
    const auto chan1 = generateSincDelayedSignal(stimulus, 8.35, stimulus.size());

    // Channel 2: Polarity inverted, delayed by +4.0 samples
    auto chan2 = generateSincDelayedSignal(stimulus, 4.0, stimulus.size());
    for (float& s : chan2) s = -s;

    std::vector<std::span<const float>> multichannel = {
        std::span<const float>(chan0.data(), chan0.size()),
        std::span<const float>(chan1.data(), chan1.size()),
        std::span<const float>(chan2.data(), chan2.size())
    };

    FineLatencyConfig cfg;
    cfg.phaseReferenceFrequencyHz = 1000.0;

    const auto skews = FineLatencyAnalyzer::analyzeMultichannelSkew(multichannel, sampleRate, cfg);

    REQUIRE(skews.size() == 2);

    // Check Channel 1 skew
    CHECK(skews[0].channelIndex == 1);
    CHECK_FALSE(skews[0].polarityInverted);
    CHECK(skews[0].skewSamples == Catch::Approx(8.35).margin(0.15));
    CHECK(skews[0].phaseAngleRad.has_value());
    CHECK(skews[0].status == "resolved");

    // Check Channel 2 polarity inverted skew
    CHECK(skews[1].channelIndex == 2);
    CHECK(skews[1].polarityInverted);
    CHECK(skews[1].skewSamples == Catch::Approx(4.0).margin(0.15));
    CHECK(skews[1].status == "polarity_inverted");
}

TEST_CASE("FineLatency: Non-Destructive Compensation Views and Immutability", "[fine_latency][immutability]")
{
    const double sampleRate = 48000.0;
    const auto rawAudio = generateChirpStimulus(sampleRate, 0.1);
    const std::string initialSha = FineLatencyAnalyzer::computeAudioSha256(rawAudio);

    FineLatencyCalibrationRecord rec;
    rec.nominalSampleRateHz = sampleRate;
    rec.integerLatencySamples = 64;
    rec.fractionalLatencySamples = 0.25;
    rec.fixedLatencySeconds = 64.25 / sampleRate;

    SECTION("TimeAxisOnly view preserves pointer identity and zero allocation mutation")
    {
        const auto view = FineLatencyAnalyzer::createCompensationView(
            rawAudio,
            CompensationTransformationType::TimeAxisOnly,
            rec);

        CHECK(view.transformationType() == CompensationTransformationType::TimeAxisOnly);
        CHECK(view.rawSamples().data() == rawAudio.data());
        CHECK(view.compensatedSamples().data() == rawAudio.data());
        CHECK(view.timeOffsetSeconds() == Catch::Approx(64.25 / sampleRate).margin(1e-8));

        // Verify raw audio was never mutated
        const std::string postSha = FineLatencyAnalyzer::computeAudioSha256(rawAudio);
        CHECK(initialSha == postSha);
    }

    SECTION("IntegerShift view creates independent compensated buffer while keeping raw intact")
    {
        const auto view = FineLatencyAnalyzer::createCompensationView(
            rawAudio,
            CompensationTransformationType::IntegerShift,
            rec);

        CHECK(view.transformationType() == CompensationTransformationType::IntegerShift);
        CHECK(view.rawSamples().data() == rawAudio.data());
        CHECK(view.compensatedSamples().data() != rawAudio.data());
        CHECK(view.compensatedSamples()[0] == rawAudio[64]);

        const std::string postSha = FineLatencyAnalyzer::computeAudioSha256(rawAudio);
        CHECK(initialSha == postSha);
    }
}

TEST_CASE("FineLatency: Pathological Cases and Deterministic RFC 8785 JSON", "[fine_latency][rfc8785]")
{
    const double sampleRate = 48000.0;

    SECTION("Silence / Low SNR yields insufficient_signal")
    {
        std::vector<float> silenceStim(4800, 0.0f);
        std::vector<float> silenceCap(4800, 0.0f);

        const auto rec = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(silenceStim, silenceCap, sampleRate);
        CHECK(rec.status == "insufficient_signal");
        CHECK(rec.fitStatus == LinearFitStatus::Invalid);
        CHECK(rec.driftInterpretation == DriftInterpretation::NotIdentifiable);
    }

    SECTION("Dual identical impulses trigger ambiguity detection")
    {
        std::vector<float> stimulus(4800, 0.0f);
        stimulus[100] = 1.0f;

        std::vector<float> capture(4800, 0.0f);
        capture[200] = 1.0f; // Peak 1
        capture[300] = 0.95f; // Peak 2 with peakRatio = 0.95 >= 0.85

        FineLatencyConfig cfg;
        cfg.minCorrelationConfidence = 0.20;

        const auto rec = FineLatencyAnalyzer::analyzeFineLatencyAndDrift(stimulus, capture, sampleRate, cfg);
        CHECK(rec.status == "ambiguous");
        CHECK(rec.peakRatio >= 0.85);
        CHECK(rec.ambiguityMargin <= 0.15);
    }

    SECTION("Deterministic canonical JSON serialization under RFC 8785")
    {
        FineLatencyCalibrationRecord rec;
        rec.calibrationId = "test_calib_rfc8785";
        rec.nominalSampleRateHz = 48000.0;
        rec.integerLatencySamples = 512;
        rec.fractionalLatencySamples = 0.334;
        rec.fittedDriftRatePpm = 12.5;
        rec.driftRSquared = 0.992;
        rec.status = "resolved";
        rec.clockTopology = ClockTopology::IndependentClocks;
        rec.driftInterpretation = DriftInterpretation::RelativeClockDrift;

        ChannelSkewObservation sk;
        sk.channelIndex = 1;
        sk.skewSamples = 3.25;
        sk.phaseAngleRad = 0.425;
        rec.channelSkews.push_back(sk);

        const auto j = rec.toCanonicalJson();
        const std::string jsonStr1 = j.dump();
        const std::string jsonStr2 = rec.toCanonicalJson().dump();

        // Exact byte-by-byte string determinism
        CHECK(jsonStr1 == jsonStr2);
        CHECK(j.contains("clockTopology"));
        CHECK(j["clockTopology"] == "independent_clocks");
        CHECK(j.contains("fittedDriftRatePpm"));
        CHECK(j["offsetUnit"] == "samples");
        CHECK(j["timeUnit"] == "seconds");
        CHECK(j["slopeUnit"] == "samples_per_second");
    }
}
