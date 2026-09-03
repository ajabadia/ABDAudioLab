/**
 * @file NamDatasetExporter.cpp
 * @brief Implementation of NAM / RTNeural training dataset alignment and export.
 * @author ABDSynths
 * @date 2026
 */

#include "NamDatasetExporter.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::exporting
{

int NamDatasetExporter::findLatencyOffsetSamples(const float* input,
                                                const float* recorded,
                                                int numSamplesToSearch,
                                                int maxLagSamples)
{
    if (input == nullptr || recorded == nullptr || numSamplesToSearch <= 0 || maxLagSamples <= 0)
        return 0;

    double maxCorr = -1.0;
    int bestLag = 0;

    // Cross-correlation over search window
    for (int lag = 0; lag < maxLagSamples; ++lag)
    {
        double sumProd = 0.0;
        double sumInSq = 0.0;
        double sumRecSq = 0.0;

        for (int i = 0; i < numSamplesToSearch; ++i)
        {
            float inVal = input[i];
            float recVal = recorded[i + lag];

            sumProd += inVal * recVal;
            sumInSq += inVal * inVal;
            sumRecSq += recVal * recVal;
        }

        double denom = std::sqrt(sumInSq * sumRecSq);
        double corr = (denom > 1e-12) ? (sumProd / denom) : 0.0;

        if (corr > maxCorr)
        {
            maxCorr = corr;
            bestLag = lag;
        }
    }

    return bestLag;
}

bool NamDatasetExporter::exportDataset(const juce::File& targetDirectory,
                                      const juce::AudioBuffer<float>& inputBuffer,
                                      const juce::AudioBuffer<float>& recordedBuffer,
                                      double sampleRate,
                                      NamDatasetManifest manifest)
{
    if (!targetDirectory.exists())
    {
        auto res = targetDirectory.createDirectory();
        if (res.failed())
            return false;
    }

    int inTotal = inputBuffer.getNumSamples();
    int recTotal = recordedBuffer.getNumSamples();

    if (inTotal <= 0 || recTotal <= 0)
        return false;

    // 1. Compute pre-roll latency search window (~0.3s)
    int searchWindow = static_cast<int>(std::min(0.3 * sampleRate, static_cast<double>(inTotal)));
    int maxLag = static_cast<int>(std::min(0.08 * sampleRate, static_cast<double>(recTotal - searchWindow)));
    if (maxLag <= 0) maxLag = 1;

    int latency = findLatencyOffsetSamples(inputBuffer.getReadPointer(0),
                                          recordedBuffer.getReadPointer(0),
                                          searchWindow,
                                          maxLag);

    manifest.latencyOffsetSamples = latency;

    // 2. Prepare time-aligned buffers
    int alignedSamples = std::min(inTotal, recTotal - latency);
    if (alignedSamples <= 0)
        return false;

    juce::AudioBuffer<float> alignedInput(1, alignedSamples);
    alignedInput.copyFrom(0, 0, inputBuffer, 0, 0, alignedSamples);

    juce::AudioBuffer<float> alignedTarget(1, alignedSamples);
    for (int i = 0; i < alignedSamples; ++i)
    {
        int srcIdx = i + latency;
        float sample = (srcIdx >= 0 && srcIdx < recTotal) ? recordedBuffer.getSample(0, srcIdx) : 0.0f;
        alignedTarget.setSample(0, i, sample);
    }

    float inMag = alignedInput.getMagnitude(0, alignedSamples);
    float tgtMag = alignedTarget.getMagnitude(0, alignedSamples);

    manifest.inputPeakDb = (inMag > 1e-7f) ? (20.0f * std::log10(inMag)) : -120.0f;
    manifest.targetPeakDb = (tgtMag > 1e-7f) ? (20.0f * std::log10(tgtMag)) : -120.0f;
    manifest.totalSamples = alignedSamples;
    manifest.sampleRate = sampleRate;

    if (manifest.timestampUtc.empty())
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        manifest.timestampUtc = ss.str();
    }

    // 3. Write input.wav
    juce::WavAudioFormat wavFormat;
    auto inputFile = targetDirectory.getChildFile("input.wav");
    inputFile.deleteFile();
    if (auto outStream = std::unique_ptr<juce::FileOutputStream>(inputFile.createOutputStream()))
    {
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream.get(),
                                                                                  sampleRate,
                                                                                  1,
                                                                                  manifest.bitDepth,
                                                                                  {},
                                                                                  0));
        if (writer != nullptr)
        {
            outStream.release(); // Writer took ownership
            writer->writeFromAudioSampleBuffer(alignedInput, 0, alignedSamples);
        }
        else
        {
            return false;
        }
    }

    // 4. Write target.wav
    auto targetFile = targetDirectory.getChildFile("target.wav");
    targetFile.deleteFile();
    if (auto outStream = std::unique_ptr<juce::FileOutputStream>(targetFile.createOutputStream()))
    {
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream.get(),
                                                                                  sampleRate,
                                                                                  1,
                                                                                  manifest.bitDepth,
                                                                                  {},
                                                                                  0));
        if (writer != nullptr)
        {
            outStream.release();
            writer->writeFromAudioSampleBuffer(alignedTarget, 0, alignedSamples);
        }
        else
        {
            return false;
        }
    }

    // 5. Write JSON manifest
    nlohmann::json j;
    j["datasetSchemaVersion"] = "1.0";
    j["hardwareId"] = manifest.hardwareId;
    j["hardwareDisplayName"] = manifest.hardwareDisplayName;
    j["functionId"] = manifest.functionId;
    j["captureMode"] = manifest.captureMode;
    j["timestampUtc"] = manifest.timestampUtc;
    j["sampleRate"] = manifest.sampleRate;
    j["bitDepth"] = manifest.bitDepth;
    j["totalSamples"] = manifest.totalSamples;
    j["latencyOffsetSamples"] = manifest.latencyOffsetSamples;
    j["inputPeakDb"] = manifest.inputPeakDb;
    j["targetPeakDb"] = manifest.targetPeakDb;
    j["estimatedThdPercent"] = manifest.estimatedThdPercent;

    j["controlPositions"] = nlohmann::json::object();
    for (const auto& [k, v] : manifest.controlPositions)
    {
        j["controlPositions"][k] = v;
    }

    auto manifestFile = targetDirectory.getChildFile("nam_dataset_manifest.json");
    std::ofstream ofs(manifestFile.getFullPathName().toStdString());
    if (!ofs.is_open())
        return false;

    ofs << j.dump(2);
    ofs.close();

    return true;
}

} // namespace abdaudiolab::exporting
