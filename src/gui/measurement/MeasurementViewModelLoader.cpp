/**
 * @file MeasurementViewModelLoader.cpp
 * @brief Implementation of MeasurementViewModelLoader.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementViewModelLoader.h"
#include "../../measurement/MeasurementSerialization.h"
#include "../../core/ExperimentStorage.h"
#include <nlohmann/json.hpp>

namespace abdaudiolab::gui::measurement
{

bool MeasurementViewModelLoader::loadFromContainer(const juce::File& containerDir,
                                                   MeasurementViewModel& outModel,
                                                   juce::String& outError)
{
    if (!containerDir.isDirectory())
    {
        outError = "Container directory does not exist or is not a directory: " + containerDir.getFullPathName();
        return false;
    }

    outModel = MeasurementViewModel();
    outModel.containerDirectory = containerDir;

    // 1. Confinement and existence of key files
    juce::File manifestFile = containerDir.getChildFile("manifest.json");
    juce::File specFile = containerDir.getChildFile("specs/measurement_spec.json");
    juce::File resultFile = containerDir.getChildFile("results/measurement_result.json");
    juce::File audioFile = containerDir.getChildFile("audio/envelope_reference.wav");
    if (!audioFile.existsAsFile())
        audioFile = containerDir.getChildFile("audio/modulation_reference.wav");
    if (!audioFile.existsAsFile())
        audioFile = containerDir.getChildFile("audio/audio_captured.wav");

    juce::File stimulusAudioFile = containerDir.getChildFile("audio/audio_stimulus.wav");
    juce::File impulseResponseFile = containerDir.getChildFile("audio/impulse_response.wav");
    juce::File curveFile = containerDir.getChildFile("curves/filter_response_curve.json");
    if (!curveFile.existsAsFile())
        curveFile = containerDir.getChildFile("curves/dynamics_velocity_level_curve.json");
    if (!curveFile.existsAsFile())
        curveFile = containerDir.getChildFile("curves/modulation_time_curve.json");
    if (!curveFile.existsAsFile())
        curveFile = containerDir.getChildFile("curves/envelope_curve.json");

    juce::File secondaryCurveFile = containerDir.getChildFile("curves/dynamics_velocity_timbre_curve.json");
    if (!secondaryCurveFile.existsAsFile())
        secondaryCurveFile = containerDir.getChildFile("curves/modulation_spectrum_curve.json");

    juce::File htmlReportFile = containerDir.getChildFile("reports/measurement_report.html");

    // Path traversal verification: all resolved files must be strictly confined inside containerDir
    auto checkConfinement = [&](const juce::File& f) {
        return f.getFullPathName().startsWith(containerDir.getFullPathName()) && f.isAChildOf(containerDir);
    };

    if (!checkConfinement(manifestFile) || !checkConfinement(specFile) ||
        !checkConfinement(resultFile) || !checkConfinement(audioFile) ||
        !checkConfinement(stimulusAudioFile) || !checkConfinement(impulseResponseFile) ||
        !checkConfinement(curveFile) || !checkConfinement(secondaryCurveFile) || !checkConfinement(htmlReportFile))
    {
        outError = "Security violation: Path traversal detected outside container directory";
        return false;
    }

    outModel.specFile = specFile;
    outModel.resultFile = resultFile;
    outModel.audioFile = audioFile;
    outModel.stimulusAudioFile = stimulusAudioFile;
    outModel.impulseResponseFile = impulseResponseFile;
    outModel.curveFile = curveFile;
    outModel.secondaryCurveFile = secondaryCurveFile;
    outModel.htmlReportFile = htmlReportFile;

    if (!manifestFile.existsAsFile())
    {
        outError = "Missing required manifest.json in container: " + containerDir.getFullPathName();
        return false;
    }

    try
    {
        auto manifestJson = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());
        if (manifestJson.contains("artifacts") && manifestJson["artifacts"].is_array())
        {
            for (const auto& a : manifestJson["artifacts"])
            {
                std::string role = a.value("role", "");
                std::string sha = a.value("sha256", "");
                if (role == "measurement_stimulus_audio")
                    outModel.expectedStimulusAudioSha256 = juce::String(sha);
                else if (role == "measurement_impulse_response")
                    outModel.expectedImpulseResponseSha256 = juce::String(sha);
                else if (role == "measurement_captured_audio" || role == "measurement_baseline_audio")
                {
                    if (outModel.expectedAudioSha256.isEmpty())
                        outModel.expectedAudioSha256 = juce::String(sha);
                }
            }
        }
    }
    catch (...) {}

    // 2. Read results/measurement_result.json
    if (!resultFile.existsAsFile())
    {
        outError = "Missing results/measurement_result.json in container";
        return false;
    }

    std::string resJson = resultFile.loadFileAsString().toStdString();
    abdaudiolab::measurement::MeasurementResult result;
    std::string parseErr;
    if (!abdaudiolab::measurement::MeasurementSerialization::deserializeResult(resJson, result, parseErr))
    {
        outError = "Failed to parse measurement_result.json: " + juce::String(parseErr);
        return false;
    }

    outModel.measurementId = result.measurementId;
    outModel.measurementType = result.measurementType;
    outModel.dutName = result.dut.name;
    outModel.dutFormat = result.dut.format;
    outModel.measurementStatus = result.status;
    outModel.diagnosticReason = result.reason;
    outModel.sampleRateHz = result.execution.sampleRateHz;
    outModel.blockSize = result.execution.blockSize;
    outModel.latencySamples = result.execution.latencySamples;
    outModel.analyzerName = result.analyzer.name;
    outModel.analyzerVersion = result.analyzer.version;
    outModel.metrics = result.metrics;
    outModel.curve = result.curve;
    outModel.expectedAudioSha256 = result.artifacts.audioSha256;
    outModel.filterTopology = result.filterTopology;
    outModel.measurementDomain = result.measurementDomain;
    outModel.slopeFit = result.slopeFit;
    outModel.dynamicsResult = result.dynamicResult;
    outModel.modulationResult = result.modulationResult;

    // 3. Read specs/measurement_spec.json for additional execution context if available
    if (specFile.existsAsFile())
    {
        std::string specJson = specFile.loadFileAsString().toStdString();
        abdaudiolab::measurement::MeasurementSpec spec;
        std::string specErr;
        if (abdaudiolab::measurement::MeasurementSerialization::deserializeSpec(specJson, spec, specErr))
        {
            if (outModel.sampleRateHz <= 0.0)
                outModel.sampleRateHz = spec.execution.sampleRateHz;
            if (outModel.blockSize <= 0)
                outModel.blockSize = spec.execution.blockSize;
            if (outModel.latencySamples <= 0)
                outModel.latencySamples = spec.execution.latencySamples;
        }
    }

    // 4. Map UI status strings and icons (Never PASS)
    switch (result.status)
    {
        case abdaudiolab::measurement::MeasurementStatus::completed:
            outModel.statusText = "COMPLETED";
            outModel.statusIcon = "[OK]";
            break;
        case abdaudiolab::measurement::MeasurementStatus::unreliable:
            outModel.statusText = "UNRELIABLE";
            outModel.statusIcon = "[!]";
            break;
        case abdaudiolab::measurement::MeasurementStatus::invalid:
            outModel.statusText = "INVALID";
            outModel.statusIcon = "[X]";
            break;
        case abdaudiolab::measurement::MeasurementStatus::failed:
            outModel.statusText = "FAILED";
            outModel.statusIcon = "[!]";
            break;
        case abdaudiolab::measurement::MeasurementStatus::skipped:
            outModel.statusText = "SKIPPED";
            outModel.statusIcon = "[!]";
            break;
    }

    // 5. Initial cryptographic fixity verification via ExperimentFolderReader
    juce::String verifyErr;
    if (verifyContainerIntegrity(containerDir, verifyErr))
    {
        outModel.integrityStatus = UiIntegrityStatus::Verified;
        outModel.integrityDiagnostic = "Manifest and artifact SHA-256 hashes verified successfully";
    }
    else
    {
        outModel.integrityStatus = UiIntegrityStatus::Corrupt;
        outModel.statusText = "CORRUPT";
        outModel.statusIcon = "[X]";
        outModel.integrityDiagnostic = verifyErr;
    }

    return true;
}

bool MeasurementViewModelLoader::verifyContainerIntegrity(const juce::File& containerDir,
                                                          juce::String& outDiagnostic)
{
    core::ExperimentFolderReader reader;
    if (!reader.canRead(containerDir))
    {
        outDiagnostic = "Directory lacks experiment.json or manifest.json required for verification";
        return false;
    }

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    if (!recordOpt.has_value() || recordOpt->status == core::ExperimentStatus::Corrupt)
    {
        outDiagnostic = readErr.isNotEmpty() ? readErr : "Integrity verification failed (Corrupt status)";
        return false;
    }

    outDiagnostic = "Verified: all artifacts match manifest SHA-256";
    return true;
}

bool MeasurementViewModelLoader::verifyAudioFileSha256(const juce::File& audioFile,
                                                      const juce::String& expectedSha256)
{
    if (!audioFile.existsAsFile())
        return false;

    if (expectedSha256.isEmpty())
        return false;

    std::string computedSha = core::ExperimentStorage::computeFileSha256(audioFile);
    return juce::String(computedSha).equalsIgnoreCase(expectedSha256.trim());
}

} // namespace abdaudiolab::gui::measurement
