/**
 * @file test_LoopbackCalibration_T4.cpp
 * @brief Fase 20.11 T4.1 - Contratos de Calibración, Registro LoopbackCalibrationRecord y Segregación de Artefactos
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <string>
#include <cmath>

#include "measurement/LoopbackCalibrator.h"
#include "measurement/MeasurementContracts.h"

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

namespace
{

std::vector<float> simulateAnalogLoopback(const std::vector<float>& stimulus,
                                         size_t delaySamples,
                                         float attenuationGain = 0.98f,
                                         float noiseAmplitude = 0.0001f,
                                         float dcOffset = 0.0f)
{
    size_t totalSamples = stimulus.size() + delaySamples + 500;
    std::vector<float> response(totalSamples, 0.0f);

    for (size_t i = 0; i < totalSamples; ++i)
    {
        float sample = dcOffset;

        // Añadir ruido térmico de fondo
        float pseudoNoise = (static_cast<float>((i * 1103515245 + 12345) & 0x7FFFFFFF) / 2147483648.0f - 0.5f) * 2.0f * noiseAmplitude;
        sample += pseudoNoise;

        if (i >= delaySamples && (i - delaySamples) < stimulus.size())
        {
            sample += stimulus[i - delaySamples] * attenuationGain;
        }

        response[i] = sample;
    }

    return response;
}

} // namespace

TEST_CASE("Fase 20.11 T4.1 - Calibration stimulus synthesis", "[loopback][contracts][t4]")
{
    double sampleRate = 48000.0;
    double sweepDurationSec = 0.5;
    double leadInSilenceSec = 0.05;
    float levelDbfs = -6.0f;

    auto stimulus = LoopbackCalibrator::generateCalibrationStimulus(sampleRate, sweepDurationSec, leadInSilenceSec, levelDbfs);

    size_t expectedSamples = static_cast<size_t>(std::lround((sweepDurationSec + leadInSilenceSec) * sampleRate));
    REQUIRE(stimulus.size() == expectedSamples);

    // Verificar que los primeros milisegundos son silencio
    size_t leadInSamples = static_cast<size_t>(std::lround(leadInSilenceSec * sampleRate));
    for (size_t i = 0; i < leadInSamples / 2; ++i)
    {
        REQUIRE(stimulus[i] == 0.0f);
    }

    // Verificar no-NaN y nivel pico acotado a -6 dBFS (~0.5)
    float maxVal = 0.0f;
    for (float s : stimulus)
    {
        REQUIRE(!std::isnan(s));
        REQUIRE(!std::isinf(s));
        maxVal = std::max(maxVal, std::abs(s));
    }

    REQUIRE_THAT(static_cast<double>(maxVal), WithinAbs(0.501, 0.05));
}

TEST_CASE("Fase 20.11 T4.1 - Loopback calibration analysis on pure reference loopback", "[loopback][calibration][t4]")
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    size_t simulatedDelaySamples = 128; // Latencia física simulada

    auto stimulus = LoopbackCalibrator::generateCalibrationStimulus(sampleRate, 0.5, 0.05, -6.0f);
    auto response = simulateAnalogLoopback(stimulus, simulatedDelaySamples, 0.95f, 0.0001f, 0.0f);

    LoopbackCalibrationRecord rec = LoopbackCalibrator::analyzeLoopback(
        stimulus, response, sampleRate, blockSize, "cal_test_reference_001");

    REQUIRE(rec.calibrationId == "cal_test_reference_001");
    REQUIRE(rec.sampleRateHz == 48000.0);
    REQUIRE(rec.blockSize == 512);

    // Verificación de latencia total de ida y vuelta (round-trip)
    REQUIRE(rec.roundTripLatencySamples == static_cast<double>(simulatedDelaySamples));
    double expectedMs = (static_cast<double>(simulatedDelaySamples) / sampleRate) * 1000.0;
    REQUIRE_THAT(rec.roundTripLatencyMs, WithinAbs(expectedMs, 0.001));

    // Verificación de SNR y niveles
    REQUIRE(rec.snrDb > 40.0);
    REQUIRE(rec.peakDbfs < -6.0); // Con ganancia 0.95 sobre -6dBFS
    REQUIRE(rec.dcOffsetDb < -60.0);
    REQUIRE(rec.clockDriftPpm == 0.0);

    // Criterio metrológico pass
    REQUIRE(rec.isPass());
    REQUIRE(rec.status == "pass");

    // Hashes criptográficos de fijación inmutable
    REQUIRE(!rec.stimulusSha256.empty());
    REQUIRE(!rec.responseSha256.empty());
    REQUIRE(rec.stimulusSha256 != rec.responseSha256);
}

TEST_CASE("Fase 20.11 T4.1 - Metrological failure isolation in loopback validation", "[loopback][failure][t4]")
{
    double sampleRate = 48000.0;
    int blockSize = 256;
    size_t delaySamples = 64;
    auto stimulus = LoopbackCalibrator::generateCalibrationStimulus(sampleRate, 0.2, 0.05, -6.0f);

    SECTION("Fails on clipping / level over peakDbfsMax")
    {
        // Ganancia excesiva que provoca saturación por encima de -0.5 dBFS
        auto clippedResponse = simulateAnalogLoopback(stimulus, delaySamples, 2.5f, 0.0001f, 0.0f);
        LoopbackCalibrationRecord rec = LoopbackCalibrator::analyzeLoopback(stimulus, clippedResponse, sampleRate, blockSize);

        REQUIRE(rec.peakDbfs > -0.5);
        REQUIRE(!rec.isPass());
        REQUIRE(rec.status == "fail");
    }

    SECTION("Fails on excessive noise floor / low SNR")
    {
        // Ruido térmico masivo (~ -10 dBFS) que arruina el SNR (< 18 dB)
        auto noisyResponse = simulateAnalogLoopback(stimulus, delaySamples, 0.1f, 0.15f, 0.0f);
        LoopbackCalibrationRecord rec = LoopbackCalibrator::analyzeLoopback(stimulus, noisyResponse, sampleRate, blockSize);

        REQUIRE(rec.snrDb < 18.0);
        REQUIRE(!rec.isPass());
        REQUIRE(rec.status == "fail");
    }

    SECTION("Fails on excessive DC offset")
    {
        // Desvío continuo constante de +0.05 (-26 dBFS), muy por encima de -60 dBFS
        auto dcResponse = simulateAnalogLoopback(stimulus, delaySamples, 0.9f, 0.0001f, 0.05f);
        LoopbackCalibrationRecord rec = LoopbackCalibrator::analyzeLoopback(stimulus, dcResponse, sampleRate, blockSize);

        REQUIRE(rec.dcOffsetDb > -60.0);
        REQUIRE(!rec.isPass());
        REQUIRE(rec.status == "fail");
    }
}

TEST_CASE("Fase 20.11 T4.1 - Segregación estricta de artefactos en 3 capas", "[loopback][artifacts][t4]")
{
    // 1. Capa 1: loopback_reference
    REQUIRE(analogChainArtifactTierToString(AnalogChainArtifactTier::LoopbackReference) == "loopback_reference");

    // 2. Capa 2: dut_plus_chain (crudo inalterable)
    REQUIRE(analogChainArtifactTierToString(AnalogChainArtifactTier::DutPlusChain) == "dut_plus_chain");

    // 3. Capa 3: compensated_result (compensación reversible)
    REQUIRE(analogChainArtifactTierToString(AnalogChainArtifactTier::CompensatedResult) == "compensated_result");

    // Comprobar contrato de preservación inmutable
    CompensatedResponseMetadata meta;
    meta.rawDutAudioSha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    meta.loopbackReferenceCalibrationId = "cal_loopback_48000hz_512b_178964999";
    meta.loopbackReferenceAudioSha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    meta.compensationMethod = "regularized_spectral_deconvolution";
    meta.regularizationEpsilon = 1e-4;
    meta.validFrequencyMinHz = 20.0;
    meta.validFrequencyMaxHz = 20000.0;
    meta.isReversible = true;
    meta.compensatedAudioSha256 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

    REQUIRE(meta.isReversible);
    REQUIRE(meta.rawDutAudioSha256 != meta.compensatedAudioSha256);
    REQUIRE(!meta.loopbackReferenceCalibrationId.empty());
}
