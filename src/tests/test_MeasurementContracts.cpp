/**
 * @file test_MeasurementContracts.cpp
 * @brief Catch2 unit tests for response-measurement-1.0 contracts and serialization.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/MeasurementContracts.h"
#include "measurement/MeasurementSerialization.h"
#include <cmath>
#include <limits>

using namespace abdaudiolab::measurement;
using Catch::Matchers::WithinAbs;

TEST_CASE("MeasurementContracts - StimulusSpec round-trip", "[measurement][contracts]")
{
    StimulusSpec stim;
    stim.type = StimulusType::midiNote;
    stim.startFreqHz = 40.0f;
    stim.endFreqHz = 16000.0f;
    stim.durationSec = 1.5;
    stim.levelDbfs = -3.0f;
    stim.phaseRad = 0.5f;
    stim.seed = 0x12345678u;
    stim.midiChannel = 2;
    stim.midiNoteNumber = 69; // A4
    stim.midiVelocity = 0.95f;
    stim.noteOnSample = 100;
    stim.noteOffSample = 48100;
    stim.sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    std::string jsonStr = MeasurementSerialization::serializeStimulus(stim);
    REQUIRE_FALSE(jsonStr.empty());

    StimulusSpec parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeStimulus(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.type == StimulusType::midiNote);
    REQUIRE_THAT(static_cast<double>(parsed.startFreqHz), WithinAbs(40.0, 1e-4));
    REQUIRE_THAT(static_cast<double>(parsed.endFreqHz), WithinAbs(16000.0, 1e-4));
    REQUIRE_THAT(parsed.durationSec, WithinAbs(1.5, 1e-4));
    REQUIRE_THAT(static_cast<double>(parsed.levelDbfs), WithinAbs(-3.0, 1e-4));
    REQUIRE_THAT(static_cast<double>(parsed.phaseRad), WithinAbs(0.5, 1e-4));
    REQUIRE(parsed.seed == 0x12345678u);
    REQUIRE(parsed.midiChannel == 2);
    REQUIRE(parsed.midiNoteNumber == 69);
    REQUIRE_THAT(static_cast<double>(parsed.midiVelocity), WithinAbs(0.95, 1e-4));
    REQUIRE(parsed.noteOnSample == 100);
    REQUIRE(parsed.noteOffSample == 48100);
    REQUIRE(parsed.sha256 == stim.sha256);
}

TEST_CASE("MeasurementContracts - MeasurementSpec round-trip and schema validation", "[measurement][contracts]")
{
    MeasurementSpec spec;
    spec.schemaVersion = "response-measurement-1.0";
    spec.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    spec.measurementId = "meas-spec-001";
    spec.measurementType = "envelope";
    spec.dutType = DeviceUnderTest::instrument;
    spec.parameterId = "param_32";
    spec.parameterName = "OP1 OUTPUT LEVEL";

    spec.execution.sampleRateHz = 96000.0;
    spec.execution.blockSize = 256;
    spec.execution.numChannels = 2;
    spec.execution.numSamples = 192000;
    spec.execution.latencySamples = 32;

    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.midiNoteNumber = 60;
    spec.stimulus.midiVelocity = 0.8f;
    spec.stimulus.noteOnSample = 0;
    spec.stimulus.noteOffSample = 96000;

    spec.analysis.analysisType = "adsr_envelope";
    spec.analysis.options.push_back({ "thresholdDbfs", "-60.0" });

    spec.repetition.numPasses = 3;
    spec.repetition.stabilizationWaitMs = 100.0;

    std::string jsonStr = MeasurementSerialization::serializeSpec(spec);
    REQUIRE_FALSE(jsonStr.empty());

    MeasurementSpec parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeSpec(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.schemaVersion == "response-measurement-1.0");
    REQUIRE(parsed.schemaUri == "urn:abdaudiolab:response-measurement:1.0");
    REQUIRE(parsed.measurementId == "meas-spec-001");
    REQUIRE(parsed.measurementType == "envelope");
    REQUIRE(parsed.dutType == DeviceUnderTest::instrument);
    REQUIRE(parsed.parameterId == "param_32");
    REQUIRE(parsed.parameterName == "OP1 OUTPUT LEVEL");
    REQUIRE(parsed.execution.sampleRateHz == 96000.0);
    REQUIRE(parsed.execution.blockSize == 256);
    REQUIRE(parsed.execution.latencySamples == 32);
    REQUIRE(parsed.repetition.numPasses == 3);
    REQUIRE(parsed.analysis.analysisType == "adsr_envelope");
    REQUIRE(parsed.analysis.options.size() == 1);
    REQUIRE(parsed.analysis.options[0].first == "thresholdDbfs");
    REQUIRE(parsed.analysis.options[0].second == "-60.0");

    SECTION("Rejection of incompatible schemaVersion")
    {
        MeasurementSpec badSpec = spec;
        badSpec.schemaVersion = "invalid-version-2.0";
        std::string badJson = MeasurementSerialization::serializeSpec(badSpec);

        MeasurementSpec badParsed;
        std::string badErr;
        bool badOk = MeasurementSerialization::deserializeSpec(badJson, badParsed, badErr);
        REQUIRE_FALSE(badOk);
        REQUIRE(badErr.find("Unsupported schemaVersion") != std::string::npos);
    }

    SECTION("Rejection of incompatible schemaUri")
    {
        MeasurementSpec badSpec = spec;
        badSpec.schemaUri = "urn:other:unknown";
        std::string badJson = MeasurementSerialization::serializeSpec(badSpec);

        MeasurementSpec badParsed;
        std::string badErr;
        bool badOk = MeasurementSerialization::deserializeSpec(badJson, badParsed, badErr);
        REQUIRE_FALSE(badOk);
        REQUIRE(badErr.find("Unsupported schemaUri") != std::string::npos);
    }
}

TEST_CASE("MeasurementContracts - MeasurementResult round-trip with typed metrics and curve", "[measurement][contracts]")
{
    MeasurementResult res;
    res.schemaVersion = "response-measurement-1.0";
    res.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    res.measurementId = "dexed-envelope-op1-eg-rate-1";
    res.measurementType = "envelope";
    res.status = MeasurementStatus::completed;
    res.reason = "Measurement successfully observed under declared gate";

    res.dut.name = "Dexed";
    res.dut.format = "VST3";
    res.dut.version = "1.0.1";
    res.dut.type = "instrument";

    res.execution.sampleRateHz = 48000.0;
    res.execution.blockSize = 512;
    res.execution.numChannels = 2;
    res.execution.numSamples = 72000;
    res.execution.latencySamples = 0;

    res.stimulus.type = StimulusType::midiNote;
    res.stimulus.midiNoteNumber = 60;
    res.stimulus.midiVelocity = 0.8f;
    res.stimulus.noteOnSample = 0;
    res.stimulus.noteOffSample = 57600;
    res.stimulus.sha256 = "aabbccddeeff";

    res.analyzer.name = "SynthEnvelopeAnalyzer";
    res.analyzer.version = "1.0.0";

    res.observability.status = "observed";
    res.observability.reason = std::nullopt;

    res.metrics.push_back({ "attackTime", 123.4, "ms", "observed" });
    res.metrics.push_back({ "decayTime", 456.7, "ms", "observed" });
    res.metrics.push_back({ "sustainLevel", -18.2, "dBFS", "observed" });
    res.metrics.push_back({ "releaseTime", 820.0, "ms", "observed" });

    res.curve.xName = "time";
    res.curve.xUnit = "ms";
    res.curve.yName = "amplitude";
    res.curve.yUnit = "dBFS";
    res.curve.x = { 0.0, 10.0, 20.0, 30.0 };
    res.curve.y = { -96.0, -12.0, -6.0, -18.0 };

    res.artifacts.audioPath = "audio/envelope/reference.wav";
    res.artifacts.audioSha256 = "112233445566";
    res.integrityVerified = true;

    std::string jsonStr = MeasurementSerialization::serializeResult(res);
    REQUIRE_FALSE(jsonStr.empty());
    REQUIRE(jsonStr.find("\"reason\": null") != std::string::npos);

    MeasurementResult parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.measurementId == res.measurementId);
    REQUIRE(parsed.status == MeasurementStatus::completed);
    REQUIRE(parsed.dut.name == "Dexed");
    REQUIRE(parsed.analyzer.name == "SynthEnvelopeAnalyzer");
    REQUIRE(parsed.analyzer.version == "1.0.0");
    REQUIRE(parsed.observability.status == "observed");
    REQUIRE_FALSE(parsed.observability.reason.has_value());

    REQUIRE(parsed.metrics.size() == 4);
    REQUIRE(parsed.metrics[0].name == "attackTime");
    REQUIRE_THAT(parsed.metrics[0].value, WithinAbs(123.4, 1e-4));
    REQUIRE(parsed.metrics[0].unit == "ms");
    REQUIRE(parsed.metrics[0].status == "observed");

    REQUIRE(parsed.curve.x.size() == 4);
    REQUIRE(parsed.curve.y.size() == 4);
    REQUIRE(parsed.curve.xName == "time");
    REQUIRE(parsed.curve.xUnit == "ms");
    REQUIRE(parsed.curve.yName == "amplitude");
    REQUIRE(parsed.curve.yUnit == "dBFS");
    REQUIRE_THAT(parsed.curve.x[1], WithinAbs(10.0, 1e-4));
    REQUIRE_THAT(parsed.curve.y[1], WithinAbs(-12.0, 1e-4));

    REQUIRE(parsed.artifacts.audioPath == "audio/envelope/reference.wav");
    REQUIRE(parsed.artifacts.audioSha256 == "112233445566");
    REQUIRE(parsed.integrityVerified == true);
}

TEST_CASE("MeasurementContracts - Curve integrity and validation rules", "[measurement][contracts][curve]")
{
    MeasurementCurve curve;
    curve.xName = "time";
    curve.xUnit = "ms";
    curve.yName = "amplitude";
    curve.yUnit = "dBFS";

    std::string err;

    SECTION("Equal length and valid numbers pass for completed")
    {
        curve.x = { 0.0, 1.0, 2.0 };
        curve.y = { -60.0, -20.0, -10.0 };
        REQUIRE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
    }

    SECTION("Rejection of dimension mismatch")
    {
        curve.x = { 0.0, 1.0, 2.0 };
        curve.y = { -60.0, -20.0 }; // size 2 vs 3
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("dimension mismatch") != std::string::npos);
    }

    SECTION("Rejection of NaN in X")
    {
        curve.x = { 0.0, std::numeric_limits<double>::quiet_NaN(), 2.0 };
        curve.y = { -60.0, -20.0, -10.0 };
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("NaN or Infinity") != std::string::npos);
    }

    SECTION("Rejection of Infinity in Y")
    {
        curve.x = { 0.0, 1.0, 2.0 };
        curve.y = { -60.0, std::numeric_limits<double>::infinity(), -10.0 };
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("NaN or Infinity") != std::string::npos);
    }

    SECTION("Rejection of missing units when curve has data")
    {
        curve.x = { 0.0, 1.0 };
        curve.y = { -60.0, -20.0 };
        curve.xUnit = ""; // missing
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("missing required X or Y units") != std::string::npos);
    }

    SECTION("Rejection of missing variable names when curve has data")
    {
        curve.x = { 0.0, 1.0 };
        curve.y = { -60.0, -20.0 };
        curve.yName = ""; // missing
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("missing required X or Y variable names") != std::string::npos);
    }

    SECTION("Rejection of empty curve when status is completed")
    {
        curve.x.clear();
        curve.y.clear();
        REQUIRE_FALSE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::completed, err));
        REQUIRE(err.find("Completed measurement cannot have an empty curve") != std::string::npos);
    }

    SECTION("Acceptance of empty curve when status is unreliable, skipped or failed")
    {
        curve.x.clear();
        curve.y.clear();
        REQUIRE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::unreliable, err));
        REQUIRE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::skipped, err));
        REQUIRE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::failed, err));
        REQUIRE(MeasurementSerialization::validateCurve(curve, MeasurementStatus::invalid, err));
    }
}

TEST_CASE("MeasurementContracts - Deterministic serialization for hashing", "[measurement][contracts][hash]")
{
    MeasurementResult r1;
    r1.schemaVersion = "response-measurement-1.0";
    r1.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    r1.measurementId = "meas-canonical-001";
    r1.measurementType = "envelope";
    r1.status = MeasurementStatus::unreliable;
    r1.reason = "onset_not_detected";
    r1.observability.status = "unreliable";
    r1.observability.reason = "onset_not_detected";

    std::string s1 = MeasurementSerialization::serializeResult(r1);
    std::string s2 = MeasurementSerialization::serializeResult(r1);

    // Exact string identity for hashing
    REQUIRE(s1 == s2);
    REQUIRE_FALSE(s1.empty());
}
