/**
 * @file MeasurementContainerExporter.cpp
 * @brief Implementation of MeasurementContainerExporter.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementContainerExporter.h"
#include "MeasurementSerialization.h"
#include "MeasurementSvgGenerator.h"
#include "MeasurementReportGenerator.h"
#include "../synth/Sha256.h"
#include "../core/ExperimentRecord.h"
#include "../core/ExperimentStorage.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace abdaudiolab::measurement
{

using ordered_json = nlohmann::ordered_json;

// ==============================================================================
// Delegation to MeasurementSvgGenerator
// ==============================================================================

std::string MeasurementContainerExporter::generateTemporalCurveSvg(const std::vector<double>& timeMs,
                                                                   const std::vector<double>& amplitudeDbfs,
                                                                   int width,
                                                                   int height)
{
    return MeasurementSvgGenerator::generateTemporalCurveSvg(timeMs, amplitudeDbfs, width, height);
}

std::string MeasurementContainerExporter::generateFilterCurveSvg(const std::vector<double>& frequenciesHz,
                                                                 const std::vector<double>& magnitudesDb,
                                                                 const std::optional<SlopeFitMetadata>& slopeFit,
                                                                 double cutoffHz,
                                                                 int width,
                                                                 int height)
{
    return MeasurementSvgGenerator::generateFilterCurveSvg(frequenciesHz, magnitudesDb, slopeFit, cutoffHz, width, height);
}

// ==============================================================================
// Delegation to MeasurementReportGenerator
// ==============================================================================

std::string MeasurementContainerExporter::generateReportHtml(const MeasurementSpec& spec,
                                                             const MeasurementResult& result,
                                                             const std::string& relativeAudioPath)
{
    return MeasurementReportGenerator::generateReportHtml(spec, result, relativeAudioPath);
}

std::string MeasurementContainerExporter::generateFilterReportHtml(const MeasurementSpec& spec,
                                                                   const MeasurementResult& result,
                                                                   const std::string& relCapturedAudio,
                                                                   const std::string& relStimulusAudio,
                                                                   const std::string& relImpulseResponse)
{
    return MeasurementReportGenerator::generateFilterReportHtml(spec, result, relCapturedAudio, relStimulusAudio, relImpulseResponse);
}

// ==============================================================================
// Audio File Writing
// ==============================================================================

bool MeasurementContainerExporter::writeWavFile(const juce::File& file,
                                                const std::vector<float>& samples,
                                                double sampleRateHz,
                                                int numChannels)
{
    if (file.exists())
        file.deleteFile();

    file.getParentDirectory().createDirectory();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
        new juce::FileOutputStream(file),
        sampleRateHz,
        static_cast<unsigned int>(std::max(1, numChannels)),
        16,
        {},
        0));

    if (writer == nullptr)
        return false;

    if (samples.empty())
        return true;

    int totalSamples = static_cast<int>(samples.size());
    juce::AudioBuffer<float> buf(numChannels, totalSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < totalSamples; ++i)
            buf.setSample(ch, i, samples[static_cast<size_t>(i)]);
    }

    return writer->writeFromAudioSampleBuffer(buf, 0, totalSamples);
}

// ==============================================================================
// Internal Container Packaging Helpers
// ==============================================================================

namespace
{

void writeExperimentJson(const juce::File& containerDir,
                         const MeasurementSpec& spec,
                         double sampleRateHz,
                         double durationSeconds,
                         const std::string& stimulusSha256)
{
    juce::File expJsonFile = containerDir.getChildFile("experiment.json");
    ordered_json expJson;
    expJson["schemaVersion"] = 1;
    expJson["experimentId"] = spec.measurementId;
    expJson["revision"] = 1;
    expJson["kind"] = "Measurement";
    expJson["status"] = "AuditedApproved";

    ordered_json capJson;
    capJson["sampleRate"] = (sampleRateHz > 0) ? sampleRateHz : spec.execution.sampleRateHz;
    capJson["hostBufferSize"] = spec.execution.blockSize;
    capJson["processingBlockSize"] = spec.execution.blockSize;
    capJson["channels"] = spec.execution.numChannels;
    capJson["durationSeconds"] = durationSeconds;
    capJson["presetStateHash"] = "";
    capJson["excitationPlanHash"] = stimulusSha256.empty() ? spec.stimulus.sha256 : stimulusSha256;
    capJson["storageProfile"] = "Standard";
    expJson["capture"] = capJson;

    expJsonFile.replaceWithText(juce::String::fromUTF8(expJson.dump(2).c_str()));
}

core::ExperimentArtifact registerAudioArtifact(const juce::File& file,
                                              const std::string& relPath,
                                              const std::string& role)
{
    core::ExperimentArtifact art;
    art.relativePath = relPath;
    art.role = role;
    art.sizeBytes = static_cast<uint64_t>(file.getSize());
    art.sha256 = core::ExperimentStorage::computeFileSha256(file);

    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
    if (reader != nullptr)
    {
        core::AudioArtifactMetadata meta;
        meta.sampleRate = reader->sampleRate;
        meta.channels = static_cast<int>(reader->numChannels);
        meta.bitsPerSample = static_cast<int>(reader->bitsPerSample);
        meta.sampleCount = reader->lengthInSamples;
        meta.durationSeconds = (reader->sampleRate > 0) ? (static_cast<double>(reader->lengthInSamples) / reader->sampleRate) : 0.0;
        meta.format = "WAV_PCM";
        meta.isInterleaved = true;
        art.audio = meta;
    }
    return art;
}

void writeManifestJson(const juce::File& containerDir,
                       const MeasurementSpec& spec,
                       const std::vector<core::ExperimentArtifact>& artifacts)
{
    ordered_json manifestJson;
    manifestJson["manifestFormat"] = "artifact-list-v1";
    manifestJson["experimentId"] = spec.measurementId;
    manifestJson["experimentKind"] = "Measurement";
    manifestJson["targetName"] = spec.parameterName.empty() ? spec.measurementId : spec.parameterName;
    manifestJson["operatorMode"] = "AUTOMATIC_PLUGIN";

    ordered_json artsArray = ordered_json::array();
    for (const auto& art : artifacts)
    {
        ordered_json a;
        a["path"] = art.relativePath;
        a["role"] = art.role;
        a["sizeBytes"] = art.sizeBytes;
        a["sha256"] = art.sha256;

        if (art.audio.has_value())
        {
            ordered_json audioObj;
            audioObj["sampleRate"] = art.audio->sampleRate;
            audioObj["channels"] = art.audio->channels;
            audioObj["bitsPerSample"] = art.audio->bitsPerSample;
            audioObj["sampleCount"] = art.audio->sampleCount;
            audioObj["durationSeconds"] = art.audio->durationSeconds;
            audioObj["format"] = art.audio->format;
            audioObj["isInterleaved"] = art.audio->isInterleaved;
            a["audio"] = audioObj;
        }

        artsArray.push_back(a);
    }
    manifestJson["artifacts"] = artsArray;

    juce::File manifestFile = containerDir.getChildFile("manifest.json");
    manifestFile.replaceWithText(juce::String::fromUTF8(manifestJson.dump(2).c_str()));
}

} // namespace

// ==============================================================================
// FAIR Container Exporters
// ==============================================================================

bool MeasurementContainerExporter::exportMeasurement(const juce::File& containerDir,
                                                     const MeasurementSpec& spec,
                                                     const MeasurementResult& result,
                                                     const juce::File& sourceAudioWav,
                                                     juce::String& outError)
{
    if (containerDir.existsAsFile())
    {
        outError = "Target container path is an existing file, directory required";
        return false;
    }

    if (!containerDir.isDirectory() && !containerDir.createDirectory())
    {
        outError = "Failed to create container directory: " + containerDir.getFullPathName();
        return false;
    }

    juce::File specsDir = containerDir.getChildFile("specs");
    juce::File resultsDir = containerDir.getChildFile("results");
    juce::File curvesDir = containerDir.getChildFile("curves");
    juce::File audioDir = containerDir.getChildFile("audio");
    juce::File reportsDir = containerDir.getChildFile("reports");

    specsDir.createDirectory();
    resultsDir.createDirectory();
    curvesDir.createDirectory();
    audioDir.createDirectory();
    reportsDir.createDirectory();

    // 0. Write experiment.json
    writeExperimentJson(containerDir, spec, spec.execution.sampleRateHz, spec.stimulus.durationSec, spec.stimulus.sha256);

    std::vector<core::ExperimentArtifact> artifacts;

    // 1. Write specs/measurement_spec.json
    juce::File specFile = specsDir.getChildFile("measurement_spec.json");
    std::string specJson = MeasurementSerialization::serializeSpec(spec);
    specFile.replaceWithText(juce::String::fromUTF8(specJson.c_str()));

    core::ExperimentArtifact artSpec;
    artSpec.relativePath = "specs/measurement_spec.json";
    artSpec.role = "measurement_spec";
    artSpec.sizeBytes = static_cast<uint64_t>(specFile.getSize());
    artSpec.sha256 = core::ExperimentStorage::computeFileSha256(specFile);
    artifacts.push_back(artSpec);

    // 2. Write specs/measurement_stimulus.json
    juce::File stimFile = specsDir.getChildFile("measurement_stimulus.json");
    std::string stimJson = MeasurementSerialization::serializeStimulus(spec.stimulus);
    stimFile.replaceWithText(juce::String::fromUTF8(stimJson.c_str()));

    core::ExperimentArtifact artStim;
    artStim.relativePath = "specs/measurement_stimulus.json";
    artStim.role = "measurement_stimulus";
    artStim.sizeBytes = static_cast<uint64_t>(stimFile.getSize());
    artStim.sha256 = core::ExperimentStorage::computeFileSha256(stimFile);
    artifacts.push_back(artStim);

    // 3. Write curves/envelope_curve.json
    juce::File curveFile = curvesDir.getChildFile("envelope_curve.json");
    ordered_json curveJson;
    curveJson["xName"] = result.curve.xName.toStdString();
    curveJson["xUnit"] = result.curve.xUnit.toStdString();
    curveJson["yName"] = result.curve.yName.toStdString();
    curveJson["yUnit"] = result.curve.yUnit.toStdString();
    curveJson["sampleCount"] = result.curve.x.size();
    curveJson["x"] = result.curve.x;
    curveJson["y"] = result.curve.y;
    std::string curveStr = curveJson.dump(2);
    curveFile.replaceWithText(juce::String::fromUTF8(curveStr.c_str()));

    core::ExperimentArtifact artCurve;
    artCurve.relativePath = "curves/envelope_curve.json";
    artCurve.role = "envelope_curve";
    artCurve.sizeBytes = static_cast<uint64_t>(curveFile.getSize());
    artCurve.sha256 = core::ExperimentStorage::computeFileSha256(curveFile);
    artifacts.push_back(artCurve);

    // 4. Audio handling: copy WAV if available
    std::string relAudioPathForHtml;
    if (sourceAudioWav.existsAsFile())
    {
        juce::File destAudio = audioDir.getChildFile("envelope_reference.wav");
        if (!sourceAudioWav.copyFileTo(destAudio))
        {
            outError = "Failed to copy audio artifact to " + destAudio.getFullPathName();
            return false;
        }

        auto artAudio = registerAudioArtifact(destAudio, "audio/envelope_reference.wav", "measurement_baseline_audio");
        artifacts.push_back(artAudio);
        relAudioPathForHtml = "../audio/envelope_reference.wav";
    }

    // 5. Write results/measurement_result.json (updating artifact refs)
    MeasurementResult updatedResult = result;
    if (!artifacts.empty())
    {
        for (const auto& a : artifacts)
        {
            if (a.role == "measurement_baseline_audio")
            {
                updatedResult.artifacts.audioPath = a.relativePath;
                updatedResult.artifacts.audioSha256 = a.sha256;
                updatedResult.integrityVerified = true;
                break;
            }
        }
    }

    juce::File resultFile = resultsDir.getChildFile("measurement_result.json");
    std::string resultJson = MeasurementSerialization::serializeResult(updatedResult);
    resultFile.replaceWithText(juce::String::fromUTF8(resultJson.c_str()));

    core::ExperimentArtifact artResult;
    artResult.relativePath = "results/measurement_result.json";
    artResult.role = "measurement_result";
    artResult.sizeBytes = static_cast<uint64_t>(resultFile.getSize());
    artResult.sha256 = core::ExperimentStorage::computeFileSha256(resultFile);
    artifacts.push_back(artResult);

    // 6. Write reports/measurement_report.html
    juce::File reportFile = reportsDir.getChildFile("measurement_report.html");
    std::string htmlContent = generateReportHtml(spec, updatedResult, relAudioPathForHtml);
    reportFile.replaceWithText(juce::String::fromUTF8(htmlContent.c_str()));

    core::ExperimentArtifact artReport;
    artReport.relativePath = "reports/measurement_report.html";
    artReport.role = "measurement_report";
    artReport.sizeBytes = static_cast<uint64_t>(reportFile.getSize());
    artReport.sha256 = core::ExperimentStorage::computeFileSha256(reportFile);
    artifacts.push_back(artReport);

    // 7. Write manifest.json with all computed hashes
    writeManifestJson(containerDir, spec, artifacts);

    // 8. Verify cryptographic integrity with ExperimentFolderReader
    core::ExperimentFolderReader reader;
    auto readResult = reader.read(containerDir, outError);
    if (!readResult.has_value() || readResult->status == core::ExperimentStatus::Corrupt)
    {
        outError = "Verification of generated manifest failed: " + outError;
        return false;
    }

    return true;
}

bool MeasurementContainerExporter::exportFilterMeasurement(const juce::File& containerDir,
                                                           const MeasurementSpec& spec,
                                                           const MeasurementResult& result,
                                                           const FilterExportArtifacts& artifacts,
                                                           juce::String& outError)
{
    if (containerDir.existsAsFile())
    {
        outError = "Target container path is an existing file, directory required";
        return false;
    }

    if (!containerDir.isDirectory() && !containerDir.createDirectory())
    {
        outError = "Failed to create container directory: " + containerDir.getFullPathName();
        return false;
    }

    juce::File specsDir = containerDir.getChildFile("specs");
    juce::File resultsDir = containerDir.getChildFile("results");
    juce::File curvesDir = containerDir.getChildFile("curves");
    juce::File audioDir = containerDir.getChildFile("audio");
    juce::File reportsDir = containerDir.getChildFile("reports");

    specsDir.createDirectory();
    resultsDir.createDirectory();
    curvesDir.createDirectory();
    audioDir.createDirectory();
    reportsDir.createDirectory();

    double sr = (artifacts.sampleRateHz > 0) ? artifacts.sampleRateHz : spec.execution.sampleRateHz;

    // 0. Write experiment.json
    writeExperimentJson(containerDir, spec, sr, spec.stimulus.durationSec, spec.stimulus.sha256);

    std::vector<core::ExperimentArtifact> manifestArtifacts;

    // 1. Write specs/measurement_spec.json
    juce::File specFile = specsDir.getChildFile("measurement_spec.json");
    std::string specJson = MeasurementSerialization::serializeSpec(spec);
    specFile.replaceWithText(juce::String::fromUTF8(specJson.c_str()));

    core::ExperimentArtifact artSpec;
    artSpec.relativePath = "specs/measurement_spec.json";
    artSpec.role = "measurement_spec";
    artSpec.sizeBytes = static_cast<uint64_t>(specFile.getSize());
    artSpec.sha256 = core::ExperimentStorage::computeFileSha256(specFile);
    manifestArtifacts.push_back(artSpec);

    // 2. Write specs/measurement_stimulus.json
    juce::File stimFile = specsDir.getChildFile("measurement_stimulus.json");
    std::string stimJson = MeasurementSerialization::serializeStimulus(spec.stimulus);
    stimFile.replaceWithText(juce::String::fromUTF8(stimJson.c_str()));

    core::ExperimentArtifact artStim;
    artStim.relativePath = "specs/measurement_stimulus.json";
    artStim.role = "measurement_stimulus";
    artStim.sizeBytes = static_cast<uint64_t>(stimFile.getSize());
    artStim.sha256 = core::ExperimentStorage::computeFileSha256(stimFile);
    manifestArtifacts.push_back(artStim);

    // 3. Write curves/filter_response_curve.json
    juce::File curveFile = curvesDir.getChildFile("filter_response_curve.json");
    ordered_json curveJson;
    curveJson["xName"] = result.curve.xName.isEmpty() ? "frequency" : result.curve.xName.toStdString();
    curveJson["xUnit"] = result.curve.xUnit.isEmpty() ? "Hz" : result.curve.xUnit.toStdString();
    curveJson["yName"] = result.curve.yName.isEmpty() ? "magnitude" : result.curve.yName.toStdString();
    curveJson["yUnit"] = result.curve.yUnit.isEmpty() ? "dB" : result.curve.yUnit.toStdString();
    curveJson["sampleCount"] = result.curve.x.size();
    curveJson["x"] = result.curve.x;
    curveJson["y"] = result.curve.y;
    std::string curveStr = curveJson.dump(2);
    curveFile.replaceWithText(juce::String::fromUTF8(curveStr.c_str()));

    core::ExperimentArtifact artCurve;
    artCurve.relativePath = "curves/filter_response_curve.json";
    artCurve.role = "filter_response_curve";
    artCurve.sizeBytes = static_cast<uint64_t>(curveFile.getSize());
    artCurve.sha256 = core::ExperimentStorage::computeFileSha256(curveFile);
    manifestArtifacts.push_back(artCurve);

    // 4. Audio handling
    std::string capturedSha;
    std::string relCapturedAudioForHtml;
    std::string relStimulusAudioForHtml;
    std::string relImpulseResponseForHtml;

    // 4a. Captured Audio: audio/audio_captured.wav
    juce::File destCaptured = audioDir.getChildFile("audio_captured.wav");
    bool hasCaptured = false;
    if (!artifacts.capturedAudio.empty())
    {
        if (!writeWavFile(destCaptured, artifacts.capturedAudio, sr, 1))
        {
            outError = "Failed to write captured audio WAV: " + destCaptured.getFullPathName();
            return false;
        }
        hasCaptured = true;
    }
    else if (artifacts.capturedAudioWav.existsAsFile())
    {
        if (!artifacts.capturedAudioWav.copyFileTo(destCaptured))
        {
            outError = "Failed to copy captured audio to " + destCaptured.getFullPathName();
            return false;
        }
        hasCaptured = true;
    }

    if (hasCaptured)
    {
        auto artCaptured = registerAudioArtifact(destCaptured, "audio/audio_captured.wav", "measurement_captured_audio");
        capturedSha = artCaptured.sha256;
        manifestArtifacts.push_back(artCaptured);
        relCapturedAudioForHtml = "../audio/audio_captured.wav";
    }

    // 4b. Stimulus Audio: audio/audio_stimulus.wav
    juce::File destStim = audioDir.getChildFile("audio_stimulus.wav");
    bool hasStim = false;
    if (!artifacts.stimulusAudio.empty())
    {
        if (!writeWavFile(destStim, artifacts.stimulusAudio, sr, 1))
        {
            outError = "Failed to write stimulus audio WAV: " + destStim.getFullPathName();
            return false;
        }
        hasStim = true;
    }
    else if (artifacts.stimulusAudioWav.existsAsFile())
    {
        if (!artifacts.stimulusAudioWav.copyFileTo(destStim))
        {
            outError = "Failed to copy stimulus audio to " + destStim.getFullPathName();
            return false;
        }
        hasStim = true;
    }

    if (hasStim)
    {
        auto artStimAudio = registerAudioArtifact(destStim, "audio/audio_stimulus.wav", "measurement_stimulus_audio");
        manifestArtifacts.push_back(artStimAudio);
        relStimulusAudioForHtml = "../audio/audio_stimulus.wav";
    }

    // 4c. Impulse Response: audio/impulse_response.wav
    juce::File destIr = audioDir.getChildFile("impulse_response.wav");
    bool hasIr = false;
    if (!artifacts.impulseResponse.empty())
    {
        if (!writeWavFile(destIr, artifacts.impulseResponse, sr, 1))
        {
            outError = "Failed to write impulse response WAV: " + destIr.getFullPathName();
            return false;
        }
        hasIr = true;
    }
    else if (artifacts.impulseResponseWav.existsAsFile())
    {
        if (!artifacts.impulseResponseWav.copyFileTo(destIr))
        {
            outError = "Failed to copy impulse response to " + destIr.getFullPathName();
            return false;
        }
        hasIr = true;
    }

    if (hasIr)
    {
        auto artIr = registerAudioArtifact(destIr, "audio/impulse_response.wav", "measurement_impulse_response");
        manifestArtifacts.push_back(artIr);
        relImpulseResponseForHtml = "../audio/impulse_response.wav";
    }

    // 5. Results: results/measurement_result.json
    MeasurementResult updatedResult = result;
    if (!updatedResult.curve.x.empty())
    {
        if (updatedResult.curve.xName.trim().isEmpty()) updatedResult.curve.xName = "frequency";
        if (updatedResult.curve.xUnit.trim().isEmpty()) updatedResult.curve.xUnit = "Hz";
        if (updatedResult.curve.yName.trim().isEmpty()) updatedResult.curve.yName = "magnitude";
        if (updatedResult.curve.yUnit.trim().isEmpty()) updatedResult.curve.yUnit = "dB";
    }
    if (hasCaptured)
    {
        updatedResult.artifacts.audioPath = "audio/audio_captured.wav";
        updatedResult.artifacts.audioSha256 = capturedSha;
        updatedResult.integrityVerified = true;
    }

    juce::File resultFile = resultsDir.getChildFile("measurement_result.json");
    std::string resultJson = MeasurementSerialization::serializeResult(updatedResult);
    resultFile.replaceWithText(juce::String::fromUTF8(resultJson.c_str()));

    core::ExperimentArtifact artResult;
    artResult.relativePath = "results/measurement_result.json";
    artResult.role = "measurement_result";
    artResult.sizeBytes = static_cast<uint64_t>(resultFile.getSize());
    artResult.sha256 = core::ExperimentStorage::computeFileSha256(resultFile);
    manifestArtifacts.push_back(artResult);

    // 6. Reports: reports/measurement_report.html
    juce::File reportFile = reportsDir.getChildFile("measurement_report.html");
    std::string htmlContent = generateFilterReportHtml(spec, updatedResult,
                                                       relCapturedAudioForHtml,
                                                       relStimulusAudioForHtml,
                                                       relImpulseResponseForHtml);
    reportFile.replaceWithText(juce::String::fromUTF8(htmlContent.c_str()));

    core::ExperimentArtifact artReport;
    artReport.relativePath = "reports/measurement_report.html";
    artReport.role = "measurement_report";
    artReport.sizeBytes = static_cast<uint64_t>(reportFile.getSize());
    artReport.sha256 = core::ExperimentStorage::computeFileSha256(reportFile);
    manifestArtifacts.push_back(artReport);

    // 7. Write manifest.json
    writeManifestJson(containerDir, spec, manifestArtifacts);

    // 8. Cryptographic fixity verification with ExperimentFolderReader
    core::ExperimentFolderReader reader;
    auto readResult = reader.read(containerDir, outError);
    if (!readResult.has_value() || readResult->status == core::ExperimentStatus::Corrupt)
    {
        outError = "Verification of generated filter manifest failed: " + outError;
        return false;
    }

    return true;
}

} // namespace abdaudiolab::measurement
