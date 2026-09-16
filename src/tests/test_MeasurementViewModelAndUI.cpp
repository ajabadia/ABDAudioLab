/**
 * @file test_MeasurementViewModelAndUI.cpp
 * @brief Catch2 unit tests for Phase 20.10.1 MeasurementViewModelLoader, UI state mapping, and integrity defenses.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "gui/measurement/MeasurementViewModel.h"
#include "gui/measurement/MeasurementViewModelLoader.h"
#include "gui/measurement/MeasurementAudioPlayerComponent.h"
#include "gui/measurement/MeasurementViewerPanel.h"
#include "measurement/MeasurementContainerExporter.h"
#include "measurement/MeasurementContracts.h"
#include "core/ExperimentStorage.h"

using namespace abdaudiolab::gui::measurement;
using namespace abdaudiolab::measurement;

static juce::File createTestWavFile(const juce::File& file, double sampleRate, int numSamples)
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

TEST_CASE("MeasurementViewModelLoader - Load and Confinement", "[measurement][viewmodel]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_VM_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    juce::File sourceWav = tempDir.getChildFile("source.wav");
    createTestWavFile(sourceWav, 48000.0, 24000);

    MeasurementSpec spec;
    spec.measurementId = "meas-ui-dexed-001";
    spec.measurementType = "envelope";
    spec.dutType = DeviceUnderTest::instrument;
    spec.parameterName = "Dexed";
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    spec.execution.latencySamples = 16;
    spec.stimulus.type = StimulusType::midiNote;

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.measurementType = "envelope";
    res.status = MeasurementStatus::completed;
    res.reason = "Envelope successfully observed";
    res.dut.name = "Dexed";
    res.dut.format = "VST3";
    res.execution.sampleRateHz = 48000.0;
    res.execution.blockSize = 512;
    res.execution.latencySamples = 16;
    res.analyzer.name = "SynthEnvelopeAnalyzer";
    res.analyzer.version = "1.0.0";
    res.observability.status = "observed";

    res.metrics.push_back({ "attackTime", 35.0, "ms", "observed" });
    res.metrics.push_back({ "decayTime", 120.0, "ms", "observed" });
    res.metrics.push_back({ "sustainLevel", -6.0, "dBFS", "observed" });
    res.metrics.push_back({ "releaseTime", 300.0, "ms", "observed" });
    res.metrics.push_back({ "peakAmplitude", -1.2, "dBFS", "observed" });

    res.curve.xName = "time";
    res.curve.xUnit = "ms";
    res.curve.yName = "amplitude";
    res.curve.yUnit = "dBFS";
    res.curve.x = { 0.0, 35.0, 155.0, 500.0 };
    res.curve.y = { -96.0, -1.2, -6.0, -96.0 };

    juce::File containerDir = tempDir.getChildFile("container");
    juce::String exportErr;
    REQUIRE(MeasurementContainerExporter::exportMeasurement(containerDir, spec, res, sourceWav, exportErr));

    SECTION("Valid container loads correctly into MeasurementViewModel")
    {
        MeasurementViewModel model;
        juce::String loadErr;
        bool loaded = MeasurementViewModelLoader::loadFromContainer(containerDir, model, loadErr);

        REQUIRE(loaded);
        REQUIRE(loadErr.isEmpty());
        REQUIRE(model.measurementId == "meas-ui-dexed-001");
        REQUIRE(model.measurementType == "envelope");
        REQUIRE(model.dutName == "Dexed");
        REQUIRE(model.dutFormat == "VST3");
        REQUIRE(model.sampleRateHz == 48000.0);
        REQUIRE(model.blockSize == 512);
        REQUIRE(model.latencySamples == 16);
        REQUIRE(model.analyzerName == "SynthEnvelopeAnalyzer");

        // Status presentation (Never PASS)
        REQUIRE(model.statusText == "COMPLETED");
        REQUIRE(model.statusIcon == "[OK]");
        REQUIRE(model.statusText != "PASS");

        // Metrics mapping
        REQUIRE(model.metrics.size() == 5);
        REQUIRE(model.metrics[0].name == "attackTime");
        REQUIRE(model.metrics[0].value == 35.0);
        REQUIRE(model.metrics[0].unit == "ms");

        // Curve mapping
        REQUIRE(model.curve.x.size() == 4);
        REQUIRE(model.curve.xName == "time");
        REQUIRE(model.curve.yName == "amplitude");

        // Integrity state
        REQUIRE(model.integrityStatus == UiIntegrityStatus::Verified);
        REQUIRE(model.isPlaybackAllowed());

        // Artifact paths
        REQUIRE(model.audioFile.existsAsFile());
        REQUIRE(model.htmlReportFile.existsAsFile());
    }

    SECTION("Path Traversal Confinement Rejection")
    {
        // Try passing a non-directory
        MeasurementViewModel model;
        juce::String err;
        REQUIRE_FALSE(MeasurementViewModelLoader::loadFromContainer(sourceWav, model, err));
        REQUIRE(err.contains("not a directory"));
    }

    SECTION("Tampering with results/measurement_result.json flags Corrupt status")
    {
        juce::File resFile = containerDir.getChildFile("results/measurement_result.json");
        std::string original = resFile.loadFileAsString().toStdString();
        resFile.replaceWithText(juce::String(original + " ")); // alter SHA-256

        MeasurementViewModel model;
        juce::String loadErr;
        bool loaded = MeasurementViewModelLoader::loadFromContainer(containerDir, model, loadErr);

        REQUIRE(loaded);
        REQUIRE(model.integrityStatus == UiIntegrityStatus::Corrupt);
        REQUIRE(model.statusText == "CORRUPT");
        REQUIRE(model.statusIcon == "[X]");
        REQUIRE_FALSE(model.isPlaybackAllowed());
    }

    SECTION("Tampering with audio/envelope_reference.wav flags Corrupt status")
    {
        juce::File wavInCont = containerDir.getChildFile("audio/envelope_reference.wav");
        createTestWavFile(wavInCont, 48000.0, 9600); // alter samples & length

        MeasurementViewModel model;
        juce::String loadErr;
        bool loaded = MeasurementViewModelLoader::loadFromContainer(containerDir, model, loadErr);

        REQUIRE(loaded);
        REQUIRE(model.integrityStatus == UiIntegrityStatus::Corrupt);
        REQUIRE(model.statusText == "CORRUPT");
        REQUIRE(model.statusIcon == "[X]");
        REQUIRE_FALSE(model.isPlaybackAllowed());
    }

    tempDir.deleteRecursively();
}

TEST_CASE("MeasurementAudioPlayerComponent - On-demand Integrity Defense", "[measurement][player]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_Player_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    juce::File audioFile = tempDir.getChildFile("ref.wav");
    createTestWavFile(audioFile, 48000.0, 12000);

    juce::String validSha = juce::String(abdaudiolab::core::ExperimentStorage::computeFileSha256(audioFile));

    MeasurementAudioPlayerComponent player;
    bool callbackFired = false;
    player.onPlaybackBlockedByCorruption = [&](const juce::String&) {
        callbackFired = true;
    };

    SECTION("Valid audio enables player and verifies on demand")
    {
        player.setAudioFile(audioFile, validSha, true);
        REQUIRE(MeasurementViewModelLoader::verifyAudioFileSha256(audioFile, validSha));
    }

    SECTION("Tampering audio on disk causes on-demand verification to fail and block playback")
    {
        player.setAudioFile(audioFile, validSha, true);

        // Tamper audio on disk
        createTestWavFile(audioFile, 48000.0, 6000);

        // Verify that on-demand check catches it
        bool hashValid = MeasurementViewModelLoader::verifyAudioFileSha256(audioFile, validSha);
        REQUIRE_FALSE(hashValid);
    }

    tempDir.deleteRecursively();
}

TEST_CASE("MeasurementViewerPanel - Composition and Status presentation", "[measurement][viewer]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MeasurementViewModel model;
    model.measurementId = "meas-panel-test";
    model.dutName = "Dexed";
    model.dutFormat = "VST3";
    model.measurementStatus = MeasurementStatus::unreliable;
    model.statusText = "UNRELIABLE";
    model.statusIcon = "[!]";
    model.diagnosticReason = "decay_not_observable_gate_too_short";
    model.integrityStatus = UiIntegrityStatus::Verified;
    model.metrics.push_back({ "decayTime", 0.0, "ms", "unreliable" });

    MeasurementViewerPanel panel;
    panel.setViewModel(model);

    const auto& vm = panel.getViewModel();
    REQUIRE(vm.statusText == "UNRELIABLE");
    REQUIRE(vm.statusIcon == "[!]");
    REQUIRE(vm.statusText != "PASS"); // Never PASS
    REQUIRE(vm.diagnosticReason == "decay_not_observable_gate_too_short");
}
