/**
 * @file test_FilterMeasurementPersistenceAndReport.cpp
 * @brief Catch2 unit tests for Filter Response FAIR container persistence, anti-tampering, and HTML report (Phase 20.10.2 - T3).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "measurement/MeasurementContainerExporter.h"
#include "measurement/MeasurementContracts.h"
#include "gui/measurement/MeasurementViewModelLoader.h"
#include "core/ExperimentStorage.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::core;
using namespace abdaudiolab::gui::measurement;

namespace
{

std::vector<float> generateSineWave(double sampleRate, double freqHz, double durationSec, float amp = 0.5f)
{
    size_t count = static_cast<size_t>(std::lround(sampleRate * durationSec));
    std::vector<float> data(count);
    double phaseInc = 2.0 * std::numbers::pi * freqHz / sampleRate;
    double phase = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = static_cast<float>(std::sin(phase) * amp);
        phase += phaseInc;
    }
    return data;
}

std::vector<float> generateImpulseResponse(double sampleRate, double decayTimeSec, int numSamples = 2048)
{
    std::vector<float> ir(static_cast<size_t>(numSamples), 0.0f);
    ir[0] = 1.0f; // Direct impulse
    double alpha = std::exp(-1.0 / (decayTimeSec * sampleRate));
    float val = 0.8f;
    for (int i = 1; i < numSamples; ++i)
    {
        val = static_cast<float>(val * alpha);
        ir[static_cast<size_t>(i)] = (i % 2 == 0 ? val : -val * 0.5f);
    }
    return ir;
}

} // namespace

TEST_CASE("Filter Measurement - FAIR persistence, manifest export, and full reopening", "[measurement][filter][persistence]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_T3_Filter_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    const double sampleRate = 48000.0;
    const double duration = 0.1; // 4800 samples

    MeasurementSpec spec;
    spec.measurementId = "meas-fair-filter-001";
    spec.measurementType = "filter";
    spec.parameterName = "LadderFilter";
    spec.filterTopology = "lowPass";
    spec.measurementDomain = "directTransferFunction";
    spec.execution.sampleRateHz = sampleRate;
    spec.execution.blockSize = 512;
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.startFreqHz = 20.0f;
    spec.stimulus.endFreqHz = 20000.0f;
    spec.stimulus.durationSec = duration;
    spec.stimulus.sha256 = "dummy-sweep-spec-hash";

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.measurementType = "filter";
    res.filterTopology = "lowPass";
    res.measurementDomain = "directTransferFunction";
    res.status = MeasurementStatus::completed;
    res.reason = "Filter response successfully observed";
    res.dut.name = "ReferenceLadderFilter";
    res.dut.format = "Internal";
    res.analyzer.name = "FilterMeasurementAdapter";
    res.analyzer.version = "1.0.0";
    res.observability.status = "observed";

    // Metrics
    res.metrics.push_back({ "cutoffFrequency", 1000.0, "Hz", "observed", "detected_at_gpass_minus_3db" });
    res.metrics.push_back({ "asymptoticSlope", -12.0, "dB/oct", "observed", "octave_regression_fit" });
    res.metrics.push_back({ "qFactor", 0.707, "dim", "observed", "two_crossings_detected" });
    res.metrics.push_back({ "resonance", 0.0, "dB", "observed", "no_significant_peaking" });

    // Slope fit
    SlopeFitMetadata sf;
    sf.frequencyStartHz = 1400.0;
    sf.frequencyEndHz = 20000.0;
    sf.rSquared = 0.9985;
    sf.sampleCount = 120;
    sf.selectionReason = "asymptotic_region_above_1.4fc";
    res.slopeFit = sf;

    // Curve
    res.curve.xName = "frequency";
    res.curve.xUnit = "Hz";
    res.curve.yName = "magnitude";
    res.curve.yUnit = "dB";
    res.curve.x = { 20.0, 100.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    res.curve.y = { 0.0, -0.1, -0.8, -3.0, -9.0, -21.0, -33.0, -45.0 };

    FilterExportArtifacts artifacts;
    artifacts.sampleRateHz = sampleRate;
    artifacts.stimulusAudio = generateSineWave(sampleRate, 440.0, duration, 0.5f);
    artifacts.capturedAudio = generateSineWave(sampleRate, 440.0, duration, 0.35f);
    artifacts.impulseResponse = generateImpulseResponse(sampleRate, 0.01, 1024);

    juce::File containerDir = tempDir.getChildFile("container");
    juce::String err;
    bool ok = MeasurementContainerExporter::exportFilterMeasurement(containerDir, spec, res, artifacts, err);
    REQUIRE(ok);
    REQUIRE(err.isEmpty());

    // 1. Verify existence of all required FAIR files on disk
    REQUIRE(containerDir.getChildFile("experiment.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_spec.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_stimulus.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/filter_response_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/audio_captured.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/audio_stimulus.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/impulse_response.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("results/measurement_result.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("reports/measurement_report.html").existsAsFile());
    REQUIRE(containerDir.getChildFile("manifest.json").existsAsFile());

    // 2. Full reopening and cryptographic integrity audit via ExperimentFolderReader
    ExperimentFolderReader reader;
    REQUIRE(reader.canRead(containerDir));

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    REQUIRE(recordOpt.has_value());
    REQUIRE(recordOpt->status != ExperimentStatus::Corrupt);
    REQUIRE(recordOpt->artifacts.size() >= 7);

    auto findArtifact = [&](const std::string& role) -> const ExperimentArtifact* {
        for (const auto& a : recordOpt->artifacts)
            if (a.role == role) return &a;
        return nullptr;
    };

    // Verify FAIR roles
    auto* artSpec = findArtifact("measurement_spec");
    REQUIRE(artSpec != nullptr);
    CHECK(artSpec->relativePath == "specs/measurement_spec.json");

    auto* artStim = findArtifact("measurement_stimulus");
    REQUIRE(artStim != nullptr);

    auto* artCurve = findArtifact("filter_response_curve");
    REQUIRE(artCurve != nullptr);
    CHECK(artCurve->relativePath == "curves/filter_response_curve.json");

    auto* artCaptured = findArtifact("measurement_captured_audio");
    REQUIRE(artCaptured != nullptr);
    CHECK(artCaptured->relativePath == "audio/audio_captured.wav");
    REQUIRE(artCaptured->audio.has_value());
    CHECK(artCaptured->audio->sampleRate == sampleRate);

    auto* artStimAudio = findArtifact("measurement_stimulus_audio");
    REQUIRE(artStimAudio != nullptr);
    CHECK(artStimAudio->relativePath == "audio/audio_stimulus.wav");

    auto* artIr = findArtifact("measurement_impulse_response");
    REQUIRE(artIr != nullptr);
    CHECK(artIr->relativePath == "audio/impulse_response.wav");

    auto* artResult = findArtifact("measurement_result");
    REQUIRE(artResult != nullptr);

    auto* artReport = findArtifact("measurement_report");
    REQUIRE(artReport != nullptr);

    // 3. UI ViewModel Loading and verification
    MeasurementViewModel vm;
    juce::String vmErr;
    bool vmLoaded = MeasurementViewModelLoader::loadFromContainer(containerDir, vm, vmErr);
    REQUIRE(vmLoaded);
    CHECK(vmErr.isEmpty());
    CHECK(vm.integrityStatus == UiIntegrityStatus::Verified);
    CHECK(vm.filterTopology == "lowPass");
    CHECK(vm.measurementDomain == "directTransferFunction");
    REQUIRE(vm.slopeFit.has_value());
    CHECK(vm.slopeFit->rSquared >= 0.99);
    CHECK(vm.isPlaybackAllowed() == true);
    CHECK(vm.isStimulusPlaybackAllowed() == true);
    CHECK(vm.isImpulseResponsePlaybackAllowed() == true);

    // Cleanup
    tempDir.deleteRecursively();
}

TEST_CASE("Filter Measurement - Anti-Tampering: sweep, capture, IR, deletion, and path traversal", "[measurement][filter][tamper]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_T3_FilterTamper_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    const double sampleRate = 48000.0;
    const double duration = 0.05;

    MeasurementSpec spec;
    spec.measurementId = "meas-tamper-filter";
    spec.measurementType = "filter";
    spec.filterTopology = "lowPass";
    spec.measurementDomain = "directTransferFunction";

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.measurementType = "filter";
    res.status = MeasurementStatus::completed;
    res.curve.x = { 100.0, 1000.0 };
    res.curve.y = { 0.0, -3.0 };

    FilterExportArtifacts artifacts;
    artifacts.sampleRateHz = sampleRate;
    artifacts.stimulusAudio = generateSineWave(sampleRate, 100.0, duration);
    artifacts.capturedAudio = generateSineWave(sampleRate, 100.0, duration, 0.4f);
    artifacts.impulseResponse = generateImpulseResponse(sampleRate, 0.01, 512);

    juce::File containerDir = tempDir.getChildFile("container");
    juce::String err;
    REQUIRE(MeasurementContainerExporter::exportFilterMeasurement(containerDir, spec, res, artifacts, err));

    ExperimentFolderReader reader;

    SECTION("Tampering with audio_stimulus.wav triggers Corrupt status")
    {
        juce::File sweepFile = containerDir.getChildFile("audio/audio_stimulus.wav");
        auto altered = generateSineWave(sampleRate, 880.0, duration); // changed audio
        REQUIRE(MeasurementContainerExporter::writeWavFile(sweepFile, altered, sampleRate));

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        CHECK(rec->status == ExperimentStatus::Corrupt);
        CHECK(readErr.contains("Cryptographic mismatch"));
    }

    SECTION("Tampering with audio_captured.wav triggers Corrupt status")
    {
        juce::File capFile = containerDir.getChildFile("audio/audio_captured.wav");
        auto altered = generateSineWave(sampleRate, 220.0, duration * 2);
        REQUIRE(MeasurementContainerExporter::writeWavFile(capFile, altered, sampleRate));

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        CHECK(rec->status == ExperimentStatus::Corrupt);
        CHECK(readErr.contains("Cryptographic mismatch"));
    }

    SECTION("Tampering with impulse_response.wav triggers Corrupt status")
    {
        juce::File irFile = containerDir.getChildFile("audio/impulse_response.wav");
        auto altered = generateImpulseResponse(sampleRate, 0.05, 1024);
        REQUIRE(MeasurementContainerExporter::writeWavFile(irFile, altered, sampleRate));

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        CHECK(rec->status == ExperimentStatus::Corrupt);
        CHECK(readErr.contains("Cryptographic mismatch"));
    }

    SECTION("Deleting an artifact triggers Corrupt status")
    {
        juce::File curveFile = containerDir.getChildFile("curves/filter_response_curve.json");
        curveFile.deleteFile();

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        CHECK(rec->status == ExperimentStatus::Corrupt);
        CHECK(readErr.contains("Missing required artifact"));
    }

    SECTION("Path traversal outside container is rejected as Corrupt")
    {
        juce::File manifestFile = containerDir.getChildFile("manifest.json");
        std::string manifestText = manifestFile.loadFileAsString().toStdString();
        // Replace audio/audio_captured.wav with ../../outside.wav
        auto pos = manifestText.find("audio/audio_captured.wav");
        REQUIRE(pos != std::string::npos);
        manifestText.replace(pos, std::string("audio/audio_captured.wav").length(), "../../outside.wav");
        manifestFile.replaceWithText(juce::String::fromUTF8(manifestText.c_str()));

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        CHECK(rec->status == ExperimentStatus::Corrupt);
        CHECK(readErr.contains("Security violation"));
    }

    tempDir.deleteRecursively();
}

TEST_CASE("Filter Measurement - HTML Report presentation rules and domain distinction", "[measurement][filter][report]")
{
    MeasurementSpec spec;
    spec.measurementId = "meas-report-filter";
    spec.filterTopology = "lowPass";

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.filterTopology = "lowPass";
    res.status = MeasurementStatus::completed;
    res.reason = "Filter response successfully observed";
    res.dut.name = "OberheimFilter";
    res.dut.format = "Hardware";
    res.analyzer.name = "FilterMeasurementAdapter";
    res.analyzer.version = "1.0.0";
    res.observability.status = "observed";

    SECTION("directTransferFunction report displays transfer metrics, slope fit, R2, and NEVER PASS")
    {
        spec.measurementDomain = "directTransferFunction";
        res.measurementDomain = "directTransferFunction";

        res.metrics.push_back({ "cutoffFrequency", 1250.0, "Hz", "observed", "gpass_minus_3db" });
        res.metrics.push_back({ "asymptoticSlope", -23.8, "dB/oct", "observed", "octave_fit" });
        res.metrics.push_back({ "qFactor", 1.414, "dim", "observed", "dual_crossing" });
        res.metrics.push_back({ "resonance", 3.0, "dB", "observed", "resonance_peak" });

        SlopeFitMetadata sf;
        sf.frequencyStartHz = 1750.0;
        sf.frequencyEndHz = 18000.0;
        sf.rSquared = 0.9991;
        sf.sampleCount = 95;
        res.slopeFit = sf;

        res.curve.x = { 100.0, 1000.0, 10000.0 };
        res.curve.y = { 0.0, -1.0, -20.0 };

        std::string html = MeasurementContainerExporter::generateFilterReportHtml(
            spec, res, "../audio/audio_captured.wav", "../audio/audio_stimulus.wav", "../audio/impulse_response.wav");

        // Checks:
        CHECK(html.find("DIRECT TRANSFER FUNCTION") != std::string::npos);
        CHECK(html.find("Topology: lowPass") != std::string::npos);
        CHECK(html.find("cutoffFrequency") != std::string::npos);
        CHECK(html.find("1250.00") != std::string::npos);
        CHECK(html.find("Fit Region") != std::string::npos);
        CHECK(html.find("0.9991") != std::string::npos);
        CHECK(html.find("audio_captured.wav") != std::string::npos);
        CHECK(html.find("audio_stimulus.wav") != std::string::npos);
        CHECK(html.find("impulse_response.wav") != std::string::npos);

        // Strict metrological rule: NEVER display false PASS
        CHECK(html.find("PASS") == std::string::npos);
        CHECK(html.find("COMPLETED") != std::string::npos);
    }

    SECTION("synthesizedSpectralResponse report hides direct transfer metrics and displays proxy warning")
    {
        spec.measurementDomain = "synthesizedSpectralResponse";
        res.measurementDomain = "synthesizedSpectralResponse";

        // Filter adapter in MIDI domain marks direct transfer metrics as not_observable
        res.metrics.push_back({ "observedSpectralPeak", 520.0, "Hz", "observed", "midi_composite_response" });
        res.metrics.push_back({ "observedSpectralRolloff", -18.0, "dB/oct", "observed", "composite_synth_slope" });
        // Direct transfer metrics:
        res.metrics.push_back({ "cutoffFrequency", 0.0, "Hz", "not_observable", "midi_composite_response_only" });
        res.metrics.push_back({ "asymptoticSlope", 0.0, "dB/oct", "not_observable", "midi_composite_response_only" });

        std::string html = MeasurementContainerExporter::generateFilterReportHtml(
            spec, res, "../audio/audio_captured.wav", "", "");

        CHECK(html.find("SYNTHESIZED SPECTRAL RESPONSE [PROXY]") != std::string::npos);
        CHECK(html.find("Direct transfer function of the isolated filter is NOT observable and is not claimed") != std::string::npos);
        CHECK(html.find("observedSpectralPeak") != std::string::npos);
        // Direct transfer function metric cutoffFrequency should NOT be presented as valid transfer metric in table
        CHECK(html.find("cutoffFrequency") == std::string::npos);

        // NEVER display PASS
        CHECK(html.find("PASS") == std::string::npos);
    }
}
