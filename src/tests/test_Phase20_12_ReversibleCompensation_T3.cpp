/**
 * @file test_Phase20_12_ReversibleCompensation_T3.cpp
 * @brief Unit tests for Phase 20.12 Increment 3 (T20.12-3):
 *        3-Tier Reversible Analog Chain Compensation, Physical Saturation
 *        Diagnosis, and Hardware Validation Protocol.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "measurement/AnalogChainCompensationContracts.h"
#include "measurement/AnalogChainReversibleCompensator.h"
#include "measurement/MeasurementDspUtils.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::measurement;

namespace
{
    std::vector<float> generateSineWave(double sampleRate, double freqHz, double amplitude, size_t lengthSamples)
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

TEST_CASE("ChainCompensation: Tier 1 Loopback Calibration Acceptance and Rejection", "[chain_compensation][tier1]")
{
    const double sampleRate = 48000.0;
    const size_t lengthSamples = 4800;

    auto stimulus = generateSineWave(sampleRate, 1000.0, 0.5, lengthSamples);
    auto cleanResponse = generateSineWave(sampleRate, 1000.0, 0.49, lengthSamples);

    HardwareConnectionMetadata connection;
    connection.interfaceModel = "RME Babyface Pro FS";
    connection.firmwareVersion = "v1.24";
    connection.interfaceInputImpedance.valueOhms = 10000.0;
    connection.interfaceInputImpedance.source = "datasheet";

    SECTION("Clean Loopback passes as valid calibration")
    {
        auto rec = AnalogChainReversibleCompensator::evaluateChainCalibration(
            stimulus, cleanResponse, sampleRate, 512, connection, "calib_pass_01");

        REQUIRE(rec.status == "valid");
        REQUIRE(rec.isValid() == true);
        REQUIRE(rec.isDegraded() == false);
        REQUIRE_FALSE(rec.stimulusSha256.empty());
        REQUIRE_FALSE(rec.rawResponseSha256.empty());
        REQUIRE(rec.connectionInfo.interfaceModel == "RME Babyface Pro FS");
        REQUIRE(rec.flatnessRippleDb <= 1.0);
    }

    SECTION("Loopback with ADC rail clip is strictly rejected as chain_invalid")
    {
        auto clippedResponse = cleanResponse;
        // Inject rail clipping on 3 samples
        clippedResponse[100] = 0.9995f;
        clippedResponse[101] = 1.0f;
        clippedResponse[102] = 0.9999f;

        auto rec = AnalogChainReversibleCompensator::evaluateChainCalibration(
            stimulus, clippedResponse, sampleRate, 512, connection, "calib_clipped_01");

        REQUIRE(rec.status == "chain_invalid");
        REQUIRE(rec.isValid() == false);
        REQUIRE(rec.evaluationNotes.find("ADC clipping detected") != std::string::npos);
    }

    SECTION("Empty response is safely handled as chain_invalid")
    {
        std::vector<float> emptyResponse;
        auto rec = AnalogChainReversibleCompensator::evaluateChainCalibration(
            stimulus, emptyResponse, sampleRate, 512, connection, "calib_empty");

        REQUIRE(rec.status == "chain_invalid");
    }
}

TEST_CASE("ChainCompensation: Tier 2 Physical Saturation Diagnosis", "[chain_compensation][tier2][clipping]")
{
    const double sampleRate = 48000.0;
    const size_t lengthSamples = 2048;

    SECTION("Linear signal diagnosed as linear and valid for distortion metrics")
    {
        auto signal = generateSineWave(sampleRate, 1000.0, 0.4, lengthSamples);
        auto diag = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(signal, 0.999f);

        REQUIRE(diag.status == "linear");
        REQUIRE(diag.adcClipEvidence == false);
        REQUIRE(diag.clipSampleCount == 0);
        REQUIRE(diag.isValidForDistortionMetrics() == true);
        REQUIRE(diag.preampOverloadEvidence == "not_assessed");
    }

    SECTION("Positive and negative ADC rail clips detected even on single samples")
    {
        auto signal = generateSineWave(sampleRate, 1000.0, 0.5, lengthSamples);
        signal[150] = 0.9995f;   // Positive rail
        signal[300] = -0.9999f;  // Negative rail

        auto diag = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(signal, 0.999f);

        REQUIRE(diag.adcClipEvidence == true);
        REQUIRE(diag.clipSampleCount == 2);
        REQUIRE(diag.positiveRailCount == 1);
        REQUIRE(diag.negativeRailCount == 1);
        REQUIRE(diag.firstClipSample == 150);
        REQUIRE(diag.lastClipSample == 300);
        REQUIRE(diag.status == "measurement_invalid_due_to_adc_clipping");
        REQUIRE(diag.isValidForDistortionMetrics() == false);
    }

    SECTION("DUT compression detected when crest factor drops without ADC clip")
    {
        // Synthesize a compressed/squashed wave: clip at 0.7 (well below ADC 0.999)
        auto compressedSignal = generateSineWave(sampleRate, 1000.0, 1.2, lengthSamples);
        for (auto& s : compressedSignal)
            s = std::clamp(s, -0.65f, 0.65f);

        auto diag = AnalogChainReversibleCompensator::diagnosePhysicalSaturation(compressedSignal, 0.999f);

        REQUIRE(diag.adcClipEvidence == false);
        REQUIRE(diag.clipSampleCount == 0);
        REQUIRE(diag.dutCompressionEvidence == true);
        REQUIRE(diag.status == "dut_saturating");
        REQUIRE(diag.observedCompressionDb > 0.0);
        REQUIRE(diag.isValidForDistortionMetrics() == true);
    }
}

TEST_CASE("ChainCompensation: Tier 3 Regularized Reversible Spectral Compensation", "[chain_compensation][tier3]")
{
    const double sampleRate = 48000.0;
    const size_t numPoints = 64;

    // Create a dummy DUT response with a resonance peak
    FrequencyResponseResult dutResp;
    dutResp.sampleRateHz = sampleRate;
    dutResp.sweepDurationSec = 1.0;
    dutResp.startFrequencyHz = 20.0;
    dutResp.endFrequencyHz = 20000.0;

    for (size_t i = 0; i < numPoints; ++i)
    {
        const double f = 20.0 * std::pow(1000.0, static_cast<double>(i) / static_cast<double>(numPoints - 1));
        dutResp.frequenciesHz.push_back(static_cast<float>(f));
        // DUT has a filter curve
        double magDb = -3.0 * (f / 10000.0);
        dutResp.magnitudeDb.push_back(static_cast<float>(magDb));
        dutResp.phaseRad.push_back(0.0f);
    }

    // Create a chain calibration with a deep notch at high frequency (simulating cable/converter loss)
    ChainCalibrationRecord chainCalib;
    chainCalib.calibrationId = "calib_rme_01";
    chainCalib.sampleRateHz = sampleRate;
    chainCalib.rawResponseSha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    chainCalib.status = "valid";
    for (float f : dutResp.frequenciesHz)
        chainCalib.frequencyGridHz.push_back(static_cast<double>(f));
    chainCalib.magnitudeResponseDb = std::vector<double>(numPoints, 0.0);
    chainCalib.phaseResponseRad = std::vector<double>(numPoints, 0.0);

    // Deep notch of -35 dB at bin 50
    chainCalib.magnitudeResponseDb[50] = -35.0;

    const std::string rawCaptureSha = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

    SECTION("Deep notch inverse gain is strictly capped by maxInverseGainDb (+12 dB)")
    {
        CompensationValidity validity;
        validity.configuredMaxInverseGainDb = 12.0;
        validity.gainLimitPolicy = "capped";
        validity.regularizationLambda = 1e-3;

        auto compRec = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dutResp, rawCaptureSha, chainCalib, validity, false);

        REQUIRE(compRec.status == "compensation_applied");
        REQUIRE(compRec.isReversible == true);
        REQUIRE(compRec.validity.appliedMaxInverseGainDb <= 12.001);
        REQUIRE(compRec.magnitudeCorrectionMaxDb <= 12.001);
        REQUIRE_FALSE(compRec.compensationModelSha256.empty());
    }

    SECTION("Zero lambda with ill-conditioned chain (-35 dB dip) is rejected")
    {
        CompensationValidity validity;
        validity.regularizationLambda = 0.0; // unregularized

        auto compRec = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dutResp, rawCaptureSha, chainCalib, validity, false);

        REQUIRE(compRec.status == "compensation_not_applied");
        REQUIRE(compRec.isReversible == false);
        REQUIRE(compRec.validity.status == "invalid");
        REQUIRE(compRec.validity.reason.find("zero_lambda_rejected") != std::string::npos);
    }

    SECTION("Degraded chain calibration blocks compensation by default unless allowDegraded is true")
    {
        auto degradedCalib = chainCalib;
        degradedCalib.status = "degraded";

        CompensationValidity validity;
        validity.regularizationLambda = 1e-4;

        // Blocked by default
        auto compRecBlocked = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dutResp, rawCaptureSha, degradedCalib, validity, false);

        REQUIRE(compRecBlocked.status == "compensation_not_applied");
        REQUIRE(compRecBlocked.isReversible == false);

        // Allowed explicitly
        auto compRecAllowed = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dutResp, rawCaptureSha, degradedCalib, validity, true);

        REQUIRE(compRecAllowed.status == "compensation_degraded");
        REQUIRE(compRecAllowed.isReversible == true);
        REQUIRE(compRecAllowed.validity.status == "degraded");
    }

    SECTION("Invalid chain calibration blocks compensation unconditionally")
    {
        auto invalidCalib = chainCalib;
        invalidCalib.status = "chain_invalid";
        invalidCalib.evaluationNotes = "ADC clipping in loopback";

        auto compRec = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dutResp, rawCaptureSha, invalidCalib, {}, true);

        REQUIRE(compRec.status == "chain_invalid");
        REQUIRE(compRec.isReversible == false);
    }
}

TEST_CASE("ChainCompensation: Hardware Provenance, Impedance Knowledge and RFC 8785", "[chain_compensation][provenance]")
{
    SECTION("Impedance descriptor never represents unknown as 0.0 Ohms")
    {
        ImpedanceDescriptor imp;
        REQUIRE_FALSE(imp.valueOhms.has_value());
        REQUIRE(imp.source == "unknown");

        auto j = imp.toJson();
        REQUIRE(j["valueOhms"].is_null());
        REQUIRE(j["source"] == "unknown");

        // When known
        imp.valueOhms = 600.0;
        imp.source = "measured";
        auto jKnown = imp.toJson();
        REQUIRE(jKnown["valueOhms"] == 600.0);
        REQUIRE(jKnown["source"] == "measured");
    }

    SECTION("Missing provenance hashes mark result as non-reversible")
    {
        FrequencyResponseResult dummyResp;
        dummyResp.sampleRateHz = 48000.0;
        ChainCalibrationRecord calib;
        calib.status = "valid";
        calib.rawResponseSha256 = ""; // Empty hash!

        auto rec = AnalogChainReversibleCompensator::compensateFrequencyResponse(
            dummyResp, "", calib, {}, false);

        REQUIRE(rec.status == "incomplete_provenance");
        REQUIRE(rec.isReversible == false);
    }

    SECTION("Idempotency guard detects already compensated records")
    {
        CompensatedDutMeasurementRecord rec;
        rec.status = "compensation_applied";
        REQUIRE(AnalogChainReversibleCompensator::isAlreadyCompensated(rec) == true);

        rec.status = "compensation_degraded";
        REQUIRE(AnalogChainReversibleCompensator::isAlreadyCompensated(rec) == true);

        rec.status = "compensation_not_applied";
        REQUIRE(AnalogChainReversibleCompensator::isAlreadyCompensated(rec) == false);
    }

    SECTION("Canonical JSON serialization is strictly deterministic")
    {
        CompensatedDutMeasurementRecord rec;
        rec.measurementId = "meas_001";
        rec.rawCaptureSha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        rec.chainReferenceSha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        rec.compensationModelSha256 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
        rec.status = "compensation_applied";
        rec.isReversible = true;

        std::string jsonPass1 = rec.toCanonicalJson();
        std::string jsonPass2 = rec.toCanonicalJson();

        REQUIRE(jsonPass1 == jsonPass2);
        REQUIRE_FALSE(jsonPass1.empty());
    }
}
