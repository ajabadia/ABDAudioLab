#include <catch2/catch_test_macros.hpp>
#include "audio/LabStimulusGenerator.h"
#include "export/NamDatasetExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include <fstream>

TEST_CASE("NAM Calibration Stimulus Generation", "[audio][nam]")
{
    using namespace abdaudiolab::audio;

    double sampleRate = 48000.0;
    double duration = 1.0; // 1 second test slice
    auto buffer = LabStimulusGenerator::generateNamCalibrationBuffer(sampleRate, duration);

    REQUIRE(buffer.getNumChannels() == 1);
    REQUIRE(buffer.getNumSamples() == 48000);

    float peak = buffer.getMagnitude(0, buffer.getNumSamples());
    float rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());

    REQUIRE(peak > 0.1f);
    REQUIRE(peak <= 1.0f);
    REQUIRE(rms > 0.01f);

    // Ensure no NaNs or Infs
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float s = buffer.getSample(0, i);
        REQUIRE(!std::isnan(s));
        REQUIRE(!std::isinf(s));
    }
}

TEST_CASE("NAM Dataset Cross-Correlation Latency Alignment", "[export][nam]")
{
    using namespace abdaudiolab::audio;
    using namespace abdaudiolab::exporting;

    double sampleRate = 48000.0;
    auto input = LabStimulusGenerator::generateNamCalibrationBuffer(sampleRate, 1.0);

    // Simulate hardware delay of 45 samples with slight saturation and attenuation
    int simulatedDelay = 45;
    juce::AudioBuffer<float> recorded(1, input.getNumSamples() + simulatedDelay);
    recorded.clear();

    for (int i = 0; i < input.getNumSamples(); ++i)
    {
        float inSample = input.getSample(0, i);
        // Soft clipping non-linearity simulation
        float outSample = std::tanh(inSample * 1.2f) * 0.9f;
        recorded.setSample(0, i + simulatedDelay, outSample);
    }

    int detectedLag = NamDatasetExporter::findLatencyOffsetSamples(input.getReadPointer(0),
                                                                 recorded.getReadPointer(0),
                                                                 static_cast<int>(0.3 * sampleRate),
                                                                 static_cast<int>(0.05 * sampleRate));

    REQUIRE(detectedLag == simulatedDelay);
}

TEST_CASE("NAM Dataset Export Files and Manifest", "[export][nam]")
{
    using namespace abdaudiolab::audio;
    using namespace abdaudiolab::exporting;

    double sampleRate = 48000.0;
    auto input = LabStimulusGenerator::generateNamCalibrationBuffer(sampleRate, 0.5);

    int simulatedDelay = 20;
    juce::AudioBuffer<float> recorded(1, input.getNumSamples() + simulatedDelay);
    recorded.clear();

    for (int i = 0; i < input.getNumSamples(); ++i)
    {
        recorded.setSample(0, i + simulatedDelay, input.getSample(0, i) * 0.8f);
    }

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("abdaudiolab_test_nam_dataset");
    tempDir.deleteRecursively();

    NamDatasetManifest manifest;
    manifest.hardwareId = "roland_aira_bitrazer";
    manifest.hardwareDisplayName = "Roland AIRA Bitrazer";
    manifest.functionId = "bitcrush_main";
    manifest.sampleRate = sampleRate;
    manifest.bitDepth = 24;
    manifest.controlPositions["BitDepth"] = 0.5f;
    manifest.controlPositions["SampleRate"] = 0.75f;

    bool success = NamDatasetExporter::exportDataset(tempDir, input, recorded, sampleRate, manifest);
    REQUIRE(success);

    auto inputFile = tempDir.getChildFile("input.wav");
    auto targetFile = tempDir.getChildFile("target.wav");
    auto manifestFile = tempDir.getChildFile("nam_dataset_manifest.json");

    REQUIRE(inputFile.existsAsFile());
    REQUIRE(targetFile.existsAsFile());
    REQUIRE(manifestFile.existsAsFile());

    // Verify WAV files using JUCE AudioFormatManager
    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> inReader(formatMgr.createReaderFor(inputFile));
    std::unique_ptr<juce::AudioFormatReader> tgtReader(formatMgr.createReaderFor(targetFile));

    REQUIRE(inReader != nullptr);
    REQUIRE(tgtReader != nullptr);
    REQUIRE(inReader->lengthInSamples == tgtReader->lengthInSamples);
    REQUIRE(inReader->sampleRate == sampleRate);
    REQUIRE(tgtReader->sampleRate == sampleRate);

    // Verify JSON manifest content
    std::ifstream ifs(manifestFile.getFullPathName().toStdString());
    REQUIRE(ifs.is_open());
    nlohmann::json j;
    ifs >> j;
    REQUIRE(j["hardwareId"] == "roland_aira_bitrazer");
    REQUIRE(j["latencyOffsetSamples"] == simulatedDelay);
    REQUIRE(j["controlPositions"]["BitDepth"] == 0.5f);

    tempDir.deleteRecursively();
}
