/**
 * @file GuidedParameterEvidence.h
 * @brief Typed domain model and parser for guided single-parameter differential evidence.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <string>
#include <optional>
#include <vector>
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
    double baselineRmse { 0.0 };
    double modifiedRmse { 0.0 };
    double repeatabilityTolerance { 1e-4 };

    // Status and Auditable Execution
    juce::String status { "completed" }; // "completed", "skipped", "failed"
    juce::String statusReason;

    // Semantic Mapping (never assumed as verified internal DSP architecture without explicit test)
    juce::String nameObserved;
    juce::String semanticRole;
    bool semanticRoleVerified { false };
    juce::String verificationMethod { "name_and_parameter_mapping" };

    // Explicit Waveform Correlation (Pearson temporal correlation between samples)
    double waveformCorrelation { 0.0 };

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
        if (status == "skipped")
            return !parameterName.isEmpty() || !parameterId.isEmpty();
        if (status == "failed")
            return true;
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

        // Status
        if (root.contains("status"))
        {
            if (root["status"].is_string())
                evidence.status = juce::String(root["status"].get<std::string>());
            else if (root["status"].is_object())
            {
                evidence.status = juce::String(root["status"].value("code", "completed"));
                evidence.statusReason = juce::String(root["status"].value("reason", ""));
            }
        }
        if (root.contains("reason"))
            evidence.statusReason = juce::String(root["reason"].get<std::string>());

        // Semantics
        if (root.contains("semantics"))
        {
            const auto& sem = root["semantics"];
            evidence.nameObserved = juce::String(sem.value("nameObserved", ""));
            evidence.semanticRole = juce::String(sem.value("semanticRole", ""));
            evidence.semanticRoleVerified = sem.value("semanticRoleVerified", false);
            evidence.verificationMethod = juce::String(sem.value("verificationMethod", "name_and_parameter_mapping"));
        }

        // Plugin
        if (root.contains("plugin"))
        {
            const auto& pl = root["plugin"];
            evidence.pluginName = juce::String(pl.value("name", ""));
            evidence.pluginFormat = juce::String(pl.value("format", ""));
            evidence.pluginVersion = juce::String(pl.value("version", ""));
        }

        // Preset
        if (root.contains("preset"))
        {
            if (root["preset"].is_object())
                evidence.presetName = juce::String(root["preset"].value("name", ""));
            else if (root["preset"].is_string())
                evidence.presetName = juce::String(root["preset"].get<std::string>());
        }
        else if (root.contains("plugin") && root["plugin"].contains("preset"))
        {
            evidence.presetName = juce::String(root["plugin"].value("preset", ""));
        }

        // Parameter
        if (root.contains("parameter"))
        {
            const auto& pr = root["parameter"];
            evidence.parameterId = juce::String(pr.value("paramId", pr.value("id", "")));
            evidence.parameterName = juce::String(pr.value("name", ""));
            evidence.parameterIndex = pr.value("index", -1);

            if (pr.contains("initial") && pr["initial"].is_object())
            {
                evidence.initialNormalized = pr["initial"].value("normalized", 0.0);
                evidence.initialDisplay = juce::String(pr["initial"].value("display", ""));
            }
            else
            {
                evidence.initialNormalized = pr.value("initialNormalized", 0.0);
                evidence.initialDisplay = juce::String(pr.value("initialDisplay", ""));
            }

            if (pr.contains("readBack") && pr["readBack"].is_object())
            {
                evidence.modifiedNormalized = pr["readBack"].value("normalized", 0.0);
                evidence.modifiedDisplay = juce::String(pr["readBack"].value("display", ""));
            }
            else if (pr.contains("requested") && pr["requested"].is_object())
            {
                evidence.modifiedNormalized = pr["requested"].value("normalized", 0.0);
                evidence.modifiedDisplay = juce::String(pr["requested"].value("display", ""));
            }
            else if (pr.contains("readBackNormalized"))
            {
                evidence.modifiedNormalized = pr.value("readBackNormalized", 0.0);
                evidence.modifiedDisplay = juce::String(pr.value("readBackDisplay", ""));
            }
            else
            {
                evidence.modifiedNormalized = pr.value("requestedNormalized", 0.0);
                evidence.modifiedDisplay = juce::String(pr.value("requestedDisplay", ""));
            }

            evidence.writeConfirmed = pr.value("writeConfirmed", false);
        }

        if (evidence.nameObserved.isEmpty())
            evidence.nameObserved = evidence.parameterName;

        // If skipped (e.g. parameter not found in plugin), no WAVs exist and that is auditable
        if (evidence.status == "skipped")
        {
            return evidence;
        }

        // Stimulus & Audio setup (support "render" and "stimulus")
        if (root.contains("render"))
        {
            const auto& rnd = root["render"];
            evidence.sampleRateHz = rnd.value("sampleRateHz", rnd.value("sampleRate", 48000.0));
            evidence.blockSize = rnd.value("blockSize", 512);
            evidence.channels = rnd.value("channels", 2);
            evidence.midiNote = rnd.value("note", rnd.value("midiNote", -1));
            evidence.midiVelocity = rnd.value("velocity", rnd.value("midiVelocity", -1));
            evidence.midiSampleOffset = rnd.value("midiSampleOffset", 0);
            evidence.durationSamples = rnd.value("durationSamples", 0);
        }
        else if (root.contains("stimulus"))
        {
            const auto& st = root["stimulus"];
            evidence.midiNote = st.value("midiNote", -1);
            evidence.midiVelocity = st.value("midiVelocity", -1);
            evidence.durationSamples = st.value("durationSamples", 0);
            evidence.sampleRateHz = st.value("sampleRate", 48000.0);
            evidence.blockSize = st.value("blockSize", 512);
            evidence.channels = st.value("channels", 2);
        }

        // Render flags & Conclusion
        if (root.contains("conclusion"))
        {
            const auto& c = root["conclusion"];
            evidence.audioRendered = c.value("audioRendered", false);
            if (!evidence.writeConfirmed)
                evidence.writeConfirmed = c.value("parameterWriteVerified", false);
        }
        if (root.contains("comparison"))
        {
            const auto& comp = root["comparison"];
            if (comp.contains("baseline") && comp.contains("modified"))
            {
                bool baseSilent = comp["baseline"].value("silent", true);
                bool modSilent = comp["modified"].value("silent", true);
                evidence.audioRendered = (!baseSilent && !modSilent);
            }
        }
        else if (root.contains("baseline") && root.contains("modified"))
        {
            bool baseSilent = root["baseline"].value("silent", true);
            bool modSilent = root["modified"].value("silent", true);
            evidence.audioRendered = (!baseSilent && !modSilent);
        }

        // Difference metrics (support "comparison.difference" and "difference")
        nlohmann::json diff;
        if (root.contains("comparison") && root["comparison"].contains("difference"))
            diff = root["comparison"]["difference"];
        else if (root.contains("difference"))
            diff = root["difference"];

        if (!diff.is_null())
        {
            evidence.rmse = diff.value("rmse", 0.0);
            evidence.deltaRmsDb = diff.value("deltaRmsDb", 0.0);
            evidence.correlation = diff.value("correlation", 0.0);
            evidence.waveformCorrelation = diff.value("waveformCorrelation", evidence.correlation);
            evidence.peakDifference = diff.value("peakDifference", 0.0);
            evidence.effectDetected = diff.value("audibleChangeDetected", false);
        }

        // Repeatability
        if (root.contains("repeatability"))
        {
            const auto& rep = root["repeatability"];
            bool detA = rep.contains("conditionA_baseline") ? rep["conditionA_baseline"].value("deterministic", false) : false;
            bool detB = rep.contains("conditionB_modified") ? rep["conditionB_modified"].value("deterministic", false) : false;
            evidence.repeatabilityVerified = detA && detB;
            if (rep.contains("conditionA_baseline"))
                evidence.baselineRmse = rep["conditionA_baseline"].value("repeatRmse", 0.0);
            if (rep.contains("conditionB_modified"))
            {
                evidence.repeatPeakDifference = rep["conditionB_modified"].value("repeatPeakDifference", 0.0);
                evidence.repeatRmse = rep["conditionB_modified"].value("repeatRmse", 0.0);
                evidence.modifiedRmse = evidence.repeatRmse;
            }
            evidence.repeatabilityTolerance = rep.value("tolerance", 1e-4);
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

            // Si se importaron a 'evidence/guided/<basePath>' (ej: evidence/guided/audio/...):
            if (!evidence.baselineWav.existsAsFile())
                evidence.baselineWav = containerDir.getChildFile("evidence").getChildFile("guided").getChildFile(juce::String(basePath));
            if (!evidence.modifiedWav.existsAsFile())
                evidence.modifiedWav = containerDir.getChildFile("evidence").getChildFile("guided").getChildFile(juce::String(modPath));
            if (!evidence.differenceWav.existsAsFile())
                evidence.differenceWav = containerDir.getChildFile("evidence").getChildFile("guided").getChildFile(juce::String(diffPath));

            // Si tampoco existen allí, buscar relativo al directorio contenedor de parameters (abuelo de jsonFile):
            if (!evidence.baselineWav.existsAsFile())
                evidence.baselineWav = jsonFile.getParentDirectory().getParentDirectory().getChildFile(juce::String(basePath));
            if (!evidence.modifiedWav.existsAsFile())
                evidence.modifiedWav = jsonFile.getParentDirectory().getParentDirectory().getChildFile(juce::String(modPath));
            if (!evidence.differenceWav.existsAsFile())
                evidence.differenceWav = jsonFile.getParentDirectory().getParentDirectory().getChildFile(juce::String(diffPath));

            // Si tampoco existen allí, buscar en la misma carpeta que el jsonFile:
            if (!evidence.baselineWav.existsAsFile())
                evidence.baselineWav = jsonFile.getParentDirectory().getChildFile("baseline.wav");
            if (!evidence.modifiedWav.existsAsFile())
                evidence.modifiedWav = jsonFile.getParentDirectory().getChildFile("modified.wav");
            if (!evidence.differenceWav.existsAsFile())
                evidence.differenceWav = jsonFile.getParentDirectory().getChildFile("difference.wav");
        }

        if (evidence.status != "skipped" && (!evidence.baselineWav.existsAsFile() || !evidence.modifiedWav.existsAsFile() || !evidence.differenceWav.existsAsFile()))
        {
            outError = "One or more guided differential WAV files are missing on disk";
            return std::nullopt;
        }

        return evidence;
    }
};

/**
 * @struct GuidedSessionEvidence
 * @brief Aggregate container representing an auditable multiparameter differential inspection session.
 */
struct GuidedSessionEvidence
{
    juce::String schemaVersion { "guided-session-evidence-1.0" };
    juce::String pluginName;
    juce::String pluginFormat;
    juce::String pluginVersion;
    juce::String presetName;

    std::vector<GuidedParameterEvidence> parameterTests;

    int plannedTestsCount { 0 };
    int completedTestsCount { 0 };
    int skippedTestsCount { 0 };
    int failedTestsCount { 0 };

    bool allTestsCompleted { false };
    bool integrityVerified { false };

    juce::File sessionJsonFile;
    std::string sessionJsonSha256;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !pluginName.isEmpty() &&
               plannedTestsCount > 0 &&
               (completedTestsCount + skippedTestsCount + failedTestsCount == plannedTestsCount);
    }

    [[nodiscard]] static std::optional<GuidedSessionEvidence> fromJsonFile(const juce::File& sessionJsonFile,
                                                                          const juce::File& containerDir,
                                                                          juce::String& outError)
    {
        if (!sessionJsonFile.existsAsFile())
        {
            outError = "Guided session report JSON not found: " + sessionJsonFile.getFullPathName();
            return std::nullopt;
        }

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(sessionJsonFile.loadFileAsString().toStdString());
        }
        catch (const std::exception& e)
        {
            outError = "Failed to parse guided session report JSON: " + juce::String(e.what());
            return std::nullopt;
        }

        if (root.value("schemaVersion", "") != "guided-session-evidence-1.0")
        {
            outError = "Unsupported or missing schemaVersion in guided session report";
            return std::nullopt;
        }

        GuidedSessionEvidence session;
        session.sessionJsonFile = sessionJsonFile;

        if (root.contains("plugin"))
        {
            const auto& pl = root["plugin"];
            session.pluginName = juce::String(pl.value("name", ""));
            session.pluginFormat = juce::String(pl.value("format", ""));
            session.pluginVersion = juce::String(pl.value("version", ""));
        }

        if (root.contains("preset"))
            session.presetName = juce::String(root["preset"].value("name", ""));

        if (root.contains("summary"))
        {
            const auto& sm = root["summary"];
            session.plannedTestsCount = sm.value("plannedTests", 0);
            session.completedTestsCount = sm.value("completedTests", 0);
            session.skippedTestsCount = sm.value("skippedTests", 0);
            session.failedTestsCount = sm.value("failedTests", 0);
            session.allTestsCompleted = sm.value("allTestsCompleted", false);
        }

        if (root.contains("tests") && root["tests"].is_array())
        {
            for (const auto& testItem : root["tests"])
            {
                std::string relReportPath = testItem.value("reportPath", "");
                if (relReportPath.empty())
                    continue;

                juce::File testJson = containerDir.getChildFile(juce::String(relReportPath));
                if (!testJson.existsAsFile())
                {
                    // Fallback to searching relative to sessionJsonFile parent
                    testJson = sessionJsonFile.getParentDirectory().getChildFile(juce::String(relReportPath));
                }
                if (!testJson.existsAsFile())
                {
                    // Fallback to searching inside evidence/guided/
                    testJson = containerDir.getChildFile("evidence").getChildFile("guided").getChildFile(juce::String(relReportPath));
                }

                juce::String paramErr;
                auto optParam = GuidedParameterEvidence::fromJsonFile(testJson, containerDir, paramErr);
                if (!optParam.has_value())
                {
                    outError = "Failed to parse parameter evidence item (" + juce::String(relReportPath) + "): " + paramErr;
                    return std::nullopt;
                }
                session.parameterTests.push_back(*optParam);
            }
        }

        // Semantic check: ensure declared count matches parsed elements
        int parsedTotal = static_cast<int>(session.parameterTests.size());
        int expectedTotal = session.completedTestsCount + session.skippedTestsCount + session.failedTestsCount;
        if (session.plannedTestsCount > 0 && parsedTotal != expectedTotal)
        {
            outError = "Semantic incoherency: parsed parameter tests (" + juce::String(parsedTotal) +
                       ") does not match summary total (" + juce::String(expectedTotal) + ")";
            return std::nullopt;
        }

        return session;
    }
};

/**
 * @struct ParameterTargetSpec
 * @brief Formal specification and candidate identifiers for a parameter to inspect.
 */
struct ParameterTargetSpec
{
    juce::String candidateId;             // e.g. "param_5"
    juce::String candidateName;           // e.g. "ALGORITHM"
    std::vector<juce::String> aliases;    // e.g. { "Algorithm", "DX7_ALGORITHM" }
    juce::String semanticRole;            // e.g. "fm_algorithm_routing"
    double baselineNormalized { 0.0 };
    double modifiedNormalized { 0.5 };
    int durationSamples { 48000 };
    int midiNote { 48 };
    int midiVelocity { 100 };
    int noteOffSample { 38400 };
};

/**
 * @struct ParameterResolutionResult
 * @brief Outcome of resolving a candidate parameter against an active AudioProcessor instance.
 */
struct ParameterResolutionResult
{
    bool found { false };
    juce::String paramId;
    juce::String nameObserved;
    int index { -1 };
    double currentNormalized { 0.0 };
    juce::String currentDisplay;
    juce::String semanticRole;
    bool semanticRoleVerified { false };
    juce::String verificationMethod;
    juce::String failureReason;
    juce::AudioProcessorParameter* parameter { nullptr };
};

/**
 * @class GuidedParameterResolver
 * @brief Resolves parameters against real plugin instances: ParamID -> exact name -> explicit alias -> failure.
 */
class GuidedParameterResolver
{
public:
    static ParameterResolutionResult resolve(juce::AudioProcessor* processor, const ParameterTargetSpec& spec)
    {
        ParameterResolutionResult res;
        res.semanticRole = spec.semanticRole;
        res.semanticRoleVerified = false;

        if (!processor)
        {
            res.failureReason = "Null processor instance";
            return res;
        }

        const auto& params = processor->getParameters();
        int numParams = params.size();

        // 1. ParamID exacto
        if (!spec.candidateId.isEmpty())
        {
            for (int i = 0; i < numParams; ++i)
            {
                auto* p = params[i];
                if (p == nullptr) continue;

                juce::String pid;
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                    pid = withId->paramID;
                else
                    pid = "param_" + juce::String(p->getParameterIndex());

                if (pid == spec.candidateId)
                {
                    res.found = true;
                    res.paramId = pid;
                    res.nameObserved = p->getName(64);
                    res.index = p->getParameterIndex();
                    res.currentNormalized = p->getValue();
                    res.currentDisplay = p->getCurrentValueAsText();
                    res.verificationMethod = "exact_param_id";
                    res.parameter = p;
                    return res;
                }
            }
        }

        // 2. Nombre exacto
        if (!spec.candidateName.isEmpty())
        {
            for (int i = 0; i < numParams; ++i)
            {
                auto* p = params[i];
                if (p == nullptr) continue;

                juce::String pName = p->getName(64);
                if (pName.equalsIgnoreCase(spec.candidateName))
                {
                    juce::String pid;
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                        pid = withId->paramID;
                    else
                        pid = "param_" + juce::String(p->getParameterIndex());

                    res.found = true;
                    res.paramId = pid;
                    res.nameObserved = pName;
                    res.index = p->getParameterIndex();
                    res.currentNormalized = p->getValue();
                    res.currentDisplay = p->getCurrentValueAsText();
                    res.verificationMethod = "exact_name";
                    res.parameter = p;
                    return res;
                }
            }
        }

        // 3. Alias explícitos
        for (const auto& alias : spec.aliases)
        {
            for (int i = 0; i < numParams; ++i)
            {
                auto* p = params[i];
                if (p == nullptr) continue;

                juce::String pName = p->getName(64);
                if (pName.equalsIgnoreCase(alias))
                {
                    juce::String pid;
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                        pid = withId->paramID;
                    else
                        pid = "param_" + juce::String(p->getParameterIndex());

                    res.found = true;
                    res.paramId = pid;
                    res.nameObserved = pName;
                    res.index = p->getParameterIndex();
                    res.currentNormalized = p->getValue();
                    res.currentDisplay = p->getCurrentValueAsText();
                    res.verificationMethod = "explicit_alias";
                    res.parameter = p;
                    return res;
                }
            }
        }

        // 4. Fallo documentado
        res.failureReason = "parameter_not_found";
        return res;
    }
};

} // namespace abdaudiolab::core
