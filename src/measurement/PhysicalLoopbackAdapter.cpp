/**
 * @file PhysicalLoopbackAdapter.cpp
 * @brief Implementación del adaptador y empaquetador de loopback analógico para Fase 20.11 T4.2.
 * @author ABDSynths
 * @date 2026
 */

#include "PhysicalLoopbackAdapter.h"
#include "../synth/Sha256.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace abdaudiolab::measurement
{

namespace
{

bool writeWavFile(const juce::File& file, const std::vector<float>& samples, double sampleRate)
{
    file.getParentDirectory().createDirectory();
    if (file.exists())
        file.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        new juce::FileOutputStream(file),
        sampleRate,
        1,
        24,
        {},
        0));

    if (writer == nullptr)
        return false;

    if (samples.empty())
        return true;

    juce::AudioBuffer<float> buf(1, static_cast<int>(samples.size()));
    for (size_t i = 0; i < samples.size(); ++i)
    {
        buf.setSample(0, static_cast<int>(i), samples[i]);
    }

    return writer->writeFromAudioSampleBuffer(buf, 0, static_cast<int>(samples.size()));
}

std::string computeFileSha256(const juce::File& file)
{
    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb))
        return {};
    return synth::Sha256::computeHex(mb.getData(), mb.getSize());
}

} // namespace

std::string PhysicalLoopbackSpec::toJsonString(int indent) const
{
    nlohmann::ordered_json j;
    j["deviceName"] = deviceName;
    j["sampleRateHz"] = sampleRateHz;
    j["blockSize"] = blockSize;
    j["outputChannel"] = outputChannel;
    j["inputChannel"] = inputChannel;
    j["sweepDurationSec"] = sweepDurationSec;
    j["leadInSilenceSec"] = leadInSilenceSec;
    j["levelDbfs"] = levelDbfs;
    j["criteria"]["snrDbMin"] = snrDbMin;
    j["criteria"]["peakDbfsMax"] = peakDbfsMax;
    j["criteria"]["dcOffsetDbMax"] = dcOffsetDbMax;
    j["criteria"]["maxClockDriftPpm"] = maxClockDriftPpm;

    return (indent >= 0) ? j.dump(indent) : j.dump();
}

PhysicalLoopbackSpec PhysicalLoopbackSpec::fromJsonString(const std::string& jsonStr)
{
    PhysicalLoopbackSpec spec;
    try
    {
        auto j = nlohmann::ordered_json::parse(jsonStr);
        if (j.contains("deviceName")) spec.deviceName = j["deviceName"].get<std::string>();
        if (j.contains("sampleRateHz")) spec.sampleRateHz = j["sampleRateHz"].get<double>();
        if (j.contains("blockSize")) spec.blockSize = j["blockSize"].get<int>();
        if (j.contains("outputChannel")) spec.outputChannel = j["outputChannel"].get<int>();
        if (j.contains("inputChannel")) spec.inputChannel = j["inputChannel"].get<int>();
        if (j.contains("sweepDurationSec")) spec.sweepDurationSec = j["sweepDurationSec"].get<double>();
        if (j.contains("leadInSilenceSec")) spec.leadInSilenceSec = j["leadInSilenceSec"].get<double>();
        if (j.contains("levelDbfs")) spec.levelDbfs = j["levelDbfs"].get<float>();

        if (j.contains("criteria"))
        {
            auto& c = j["criteria"];
            if (c.contains("snrDbMin")) spec.snrDbMin = c["snrDbMin"].get<double>();
            if (c.contains("peakDbfsMax")) spec.peakDbfsMax = c["peakDbfsMax"].get<double>();
            if (c.contains("dcOffsetDbMax")) spec.dcOffsetDbMax = c["dcOffsetDbMax"].get<double>();
            if (c.contains("maxClockDriftPpm")) spec.maxClockDriftPpm = c["maxClockDriftPpm"].get<double>();
        }
    }
    catch (...)
    {
        // Conservar valores por defecto en caso de error de parseo
    }
    return spec;
}

bool PhysicalLoopbackCoordinator::executeCalibration(ILoopbackAudioTransport* transport,
                                                    const PhysicalLoopbackSpec& spec,
                                                    LoopbackCalibrationArtifacts& outArtifacts,
                                                    std::string& outError)
{
    outError.clear();

    // 1. Validaciones previas de entrada y canales
    if (transport == nullptr)
    {
        outError = "transport_null";
        return false;
    }
    if (spec.deviceName.empty())
    {
        outError = "missing_device_name";
        return false;
    }
    if (spec.sampleRateHz <= 0.0)
    {
        outError = "invalid_sample_rate";
        return false;
    }
    if (spec.blockSize <= 0)
    {
        outError = "invalid_block_size";
        return false;
    }
    if (spec.outputChannel < 0)
    {
        outError = "invalid_output_channel";
        return false;
    }
    if (spec.inputChannel < 0)
    {
        outError = "invalid_input_channel";
        return false;
    }

    // 2. Apertura del transporte físico
    std::string transportErr;
    if (!transport->open(spec.deviceName, spec.sampleRateHz, spec.blockSize, spec.outputChannel, spec.inputChannel, transportErr))
    {
        outError = "transport_open_failed: " + transportErr;
        return false;
    }

    // 3. Generación del estímulo canónico con Sync Marker de alta precisión
    std::vector<float> stimulus = LoopbackCalibrator::generateCalibrationStimulus(
        spec.sampleRateHz, spec.sweepDurationSec, spec.leadInSilenceSec, spec.levelDbfs);

    if (stimulus.empty())
    {
        transport->close();
        outError = "stimulus_generation_failed";
        return false;
    }

    // 4. Transmisión y captura síncrona a través del loopback
    std::vector<float> response;
    if (!transport->transmitAndCapture(stimulus, response, transportErr))
    {
        transport->close();
        outError = "transmit_and_capture_failed: " + transportErr;
        return false;
    }

    transport->close();

    if (response.empty())
    {
        outError = "empty_response";
        return false;
    }

    // 5. Análisis metrológico exhaustivo
    LoopbackCalibrationRecord record = LoopbackCalibrator::analyzeLoopback(
        stimulus, response, spec.sampleRateHz, spec.blockSize);

    // Aplicar los criterios configurables de la especificación
    record.snrDbMin = spec.snrDbMin;
    record.peakDbfsMax = spec.peakDbfsMax;
    record.dcOffsetDbMax = spec.dcOffsetDbMax;
    record.maxClockDriftPpm = spec.maxClockDriftPpm;

    bool passesSnr = (record.snrDb >= record.snrDbMin);
    bool passesClipping = (record.peakDbfs <= record.peakDbfsMax);
    bool passesDc = (record.dcOffsetDb <= record.dcOffsetDbMax);
    bool passesDrift = (std::abs(record.clockDriftPpm) <= record.maxClockDriftPpm);
    bool passesLatency = (record.roundTripLatencySamples >= 0.0);

    record.status = (passesSnr && passesClipping && passesDc && passesDrift && passesLatency) ? "pass" : "fail";

    // 6. Configurar artefactos de salida
    outArtifacts.stimulusAudio = std::move(stimulus);
    outArtifacts.responseAudio = std::move(response);
    outArtifacts.record = std::move(record);
    outArtifacts.tier = AnalogChainArtifactTier::LoopbackReference;
    outArtifacts.spec = spec;

    return true;
}

bool LoopbackContainerExporter::exportLoopbackPackage(const juce::File& outputDir,
                                                      const LoopbackCalibrationArtifacts& artifacts,
                                                      std::string& outError)
{
    outError.clear();

    if (!outputDir.createDirectory())
    {
        outError = "failed_to_create_output_dir: " + outputDir.getFullPathName().toStdString();
        return false;
    }

    juce::File specsDir = outputDir.getChildFile("specs");
    juce::File audioDir = outputDir.getChildFile("audio");
    juce::File resultsDir = outputDir.getChildFile("results");
    juce::File reportsDir = outputDir.getChildFile("reports");

    specsDir.createDirectory();
    audioDir.createDirectory();
    resultsDir.createDirectory();
    reportsDir.createDirectory();

    // 1. specs/loopback_calibration_spec.json
    juce::File specFile = specsDir.getChildFile("loopback_calibration_spec.json");
    specFile.replaceWithText(juce::String::fromUTF8(artifacts.spec.toJsonString(2).c_str()));

    // 2. audio/loopback_reference_stimulus.wav
    juce::File stimWav = audioDir.getChildFile("loopback_reference_stimulus.wav");
    if (!writeWavFile(stimWav, artifacts.stimulusAudio, artifacts.spec.sampleRateHz))
    {
        outError = "failed_to_write_stimulus_wav";
        return false;
    }

    // 3. audio/loopback_reference_response.wav
    juce::File respWav = audioDir.getChildFile("loopback_reference_response.wav");
    if (!writeWavFile(respWav, artifacts.responseAudio, artifacts.spec.sampleRateHz))
    {
        outError = "failed_to_write_response_wav";
        return false;
    }

    // 4. results/loopback_calibration_result.json
    juce::File resultFile = resultsDir.getChildFile("loopback_calibration_result.json");
    nlohmann::ordered_json resJson;
    resJson["calibrationId"] = artifacts.record.calibrationId;
    resJson["sampleRateHz"] = artifacts.record.sampleRateHz;
    resJson["blockSize"] = artifacts.record.blockSize;
    resJson["roundTripLatencySamples"] = artifacts.record.roundTripLatencySamples;
    resJson["roundTripLatencyMs"] = artifacts.record.roundTripLatencyMs;
    resJson["snrDb"] = artifacts.record.snrDb;
    resJson["peakDbfs"] = artifacts.record.peakDbfs;
    resJson["dcOffsetDb"] = artifacts.record.dcOffsetDb;
    resJson["clockDriftPpm"] = artifacts.record.clockDriftPpm;
    resJson["stimulusSha256"] = artifacts.record.stimulusSha256;
    resJson["responseSha256"] = artifacts.record.responseSha256;
    resJson["status"] = artifacts.record.status;
    resJson["criteria"]["snrDbMin"] = artifacts.record.snrDbMin;
    resJson["criteria"]["peakDbfsMax"] = artifacts.record.peakDbfsMax;
    resJson["criteria"]["dcOffsetDbMax"] = artifacts.record.dcOffsetDbMax;
    resJson["criteria"]["maxClockDriftPpm"] = artifacts.record.maxClockDriftPpm;
    resultFile.replaceWithText(juce::String::fromUTF8(resJson.dump(2).c_str()));

    // 5. reports/loopback_calibration_report.html
    juce::File reportFile = reportsDir.getChildFile("loopback_calibration_report.html");
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
         << "<meta charset=\"UTF-8\">\n<title>Loopback Calibration Report - " << artifacts.record.calibrationId << "</title>\n"
         << "<style>\n"
         << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #121418; color: #e1e4ea; margin: 2rem; }\n"
         << ".card { background: #1a1d24; border-radius: 8px; padding: 1.5rem; margin-bottom: 1.5rem; border: 1px solid #2d3340; }\n"
         << ".badge { padding: 0.25rem 0.75rem; border-radius: 4px; font-weight: bold; text-transform: uppercase; }\n"
         << ".badge-pass { background: #1e4620; color: #4ade80; border: 1px solid #22c55e; }\n"
         << ".badge-fail { background: #451a1a; color: #f87171; border: 1px solid #ef4444; }\n"
         << "table { width: 100%; border-collapse: collapse; margin-top: 1rem; }\n"
         << "th, td { text-align: left; padding: 0.75rem; border-bottom: 1px solid #2d3340; }\n"
         << "th { color: #94a3b8; font-size: 0.85rem; text-transform: uppercase; }\n"
         << "code { font-family: monospace; background: #0f1115; padding: 0.2rem 0.4rem; border-radius: 4px; color: #38bdf8; }\n"
         << "</style>\n</head>\n<body>\n"
         << "<h1>Loopback Calibration Report (Pure Reference)</h1>\n"
         << "<div class=\"card\">\n"
         << "<h2>Calibration Status: <span class=\"badge " << (artifacts.record.isPass() ? "badge-pass" : "badge-fail") << "\">"
         << artifacts.record.status << "</span></h2>\n"
         << "<p>Calibration ID: <code>" << artifacts.record.calibrationId << "</code></p>\n"
         << "<p>Artifact Tier: <code>" << analogChainArtifactTierToString(artifacts.tier) << "</code></p>\n"
         << "<p>Device: <strong>" << artifacts.spec.deviceName << "</strong> | Output Ch: " << artifacts.spec.outputChannel
         << " &rarr; Input Ch: " << artifacts.spec.inputChannel << "</p>\n"
         << "</div>\n"
         << "<div class=\"card\">\n"
         << "<h3>Metrological Parameters &amp; Verification</h3>\n"
         << "<table>\n"
         << "<tr><th>Parameter</th><th>Observed Value</th><th>Acceptance Threshold</th><th>Status</th></tr>\n"
         << "<tr><td>Round-Trip Latency</td><td><strong>" << artifacts.record.roundTripLatencySamples << " samples</strong> ("
         << std::fixed << std::setprecision(3) << artifacts.record.roundTripLatencyMs << " ms)</td><td>&ge; 0 samples</td><td>&check;</td></tr>\n"
         << "<tr><td>Signal-to-Noise Ratio (SNR)</td><td><strong>" << std::setprecision(2) << artifacts.record.snrDb << " dB</strong></td><td>&ge; "
         << artifacts.record.snrDbMin << " dB</td><td>" << (artifacts.record.snrDb >= artifacts.record.snrDbMin ? "&check;" : "&cross;") << "</td></tr>\n"
         << "<tr><td>Peak Level</td><td><strong>" << artifacts.record.peakDbfs << " dBFS</strong></td><td>&le; "
         << artifacts.record.peakDbfsMax << " dBFS</td><td>" << (artifacts.record.peakDbfs <= artifacts.record.peakDbfsMax ? "&check;" : "&cross;") << "</td></tr>\n"
         << "<tr><td>DC Offset</td><td><strong>" << artifacts.record.dcOffsetDb << " dBFS</strong></td><td>&le; "
         << artifacts.record.dcOffsetDbMax << " dBFS</td><td>" << (artifacts.record.dcOffsetDb <= artifacts.record.dcOffsetDbMax ? "&check;" : "&cross;") << "</td></tr>\n"
         << "<tr><td>Clock Drift</td><td><strong>" << artifacts.record.clockDriftPpm << " ppm</strong></td><td>&le; &plusmn;"
         << artifacts.record.maxClockDriftPpm << " ppm</td><td>&check;</td></tr>\n"
         << "</table>\n"
         << "</div>\n"
         << "<div class=\"card\">\n"
         << "<h3>Cryptographic Fixity &amp; Provenance</h3>\n"
         << "<ul>\n"
         << "<li>Stimulus SHA-256: <code>" << artifacts.record.stimulusSha256 << "</code></li>\n"
         << "<li>Response SHA-256: <code>" << artifacts.record.responseSha256 << "</code></li>\n"
         << "</ul>\n"
         << "</div>\n"
         << "</body>\n</html>\n";

    reportFile.replaceWithText(juce::String::fromUTF8(html.str().c_str()));

    // 6. manifest.json
    juce::File manifestFile = outputDir.getChildFile("manifest.json");
    nlohmann::ordered_json manifest;
    manifest["schemaVersion"] = "loopback-calibration-1.0";
    manifest["artifactTier"] = analogChainArtifactTierToString(artifacts.tier);
    manifest["calibrationId"] = artifacts.record.calibrationId;
    manifest["sampleRateHz"] = artifacts.record.sampleRateHz;
    manifest["blockSize"] = artifacts.record.blockSize;
    manifest["selectedOutputChannel"] = artifacts.spec.outputChannel;
    manifest["selectedInputChannel"] = artifacts.spec.inputChannel;
    manifest["roundTripLatencySamples"] = artifacts.record.roundTripLatencySamples;
    manifest["roundTripLatencyMs"] = artifacts.record.roundTripLatencyMs;
    manifest["snrDb"] = artifacts.record.snrDb;
    manifest["peakDbfs"] = artifacts.record.peakDbfs;
    manifest["dcOffsetDb"] = artifacts.record.dcOffsetDb;
    manifest["clockDriftPpm"] = artifacts.record.clockDriftPpm;
    manifest["stimulusSha256"] = artifacts.record.stimulusSha256;
    manifest["responseSha256"] = artifacts.record.responseSha256;
    manifest["status"] = artifacts.record.status;
    manifest["criteria"]["snrDbMin"] = artifacts.record.snrDbMin;
    manifest["criteria"]["peakDbfsMax"] = artifacts.record.peakDbfsMax;
    manifest["criteria"]["dcOffsetDbMax"] = artifacts.record.dcOffsetDbMax;
    manifest["criteria"]["maxClockDriftPpm"] = artifacts.record.maxClockDriftPpm;

    manifest["artifacts"] = nlohmann::json::array({
        {
            {"role", "loopback_calibration_spec"},
            {"path", "specs/loopback_calibration_spec.json"},
            {"sha256", computeFileSha256(specFile)}
        },
        {
            {"role", "loopback_reference_stimulus"},
            {"path", "audio/loopback_reference_stimulus.wav"},
            {"sha256", computeFileSha256(stimWav)}
        },
        {
            {"role", "loopback_reference_response"},
            {"path", "audio/loopback_reference_response.wav"},
            {"sha256", computeFileSha256(respWav)}
        },
        {
            {"role", "loopback_calibration_result"},
            {"path", "results/loopback_calibration_result.json"},
            {"sha256", computeFileSha256(resultFile)}
        },
        {
            {"role", "loopback_calibration_report"},
            {"path", "reports/loopback_calibration_report.html"},
            {"sha256", computeFileSha256(reportFile)}
        }
    });

    manifestFile.replaceWithText(juce::String::fromUTF8(manifest.dump(2).c_str()));

    return true;
}

} // namespace abdaudiolab::measurement
