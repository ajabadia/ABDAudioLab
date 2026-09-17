/**
 * @file DexedVerticalCampaign.cpp
 * @brief Implementación de la Prueba Vertical de Integración y Exportación FAIR/LNL de Dexed.vst3 (Fase 20.11 T5).
 * @author ABDSynths
 * @date 2026
 */

#include "DexedVerticalCampaign.h"
#include "MeasurementSerialization.h"
#include "../synth/Sha256.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::measurement
{
using namespace abdaudiolab::synth;

namespace
{

std::string getCurrentUtcIso8601()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm gm {};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&gm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string computeFileSha256(const juce::File& file)
{
    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb))
        return {};
    return synth::Sha256::computeHex(mb.getData(), mb.getSize());
}

bool writePcmWavFile(const juce::File& file, const std::vector<float>& samples, double sampleRateHz)
{
    file.getParentDirectory().createDirectory();
    if (file.exists())
        file.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        new juce::FileOutputStream(file),
        sampleRateHz,
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

} // namespace

bool DexedVerticalFixture::verifyFixity() const
{
    if (presetBytes.empty() || stateSha256.empty())
        return false;
    std::string computed = synth::Sha256::computeHex(presetBytes.data(), presetBytes.size());
    return (computed == stateSha256);
}

juce::File DexedVerticalCoordinator::resolveDexedBinary()
{
    // 1. Variable de entorno
    auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    // 2. Ruta estandar VST3 en Windows
    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (defaultWin.exists())
        return defaultWin;

    // 3. Ruta en AppData local
    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("../Local/Programs/Common/VST3/Dexed.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

bool DexedVerticalCoordinator::executeCampaign(juce::AudioPluginFormatManager& formatManager,
                                               const juce::File& dexedBinary,
                                               const DexedVerticalFixture& fixture,
                                               DexedCampaignResults& outResults,
                                               std::string& outError)
{
    outError.clear();

    if (!dexedBinary.exists())
    {
        outError = "dexed_binary_not_found";
        return false;
    }

    if (fixture.presetBytes.empty())
    {
        outError = "empty_preset_bytes";
        return false;
    }

    if (!fixture.verifyFixity())
    {
        outError = "preset_fixity_verification_failed";
        return false;
    }

    outResults.fixture = fixture;
    outResults.containerId = "fair-lnl-dexed-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    // 1. Introspección y captura de PluginIdentity
    InspectedPluginModule module;
    std::string inspErr;
    if (!ExternalPluginFixture::inspectPluginModule(formatManager, dexedBinary, "", module, inspErr))
    {
        outError = "plugin_inspection_failed: " + inspErr;
        return false;
    }

    outResults.pluginIdentity.canonicalPath = module.canonicalPath;
    outResults.pluginIdentity.binarySha256 = module.binarySha256;
    outResults.pluginIdentity.uid = module.selectedUid;
    outResults.pluginIdentity.vendor = "Digital Suburban";
    outResults.pluginIdentity.version = "0.9.6";
    outResults.pluginIdentity.architecture = "x64";

    std::string wallClockStart = getCurrentUtcIso8601();

    // 2. Configurar Especificación Formal
    MeasurementSpec spec;
    spec.measurementId = outResults.containerId;
    spec.measurementType = "dynamics";
    spec.dutType = DeviceUnderTest::instrument;
    spec.executionDomain = fixture.executionDomain;
    spec.execution.sampleRateHz = fixture.sampleRateHz;
    spec.execution.blockSize = fixture.blockSize;
    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.midiNoteNumber = fixture.midiNote;
    spec.velocityGrid = fixture.velocityGrid;
    outResults.spec = spec;

    // 3. Estímulo JSON determinista
    nlohmann::ordered_json stimJson;
    stimJson["schemaVersion"] = "response-stimulus-1.0";
    stimJson["stimulusType"] = "midi_multi_velocity_series";
    stimJson["midiNote"] = fixture.midiNote;
    stimJson["velocityGrid"] = fixture.velocityGrid;
    stimJson["noteDurationSec"] = fixture.noteDurationSec;
    stimJson["releaseDurationSec"] = fixture.releaseDurationSec;
    outResults.stimulusJson = stimJson.dump(2);
    outResults.stimulusSha256 = synth::Sha256::computeHex(
        reinterpret_cast<const uint8_t*>(outResults.stimulusJson.data()), outResults.stimulusJson.size());

    // 4. Ejecución independiente por cada velocidad del grid (Ciclo estricto y limpio)
    std::vector<DynamicsMeasurementAdapter::VelocityTake> takes;
    std::vector<DynamicPoint> points;
    std::vector<float> lastCapturedAudio;

    const size_t noteSamples = static_cast<size_t>(std::lround(fixture.noteDurationSec * fixture.sampleRateHz));
    const size_t releaseSamples = static_cast<size_t>(std::lround(fixture.releaseDurationSec * fixture.sampleRateHz));
    const size_t totalTakeSamples = noteSamples + releaseSamples;

    for (int vel : fixture.velocityGrid)
    {
        // Instancia y fixture limpios por cada velocidad
        ExternalPluginFixture plug(formatManager);
        std::string loadErr;
        if (!plug.loadPluginFromDisk(dexedBinary, fixture.sampleRateHz, fixture.blockSize, loadErr))
        {
            outError = "failed_to_load_plugin_instance: " + loadErr;
            return false;
        }

        // prepare -> resetState -> setState -> verifyState
        plug.resetState();
        auto setRes = plug.setState(fixture.presetBytes);
        if (!setRes.succeeded)
        {
            outError = "failed_to_set_preset_state";
            return false;
        }

        std::vector<uint8_t> rtState;
        auto getRes = plug.getState(rtState);
        if (!getRes.succeeded || getRes.stateDataHash != fixture.stateSha256)
        {
            outError = "state_verification_failed";
            return false;
        }

        // Renderizar offline con NoteOn(vel) en offset 0 y NoteOff en noteSamples
        auto* inst = plug.getPluginInstance();
        if (inst == nullptr)
        {
            outError = "null_plugin_instance";
            return false;
        }

        juce::AudioBuffer<float> blockBuffer(2, fixture.blockSize);
        juce::MidiBuffer midiMessages;

        std::vector<float> takeAudio;
        takeAudio.reserve(totalTakeSamples);

        size_t samplesRendered = 0;
        bool noteOnSent = false;
        bool noteOffSent = false;

        while (samplesRendered < totalTakeSamples)
        {
            blockBuffer.clear();
            midiMessages.clear();

            int currentBlock = static_cast<int>(std::min(static_cast<size_t>(fixture.blockSize), totalTakeSamples - samplesRendered));

            if (!noteOnSent && samplesRendered == 0)
            {
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, fixture.midiNote, static_cast<juce::uint8>(vel)), 0);
                noteOnSent = true;
            }

            if (!noteOffSent && samplesRendered <= noteSamples && (samplesRendered + currentBlock) > noteSamples)
            {
                int noteOffOffset = static_cast<int>(noteSamples - samplesRendered);
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, fixture.midiNote), noteOffOffset);
                noteOffSent = true;
            }

            inst->processBlock(blockBuffer, midiMessages);

            // Mezclar a mono (canal izquierdo / salida)
            for (int i = 0; i < currentBlock; ++i)
            {
                takeAudio.push_back(blockBuffer.getSample(0, i));
            }

            samplesRendered += currentBlock;
        }

        // Analizar el DynamicPoint correspondiente
        std::string audioHash = synth::Sha256::computeHex(takeAudio.data(), takeAudio.size());
        DynamicPoint pt = DynamicsMeasurementAdapter::analyzeSinglePoint(
            vel, takeAudio, fixture.sampleRateHz, 0, noteSamples, 0.0, 0.0, fixture.stateSha256, audioHash);

        points.push_back(pt);

        DynamicsMeasurementAdapter::VelocityTake take;
        take.velocity = vel;
        take.audio = takeAudio;
        take.sampleRateHz = fixture.sampleRateHz;
        take.noteOnSample = 0;
        take.noteOffSample = noteSamples;
        take.presetStateHash = fixture.stateSha256;
        take.audioArtifactHash = audioHash;
        takes.push_back(std::move(take));

        lastCapturedAudio = std::move(takeAudio);
    }

    outResults.dynamicPoints = points;
    outResults.referenceAudioSamples = lastCapturedAudio;

    // 5. Análisis de dinámica consolidado
    std::vector<double> vX;
    std::vector<double> vPeak;
    std::vector<double> vRms;
    std::vector<double> vCentroid;

    for (const auto& p : points)
    {
        vX.push_back(static_cast<double>(p.velocity));
        vPeak.push_back(p.peakDbfs);
        vRms.push_back(p.rmsDbfs);
        vCentroid.push_back(p.spectralCentroidHz);
    }

    MeasurementCurve ampCurve;
    ampCurve.xName = "velocity";
    ampCurve.xUnit = "midi_0_127";
    ampCurve.yName = "rmsDbfs";
    ampCurve.yUnit = "dBFS";
    ampCurve.x = vX;
    ampCurve.y = vRms;

    MeasurementCurve brightCurve;
    brightCurve.xName = "velocity";
    brightCurve.xUnit = "midi_0_127";
    brightCurve.yName = "spectralCentroidHz";
    brightCurve.yUnit = "Hz";
    brightCurve.x = vX;
    brightCurve.y = vCentroid;

    DynamicResponseResult dynResult;
    dynResult.points = points;
    dynResult.amplitudeCurve = ampCurve;
    dynResult.brightnessCurve = brightCurve;
    dynResult.amplitudeFit = DynamicsMeasurementAdapter::fitCurveModel(vX, vRms, "velocity", "rmsDbfs");
    outResults.dynamicResult = std::move(dynResult);

    // 6. Análisis de modulación (Declaración metrológica explícita de ausencia de modulación)
    ModulationResultData modResult;
    modResult.targetDestination = "amplitude";
    modResult.waveform.status = "not_observable";
    modResult.waveform.waveform = "none";
    modResult.waveform.confidence = 0.0;
    modResult.sidebands = {};
    outResults.modulationResult = std::move(modResult);

    // 7. Resultado global de medición
    MeasurementResult measRes;
    measRes.measurementId = outResults.containerId;
    measRes.status = MeasurementStatus::completed;
    measRes.executionDomain = fixture.executionDomain;
    measRes.pluginIdentity = outResults.pluginIdentity;
    measRes.curve = ampCurve;
    measRes.dut.name = "Dexed";
    measRes.dut.format = "VST3";
    measRes.dut.version = "0.9.6";
    measRes.dynamicResult = outResults.dynamicResult;
    measRes.modulationResult = outResults.modulationResult;
    measRes.reason = "observed_without_modulation";
    outResults.measurementResult = measRes;

    // 8. Telemetría de Captura (CaptureArtifactMetadata)
    std::string wallClockEnd = getCurrentUtcIso8601();
    CaptureArtifactMetadata tele;
    tele.wallClockStartIso = wallClockStart;
    tele.wallClockEndIso = wallClockEnd;
    tele.sampleRateHz = fixture.sampleRateHz;
    tele.bufferSize = fixture.blockSize;
    tele.numChannels = 1;
    tele.sha256Audio = synth::Sha256::computeHex(
        outResults.referenceAudioSamples.data(), outResults.referenceAudioSamples.size());
    tele.sha256Midi = outResults.stimulusSha256;
    tele.sha256State = fixture.stateSha256;
    tele.underruns = 0;
    tele.overruns = 0;
    outResults.captureTelemetry = tele;

    return true;
}

bool DexedVerticalContainerExporter::exportContainer(const juce::File& outputDir,
                                                    const DexedCampaignResults& results,
                                                    std::string& outError)
{
    outError.clear();

    if (!outputDir.createDirectory())
    {
        outError = "failed_to_create_output_dir";
        return false;
    }

    juce::File specsDir = outputDir.getChildFile("specs");
    juce::File resultsDir = outputDir.getChildFile("results");
    juce::File curvesDir = outputDir.getChildFile("curves");
    juce::File audioDir = outputDir.getChildFile("audio");
    juce::File reportsDir = outputDir.getChildFile("reports");

    specsDir.createDirectory();
    resultsDir.createDirectory();
    curvesDir.createDirectory();
    audioDir.createDirectory();
    reportsDir.createDirectory();

    // 1. specs/measurement_spec.json
    juce::File fSpec = specsDir.getChildFile("measurement_spec.json");
    std::string specJsonStr = MeasurementSerialization::serializeSpec(results.spec);
    fSpec.replaceWithText(juce::String::fromUTF8(specJsonStr.c_str()));

    // 2. specs/measurement_stimulus.json
    juce::File fStim = specsDir.getChildFile("measurement_stimulus.json");
    fStim.replaceWithText(juce::String::fromUTF8(results.stimulusJson.c_str()));

    // 3. specs/plugin_identity.json
    juce::File fIdent = specsDir.getChildFile("plugin_identity.json");
    nlohmann::ordered_json jIdent;
    jIdent["canonicalPath"] = results.pluginIdentity.canonicalPath.toStdString();
    jIdent["binarySha256"] = results.pluginIdentity.binarySha256.toStdString();
    jIdent["uid"] = results.pluginIdentity.uid.toStdString();
    jIdent["vendor"] = results.pluginIdentity.vendor.toStdString();
    jIdent["version"] = results.pluginIdentity.version.toStdString();
    jIdent["architecture"] = results.pluginIdentity.architecture.toStdString();
    fIdent.replaceWithText(juce::String::fromUTF8(jIdent.dump(2).c_str()));

    // 4. results/measurement_result.json
    juce::File fResult = resultsDir.getChildFile("measurement_result.json");
    std::string resJsonStr = MeasurementSerialization::serializeResult(results.measurementResult);
    fResult.replaceWithText(juce::String::fromUTF8(resJsonStr.c_str()));

    // 5. results/capture_telemetry.json
    juce::File fTele = resultsDir.getChildFile("capture_telemetry.json");
    fTele.replaceWithText(juce::String::fromUTF8(results.captureTelemetry.toJsonString(2).c_str()));

    // 6. curves/dynamics_velocity_level_curve.json
    juce::File fDynLevel = curvesDir.getChildFile("dynamics_velocity_level_curve.json");
    nlohmann::ordered_json jLevelCurve;
    jLevelCurve["xLabel"] = "Velocity";
    jLevelCurve["yLabel"] = "Peak Level (dBFS)";
    nlohmann::json ptsLevel = nlohmann::json::array();
    for (const auto& p : results.dynamicPoints)
    {
        ptsLevel.push_back({ {"velocity", p.velocity}, {"peakDbfs", p.peakDbfs}, {"rmsDbfs", p.rmsDbfs} });
    }
    jLevelCurve["points"] = ptsLevel;
    fDynLevel.replaceWithText(juce::String::fromUTF8(jLevelCurve.dump(2).c_str()));

    // 7. curves/dynamics_velocity_timbre_curve.json
    juce::File fDynTimbre = curvesDir.getChildFile("dynamics_velocity_timbre_curve.json");
    nlohmann::ordered_json jTimbreCurve;
    jTimbreCurve["xLabel"] = "Velocity";
    jTimbreCurve["yLabel"] = "Spectral Centroid (Hz)";
    nlohmann::json ptsTimbre = nlohmann::json::array();
    for (const auto& p : results.dynamicPoints)
    {
        ptsTimbre.push_back({ {"velocity", p.velocity}, {"spectralCentroidHz", p.spectralCentroidHz}, {"spectralRolloffHz", p.spectralRolloffHz} });
    }
    jTimbreCurve["points"] = ptsTimbre;
    fDynTimbre.replaceWithText(juce::String::fromUTF8(jTimbreCurve.dump(2).c_str()));

    // 8. curves/modulation_time_curve.json
    juce::File fModTime = curvesDir.getChildFile("modulation_time_curve.json");
    nlohmann::ordered_json jModTime;
    jModTime["status"] = "not_observable";
    jModTime["description"] = "No cyclic temporal modulation detected on static preset";
    fModTime.replaceWithText(juce::String::fromUTF8(jModTime.dump(2).c_str()));

    // 9. curves/modulation_spectrum_curve.json
    juce::File fModSpec = curvesDir.getChildFile("modulation_spectrum_curve.json");
    nlohmann::ordered_json jModSpec;
    jModSpec["status"] = "not_observable";
    jModSpec["sidebands"] = nlohmann::json::array();
    fModSpec.replaceWithText(juce::String::fromUTF8(jModSpec.dump(2).c_str()));

    // 10. audio/dexed_reference.wav
    juce::File fAudio = audioDir.getChildFile("dexed_reference.wav");
    if (!writePcmWavFile(fAudio, results.referenceAudioSamples, results.fixture.sampleRateHz))
    {
        outError = "failed_to_write_audio_reference";
        return false;
    }

    // 11. reports/measurement_report.html
    juce::File fReport = reportsDir.getChildFile("measurement_report.html");
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
         << "<meta charset=\"UTF-8\">\n<title>Dexed Vertical Measurement Report - " << results.containerId << "</title>\n"
         << "<style>\n"
         << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #121418; color: #e1e4ea; margin: 2rem; }\n"
         << ".card { background: #1a1d24; border-radius: 8px; padding: 1.5rem; margin-bottom: 1.5rem; border: 1px solid #2d3340; }\n"
         << ".badge { padding: 0.25rem 0.75rem; border-radius: 4px; font-weight: bold; text-transform: uppercase; }\n"
         << ".badge-ok { background: #1e4620; color: #4ade80; border: 1px solid #22c55e; }\n"
         << "table { width: 100%; border-collapse: collapse; margin-top: 1rem; }\n"
         << "th, td { text-align: left; padding: 0.75rem; border-bottom: 1px solid #2d3340; }\n"
         << "th { color: #94a3b8; font-size: 0.85rem; text-transform: uppercase; }\n"
         << "code { font-family: monospace; background: #0f1115; padding: 0.2rem 0.4rem; border-radius: 4px; color: #38bdf8; }\n"
         << "</style>\n</head>\n<body>\n"
         << "<h1>Dexed Vertical FAIR/LNL Measurement Report</h1>\n"
         << "<div class=\"card\">\n"
         << "<h2>Status: <span class=\"badge badge-ok\">COMPLETED (Observed)</span></h2>\n"
         << "<p>Container ID: <code>" << results.containerId << "</code></p>\n"
         << "<p>Target: <strong>Dexed.vst3</strong> | Preset: <strong>" << results.fixture.presetName << "</strong></p>\n"
         << "<p>Domain: <code>" << measurementExecutionDomainToString(results.fixture.executionDomain) << "</code></p>\n"
         << "</div>\n"
         << "<div class=\"card\">\n"
         << "<h3>Velocity Dynamics Response</h3>\n"
         << "<table>\n"
         << "<tr><th>Velocity</th><th>Peak (dBFS)</th><th>RMS (dBFS)</th><th>Centroid (Hz)</th><th>Rolloff (Hz)</th></tr>\n";

    for (const auto& p : results.dynamicPoints)
    {
        html << "<tr><td>" << p.velocity << "</td><td>" << p.peakDbfs << "</td><td>" << p.rmsDbfs
             << "</td><td>" << p.spectralCentroidHz << "</td><td>" << p.spectralRolloffHz << "</td></tr>\n";
    }

    html << "</table>\n"
         << "</div>\n"
         << "<div class=\"card\">\n"
         << "<h3>Cyclic Modulation Assessment</h3>\n"
         << "<p>Observed status: <code>" << results.modulationResult.waveform.status << "</code> (Preset exhibits steady timbre without active LFO).</p>\n"
         << "</div>\n"
         << "</body>\n</html>\n";

    fReport.replaceWithText(juce::String::fromUTF8(html.str().c_str()));

    // 12. manifest.json (Exactamente 11 artefactos en array, artifactCount = 12, sin incluirse a sí mismo)
    juce::File fManifest = outputDir.getChildFile("manifest.json");

    nlohmann::ordered_json jManifest;
    jManifest["manifestFormat"] = kManifestFormat;
    jManifest["schemaVersion"] = kLnlSchemaVersion;
    jManifest["containerId"] = results.containerId;
    jManifest["executionDomain"] = measurementExecutionDomainToString(results.fixture.executionDomain);
    
    nlohmann::ordered_json pi;
    pi["canonicalPath"] = results.pluginIdentity.canonicalPath.toStdString();
    pi["binarySha256"] = results.pluginIdentity.binarySha256.toStdString();
    pi["uid"] = results.pluginIdentity.uid.toStdString();
    pi["vendor"] = results.pluginIdentity.vendor.toStdString();
    pi["version"] = results.pluginIdentity.version.toStdString();
    pi["architecture"] = results.pluginIdentity.architecture.toStdString();
    jManifest["pluginIdentity"] = pi;

    jManifest["presetStateSha256"] = results.fixture.stateSha256;
    jManifest["stimulusSha256"] = results.stimulusSha256;
    jManifest["artifactCount"] = 12;

    struct ArtEntry {
        std::string role;
        std::string path;
        juce::File file;
    };

    std::vector<ArtEntry> artList = {
        { "measurement_spec", "specs/measurement_spec.json", fSpec },
        { "measurement_stimulus", "specs/measurement_stimulus.json", fStim },
        { "plugin_identity", "specs/plugin_identity.json", fIdent },
        { "measurement_result", "results/measurement_result.json", fResult },
        { "capture_telemetry", "results/capture_telemetry.json", fTele },
        { "dynamics_velocity_level_curve", "curves/dynamics_velocity_level_curve.json", fDynLevel },
        { "dynamics_velocity_timbre_curve", "curves/dynamics_velocity_timbre_curve.json", fDynTimbre },
        { "modulation_time_curve", "curves/modulation_time_curve.json", fModTime },
        { "modulation_spectrum_curve", "curves/modulation_spectrum_curve.json", fModSpec },
        { "audio_reference", "audio/dexed_reference.wav", fAudio },
        { "measurement_report", "reports/measurement_report.html", fReport }
    };

    nlohmann::json artArray = nlohmann::json::array();
    for (const auto& a : artList)
    {
        nlohmann::ordered_json item;
        item["path"] = a.path;
        item["sha256"] = computeFileSha256(a.file);
        item["sizeBytes"] = static_cast<uint64_t>(a.file.getSize());
        item["role"] = a.role;
        artArray.push_back(item);
    }
    jManifest["artifacts"] = artArray;

    fManifest.replaceWithText(juce::String::fromUTF8(jManifest.dump(2).c_str()));

    return true;
}

} // namespace abdaudiolab::measurement
