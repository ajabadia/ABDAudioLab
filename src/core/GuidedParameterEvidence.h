/**
 * @file GuidedParameterEvidence.h
 * @brief Typed domain model and parser for guided single-parameter differential evidence.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace abdaudiolab::core
{

/**
 * @struct GuidedParameterEvidence
 * @brief Represents verified empirical differential audio evidence for a single target parameter.
 *        Completely decoupled from holdout neural/LUT model validation.
 */
struct GuidedParameterEvidence
{
    juce::String pluginName;
    juce::String pluginFormat;
    juce::String pluginVersion;
    juce::String presetName;

    juce::String parameterId;
    juce::String parameterName;
    int parameterIndex { -1 };

    double initialNormalized { 0.0 };
    double modifiedNormalized { 0.0 };
    juce::String initialDisplay;
    juce::String modifiedDisplay;

    int midiNote { -1 };
    int midiVelocity { -1 };
    int midiSampleOffset { -1 };
    int durationSamples { 0 };
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    int channels { 2 };

    double peakDifference { 0.0 };
    double rmse { 0.0 };
    double deltaRmsDb { 0.0 };
    double correlation { 0.0 };

    bool writeConfirmed { false };
    bool audioRendered { false };
    bool effectDetected { false };
    bool repeatabilityVerified { false };
    double repeatPeakDifference { 0.0 };
    double repeatRmse { 0.0 };

    // File locations and integrity
    juce::File baselineWav;
    juce::File modifiedWav;
    juce::File differenceWav;
    juce::File reportJsonFile;

    std::string baselineSha256;
    std::string modifiedSha256;
    std::string differenceSha256;
    std::string reportJsonSha256;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !parameterName.isEmpty() &&
               baselineWav.existsAsFile() &&
               modifiedWav.existsAsFile() &&
               differenceWav.existsAsFile() &&
               writeConfirmed &&
               audioRendered;
    }

    /**
     * @brief Parses and validates a parameter test JSON artifact from disk.
     * @param jsonFile The parameter test JSON file (e.g. evidence/guided/parameter-test-cutoff.json).
     * @param containerDir The root container directory of the experiment to resolve relative paths.
     * @param outError Diagnostic error string if validation fails.
     * @return std::optional<GuidedParameterEvidence>
     */
    [[nodiscard]] static std::optional<GuidedParameterEvidence> fromJsonFile(const juce::File& jsonFile,
                                                                            const juce::File& containerDir,
                                                                            juce::String& outError)
    {
        if (!jsonFile.existsAsFile())
        {
            outError = "Guided parameter report JSON not found: " + jsonFile.getFullPathName();
            return std::nullopt;
        }

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(jsonFile.loadFileAsString().toStdString());
        }
        catch (const std::exception& e)
        {
            outError = "Failed to parse guided parameter report JSON: " + juce::String(e.what());
            return std::nullopt;
        }

        if (root.value("schemaVersion", "") != "guided-parameter-test-1.0")
        {
            outError = "Unsupported or missing schemaVersion in guided parameter report";
            return std::nullopt;
        }

        GuidedParameterEvidence evidence;
        evidence.reportJsonFile = jsonFile;

        // Plugin
        if (root.contains("plugin"))
        {
            const auto& pl = root["plugin"];
            evidence.pluginName = juce::String(pl.value("name", ""));
            evidence.pluginFormat = juce::String(pl.value("format", ""));
            evidence.pluginVersion = juce::String(pl.value("version", ""));
            evidence.presetName = juce::String(pl.value("preset", ""));
        }

        // Parameter
        if (root.contains("parameter"))
        {
            const auto& param = root["parameter"];
            evidence.parameterId = juce::String(param.value("id", ""));
            evidence.parameterName = juce::String(param.value("name", ""));
            evidence.parameterIndex = param.value("index", -1);
            evidence.writeConfirmed = param.value("writeConfirmed", false);

            if (param.contains("initial"))
            {
                evidence.initialNormalized = param["initial"].value("normalized", 0.0);
                evidence.initialDisplay = juce::String(param["initial"].value("display", ""));
            }
            if (param.contains("requested"))
            {
                evidence.modifiedNormalized = param["requested"].value("normalized", 0.0);
                evidence.modifiedDisplay = juce::String(param["requested"].value("display", ""));
            }
        }

        // Render
        if (root.contains("render"))
        {
            const auto& rnd = root["render"];
            evidence.sampleRateHz = rnd.value("sampleRateHz", 48000.0);
            evidence.blockSize = rnd.value("blockSize", 512);
            evidence.channels = rnd.value("channels", 2);
            evidence.midiNote = rnd.value("note", -1);
            evidence.midiVelocity = rnd.value("velocity", -1);
            evidence.midiSampleOffset = rnd.value("midiSampleOffset", 0);
            evidence.durationSamples = rnd.value("durationSamples", 0);
        }

        // Comparison difference metrics
        if (root.contains("comparison") && root["comparison"].contains("difference"))
        {
            const auto& diff = root["comparison"]["difference"];
            evidence.rmse = diff.value("rmse", 0.0);
            evidence.deltaRmsDb = diff.value("deltaRmsDb", 0.0);
            evidence.correlation = diff.value("correlation", 0.0);
            evidence.peakDifference = diff.value("peakDifference", 0.0);
            evidence.effectDetected = diff.value("audibleChangeDetected", false);
        }

        // Conclusion
        if (root.contains("conclusion"))
        {
            evidence.audioRendered = root["conclusion"].value("audioRendered", false);
            if (!evidence.writeConfirmed)
                evidence.writeConfirmed = root["conclusion"].value("parameterWriteVerified", false);
        }

        // Repeatability
        if (root.contains("repeatability"))
        {
            const auto& rep = root["repeatability"];
            bool detA = rep.contains("conditionA_baseline") &&
                        (rep["conditionA_baseline"].value("deterministic", false) ||
                         rep["conditionA_baseline"].value("repeatPeakDifference", 1.0) < 1e-4);
            bool detB = rep.contains("conditionB_modified") &&
                        (rep["conditionB_modified"].value("deterministic", false) ||
                         rep["conditionB_modified"].value("repeatPeakDifference", 1.0) < 1e-4);
            evidence.repeatabilityVerified = detA && detB;
            if (rep.contains("conditionB_modified"))
            {
                evidence.repeatPeakDifference = rep["conditionB_modified"].value("repeatPeakDifference", 0.0);
                evidence.repeatRmse = rep["conditionB_modified"].value("repeatRmse", 0.0);
            }
        }

        // Artifacts
        if (root.contains("artifacts"))
        {
            const auto& arts = root["artifacts"];
            std::string basePath = arts.value("baseline", "");
            std::string modPath = arts.value("modified", "");
            std::string diffPath = arts.value("difference", "");

            evidence.baselineWav = containerDir.getChildFile(juce::String(basePath));
            evidence.modifiedWav = containerDir.getChildFile(juce::String(modPath));
            evidence.differenceWav = containerDir.getChildFile(juce::String(diffPath));

            // Si las rutas relativas en el JSON son 'guided/baseline.wav', pero se importaron a 'evidence/guided/baseline.wav':
            if (!evidence.baselineWav.existsAsFile())
                evidence.baselineWav = containerDir.getChildFile("evidence").getChildFile(juce::String(basePath));
            if (!evidence.modifiedWav.existsAsFile())
                evidence.modifiedWav = containerDir.getChildFile("evidence").getChildFile(juce::String(modPath));
            if (!evidence.differenceWav.existsAsFile())
                evidence.differenceWav = containerDir.getChildFile("evidence").getChildFile(juce::String(diffPath));

            // Si tampoco existen allí, buscar en la misma carpeta que el jsonFile:
            if (!evidence.baselineWav.existsAsFile())
                evidence.baselineWav = jsonFile.getParentDirectory().getChildFile("baseline.wav");
            if (!evidence.modifiedWav.existsAsFile())
                evidence.modifiedWav = jsonFile.getParentDirectory().getChildFile("modified.wav");
            if (!evidence.differenceWav.existsAsFile())
                evidence.differenceWav = jsonFile.getParentDirectory().getChildFile("difference.wav");
        }

        if (!evidence.baselineWav.existsAsFile() || !evidence.modifiedWav.existsAsFile() || !evidence.differenceWav.existsAsFile())
        {
            outError = "One or more guided differential WAV files are missing on disk";
            return std::nullopt;
        }

        return evidence;
    }
};

} // namespace abdaudiolab::core
