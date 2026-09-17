/**
 * @file test_ModulationMeasurementAdapter.cpp
 * @brief Catch2 unit tests for ModulationMeasurementAdapter (Campaña 20.10.3-M).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/adapters/ModulationMeasurementAdapter.h"
#include <cmath>
#include <vector>

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

namespace
{

/**
 * @brief Helper generating AM tone (tremolo): carrier 440 Hz, LFO rate 5 Hz.
 */
std::vector<float> generateAmTone(double sampleRate,
                                  double durationSec,
                                  double carrierFreqHz,
                                  double lfoRateHz,
                                  double modIndex = 0.5) // modIndex 0.5 -> 6 dB swing
{
    size_t totalSamples = static_cast<size_t>(sampleRate * durationSec);
    std::vector<float> buffer(totalSamples, 0.0f);

    for (size_t i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double lfo = std::sin(2.0 * 3.14159265358979323846 * lfoRateHz * t);
        double amp = 1.0 + modIndex * lfo;
        double carrier = std::sin(2.0 * 3.14159265358979323846 * carrierFreqHz * t);
        buffer[i] = static_cast<float>(0.5 * amp * carrier);
    }

    return buffer;
}

} // namespace

TEST_CASE("ModulationMeasurementAdapter - Guards against empty and silent buffers", "[measurement][modulation][adapter]")
{
    MeasurementSpec spec;
    spec.measurementId = "meas-mod-guard-001";
    spec.measurementType = "modulation";
    spec.modulationDestination = "amplitude";

    SECTION("Empty buffer returns failed with not_observable")
    {
        std::vector<float> empty;
        MeasurementResult res = ModulationMeasurementAdapter::analyzeBuffer(spec, empty, 48000.0);
        REQUIRE(res.status == MeasurementStatus::failed);
        REQUIRE(res.reason == "empty_audio_buffer");
        REQUIRE(res.observability.status == "not_observable");
    }

    SECTION("Silent buffer returns unreliable without inventing LFO values")
    {
        std::vector<float> silence(48000, 1e-6f);
        MeasurementResult res = ModulationMeasurementAdapter::analyzeBuffer(spec, silence, 48000.0);
        REQUIRE(res.status == MeasurementStatus::unreliable);
        REQUIRE(res.reason == "signal_below_noise_floor_or_too_short");
        REQUIRE(res.observability.status == "unreliable");
        REQUIRE_FALSE(res.modulationResult.has_value());
    }
}

TEST_CASE("ModulationMeasurementAdapter - Amplitude demodulation and LFO rate estimation", "[measurement][modulation][adapter]")
{
    double sampleRate = 48000.0;
    double durationSec = 2.0;
    double carrierHz = 440.0;
    double lfoRateHz = 5.0;

    auto audio = generateAmTone(sampleRate, durationSec, carrierHz, lfoRateHz, 0.5);

    MeasurementSpec spec;
    spec.measurementId = "meas-mod-am-001";
    spec.measurementType = "modulation";
    spec.modulationDestination = "amplitude";
    spec.stimulus.startFreqHz = static_cast<float>(carrierHz);

    MeasurementResult res = ModulationMeasurementAdapter::analyzeBuffer(spec, audio, sampleRate);

    REQUIRE(res.status == MeasurementStatus::completed);
    REQUIRE(res.measurementType == "modulation");
    REQUIRE(res.modulationDestination == "amplitude");
    REQUIRE(res.modulationResult.has_value());

    const auto& mod = *res.modulationResult;

    // Rate estimated around 5.0 Hz
    REQUIRE(mod.rateHz.name == "lfo_rate");
    REQUIRE_THAT(mod.rateHz.value, WithinAbs(5.0, 0.4));
    REQUIRE(mod.rateHz.unit == "Hz");
    REQUIRE(mod.rateHz.status == "observed");
    REQUIRE(mod.rateMethod == "amplitude_demodulation");

    // Depth in dB (modIndex 0.5 -> 20*log10((1+0.5)/(1-0.5)) = 9.5 dB peak-to-peak -> ~4.7 dB peak)
    REQUIRE(mod.depth.unit == "dB");
    REQUIRE(mod.depth.name == "tremolo_depth");
    REQUIRE(mod.depth.value >= 3.0);
    REQUIRE(mod.depth.value <= 6.0);
    REQUIRE(mod.depth.status == "observed");

    // Waveform shape: sine inferred
    REQUIRE(mod.waveform.waveform == "sine");
    REQUIRE(mod.waveform.status == "inferred");
    REQUIRE(mod.waveform.confidence >= 0.75);

    // Sidebands: carrier 440 Hz, +/- 5 Hz sidebands
    REQUIRE_FALSE(mod.sidebands.empty());
    bool foundUpper = false;
    bool foundLower = false;
    for (const auto& sb : mod.sidebands)
    {
        if (sb.order == 1)
        {
            foundUpper = true;
            REQUIRE_THAT(sb.carrierFrequencyHz, WithinAbs(440.0, 0.5));
            REQUIRE_THAT(sb.sidebandFrequencyHz, WithinAbs(445.0, 15.0)); // bin resolution
            REQUIRE(sb.levelRelativeToCarrierDb < -6.0); // Sideband is lower than carrier
        }
        else if (sb.order == -1)
        {
            foundLower = true;
            REQUIRE_THAT(sb.sidebandFrequencyHz, WithinAbs(435.0, 15.0));
            REQUIRE(sb.levelRelativeToCarrierDb < -6.0);
        }
    }
    REQUIRE(foundUpper);
    REQUIRE(foundLower);

    // Demodulated curves
    REQUIRE_FALSE(mod.timeCurve.x.empty());
    REQUIRE(mod.timeCurve.xUnit == "ms");
    REQUIRE(mod.timeCurve.yUnit == "dB");

    REQUIRE_FALSE(mod.spectrumCurve.x.empty());
    REQUIRE(mod.spectrumCurve.xUnit == "Hz");
    REQUIRE(mod.spectrumCurve.yUnit == "dBFS");

    // Spectral metadata
    REQUIRE(mod.spectralMetadata.fftSize == 4096);
    REQUIRE(mod.spectralMetadata.window == "hann");
}

TEST_CASE("ModulationMeasurementAdapter - Waveform shape classification", "[measurement][modulation][adapter]")
{
    SECTION("Pure sine trajectory classified as sine")
    {
        std::vector<double> sineVals(128);
        for (size_t i = 0; i < sineVals.size(); ++i)
        {
            double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / 32.0;
            sineVals[i] = 5.0 * std::sin(theta); // 5 dB modulation
        }

        WaveformEstimate est = ModulationMeasurementAdapter::classifyWaveform(sineVals);
        REQUIRE(est.waveform == "sine");
        REQUIRE(est.status == "inferred");
        REQUIRE(est.confidence >= 0.85);
    }

    SECTION("Square wave trajectory classified as square")
    {
        std::vector<double> sqVals(128);
        for (size_t i = 0; i < sqVals.size(); ++i)
        {
            double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / 32.0;
            sqVals[i] = (std::sin(theta) >= 0.0) ? 6.0 : -6.0;
        }

        WaveformEstimate est = ModulationMeasurementAdapter::classifyWaveform(sqVals);
        REQUIRE(est.waveform == "square");
        REQUIRE(est.status == "inferred");
        REQUIRE(est.confidence >= 0.85);
    }

    SECTION("Flat unmodulated signal classified as not_observable")
    {
        std::vector<double> flatVals(128, 0.05); // negligible swing

        WaveformEstimate est = ModulationMeasurementAdapter::classifyWaveform(flatVals);
        REQUIRE(est.waveform == "none");
        REQUIRE(est.status == "not_observable");
        REQUIRE(est.confidence == 0.0);
    }
}
