/**
 * @file test_CalibrationNoiseBaseline.cpp
 * @brief Unit tests for physical noise baseline measurement and calibrated SNR (Phase 2.5).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "calibration/CalibrationSnapshot.h"
#include "calibration/CalibrationNoiseBaselineRunner.h"
#include "math/LoopbackCalibrator.h"
#include "audio/LabAudioReceiver.h"
#include "core/SessionSerializer.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab;
using namespace abdaudiolab::calibration;

namespace
{

std::vector<float> generateSyntheticNoise(int numSamples, float targetRmsDbfs, float seedOffset = 0.0f)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    float linearRms = std::pow(10.0f, targetRmsDbfs / 20.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        // Simple pseudorandom white noise in [-1.0, 1.0]
        float r = std::sin(static_cast<float>(i) * 12.9898f + seedOffset) * 43758.5453f;
        r = (r - std::floor(r)) * 2.0f - 1.0f;
        buf[static_cast<size_t>(i)] = r * linearRms * std::numbers::sqrt2_v<float>;
    }
    return buf;
}

} // namespace

TEST_CASE("CalibrationNoiseBaseline: Diagnostico de baseline y estados", "[calibration][noise][baseline]")
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kSamples400ms = 17640;

    SECTION("El baseline se captura con salida del sweep silenciada y calcula RMS, pico y 32 bandas")
    {
        auto noise = generateSyntheticNoise(kSamples400ms, -103.0f);
        auto base = CalibrationNoiseBaselineRunner::analyze(noise.data(), kSamples400ms, kSampleRate, true);

        CHECK(base.status == NoiseBaselineStatus::Valid);
        CHECK(base.outputMuted == true);
        CHECK(base.durationSamples == kSamples400ms);
        CHECK(base.durationMs == Catch::Approx(400.0).margin(0.1));
        CHECK(base.rmsDbfs == Catch::Approx(-103.0f).margin(2.5f));
        CHECK(base.peakDbfs > base.rmsDbfs);
        CHECK(base.hasSpectralBands == true);
        CHECK(base.spectralBandDbfs.size() == 32);
        CHECK(base.inputWasClipped == false);
    }

    SECTION("Silencio digital exacto se clasifica como BelowMeasurementFloor sin inventar constantes")
    {
        std::vector<float> digitalSilence(kSamples400ms, 0.0f);
        auto base = CalibrationNoiseBaselineRunner::analyze(digitalSilence.data(), kSamples400ms, kSampleRate, true);

        CHECK(base.status == NoiseBaselineStatus::BelowMeasurementFloor);
        CHECK(base.outputMuted == true);
        CHECK(base.rmsDbfs <= -115.0f);
    }

    SECTION("Ruido excesivo sobre umbral de contaminacion devuelve Contaminated")
    {
        // Excessive noise (-30 dBFS) indicating external active signal or wrong cable
        auto loudNoise = generateSyntheticNoise(kSamples400ms, -30.0f);
        auto base = CalibrationNoiseBaselineRunner::analyze(loudNoise.data(), kSamples400ms, kSampleRate, true);

        CHECK(base.status == NoiseBaselineStatus::Contaminated);
    }

    SECTION("Clipping durante baseline devuelve Clipped")
    {
        auto clippedNoise = generateSyntheticNoise(kSamples400ms, -80.0f);
        clippedNoise[100] = 1.0f; // Clip
        auto base = CalibrationNoiseBaselineRunner::analyze(clippedNoise.data(), kSamples400ms, kSampleRate, true);

        CHECK(base.status == NoiseBaselineStatus::Clipped);
        CHECK(base.inputWasClipped == true);
    }

    SECTION("Dispositivo detenido o parametros nulos devuelve DeviceStopped")
    {
        auto base = CalibrationNoiseBaselineRunner::analyze(nullptr, 0, 0.0, true);
        CHECK(base.status == NoiseBaselineStatus::DeviceStopped);
    }
}

TEST_CASE("CalibrationNoiseBaseline: SNR real calibrado vs fallback y ventana contractual", "[calibration][noise][snr]")
{
    constexpr double kSampleRate = 44100.0;

    SECTION("El baseline no altera requiredSamples ni la ventana de 1300 ms del loopback")
    {
        auto req = audio::CaptureRequirements::makeLoopbackRequirements(kSampleRate);
        // N_required = 1000ms sweep + 200ms margin + 100ms tail = 1300ms = 57330 samples @ 44.1k
        CHECK(req.requiredSamples == 57330);
        double durationMs = (static_cast<double>(req.requiredSamples) / kSampleRate) * 1000.0;
        CHECK(durationMs == Catch::Approx(1300.0).margin(0.1));
    }

    SECTION("SNR usa L_signal,RMS,dBFS - L_noise,RMS,dBFS sin usar -96 dBFS fijo")
    {
        // Generate sweep-like signal with RMS = -12.3 dBFS
        int numSamples = 57330;
        std::vector<float> sweepSignal(static_cast<size_t>(numSamples), 0.0f);
        float linearRms = std::pow(10.0f, -12.3f / 20.0f);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * i / static_cast<float>(kSampleRate));
            sweepSignal[static_cast<size_t>(i)] = s * linearRms * std::numbers::sqrt2_v<float>;
        }

        // Real measured noise floor: -103.0 dBFS
        float measuredNoiseFloor = -103.0f;
        auto calData = math::LoopbackCalibrator::analyzeLoopback(
            sweepSignal, kSampleRate, 1.0, 20.0f, 20000.0f, -3.0f, measuredNoiseFloor);

        // Expected SNR = -12.3 - (-103.0) = 90.7 dB
        CHECK(calData.snrDb == Catch::Approx(90.7f).margin(0.5f));
        CHECK(calData.snrMethod == math::SnrMeasurementMethod::PhysicalNoiseBaseline);
    }

    SECTION("Fallback a -96 dBFS cuando no se mide baseline")
    {
        int numSamples = 57330;
        std::vector<float> sweepSignal(static_cast<size_t>(numSamples), 0.0f);
        float linearRms = std::pow(10.0f, -12.3f / 20.0f);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * i / static_cast<float>(kSampleRate));
            sweepSignal[static_cast<size_t>(i)] = s * linearRms * std::numbers::sqrt2_v<float>;
        }

        auto calData = math::LoopbackCalibrator::analyzeLoopback(
            sweepSignal, kSampleRate, 1.0, 20.0f, 20000.0f, -3.0f, std::nullopt);

        // Expected fallback SNR = -12.3 - (-96.0) = 83.7 dB
        CHECK(calData.snrDb == Catch::Approx(83.7f).margin(0.5f));
        CHECK(calData.snrMethod == math::SnrMeasurementMethod::LegacyAssumedNoiseFloor);
    }
}

TEST_CASE("CalibrationNoiseBaseline: Persistencia en snapshot e integridad", "[calibration][noise][snapshot]")
{
    CalibrationNoiseBaseline base;
    base.status = NoiseBaselineStatus::Valid;
    base.durationSamples = 17640;
    base.durationMs = 400.0;
    base.rmsDbfs = -103.0f;
    base.peakDbfs = -89.4f;
    base.outputMuted = true;
    base.hasSpectralBands = true;
    base.spectralBandDbfs.fill(-105.0f);

    math::LoopbackCalibrationData resultData;
    resultData.isCalibrated = true;
    resultData.clippingDetected = false;
    resultData.latencySamples = 256;
    resultData.roundTripLatencyMs = 5.8f;
    resultData.peakInDbfs = -3.0f;
    resultData.frequencyFlatnessDb = 0.5f;
    resultData.snrDb = 90.7f;
    resultData.recommendedTrimGain = 0.743f;

    CalibrationCompatibility compat;
    compat.deviceStableId = "AudioBox USB";
    compat.sampleRateHz = 44100.0;

    CalibrationCaptureMetadata cap;
    cap.requiredSamples = 57330;
    cap.capturedSamples = 57330;

    auto snap = CalibrationSnapshot::create(
        "AudioBox USB — Out 1 -> In 1",
        "audiobox-001",
        compat,
        cap,
        resultData,
        0,
        {},
        base);

    CHECK(snap.verifyIntegrity() == true);
    CHECK(snap.noiseBaseline == base);

    auto json = snap.toJson();
    CHECK(json.contains("noiseBaseline"));
    CHECK(json["noiseBaseline"]["status"] == "Valid");
    CHECK(json["noiseBaseline"]["rmsDbfs"] == Catch::Approx(-103.0f));

    auto restored = CalibrationSnapshot::fromJson(json);
    CHECK(restored.verifyIntegrity() == true);
    CHECK(restored.noiseBaseline == base);

    SECTION("El hash del snapshot cambia si cambia cualquier metrica del baseline")
    {
        std::string originalHash = snap.integrity.snapshotHash;

        auto modifiedSnap = snap;
        modifiedSnap.noiseBaseline.rmsDbfs = -100.0f;
        CHECK(modifiedSnap.computeHash() != originalHash);
        CHECK_FALSE(modifiedSnap.verifyIntegrity());

        auto modifiedStatus = snap;
        modifiedStatus.noiseBaseline.status = NoiseBaselineStatus::Contaminated;
        CHECK(modifiedStatus.computeHash() != originalHash);
        CHECK_FALSE(modifiedStatus.verifyIntegrity());
    }

    SECTION("Una sesion legacy sin baseline se abre como NotMeasured sin inventar medidas historicas")
    {
        auto legacyJson = json;
        legacyJson.erase("noiseBaseline");

        auto legacyRestored = CalibrationSnapshot::fromJson(legacyJson);
        CHECK(legacyRestored.noiseBaseline.status == NoiseBaselineStatus::NotMeasured);
        CHECK(legacyRestored.noiseBaseline.rmsDbfs <= -115.0f);
    }
}
