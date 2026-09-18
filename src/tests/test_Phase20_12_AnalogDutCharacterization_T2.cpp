/**
 * @file test_Phase20_12_AnalogDutCharacterization_T2.cpp
 * @brief Unit tests for Phase 20.12 Increment 2 (T20.12-2):
 *        Analog DUT Characterization (Farina Adapter, Parametric THD IEEE/IEC,
 *        Two-Tone IMD SMPTE/CCIF, and Level Sweep / Clipping Thresholds).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "measurement/AnalogDutCharacterizationContracts.h"
#include "measurement/AnalogDutCharacterizer.h"
#include "math/FarinaDeconvolver.h"
#include "math/IntermodulationAnalyzer.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::measurement;

namespace
{
    // Generates a pure sine tone with specified frequency, amplitude, and phase
    std::vector<float> generateSine(double sampleRate, double freqHz, double amplitude, size_t lengthSamples)
    {
        std::vector<float> signal(lengthSamples, 0.0f);
        const double phaseInc = 2.0 * std::numbers::pi * freqHz / sampleRate;
        double phase = 0.0;
        for (size_t i = 0; i < lengthSamples; ++i)
        {
            signal[i] = static_cast<float>(amplitude * std::sin(phase));
            phase += phaseInc;
        }
        return signal;
    }
}

TEST_CASE("AnalogDut: Farina Frequency Response Adapter", "[analog_dut][farina]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.25;
    const float startFreqHz = 20.0f;
    const float endFreqHz = 20000.0f;

    // Generate stimulus and inverse filter using existing math::FarinaDeconvolver
    const auto stimulus = abdaudiolab::math::FarinaDeconvolver::generateLogFarinaSweep(
        sampleRate, durationSec, startFreqHz, endFreqHz);
    const auto invFilter = abdaudiolab::math::FarinaDeconvolver::generateInverseFilter(
        sampleRate, durationSec, startFreqHz, endFreqHz);

    REQUIRE(!stimulus.empty());
    REQUIRE(!invFilter.empty());

    // Clean unity-gain response (loopback)
    const auto freqResult = AnalogDutCharacterizer::measureFrequencyResponse(
        stimulus, invFilter, sampleRate, durationSec, startFreqHz, endFreqHz);

    CHECK(freqResult.status == "resolved");
    CHECK(freqResult.sampleRateHz == sampleRate);
    CHECK(freqResult.method == "farina_swept_sine_deconvolution");
    CHECK_FALSE(freqResult.frequenciesHz.empty());
    CHECK_FALSE(freqResult.magnitudeDb.empty());
    CHECK(freqResult.deconvolutionThdPercent >= 0.0);
}

TEST_CASE("AnalogDut: Parametric THD (IEEE vs IEC Denominator Resolution)", "[analog_dut][thd]")
{
    const double sampleRate = 48000.0;
    const size_t numSamples = 8192;
    const double f0 = 1000.0;

    SECTION("Pure sine has negligible THD below -80 dB / 0.01%")
    {
        const auto pureSine = generateSine(sampleRate, f0, 1.0, numSamples);

        const auto resIeee = AnalogDutCharacterizer::measureHarmonicDistortion(
            pureSine, sampleRate, f0, ThdConvention::FundamentalReferenced);

        CHECK(resIeee.status == "resolved");
        CHECK(resIeee.convention == ThdConvention::FundamentalReferenced);
        CHECK(resIeee.measuredFundamentalHz == Catch::Approx(1000.0).margin(5.0));
        CHECK(resIeee.thdPercent < 0.02); // < 0.02%
        CHECK(resIeee.thdDb < -70.0);
    }

    SECTION("Calibrated harmonics (H2 = -40 dBc [1%], H3 = -46.02 dBc [0.5%]) match theoretical THD")
    {
        // Theoretical harmonics:
        // V1 = 1.0, V2 = 0.01 (1%), V3 = 0.005 (0.5%)
        // TotalHarmonicRms = sqrt(0.01^2 + 0.005^2) = sqrt(0.000125) = 0.0111803
        // THD_IEEE = (0.0111803 / 1.0) * 100 = 1.118%
        // THD_IEC  = (0.0111803 / sqrt(1^2 + 0.000125)) * 100 = 1.1179%
        std::vector<float> signal(numSamples, 0.0f);
        for (size_t n = 0; n < numSamples; ++n)
        {
            const double t = static_cast<double>(n) / sampleRate;
            signal[n] = static_cast<float>(
                1.000 * std::sin(2.0 * std::numbers::pi * f0 * t) +
                0.010 * std::sin(2.0 * std::numbers::pi * 2.0 * f0 * t) +
                0.005 * std::sin(2.0 * std::numbers::pi * 3.0 * f0 * t)
            );
        }

        const auto resIeee = AnalogDutCharacterizer::measureHarmonicDistortion(
            signal, sampleRate, f0, ThdConvention::FundamentalReferenced);

        CHECK(resIeee.status == "resolved");
        CHECK(resIeee.harmonics.size() >= 2);
        CHECK(resIeee.harmonics[0].harmonicOrder == 2);
        CHECK(resIeee.harmonics[0].levelDbc == Catch::Approx(-40.0).margin(0.5));
        CHECK(resIeee.harmonics[1].harmonicOrder == 3);
        CHECK(resIeee.harmonics[1].levelDbc == Catch::Approx(-46.0).margin(0.5));

        CHECK(resIeee.thdFundamentalReferencedPercent == Catch::Approx(1.118).margin(0.08));
        CHECK(resIeee.thdPercent == Catch::Approx(1.118).margin(0.08));

        // Total RMS referenced check
        const auto resIec = AnalogDutCharacterizer::measureHarmonicDistortion(
            signal, sampleRate, f0, ThdConvention::TotalRmsReferenced);

        CHECK(resIec.convention == ThdConvention::TotalRmsReferenced);
        CHECK(resIec.thdPercent == Catch::Approx(1.118).margin(0.08));
    }

    SECTION("High distortion scenario clearly separates IEEE vs IEC denominator values")
    {
        // V1 = 1.0, V2 = 0.50 (50% relative to fundamental)
        // THD_IEEE = (0.50 / 1.0) * 100 = 50.0%
        // THD_IEC  = (0.50 / sqrt(1^2 + 0.5^2)) * 100 = (0.50 / 1.11803) * 100 = 44.72%
        std::vector<float> distSignal(numSamples, 0.0f);
        for (size_t n = 0; n < numSamples; ++n)
        {
            const double t = static_cast<double>(n) / sampleRate;
            distSignal[n] = static_cast<float>(
                1.0 * std::sin(2.0 * std::numbers::pi * f0 * t) +
                0.5 * std::sin(2.0 * std::numbers::pi * 2.0 * f0 * t)
            );
        }

        const auto res = AnalogDutCharacterizer::measureHarmonicDistortion(
            distSignal, sampleRate, f0, ThdConvention::FundamentalReferenced);

        CHECK(res.thdFundamentalReferencedPercent == Catch::Approx(50.0).margin(1.0));
        CHECK(res.thdTotalRmsReferencedPercent == Catch::Approx(44.72).margin(1.0));
        CHECK(res.thdFundamentalReferencedPercent > res.thdTotalRmsReferencedPercent);
    }
}

TEST_CASE("AnalogDut: Two-Tone IMD (SMPTE vs CCIF/ITU-R Strategy)", "[analog_dut][imd]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.25;

    SECTION("SMPTE dual-tone excitation (60 Hz + 7 kHz, 4:1)")
    {
        // Generate SMPTE using existing math::IntermodulationAnalyzer
        auto smpteStim = abdaudiolab::math::IntermodulationAnalyzer::generateSmpteStimulus(
            sampleRate, durationSec, 60.0f, 7000.0f, 4.0f);

        // Inject 2nd order non-linearity: y = x + 0.02 * x^2
        for (float& s : smpteStim)
        {
            s = s + 0.02f * s * s;
        }

        const auto imdRes = AnalogDutCharacterizer::measureIntermodulation(
            smpteStim, sampleRate, ImdConvention::Smpte, 60.0, 7000.0, 8192);

        CHECK(imdRes.status == "resolved");
        CHECK(imdRes.convention == ImdConvention::Smpte);
        CHECK(imdRes.referenceTone == "f2_high_carrier");
        CHECK_FALSE(imdRes.products.empty());
        CHECK(imdRes.totalImdPercent > 0.0);
    }

    SECTION("CCIF twin-tone excitation (19 kHz + 20 kHz, 1:1)")
    {
        // Generate CCIF twin-tone using existing math::IntermodulationAnalyzer
        auto ccifStim = abdaudiolab::math::IntermodulationAnalyzer::generateCcifStimulus(
            sampleRate, durationSec, 19000.0f, 20000.0f);

        // Inject non-linearity creating 1 kHz difference frequency (f2 - f1)
        for (float& s : ccifStim)
        {
            s = s + 0.015f * s * s;
        }

        const auto imdRes = AnalogDutCharacterizer::measureIntermodulation(
            ccifStim, sampleRate, ImdConvention::Ccif, 19000.0, 20000.0, 8192);

        CHECK(imdRes.status == "resolved");
        CHECK(imdRes.convention == ImdConvention::Ccif);
        CHECK(imdRes.referenceTone == "equal_split_power");
        CHECK(imdRes.d2Percent > 0.0);
    }
}

TEST_CASE("AnalogDut: Level Sweep and Clipping Thresholds", "[analog_dut][clipping]")
{
    // Synthesize input-output level sweep representing an analog pre-amp
    // Linear gain = +6.0 dB at small signals.
    // 1% THD at input = -12.0 dBFS
    // 3% THD at input = -6.0 dBFS
    // P1dB (1 dB gain drop: gain = +5.0 dB) at input = -4.0 dBFS
    // Hard clipping observed at 0.0 dBFS
    std::vector<LevelSweepPoint> sweep;

    sweep.push_back({ -30.0, -24.0, 6.0, 0.05, false, false, false });
    sweep.push_back({ -20.0, -14.0, 6.0, 0.10, false, false, false });
    sweep.push_back({ -15.0, -9.05, 5.95, 0.50, false, false, false });
    sweep.push_back({ -10.0, -4.20, 5.80, 1.50, false, true,  false }); // 1% THD crossed between -15 and -10
    sweep.push_back({ -5.0,  0.60,  5.60, 3.50, false, true,  false }); // 3% THD crossed between -10 and -5
    sweep.push_back({ -3.0,  1.80,  4.80, 5.20, false, true,  true });  // P1dB crossed (gain drops below 5.0 dB)
    sweep.push_back({ 0.0,   3.50,  3.50, 12.0, true,  true,  true });  // Hard clipping rail hit

    const auto clipResult = AnalogDutCharacterizer::analyzeLevelSweep(sweep);

    CHECK(clipResult.status == "resolved");
    CHECK(clipResult.smallSignalGainDb == Catch::Approx(6.0).margin(0.1));
    CHECK(clipResult.hardClippingObserved == true);

    // Verify 1% THD crossing is between -15.0 and -10.0 dBFS
    REQUIRE(clipResult.thd1PercentInputDbfs.has_value());
    CHECK(*clipResult.thd1PercentInputDbfs > -15.0);
    CHECK(*clipResult.thd1PercentInputDbfs < -10.0);

    // Verify 3% THD crossing is between -10.0 and -5.0 dBFS
    REQUIRE(clipResult.thd3PercentInputDbfs.has_value());
    CHECK(*clipResult.thd3PercentInputDbfs > -10.0);
    CHECK(*clipResult.thd3PercentInputDbfs < -5.0);

    // Verify P1dB crossing is between -5.0 and -3.0 dBFS
    REQUIRE(clipResult.p1dbInputDbfs.has_value());
    CHECK(*clipResult.p1dbInputDbfs > -5.0);
    CHECK(*clipResult.p1dbInputDbfs < -3.0);

    SECTION("Hard clipping detection helper")
    {
        std::vector<float> normalSignal = { 0.2f, 0.5f, 0.8f, 0.5f, 0.0f };
        CHECK_FALSE(AnalogDutCharacterizer::detectHardClipping(normalSignal));

        std::vector<float> clippedSignal = { 0.2f, 0.8f, 1.0f, 1.0f, 1.0f, 0.8f };
        CHECK(AnalogDutCharacterizer::detectHardClipping(clippedSignal));
    }
}

TEST_CASE("AnalogDut: Deterministic Canonical RFC 8785 JSON Serialization", "[analog_dut][rfc8785]")
{
    AnalogDutCharacterizationRecord record;
    record.characterizationId = "dut_char_001";
    record.dutName = "AnalogSynthesizer_FilterChannel";
    record.sampleRateHz = 48000.0;
    record.status = "resolved";

    record.harmonicDistortion.fundamentalFrequencyHz = 1000.0;
    record.harmonicDistortion.thdPercent = 1.25;
    record.harmonicDistortion.convention = ThdConvention::FundamentalReferenced;

    record.intermodulation.convention = ImdConvention::Smpte;
    record.intermodulation.totalImdPercent = 0.85;

    record.clippingThresholds.smallSignalGainDb = 6.0;
    record.clippingThresholds.thd1PercentInputDbfs = -12.5;
    record.clippingThresholds.p1dbInputDbfs = -4.2;

    const auto j1 = record.toCanonicalJson();
    const auto j2 = record.toCanonicalJson();

    CHECK(j1.dump() == j2.dump());
    CHECK(j1.contains("characterizationId"));
    CHECK(j1.contains("clippingThresholds"));
    CHECK(j1.contains("harmonicDistortion"));
    CHECK(j1.contains("intermodulation"));
    CHECK(j1["harmonicDistortion"]["convention"] == "fundamental_referenced_ieee");
    CHECK(j1["intermodulation"]["convention"] == "smpte");
}
