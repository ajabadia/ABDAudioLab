/**
 * @file test_MeasurementPersistenceAndReport.cpp
 * @brief Catch2 unit tests for FAIR container persistence, anti-tampering, and HTML report generation.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "measurement/MeasurementContainerExporter.h"
#include "measurement/MeasurementContracts.h"
#include "core/ExperimentStorage.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::core;

static juce::File createTestWav(const juce::File& file, double sampleRate, int numSamples)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
        new juce::FileOutputStream(file), sampleRate, 1, 16, {}, 0));
    if (writer != nullptr)
    {
        juce::AudioBuffer<float> buf(1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buf.setSample(0, i, static_cast<float>(std::sin(2.0 * 3.141592653589793 * 440.0 * i / sampleRate) * 0.5));
        writer->writeFromAudioSampleBuffer(buf, 0, numSamples);
    }
    return file;
}

TEST_CASE("MeasurementContainerExporter - FAIR persistence and reopening", "[measurement][persistence]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_T4_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    juce::File sourceWav = tempDir.getChildFile("source.wav");
    createTestWav(sourceWav, 48000.0, 24000);

    MeasurementSpec spec;
    spec.measurementId = "meas-fair-001";
    spec.measurementType = "envelope";
    spec.parameterName = "Dexed";
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.stimulus.type = StimulusType::midiNote;

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.measurementType = "envelope";
    res.status = MeasurementStatus::completed;
    res.reason = "Envelope successfully observed";
    res.dut.name = "Dexed";
    res.analyzer.name = "SynthEnvelopeAnalyzer";
    res.analyzer.version = "1.0.0";
    res.observability.status = "observed";

    res.metrics.push_back({ "attackTime", 45.0, "ms", "observed" });
    res.metrics.push_back({ "decayTime", 120.0, "ms", "observed" });
    res.metrics.push_back({ "sustainLevel", -6.0, "dBFS", "observed" });
    res.metrics.push_back({ "releaseTime", 300.0, "ms", "observed" });
    res.metrics.push_back({ "peakAmplitude", -0.5, "dBFS", "observed" });

    res.curve.xName = "time";
    res.curve.xUnit = "ms";
    res.curve.yName = "amplitude";
    res.curve.yUnit = "dBFS";
    res.curve.x = { 0.0, 50.0, 150.0, 500.0 };
    res.curve.y = { -96.0, -0.5, -6.0, -96.0 };

    juce::File containerDir = tempDir.getChildFile("container");

    juce::String err;
    bool ok = MeasurementContainerExporter::exportMeasurement(containerDir, spec, res, sourceWav, err);
    REQUIRE(ok);
    REQUIRE(err.isEmpty());

    // Verify file existence
    REQUIRE(containerDir.getChildFile("experiment.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_spec.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_stimulus.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("results/measurement_result.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/envelope_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/envelope_reference.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("reports/measurement_report.html").existsAsFile());
    REQUIRE(containerDir.getChildFile("manifest.json").existsAsFile());

    // Reopen using standard ExperimentFolderReader
    ExperimentFolderReader reader;
    REQUIRE(reader.canRead(containerDir));

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    REQUIRE(recordOpt.has_value());
    REQUIRE(recordOpt->status != ExperimentStatus::Corrupt);
    REQUIRE(recordOpt->artifacts.size() == 6);

    // Verify FAIR roles in manifest
    auto hasRole = [&](const std::string& role) {
        for (const auto& art : recordOpt->artifacts)
            if (art.role == role) return true;
        return false;
    };

    REQUIRE(hasRole("measurement_spec"));
    REQUIRE(hasRole("measurement_stimulus"));
    REQUIRE(hasRole("measurement_result"));
    REQUIRE(hasRole("envelope_curve"));
    REQUIRE(hasRole("measurement_baseline_audio"));
    REQUIRE(hasRole("measurement_report"));

    // Cleanup
    tempDir.deleteRecursively();
}

TEST_CASE("MeasurementContainerExporter - Anti-tampering detection", "[measurement][persistence][tamper]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_T4_Tamper_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    juce::File sourceWav = tempDir.getChildFile("source.wav");
    createTestWav(sourceWav, 48000.0, 4800);

    MeasurementSpec spec;
    spec.measurementId = "tamper-001";
    MeasurementResult res;
    res.measurementId = "tamper-001";
    res.status = MeasurementStatus::completed;
    res.curve.x = { 0.0, 10.0 };
    res.curve.y = { -96.0, -10.0 };
    res.curve.xName = "time";
    res.curve.xUnit = "ms";
    res.curve.yName = "amplitude";
    res.curve.yUnit = "dBFS";

    juce::File containerDir = tempDir.getChildFile("container");
    juce::String err;
    REQUIRE(MeasurementContainerExporter::exportMeasurement(containerDir, spec, res, sourceWav, err));

    ExperimentFolderReader reader;

    SECTION("Tampering with results/measurement_result.json triggers Corrupt status")
    {
        juce::File resFile = containerDir.getChildFile("results/measurement_result.json");
        std::string original = resFile.loadFileAsString().toStdString();
        resFile.replaceWithText(juce::String(original + " ")); // add one whitespace to alter hash

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        REQUIRE(rec->status == ExperimentStatus::Corrupt);
        REQUIRE(readErr.contains("Cryptographic mismatch"));
    }

    SECTION("Tampering with audio/envelope_reference.wav triggers Corrupt status")
    {
        juce::File wavFile = containerDir.getChildFile("audio/envelope_reference.wav");
        createTestWav(wavFile, 48000.0, 9600); // altered audio length & samples

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        REQUIRE(rec->status == ExperimentStatus::Corrupt);
        REQUIRE(readErr.contains("Cryptographic mismatch"));
    }

    SECTION("Deleting an artifact triggers Corrupt status")
    {
        juce::File curveFile = containerDir.getChildFile("curves/envelope_curve.json");
        curveFile.deleteFile();

        juce::String readErr;
        auto rec = reader.read(containerDir, readErr);
        REQUIRE(rec.has_value());
        REQUIRE(rec->status == ExperimentStatus::Corrupt);
        REQUIRE(readErr.contains("Missing required artifact"));
    }

    tempDir.deleteRecursively();
}

TEST_CASE("MeasurementContainerExporter - HTML Report presentation rules", "[measurement][report]")
{
    MeasurementSpec spec;
    spec.measurementId = "meas-report-presentation";
    spec.parameterName = "Dexed";

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.status = MeasurementStatus::completed;
    res.reason = "Envelope successfully observed";
    res.dut.name = "Dexed";
    res.dut.format = "VST3";
    res.analyzer.name = "SynthEnvelopeAnalyzer";
    res.analyzer.version = "1.0.0";
    res.observability.status = "observed";

    res.metrics.push_back({ "attackTime", 50.0, "ms", "observed" });
    res.metrics.push_back({ "decayTime", 120.0, "ms", "observed" });
    res.metrics.push_back({ "sustainLevel", -12.0, "dBFS", "observed" });
    res.metrics.push_back({ "releaseTime", 400.0, "ms", "observed" });

    res.curve.x = { 0.0, 50.0, 100.0 };
    res.curve.y = { -96.0, 0.0, -12.0 };
    res.curve.xName = "time";
    res.curve.xUnit = "ms";
    res.curve.yName = "amplitude";
    res.curve.yUnit = "dBFS";

    std::string html = MeasurementContainerExporter::generateReportHtml(spec, res, "../audio/ref.wav");

    // Must display COMPLETED badge & header
    REQUIRE(html.find("Envelope measurement: COMPLETED") != std::string::npos);
    REQUIRE(html.find("COMPLETED") != std::string::npos);

    // Must NOT convert completed into PASS verdict
    REQUIRE(html.find("PASS") == std::string::npos);

    // Must display metrics
    REQUIRE(html.find("attackTime") != std::string::npos);
    REQUIRE(html.find("decayTime") != std::string::npos);
    REQUIRE(html.find("sustainLevel") != std::string::npos);
    REQUIRE(html.find("releaseTime") != std::string::npos);

    // Must include vector SVG and audio playback controls
    REQUIRE(html.find("<svg") != std::string::npos);
    REQUIRE(html.find("<audio controls") != std::string::npos);
    REQUIRE(html.find("src=\"../audio/ref.wav\"") != std::string::npos);

    SECTION("Unreliable status displays diagnostic reason")
    {
        MeasurementResult unres = res;
        unres.status = MeasurementStatus::unreliable;
        unres.reason = "decay_not_observable_gate_too_short";
        unres.observability.status = "unreliable";
        unres.observability.reason = "Gate too short to observe decay plateau";

        std::string unHtml = MeasurementContainerExporter::generateReportHtml(spec, unres, "../audio/ref.wav");

        REQUIRE(unHtml.find("Envelope measurement: UNRELIABLE") != std::string::npos);
        REQUIRE(unHtml.find("UNRELIABLE") != std::string::npos);
        REQUIRE(unHtml.find("decay_not_observable_gate_too_short") != std::string::npos);
        REQUIRE(unHtml.find("Gate too short to observe decay plateau") != std::string::npos);
    }
}
