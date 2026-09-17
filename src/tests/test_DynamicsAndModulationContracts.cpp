/**
 * @file test_DynamicsAndModulationContracts.cpp
 * @brief Catch2 unit tests for MIDI dynamics (20.10.3-D) and LFO modulation (20.10.3-M) contracts.
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

// ==============================================================================
// 1. DINÁMICA MIDI (20.10.3-D) - METROLOGÍA, R^2, OBSERVACIÓN Y DISCONTINUIDAD
// ==============================================================================

TEST_CASE("DynamicsContracts - DynamicPoint serialization with v=0 special case", "[measurement][dynamics][contracts]")
{
    SECTION("Normal velocity point (v=64)")
    {
        DynamicPoint pt;
        pt.velocity = 64;
        pt.peakDbfs = -12.4;
        pt.rmsDbfs = -18.2;
        pt.spectralCentroidHz = 1450.0;
        pt.spectralRolloffHz = 4800.0;
        pt.attackTimeMs = 15.5;
        pt.status = "observed";
        pt.reason = "";
        pt.measurementWindowStartMs = 100.0;
        pt.measurementWindowEndMs = 450.0;
        pt.presetStateHash = "hash_dexed_epiano_v64";
        pt.audioArtifactHash = "sha256_audio_v64_wav";

        DynamicResponseResult dyn;
        dyn.points.push_back(pt);

        SpectralAnalysisMetadata specMeta;
        specMeta.fftSize = 2048;
        specMeta.hopSize = 512;
        specMeta.window = "blackmanHarris";
        specMeta.frequencyResolutionHz = 23.4375;
        specMeta.averagingCount = 4;
        dyn.spectralMetadata = specMeta;

        dyn.amplitudeCurve.xName = "velocity";
        dyn.amplitudeCurve.xUnit = "midi_val";
        dyn.amplitudeCurve.yName = "rmsDbfs";
        dyn.amplitudeCurve.yUnit = "dBFS";
        dyn.amplitudeCurve.x = { 64.0 };
        dyn.amplitudeCurve.y = { -18.2 };

        std::string jsonStr = MeasurementSerialization::serializeDynamicResult(dyn);
        REQUIRE_FALSE(jsonStr.empty());

        DynamicResponseResult parsed;
        std::string err;
        bool ok = MeasurementSerialization::deserializeDynamicResult(jsonStr, parsed, err);
        REQUIRE(ok);
        REQUIRE(err.empty());
        REQUIRE(parsed.points.size() == 1);

        const auto& p = parsed.points[0];
        REQUIRE(p.velocity == 64);
        REQUIRE_THAT(p.peakDbfs, WithinAbs(-12.4, 1e-4));
        REQUIRE_THAT(p.rmsDbfs, WithinAbs(-18.2, 1e-4));
        REQUIRE_THAT(p.spectralCentroidHz, WithinAbs(1450.0, 1e-4));
        REQUIRE_THAT(p.spectralRolloffHz, WithinAbs(4800.0, 1e-4));
        REQUIRE_THAT(p.attackTimeMs, WithinAbs(15.5, 1e-4));
        REQUIRE(p.status == "observed");
        REQUIRE(p.reason.empty());
        REQUIRE_THAT(p.measurementWindowStartMs, WithinAbs(100.0, 1e-4));
        REQUIRE_THAT(p.measurementWindowEndMs, WithinAbs(450.0, 1e-4));
        REQUIRE(p.presetStateHash == "hash_dexed_epiano_v64");
        REQUIRE(p.audioArtifactHash == "sha256_audio_v64_wav");

        REQUIRE(parsed.spectralMetadata.has_value());
        REQUIRE(parsed.spectralMetadata->fftSize == 2048);
        REQUIRE(parsed.spectralMetadata->hopSize == 512);
        REQUIRE(parsed.spectralMetadata->window == "blackmanHarris");
        REQUIRE_THAT(parsed.spectralMetadata->frequencyResolutionHz, WithinAbs(23.4375, 1e-4));
        REQUIRE(parsed.spectralMetadata->averagingCount == 4);
    }

    SECTION("Velocity 0 special case (skipped or silent, midi_note_on_velocity_zero)")
    {
        DynamicPoint pt0;
        pt0.velocity = 0;
        pt0.peakDbfs = -96.0;
        pt0.rmsDbfs = -96.0;
        pt0.spectralCentroidHz = 0.0;
        pt0.spectralRolloffHz = 0.0;
        pt0.attackTimeMs = 0.0;
        pt0.status = "skipped";
        pt0.reason = "midi_note_on_velocity_zero";

        DynamicResponseResult dyn;
        dyn.points.push_back(pt0);

        std::string jsonStr = MeasurementSerialization::serializeDynamicResult(dyn);
        REQUIRE_FALSE(jsonStr.empty());

        DynamicResponseResult parsed;
        std::string err;
        bool ok = MeasurementSerialization::deserializeDynamicResult(jsonStr, parsed, err);
        REQUIRE(ok);
        REQUIRE(err.empty());
        REQUIRE(parsed.points.size() == 1);

        const auto& p = parsed.points[0];
        REQUIRE(p.velocity == 0);
        REQUIRE(p.status == "skipped");
        REQUIRE(p.reason == "midi_note_on_velocity_zero");
        REQUIRE_THAT(p.rmsDbfs, WithinAbs(-96.0, 1e-4));
    }

    SECTION("Unreliable point when signal is silent or too short (no invented zeros)")
    {
        DynamicPoint ptShort;
        ptShort.velocity = 1;
        ptShort.peakDbfs = -82.0;
        ptShort.rmsDbfs = -89.0;
        ptShort.spectralCentroidHz = 0.0; // not calculated
        ptShort.spectralRolloffHz = 0.0;
        ptShort.attackTimeMs = 0.0;
        ptShort.status = "unreliable";
        ptShort.reason = "signal_below_noise_floor_or_too_short";

        DynamicResponseResult dyn;
        dyn.points.push_back(ptShort);

        std::string jsonStr = MeasurementSerialization::serializeDynamicResult(dyn);
        DynamicResponseResult parsed;
        std::string err;
        bool ok = MeasurementSerialization::deserializeDynamicResult(jsonStr, parsed, err);
        REQUIRE(ok);
        REQUIRE(parsed.points[0].status == "unreliable");
        REQUIRE(parsed.points[0].reason == "signal_below_noise_floor_or_too_short");
    }
}

TEST_CASE("DynamicsContracts - Ajuste 1: Curve fit models decouple R^2 from assumed linearity", "[measurement][dynamics][contracts]")
{
    DynamicResponseResult dyn;
    
    // Fill with a non-linear velocity response (e.g. logarithmic / exponential volume taper)
    std::vector<int> grid = { 0, 1, 8, 16, 24, 32, 48, 64, 80, 96, 112, 120, 127 };
    for (int v : grid)
    {
        DynamicPoint pt;
        pt.velocity = v;
        if (v == 0)
        {
            pt.status = "skipped";
            pt.reason = "midi_note_on_velocity_zero";
            pt.rmsDbfs = -96.0;
        }
        else
        {
            pt.status = "observed";
            // Exponential/logarithmic taper: 40 * log10(v / 127.0)
            pt.rmsDbfs = -40.0 + 40.0 * (std::log10(static_cast<double>(v)) / std::log10(127.0));
            pt.spectralCentroidHz = 500.0 + 30.0 * v;
        }
        dyn.points.push_back(pt);
    }

    dyn.amplitudeCurve.xName = "velocity";
    dyn.amplitudeCurve.xUnit = "midi_val";
    dyn.amplitudeCurve.yName = "rmsDbfs";
    dyn.amplitudeCurve.yUnit = "dBFS";

    dyn.brightnessCurve.xName = "velocity";
    dyn.brightnessCurve.xUnit = "midi_val";
    dyn.brightnessCurve.yName = "spectralCentroidHz";
    dyn.brightnessCurve.yUnit = "Hz";

    for (const auto& pt : dyn.points)
    {
        dyn.amplitudeCurve.x.push_back(static_cast<double>(pt.velocity));
        dyn.amplitudeCurve.y.push_back(pt.rmsDbfs);
        dyn.brightnessCurve.x.push_back(static_cast<double>(pt.velocity));
        dyn.brightnessCurve.y.push_back(pt.spectralCentroidHz);
    }

    // Explicit curve fitting declarations
    CurveFitMetadata ampFit;
    ampFit.model = "logarithmic";
    ampFit.rSquared = 0.985;
    ampFit.xVariable = "velocity";
    ampFit.yVariable = "rmsDbfs";
    dyn.amplitudeFit = ampFit;

    CurveFitMetadata brightFit;
    brightFit.model = "linear";
    brightFit.rSquared = 0.992;
    brightFit.xVariable = "velocity";
    brightFit.yVariable = "spectralCentroidHz";
    dyn.brightnessFit = brightFit;

    dyn.dynamicRangeDb = 42.5;

    std::string jsonStr = MeasurementSerialization::serializeDynamicResult(dyn);
    REQUIRE_FALSE(jsonStr.empty());

    DynamicResponseResult parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeDynamicResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(parsed.points.size() == 13);
    REQUIRE(parsed.amplitudeFit.has_value());
    REQUIRE(parsed.amplitudeFit->model == "logarithmic");
    REQUIRE_THAT(parsed.amplitudeFit->rSquared, WithinAbs(0.985, 1e-4));
    REQUIRE(parsed.amplitudeFit->xVariable == "velocity");
    REQUIRE(parsed.amplitudeFit->yVariable == "rmsDbfs");

    REQUIRE(parsed.brightnessFit.has_value());
    REQUIRE(parsed.brightnessFit->model == "linear");
    REQUIRE_THAT(parsed.brightnessFit->rSquared, WithinAbs(0.992, 1e-4));
    REQUIRE_THAT(parsed.dynamicRangeDb, WithinAbs(42.5, 1e-4));
}

TEST_CASE("DynamicsContracts - Ajuste 2: Discontinuity observation separates empirical fact from layer interpretation", "[measurement][dynamics][contracts]")
{
    DynamicResponseResult dyn;
    
    // Empirical discontinuity observed between velocity 64 and 80 (e.g. 9 dB jump)
    dyn.discontinuity.detected = true;
    dyn.discontinuity.lowerVelocity = 64;
    dyn.discontinuity.upperVelocity = 80;
    dyn.discontinuity.jumpDb = 9.2;
    dyn.discontinuity.confidence = 0.89;
    dyn.discontinuity.reason = "discontinuity observed in amplitude and centroid curves";

    std::string jsonStr = MeasurementSerialization::serializeDynamicResult(dyn);
    REQUIRE_FALSE(jsonStr.empty());

    DynamicResponseResult parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeDynamicResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(parsed.discontinuity.detected);
    REQUIRE(parsed.discontinuity.lowerVelocity == 64);
    REQUIRE(parsed.discontinuity.upperVelocity == 80);
    REQUIRE_THAT(parsed.discontinuity.jumpDb, WithinAbs(9.2, 1e-4));
    REQUIRE_THAT(parsed.discontinuity.confidence, WithinAbs(0.89, 1e-4));
    REQUIRE(parsed.discontinuity.reason.contains("discontinuity observed"));
    // Strictly must NOT assert "layer switching confirmed" without independent proof
    REQUIRE_FALSE(parsed.discontinuity.reason.contains("layer switching confirmed"));
}

// ==============================================================================
// 2. MODULACIÓN LFO (20.10.3-M) - CARRIER, SIDEBANDS, WAVEFORM ESTIMATE
// ==============================================================================

TEST_CASE("ModulationContracts - ModulationResultData round-trip with carrier sidebands and waveform inference", "[measurement][modulation][contracts]")
{
    ModulationResultData mod;
    mod.targetDestination = "pitch"; // Pitch vibrato
    
    mod.rateHz.name = "lfo_rate";
    mod.rateHz.value = 5.25;
    mod.rateHz.unit = "Hz";
    mod.rateHz.status = "observed";
    mod.rateMethod = "spectral_peak"; // Declared method: temporal_period | spectral_peak | pitch_tracking | amplitude_demodulation

    mod.depth.name = "vibrato_depth";
    mod.depth.value = 35.0;
    mod.depth.unit = "cents";
    mod.depth.status = "observed";

    // Waveform estimation declaring observed vs inferred vs not_observable
    mod.waveform.waveform = "sine";
    mod.waveform.status = "inferred";
    mod.waveform.confidence = 0.91;

    // Spectral analysis metadata (FFT size, window, frequency resolution)
    mod.spectralMetadata.fftSize = 4096;
    mod.spectralMetadata.hopSize = 1024;
    mod.spectralMetadata.window = "hann";
    mod.spectralMetadata.frequencyResolutionHz = 11.71875;
    mod.spectralMetadata.averagingCount = 8;

    // Carrier-linked sidebands in spectral analysis
    ModulationSideband sb1;
    sb1.carrierFrequencyHz = 440.0;
    sb1.sidebandFrequencyHz = 445.25;
    sb1.order = 1;
    sb1.levelRelativeToCarrierDb = -22.4;

    ModulationSideband sb2;
    sb2.carrierFrequencyHz = 440.0;
    sb2.sidebandFrequencyHz = 434.75;
    sb2.order = -1;
    sb2.levelRelativeToCarrierDb = -22.6;

    mod.sidebands.push_back(sb1);
    mod.sidebands.push_back(sb2);

    // Demodulated temporal curve
    mod.timeCurve.xName = "time";
    mod.timeCurve.xUnit = "ms";
    mod.timeCurve.yName = "pitchDelta";
    mod.timeCurve.yUnit = "cents";
    mod.timeCurve.x = { 0.0, 50.0, 100.0, 150.0, 200.0 };
    mod.timeCurve.y = { 0.0, 24.7, 35.0, 24.7, 0.0 };

    // Modulation spectrum curve
    mod.spectrumCurve.xName = "frequency";
    mod.spectrumCurve.xUnit = "Hz";
    mod.spectrumCurve.yName = "magnitude";
    mod.spectrumCurve.yUnit = "dBFS";
    mod.spectrumCurve.x = { 1.0, 5.25, 10.5, 20.0 };
    mod.spectrumCurve.y = { -60.0, -6.0, -42.0, -75.0 };

    std::string jsonStr = MeasurementSerialization::serializeModulationResult(mod);
    REQUIRE_FALSE(jsonStr.empty());

    ModulationResultData parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeModulationResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.targetDestination == "pitch");
    REQUIRE_THAT(parsed.rateHz.value, WithinAbs(5.25, 1e-4));
    REQUIRE(parsed.rateHz.unit == "Hz");
    REQUIRE(parsed.rateMethod == "spectral_peak");

    REQUIRE_THAT(parsed.depth.value, WithinAbs(35.0, 1e-4));
    REQUIRE(parsed.depth.unit == "cents");

    REQUIRE(parsed.spectralMetadata.fftSize == 4096);
    REQUIRE(parsed.spectralMetadata.hopSize == 1024);
    REQUIRE(parsed.spectralMetadata.window == "hann");
    REQUIRE_THAT(parsed.spectralMetadata.frequencyResolutionHz, WithinAbs(11.71875, 1e-4));
    REQUIRE(parsed.spectralMetadata.averagingCount == 8);

    REQUIRE(parsed.waveform.waveform == "sine");
    REQUIRE(parsed.waveform.status == "inferred");
    REQUIRE_THAT(parsed.waveform.confidence, WithinAbs(0.91, 1e-4));

    REQUIRE(parsed.sidebands.size() == 2);
    REQUIRE_THAT(parsed.sidebands[0].carrierFrequencyHz, WithinAbs(440.0, 1e-4));
    REQUIRE_THAT(parsed.sidebands[0].sidebandFrequencyHz, WithinAbs(445.25, 1e-4));
    REQUIRE(parsed.sidebands[0].order == 1);
    REQUIRE_THAT(parsed.sidebands[0].levelRelativeToCarrierDb, WithinAbs(-22.4, 1e-4));

    REQUIRE_THAT(parsed.sidebands[1].sidebandFrequencyHz, WithinAbs(434.75, 1e-4));
    REQUIRE(parsed.sidebands[1].order == -1);

    REQUIRE(parsed.timeCurve.x.size() == 5);
    REQUIRE(parsed.timeCurve.yUnit == "cents");
    REQUIRE(parsed.spectrumCurve.x.size() == 4);
    REQUIRE(parsed.spectrumCurve.yUnit == "dBFS");
}

// ==============================================================================
// 3. INTEGRACIÓN COMPLETA Y TRAZABILIDAD (SPEC Y RESULT RECORD)
// ==============================================================================

TEST_CASE("DynamicsAndModulationContracts - Spec reproducibility fields round-trip", "[measurement][contracts][reproducibility]")
{
    MeasurementSpec spec;
    spec.schemaVersion = "response-measurement-1.0";
    spec.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    spec.measurementId = "meas-dyn-spec-001";
    spec.measurementType = "dynamics";
    spec.dutType = DeviceUnderTest::instrument;
    spec.parameterId = "param_velocity";
    spec.parameterName = "VELOCITY SENSITIVITY";

    // Traceability fixity
    spec.presetStateHash = "sha256_preset_state_dexed_epiano_v1";
    spec.velocityGrid = { 0, 1, 8, 16, 24, 32, 48, 64, 80, 96, 112, 120, 127 };
    spec.measurementWindowStartMs = 120.0;
    spec.measurementWindowEndMs = 650.0;

    spec.execution.sampleRateHz = 44100.0;
    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.midiNoteNumber = 60; // C4
    spec.stimulus.durationSec = 1.0;
    spec.stimulus.sha256 = "sha256_stimulus_grid_campaign";

    std::string jsonStr = MeasurementSerialization::serializeSpec(spec);
    REQUIRE_FALSE(jsonStr.empty());

    MeasurementSpec parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeSpec(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(parsed.measurementType == "dynamics");
    REQUIRE(parsed.presetStateHash == "sha256_preset_state_dexed_epiano_v1");
    REQUIRE(parsed.velocityGrid.size() == 13);
    REQUIRE(parsed.velocityGrid[0] == 0);
    REQUIRE(parsed.velocityGrid[12] == 127);
    REQUIRE_THAT(parsed.measurementWindowStartMs, WithinAbs(120.0, 1e-4));
    REQUIRE_THAT(parsed.measurementWindowEndMs, WithinAbs(650.0, 1e-4));
}

TEST_CASE("DynamicsAndModulationContracts - MeasurementResult with embedded dynamic payload round-trip", "[measurement][contracts]")
{
    MeasurementResult res;
    res.schemaVersion = "response-measurement-1.0";
    res.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    res.measurementId = "meas-dyn-res-001";
    res.measurementType = "dynamics";
    res.measurementDomain = "synthesizedSpectralResponse";
    res.status = MeasurementStatus::completed;
    res.reason = "midi_velocity_dynamic_response_completed";

    // Metadata for strict reproducibility
    res.presetStateHash = "d85f20a9e71b...preset_hash";
    res.measurementWindowStartMs = 100.0;
    res.measurementWindowEndMs = 500.0;
    res.execution.sampleRateHz = 48000.0;
    res.stimulus.midiNoteNumber = 60;
    res.stimulus.durationSec = 0.8;
    res.stimulus.sha256 = "c1a89b...stim_hash";
    res.analyzer.name = "DynamicsMeasurementAdapter";
    res.analyzer.version = "1.0.0";

    // Build Dynamic payload
    DynamicResponseResult dyn;
    dyn.dynamicRangeDb = 38.0;

    DynamicPoint p0;
    p0.velocity = 0;
    p0.rmsDbfs = -96.0;
    p0.status = "skipped";
    p0.reason = "midi_note_on_velocity_zero";

    DynamicPoint p127;
    p127.velocity = 127;
    p127.peakDbfs = -1.2;
    p127.rmsDbfs = -6.5;
    p127.spectralCentroidHz = 3200.0;
    p127.spectralRolloffHz = 8500.0;
    p127.attackTimeMs = 8.2;
    p127.status = "observed";

    dyn.points.push_back(p0);
    dyn.points.push_back(p127);

    dyn.amplitudeCurve.xName = "velocity";
    dyn.amplitudeCurve.xUnit = "midi_val";
    dyn.amplitudeCurve.yName = "rmsDbfs";
    dyn.amplitudeCurve.yUnit = "dBFS";
    dyn.amplitudeCurve.x = { 0.0, 127.0 };
    dyn.amplitudeCurve.y = { -96.0, -6.5 };

    res.dynamicResult = dyn;

    std::string jsonStr = MeasurementSerialization::serializeResult(res);
    REQUIRE_FALSE(jsonStr.empty());

    MeasurementResult parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.measurementType == "dynamics");
    REQUIRE(parsed.presetStateHash == "d85f20a9e71b...preset_hash");
    REQUIRE_THAT(parsed.measurementWindowStartMs, WithinAbs(100.0, 1e-4));
    REQUIRE_THAT(parsed.measurementWindowEndMs, WithinAbs(500.0, 1e-4));
    REQUIRE_THAT(parsed.execution.sampleRateHz, WithinAbs(48000.0, 1e-4));
    REQUIRE(parsed.stimulus.midiNoteNumber == 60);

    REQUIRE(parsed.dynamicResult.has_value());
    REQUIRE(parsed.dynamicResult->points.size() == 2);
    REQUIRE(parsed.dynamicResult->points[0].velocity == 0);
    REQUIRE(parsed.dynamicResult->points[0].status == "skipped");
    REQUIRE(parsed.dynamicResult->points[1].velocity == 127);
    REQUIRE(parsed.dynamicResult->points[1].status == "observed");
    REQUIRE(parsed.dynamicResult->amplitudeCurve.x.size() == 2);
}

TEST_CASE("DynamicsAndModulationContracts - MeasurementResult with embedded modulation payload round-trip", "[measurement][contracts]")
{
    MeasurementResult res;
    res.schemaVersion = "response-measurement-1.0";
    res.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    res.measurementId = "meas-mod-res-001";
    res.measurementType = "modulation";
    res.modulationDestination = "amplitude";
    res.status = MeasurementStatus::completed;

    res.presetStateHash = "preset_hash_tremolo_01";
    res.analyzer.name = "ModulationMeasurementAdapter";
    res.analyzer.version = "1.0.0";

    ModulationResultData mod;
    mod.targetDestination = "amplitude";
    mod.rateHz.name = "tremolo_rate";
    mod.rateHz.value = 6.0;
    mod.rateHz.unit = "Hz";
    mod.rateHz.status = "observed";
    mod.rateMethod = "amplitude_demodulation";

    mod.depth.name = "tremolo_depth";
    mod.depth.value = 6.02; // dB modulation
    mod.depth.unit = "dB";
    mod.depth.status = "observed";

    mod.waveform.waveform = "triangle";
    mod.waveform.status = "observed";
    mod.waveform.confidence = 0.95;

    mod.timeCurve.xName = "time";
    mod.timeCurve.xUnit = "ms";
    mod.timeCurve.yName = "envelopeDbfs";
    mod.timeCurve.yUnit = "dBFS";
    mod.timeCurve.x = { 0.0, 83.3, 166.6 };
    mod.timeCurve.y = { -6.0, -12.02, -6.0 };

    res.modulationResult = mod;

    std::string jsonStr = MeasurementSerialization::serializeResult(res);
    REQUIRE_FALSE(jsonStr.empty());

    MeasurementResult parsed;
    std::string err;
    bool ok = MeasurementSerialization::deserializeResult(jsonStr, parsed, err);
    REQUIRE(ok);
    REQUIRE(err.empty());

    REQUIRE(parsed.measurementType == "modulation");
    REQUIRE(parsed.modulationDestination == "amplitude");
    REQUIRE(parsed.modulationResult.has_value());
    REQUIRE(parsed.modulationResult->rateMethod == "amplitude_demodulation");
    REQUIRE_THAT(parsed.modulationResult->rateHz.value, WithinAbs(6.0, 1e-4));
    REQUIRE_THAT(parsed.modulationResult->depth.value, WithinAbs(6.02, 1e-4));
    REQUIRE(parsed.modulationResult->depth.unit == "dB");
    REQUIRE(parsed.modulationResult->waveform.waveform == "triangle");
    REQUIRE(parsed.modulationResult->waveform.status == "observed");
}
