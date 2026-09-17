/**
 * @file test_DynamicsAndModulationExport.cpp
 * @brief Catch2 unit tests for FAIR container export, report generation, and ViewModel loading (Fase 20.10.3 - T3).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/MeasurementContracts.h"
#include "measurement/MeasurementContainerExporter.h"
#include "measurement/MeasurementReportGenerator.h"
#include "measurement/adapters/DynamicsMeasurementAdapter.h"
#include "measurement/adapters/ModulationMeasurementAdapter.h"
#include "gui/measurement/MeasurementViewModelLoader.h"
#include "core/ExperimentStorage.h"
#include <juce_core/juce_core.h>
#include <cmath>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::gui::measurement;
using Catch::Matchers::WithinAbs;

namespace
{

std::vector<float> generateSineTone(double sampleRate, double durationSec, double freqHz, float amp = 0.5f)
{
    size_t n = static_cast<size_t>(sampleRate * durationSec);
    std::vector<float> buf(n, 0.0f);
    for (size_t i = 0; i < n; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        buf[i] = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * freqHz * t));
    }
    return buf;
}

std::vector<float> generateAmBuffer(double sampleRate, double durationSec, double carrierHz, double lfoRateHz, double modDepth = 0.5)
{
    size_t n = static_cast<size_t>(sampleRate * durationSec);
    std::vector<float> buf(n, 0.0f);
    for (size_t i = 0; i < n; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double lfo = std::sin(2.0 * 3.14159265358979323846 * lfoRateHz * t);
        double carrier = std::sin(2.0 * 3.14159265358979323846 * carrierHz * t);
        buf[i] = static_cast<float>(0.5 * (1.0 + modDepth * lfo) * carrier);
    }
    return buf;
}

} // namespace

TEST_CASE("MeasurementContainerExporter - Dynamics FAIR Export, Report, and ViewModel loading", "[measurement][dynamics][export]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_DynExport_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    MeasurementSpec spec;
    spec.measurementId = "meas-dyn-export-001";
    spec.measurementType = "dynamics";
    spec.parameterName = "Dexed";
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.startFreqHz = 440.0f;
    spec.stimulus.durationSec = 0.5;

    double sampleRate = 48000.0;
    std::vector<int> velocities = { 0, 1, 16, 32, 64, 96, 127 };
    std::vector<DynamicsMeasurementAdapter::VelocityTake> takes;

    for (int v : velocities)
    {
        DynamicsMeasurementAdapter::VelocityTake take;
        take.velocity = v;
        take.sampleRateHz = sampleRate;
        take.presetStateHash = "state_hash_export_test";
        take.audioArtifactHash = "audio_hash_v_" + std::to_string(v);
        float amp = (v == 0) ? 0.0f : static_cast<float>(std::pow(static_cast<double>(v) / 127.0, 1.5));
        take.audio = generateSineTone(sampleRate, 0.5, 440.0, amp);
        take.noteOnSample = 0;
        take.noteOffSample = take.audio.size();
        takes.push_back(take);
    }

    MeasurementResult res = DynamicsMeasurementAdapter::analyzeTakes(spec, takes);
    REQUIRE(res.status == MeasurementStatus::completed);
    REQUIRE(res.dynamicResult.has_value());

    juce::File containerDir = tempDir.getChildFile("dynamics_container");
    juce::String exportErr;
    bool exportOk = MeasurementContainerExporter::exportMeasurement(containerDir, spec, res, juce::File(), exportErr);

    REQUIRE(exportOk);
    REQUIRE(exportErr.isEmpty());

    // Verify artifact file structure
    REQUIRE(containerDir.getChildFile("experiment.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_spec.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_stimulus.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/dynamics_velocity_level_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/dynamics_velocity_timbre_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("results/measurement_result.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("reports/measurement_report.html").existsAsFile());
    REQUIRE(containerDir.getChildFile("manifest.json").existsAsFile());

    // Verify cryptographic integrity with ExperimentFolderReader
    abdaudiolab::core::ExperimentFolderReader reader;
    REQUIRE(reader.canRead(containerDir));

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    REQUIRE(recordOpt.has_value());
    REQUIRE(recordOpt->status != abdaudiolab::core::ExperimentStatus::Corrupt);
    REQUIRE(recordOpt->artifacts.size() == 6); // spec, stimulus, 2 curves, result, report

    // Verify HTML content features
    juce::File reportFile = containerDir.getChildFile("reports/measurement_report.html");
    juce::String html = reportFile.loadFileAsString();
    REQUIRE(html.contains("MIDI Dynamics"));
    REQUIRE(html.contains("DYNAMIC: MIDI_VELOCITY"));
    REQUIRE(html.contains("Tabulated Velocity Series"));
    REQUIRE(html.contains("Velocity vs Level Response"));

    // Verify ViewModelLoader
    MeasurementViewModel model;
    juce::String loadErr;
    bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir, model, loadErr);

    REQUIRE(loadOk);
    REQUIRE(loadErr.isEmpty());
    REQUIRE(model.measurementType == "dynamics");
    REQUIRE(model.dynamicsResult.has_value());
    REQUIRE(model.dynamicsResult->points.size() == velocities.size());
    REQUIRE(model.integrityStatus == UiIntegrityStatus::Verified);
    REQUIRE(model.secondaryCurveFile.existsAsFile());

    // Anti-Tampering Test: modify one curve file
    juce::File levelCurveFile = containerDir.getChildFile("curves/dynamics_velocity_level_curve.json");
    juce::String origContent = levelCurveFile.loadFileAsString();
    levelCurveFile.replaceWithText(origContent + " "); // append whitespace tampering

    MeasurementViewModel tamperedModel;
    juce::String tamperErr;
    MeasurementViewModelLoader::loadFromContainer(containerDir, tamperedModel, tamperErr);
    REQUIRE(tamperedModel.integrityStatus == UiIntegrityStatus::Corrupt);
    REQUIRE(tamperedModel.statusText == "CORRUPT");

    tempDir.deleteRecursively();
}

TEST_CASE("MeasurementContainerExporter - Modulation FAIR Export, Report, and ViewModel loading", "[measurement][modulation][export]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ModExport_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    MeasurementSpec spec;
    spec.measurementId = "meas-mod-export-001";
    spec.measurementType = "modulation";
    spec.modulationDestination = "amplitude";
    spec.parameterName = "Dexed";
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.stimulus.startFreqHz = 440.0f;
    spec.stimulus.durationSec = 2.0;

    double sampleRate = 48000.0;
    auto audio = generateAmBuffer(sampleRate, 2.0, 440.0, 5.0, 0.5);

    // Create baseline audio WAV
    juce::File sourceWav = tempDir.getChildFile("source_am.wav");
    REQUIRE(MeasurementContainerExporter::writeWavFile(sourceWav, audio, sampleRate, 1));

    MeasurementResult res = ModulationMeasurementAdapter::analyzeBuffer(spec, audio, sampleRate);
    REQUIRE(res.status == MeasurementStatus::completed);
    REQUIRE(res.modulationResult.has_value());

    juce::File containerDir = tempDir.getChildFile("modulation_container");
    juce::String exportErr;
    bool exportOk = MeasurementContainerExporter::exportMeasurement(containerDir, spec, res, sourceWav, exportErr);

    REQUIRE(exportOk);
    REQUIRE(exportErr.isEmpty());

    // Verify artifact file structure
    REQUIRE(containerDir.getChildFile("experiment.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_spec.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_stimulus.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/modulation_time_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/modulation_spectrum_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/modulation_reference.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("results/measurement_result.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("reports/measurement_report.html").existsAsFile());
    REQUIRE(containerDir.getChildFile("manifest.json").existsAsFile());

    // Verify cryptographic integrity with ExperimentFolderReader
    abdaudiolab::core::ExperimentFolderReader reader;
    REQUIRE(reader.canRead(containerDir));

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    REQUIRE(recordOpt.has_value());
    REQUIRE(recordOpt->status != abdaudiolab::core::ExperimentStatus::Corrupt);
    REQUIRE(recordOpt->artifacts.size() == 7); // spec, stimulus, 2 curves, audio, result, report

    // Verify HTML content features
    juce::File reportFile = containerDir.getChildFile("reports/measurement_report.html");
    juce::String html = reportFile.loadFileAsString();
    REQUIRE(html.contains("LFO Modulation"));
    REQUIRE(html.contains("MODULATION: amplitude"));
    REQUIRE(html.contains("Demodulated Modulation Trajectory"));
    REQUIRE(html.contains("Modulation Spectrum &amp; Carrier Sidebands"));
    REQUIRE(html.contains("Observed Spectral Sidebands"));

    // Verify ViewModelLoader
    MeasurementViewModel model;
    juce::String loadErr;
    bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir, model, loadErr);

    REQUIRE(loadOk);
    REQUIRE(loadErr.isEmpty());
    REQUIRE(model.measurementType == "modulation");
    REQUIRE(model.modulationResult.has_value());
    REQUIRE_THAT(model.modulationResult->rateHz.value, WithinAbs(5.0, 0.4));
    REQUIRE(model.integrityStatus == UiIntegrityStatus::Verified);
    REQUIRE(model.audioFile.existsAsFile());
    REQUIRE(model.secondaryCurveFile.existsAsFile());

    // Anti-Tampering Test: modify report file
    juce::File repFile = containerDir.getChildFile("reports/measurement_report.html");
    juce::String origReport = repFile.loadFileAsString();
    repFile.replaceWithText(origReport + "<!-- tamper -->");

    MeasurementViewModel tamperedModel;
    juce::String tamperErr;
    MeasurementViewModelLoader::loadFromContainer(containerDir, tamperedModel, tamperErr);
    REQUIRE(tamperedModel.integrityStatus == UiIntegrityStatus::Corrupt);
    REQUIRE(tamperedModel.statusText == "CORRUPT");

    tempDir.deleteRecursively();
}
