/**
 * @file MeasurementContainerExporter.cpp
 * @brief Implementation of MeasurementContainerExporter.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementContainerExporter.h"
#include "MeasurementSerialization.h"
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

std::string MeasurementContainerExporter::generateTemporalCurveSvg(const std::vector<double>& timeMs,
                                                                   const std::vector<double>& amplitudeDbfs,
                                                                   int width,
                                                                   int height)
{
    if (timeMs.empty() || amplitudeDbfs.empty() || timeMs.size() != amplitudeDbfs.size())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No curve data available</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    double minTime = timeMs.front();
    double maxTime = timeMs.back();
    if (std::abs(maxTime - minTime) < 1e-4)
        maxTime = minTime + 100.0;

    const double minDb = -96.0;
    const double maxDb = 0.0;

    const float padLeft = 60.0f;
    const float padRight = 20.0f;
    const float padTop = 20.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <defs>\n"
       << "    <linearGradient id=\"envGrad\" x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\">\n"
       << "      <stop offset=\"0%\" stop-color=\"#0284c7\" stop-opacity=\"0.35\"/>\n"
       << "      <stop offset=\"100%\" stop-color=\"#0284c7\" stop-opacity=\"0.0\"/>\n"
       << "    </linearGradient>\n"
       << "  </defs>\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Grid lines (dBFS)
    double dbSteps[] = { 0.0, -20.0, -40.0, -60.0, -80.0, -96.0 };
    for (double db : dbSteps)
    {
        float y = padTop + static_cast<float>((maxDb - db) / (maxDb - minDb)) * plotH;
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(db) << " dB</text>\n";
    }

    // Grid lines (time ms)
    int numTimeGrid = 5;
    for (int i = 0; i <= numTimeGrid; ++i)
    {
        double t = minTime + (maxTime - minTime) * (static_cast<double>(i) / numTimeGrid);
        float x = padLeft + static_cast<float>(static_cast<double>(i) / numTimeGrid) * plotW;
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << static_cast<int>(std::round(t)) << " ms</text>\n";
    }

    // Build curve path
    std::ostringstream pathD;
    std::ostringstream areaD;

    for (size_t i = 0; i < timeMs.size(); ++i)
    {
        double t = std::clamp(timeMs[i], minTime, maxTime);
        double db = std::clamp(amplitudeDbfs[i], minDb, maxDb);

        float px = padLeft + static_cast<float>((t - minTime) / (maxTime - minTime)) * plotW;
        float py = padTop + static_cast<float>((maxDb - db) / (maxDb - minDb)) * plotH;

        if (i == 0)
        {
            pathD << "M " << px << " " << py;
            areaD << "M " << px << " " << (padTop + plotH) << " L " << px << " " << py;
        }
        else
        {
            pathD << " L " << px << " " << py;
            areaD << " L " << px << " " << py;
        }
    }

    if (!timeMs.empty())
    {
        double lastT = std::clamp(timeMs.back(), minTime, maxTime);
        float lastPx = padLeft + static_cast<float>((lastT - minTime) / (maxTime - minTime)) * plotW;
        areaD << " L " << lastPx << " " << (padTop + plotH) << " Z";

        ss << "  <path d=\"" << areaD.str() << "\" fill=\"url(#envGrad)\"/>\n";
        ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string MeasurementContainerExporter::generateReportHtml(const MeasurementSpec& spec,
                                                             const MeasurementResult& result,
                                                             const std::string& relativeAudioPath)
{
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n"
      << "  <meta charset=\"UTF-8\">\n"
      << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
      << "  <title>Reporte de Medición de Respuesta Acústica — " << spec.measurementId << "</title>\n"
      << "  <style>\n"
      << "    body { font-family: Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0b0f19; color: #f1f5f9; margin: 0; padding: 32px 24px; line-height: 1.5; }\n"
      << "    .container { max-width: 860px; margin: 0 auto; background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 32px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }\n"
      << "    h1 { font-size: 22px; font-weight: 700; color: #f8fafc; margin: 0 0 8px 0; }\n"
      << "    .subtitle { font-size: 13px; color: #94a3b8; margin-bottom: 24px; }\n"
      << "    .badge { display: inline-block; padding: 4px 10px; border-radius: 9999px; font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }\n"
      << "    .badge-completed { background: #065f46; color: #34d399; border: 1px solid #059669; }\n"
      << "    .badge-unreliable { background: #78350f; color: #fbbf24; border: 1px solid #d97706; }\n"
      << "    .badge-invalid { background: #7f1d1d; color: #f87171; border: 1px solid #dc2626; }\n"
      << "    .badge-failed { background: #450a0a; color: #fca5a5; border: 1px solid #991b1b; }\n"
      << "    .badge-skipped { background: #374151; color: #9ca3af; border: 1px solid #4b5563; }\n"
      << "    .badge-observed { background: #0c4a6e; color: #38bdf8; border: 1px solid #0284c7; }\n"
      << "    .alert { padding: 14px 18px; border-radius: 8px; margin: 18px 0; font-size: 13px; }\n"
      << "    .alert-warning { background: #1c1917; border-left: 4px solid #f59e0b; color: #fef3c7; }\n"
      << "    .alert-info { background: #082f49; border-left: 4px solid #0284c7; color: #e0f2fe; }\n"
      << "    .section-title { font-size: 14px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.75px; color: #94a3b8; margin: 28px 0 12px 0; border-bottom: 1px solid #1f2937; padding-bottom: 6px; }\n"
      << "    table { width: 100%; border-collapse: collapse; font-size: 13px; margin: 12px 0 24px 0; }\n"
      << "    th { text-align: left; background: #1e293b; color: #94a3b8; padding: 10px 14px; font-size: 11px; text-transform: uppercase; font-weight: 600; letter-spacing: 0.5px; border-bottom: 1px solid #334155; }\n"
      << "    td { padding: 10px 14px; border-bottom: 1px solid #1f2937; color: #cbd5e1; }\n"
      << "    tr:hover td { background: #1e293b; }\n"
      << "    code { font-family: 'JetBrains Mono', Consolas, monospace; font-size: 12px; background: #1e293b; padding: 2px 6px; border-radius: 4px; color: #38bdf8; }\n"
      << "    .meta-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin: 16px 0; }\n"
      << "    .meta-card { background: #1e293b; padding: 12px 16px; border-radius: 8px; border: 1px solid #334155; }\n"
      << "    .meta-label { font-size: 11px; text-transform: uppercase; color: #94a3b8; margin-bottom: 4px; }\n"
      << "  </style>\n</head>\n<body>\n<div class=\"container\">\n";

    // Header & Status
    std::string badgeClass = "badge-completed";
    std::string statusLabel = "COMPLETED";
    if (result.status == MeasurementStatus::unreliable) { badgeClass = "badge-unreliable"; statusLabel = "UNRELIABLE"; }
    else if (result.status == MeasurementStatus::invalid) { badgeClass = "badge-invalid"; statusLabel = "INVALID"; }
    else if (result.status == MeasurementStatus::failed) { badgeClass = "badge-failed"; statusLabel = "FAILED"; }
    else if (result.status == MeasurementStatus::skipped) { badgeClass = "badge-skipped"; statusLabel = "SKIPPED"; }

    h << "  <div style=\"display: flex; justify-content: space-between; align-items: flex-start;\">\n"
      << "    <div>\n"
      << "      <h1>Envelope measurement: " << statusLabel << "</h1>\n"
      << "      <div class=\"subtitle\">Target: <strong>" << result.dut.name << "</strong> (" << result.dut.format << ") | ID: <code>" << spec.measurementId << "</code></div>\n"
      << "    </div>\n"
      << "    <div><span class=\"badge " << badgeClass << "\">" << statusLabel << "</span></div>\n"
      << "  </div>\n";

    // Diagnostic reason alert (if not clean completed or has note)
    if (!result.reason.empty() && result.reason != "Envelope successfully observed")
    {
        h << "  <div class=\"alert alert-warning\">\n"
          << "    <strong>Reason:</strong> <code>" << result.reason << "</code>\n";
        if (result.observability.reason.has_value())
            h << "    <p style=\"margin: 4px 0 0 0;\">" << *result.observability.reason << "</p>\n";
        h << "  </div>\n";
    }

    // Section 1: Execution Metadata
    h << "  <div class=\"section-title\">Execution & Metrological Metadata</div>\n"
      << "  <div class=\"meta-grid\">\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Sampling Rate</div><div class=\"meta-value\">" << static_cast<int>(result.execution.sampleRateHz) << " Hz</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Block Size / Latency</div><div class=\"meta-value\">" << result.execution.blockSize << " spl / " << result.execution.latencySamples << " spl</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Analyzer Engine</div><div class=\"meta-value\">" << result.analyzer.name << " (v" << result.analyzer.version << ")</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Stimulus Type</div><div class=\"meta-value\">" << stimulusTypeToString(result.stimulus.type) << " (Note " << result.stimulus.midiNoteNumber << ", Vel " << result.stimulus.midiVelocity << ")</div></div>\n"
      << "  </div>\n";

    // Section 2: Strongly-Typed Metrics
    h << "  <div class=\"section-title\">Observed ADSR Metrics</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Metric</th><th>Observed Value</th><th>Unit</th><th>Status</th></tr></thead>\n"
      << "    <tbody>\n";

    for (const auto& m : result.metrics)
    {
        std::string mBadge = (m.status == "observed") ? "badge-observed" : "badge-unreliable";
        h << "      <tr>\n"
          << "        <td><strong>" << m.name.toStdString() << "</strong></td>\n"
          << "        <td style=\"font-weight: 600; color: #f8fafc;\">" << std::fixed << std::setprecision(1) << m.value << "</td>\n"
          << "        <td><code>" << m.unit.toStdString() << "</code></td>\n"
          << "        <td><span class=\"badge " << mBadge << "\">" << m.status.toStdString() << "</span></td>\n"
          << "      </tr>\n";
    }

    h << "    </tbody>\n  </table>\n";

    // Section 3: Vector Temporal Curve
    h << "  <div class=\"section-title\">Temporal Envelope Trajectory</div>\n";
    if (!result.curve.x.empty())
    {
        h << "  <div style=\"margin: 16px 0; text-align: center;\">\n"
          << generateTemporalCurveSvg(result.curve.x, result.curve.y, 796, 260)
          << "  </div>\n";
    }
    else
    {
        h << "  <div class=\"alert alert-info\">No temporal curve persisted (signal is unobservable or execution incomplete).</div>\n";
    }

    // Section 4: Audio Playback Controls
    if (!relativeAudioPath.empty())
    {
        h << "  <div class=\"section-title\">Acoustic Artifact & Playback</div>\n"
          << "  <div style=\"background: #1e293b; padding: 16px; border-radius: 8px; display: flex; align-items: center; justify-content: space-between;\">\n"
          << "    <audio controls preload=\"none\" style=\"width: 70%;\" src=\"" << relativeAudioPath << "\"></audio>\n"
          << "    <a href=\"" << relativeAudioPath << "\" style=\"color: #38bdf8; text-decoration: none; font-size: 12px; font-weight: 600;\">Download WAV</a>\n"
          << "  </div>\n";
    }

    // Section 5: FAIR Cryptographic Provenance
    h << "  <div class=\"section-title\">FAIR Cryptographic Provenance</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Artifact</th><th>FAIR Role</th><th>Fixity SHA-256</th></tr></thead>\n"
      << "    <tbody>\n"
      << "      <tr><td>Measurement Spec</td><td><code>measurement_spec</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Measurement Stimulus</td><td><code>measurement_stimulus</code></td><td><code>" << spec.stimulus.sha256 << "</code></td></tr>\n"
      << "      <tr><td>Measurement Result</td><td><code>measurement_result</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Envelope Curve</td><td><code>envelope_curve</code></td><td><code>envelope_curve.json</code></td></tr>\n"
      << "      <tr><td>Acoustic Audio</td><td><code>measurement_baseline_audio</code></td><td><code>" << result.artifacts.audioSha256 << "</code></td></tr>\n"
      << "    </tbody>\n"
      << "  </table>\n";

    h << "</div>\n</body>\n</html>\n";
    return h.str();
}

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

    // 0. Write experiment.json to satisfy standard ExperimentFolderReader / ExperimentRecord container format
    juce::File expJsonFile = containerDir.getChildFile("experiment.json");
    ordered_json expJson;
    expJson["schemaVersion"] = 1;
    expJson["experimentId"] = spec.measurementId;
    expJson["revision"] = 1;
    expJson["kind"] = "Measurement";
    expJson["status"] = "AuditedApproved";

    ordered_json capJson;
    capJson["sampleRate"] = spec.execution.sampleRateHz;
    capJson["hostBufferSize"] = spec.execution.blockSize;
    capJson["processingBlockSize"] = spec.execution.blockSize;
    capJson["channels"] = spec.execution.numChannels;
    capJson["durationSeconds"] = spec.stimulus.durationSec;
    capJson["presetStateHash"] = "";
    capJson["excitationPlanHash"] = spec.stimulus.sha256;
    capJson["storageProfile"] = "Standard";
    expJson["capture"] = capJson;

    expJsonFile.replaceWithText(juce::String::fromUTF8(expJson.dump(2).c_str()));

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

        core::ExperimentArtifact artAudio;
        artAudio.relativePath = "audio/envelope_reference.wav";
        artAudio.role = "measurement_baseline_audio";
        artAudio.sizeBytes = static_cast<uint64_t>(destAudio.getSize());
        artAudio.sha256 = core::ExperimentStorage::computeFileSha256(destAudio);

        // Technical audio metadata
        juce::AudioFormatManager formatMgr;
        formatMgr.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(destAudio));
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
            artAudio.audio = meta;
        }

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
    ordered_json manifestJson;
    manifestJson["manifestFormat"] = "artifact-list-v1";
    manifestJson["experimentId"] = spec.measurementId;
    manifestJson["experimentKind"] = "Measurement";
    manifestJson["targetName"] = spec.parameterName;
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
    std::string manifestStr = manifestJson.dump(2);
    manifestFile.replaceWithText(juce::String::fromUTF8(manifestStr.c_str()));

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

} // namespace abdaudiolab::measurement
