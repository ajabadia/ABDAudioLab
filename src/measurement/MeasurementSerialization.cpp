/**
 * @file MeasurementSerialization.cpp
 * @brief Implementation of MeasurementSerialization.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementSerialization.h"
#include <nlohmann/json.hpp>
#include <cmath>

namespace abdaudiolab::measurement
{

using ordered_json = nlohmann::ordered_json;

bool MeasurementSerialization::validateSchema(const std::string& schemaVersion, 
                                             const std::string& schemaUri, 
                                             std::string& outError)
{
    if (schemaVersion != kExpectedSchemaVersion)
    {
        outError = "Unsupported schemaVersion: '" + schemaVersion + "' (expected: '" + kExpectedSchemaVersion + "')";
        return false;
    }

    if (schemaUri != kExpectedSchemaUri)
    {
        outError = "Unsupported schemaUri: '" + schemaUri + "' (expected: '" + kExpectedSchemaUri + "')";
        return false;
    }

    return true;
}

bool MeasurementSerialization::validateCurve(const MeasurementCurve& curve, 
                                            MeasurementStatus status, 
                                            std::string& outError)
{
    // 1. Dimension matching
    if (curve.x.size() != curve.y.size())
    {
        outError = "Curve dimension mismatch: X has " + std::to_string(curve.x.size()) +
                   " samples, but Y has " + std::to_string(curve.y.size()) + " samples";
        return false;
    }

    // 2. Completed measurement must not have an empty curve
    if (status == MeasurementStatus::completed && curve.x.empty())
    {
        outError = "Completed measurement cannot have an empty curve";
        return false;
    }

    // 3. If curve has data, units must be present
    if (!curve.x.empty())
    {
        if (curve.xUnit.trim().isEmpty() || curve.yUnit.trim().isEmpty())
        {
            outError = "Curve has samples but is missing required X or Y units";
            return false;
        }

        if (curve.xName.trim().isEmpty() || curve.yName.trim().isEmpty())
        {
            outError = "Curve has samples but is missing required X or Y variable names";
            return false;
        }
    }

    // 4. Numeric integrity (reject NaN and Inf)
    for (size_t i = 0; i < curve.x.size(); ++i)
    {
        double vx = curve.x[i];
        if (std::isnan(vx) || std::isinf(vx))
        {
            outError = "Curve X contains NaN or Infinity at sample index " + std::to_string(i);
            return false;
        }

        double vy = curve.y[i];
        if (std::isnan(vy) || std::isinf(vy))
        {
            outError = "Curve Y contains NaN or Infinity at sample index " + std::to_string(i);
            return false;
        }
    }

    return true;
}

static ordered_json stimulusToJson(const StimulusSpec& stim)
{
    ordered_json j;
    j["type"] = stimulusTypeToString(stim.type);
    j["startFreqHz"] = stim.startFreqHz;
    j["endFreqHz"] = stim.endFreqHz;
    j["durationSec"] = stim.durationSec;
    j["levelDbfs"] = stim.levelDbfs;
    j["phaseRad"] = stim.phaseRad;
    j["seed"] = stim.seed;
    j["midiChannel"] = stim.midiChannel;
    j["note"] = stim.midiNoteNumber;
    j["velocity"] = stim.midiVelocity;
    j["noteOnSample"] = stim.noteOnSample;
    j["noteOffSample"] = stim.noteOffSample;
    j["sha256"] = stim.sha256;
    return j;
}

static bool jsonToStimulus(const nlohmann::json& j, StimulusSpec& stim, std::string& outError)
{
    if (!j.is_object())
    {
        outError = "Stimulus is not a JSON object";
        return false;
    }

    stim.type = stimulusTypeFromString(j.value("type", "logSineSweep"));
    stim.startFreqHz = j.value("startFreqHz", 20.0f);
    stim.endFreqHz = j.value("endFreqHz", 20000.0f);
    stim.durationSec = j.value("durationSec", 2.0);
    stim.levelDbfs = j.value("levelDbfs", -6.0f);
    stim.phaseRad = j.value("phaseRad", 0.0f);
    stim.seed = j.value("seed", 0x48271983u);
    stim.midiChannel = j.value("midiChannel", 1);
    stim.midiNoteNumber = j.value("note", 60);
    stim.midiVelocity = j.value("velocity", 0.8f);
    stim.noteOnSample = j.value("noteOnSample", size_t(0));
    stim.noteOffSample = j.value("noteOffSample", size_t(0));
    stim.sha256 = j.value("sha256", "");
    return true;
}

std::string MeasurementSerialization::serializeStimulus(const StimulusSpec& stimulus, int indent)
{
    return stimulusToJson(stimulus).dump(indent);
}

bool MeasurementSerialization::deserializeStimulus(const std::string& jsonStr, 
                                                   StimulusSpec& outStimulus, 
                                                   std::string& outError)
{
    try
    {
        auto j = nlohmann::json::parse(jsonStr);
        return jsonToStimulus(j, outStimulus, outError);
    }
    catch (const std::exception& e)
    {
        outError = "JSON parse error in StimulusSpec: " + std::string(e.what());
        return false;
    }
}

std::string MeasurementSerialization::serializeSpec(const MeasurementSpec& spec, int indent)
{
    ordered_json j;
    j["schemaVersion"] = spec.schemaVersion;
    j["schemaUri"] = spec.schemaUri;
    j["measurementId"] = spec.measurementId;
    j["measurementType"] = spec.measurementType;
    j["dutType"] = deviceUnderTestToString(spec.dutType);
    j["parameterId"] = spec.parameterId;
    j["parameterName"] = spec.parameterName;

    ordered_json exec;
    exec["sampleRateHz"] = spec.execution.sampleRateHz;
    exec["blockSize"] = spec.execution.blockSize;
    exec["numChannels"] = spec.execution.numChannels;
    exec["numSamples"] = spec.execution.numSamples;
    exec["latencySamples"] = spec.execution.latencySamples;
    j["execution"] = exec;

    j["stimulus"] = stimulusToJson(spec.stimulus);

    ordered_json analysis;
    analysis["analysisType"] = spec.analysis.analysisType;
    ordered_json opts = ordered_json::object();
    for (const auto& opt : spec.analysis.options)
        opts[opt.first] = opt.second;
    analysis["options"] = opts;
    j["analysis"] = analysis;

    ordered_json rep;
    rep["numPasses"] = spec.repetition.numPasses;
    rep["stabilizationWaitMs"] = spec.repetition.stabilizationWaitMs;
    j["repetition"] = rep;

    return j.dump(indent);
}

bool MeasurementSerialization::deserializeSpec(const std::string& jsonStr, 
                                               MeasurementSpec& outSpec, 
                                               std::string& outError)
{
    try
    {
        auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_object())
        {
            outError = "MeasurementSpec root is not a JSON object";
            return false;
        }

        std::string sVer = j.value("schemaVersion", "");
        std::string sUri = j.value("schemaUri", "");
        if (!validateSchema(sVer, sUri, outError))
            return false;

        outSpec.schemaVersion = sVer;
        outSpec.schemaUri = sUri;
        outSpec.measurementId = j.value("measurementId", "");
        outSpec.measurementType = j.value("measurementType", "");
        outSpec.dutType = deviceUnderTestFromString(j.value("dutType", "unknown"));
        outSpec.parameterId = j.value("parameterId", "");
        outSpec.parameterName = j.value("parameterName", "");

        if (j.contains("execution") && j["execution"].is_object())
        {
            const auto& exec = j["execution"];
            outSpec.execution.sampleRateHz = exec.value("sampleRateHz", 48000.0);
            outSpec.execution.blockSize = exec.value("blockSize", 512);
            outSpec.execution.numChannels = exec.value("numChannels", 2);
            outSpec.execution.numSamples = exec.value("numSamples", int64_t(0));
            outSpec.execution.latencySamples = exec.value("latencySamples", 0);
        }

        if (j.contains("stimulus") && j["stimulus"].is_object())
        {
            if (!jsonToStimulus(j["stimulus"], outSpec.stimulus, outError))
                return false;
        }

        if (j.contains("analysis") && j["analysis"].is_object())
        {
            const auto& an = j["analysis"];
            outSpec.analysis.analysisType = an.value("analysisType", "");
            outSpec.analysis.options.clear();
            if (an.contains("options") && an["options"].is_object())
            {
                for (auto it = an["options"].begin(); it != an["options"].end(); ++it)
                {
                    outSpec.analysis.options.emplace_back(it.key(), it.value().get<std::string>());
                }
            }
        }

        if (j.contains("repetition") && j["repetition"].is_object())
        {
            const auto& rep = j["repetition"];
            outSpec.repetition.numPasses = rep.value("numPasses", 1);
            outSpec.repetition.stabilizationWaitMs = rep.value("stabilizationWaitMs", 50.0);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        outError = "JSON parse error in MeasurementSpec: " + std::string(e.what());
        return false;
    }
}

std::string MeasurementSerialization::serializeResult(const MeasurementResult& result, int indent)
{
    ordered_json j;
    j["schemaVersion"] = result.schemaVersion;
    j["schemaUri"] = result.schemaUri;
    j["measurementId"] = result.measurementId;
    j["measurementType"] = result.measurementType;
    j["status"] = measurementStatusToString(result.status);
    j["reason"] = result.reason;

    ordered_json dut;
    dut["name"] = result.dut.name;
    dut["format"] = result.dut.format;
    dut["version"] = result.dut.version;
    dut["type"] = result.dut.type;
    j["dut"] = dut;

    ordered_json exec;
    exec["sampleRateHz"] = result.execution.sampleRateHz;
    exec["blockSize"] = result.execution.blockSize;
    exec["numChannels"] = result.execution.numChannels;
    exec["numSamples"] = result.execution.numSamples;
    exec["latencySamples"] = result.execution.latencySamples;
    j["execution"] = exec;

    j["stimulus"] = stimulusToJson(result.stimulus);

    ordered_json analyzer;
    analyzer["name"] = result.analyzer.name;
    analyzer["version"] = result.analyzer.version;
    j["analyzer"] = analyzer;

    ordered_json obs;
    obs["status"] = result.observability.status;
    if (result.observability.reason.has_value())
        obs["reason"] = *result.observability.reason;
    else
        obs["reason"] = nullptr;
    j["observability"] = obs;

    ordered_json metrics = ordered_json::array();
    for (const auto& m : result.metrics)
    {
        ordered_json mj;
        mj["name"] = m.name.toStdString();
        mj["value"] = m.value;
        mj["unit"] = m.unit.toStdString();
        mj["status"] = m.status.toStdString();
        metrics.push_back(mj);
    }
    j["metrics"] = metrics;

    ordered_json curve;
    curve["xName"] = result.curve.xName.toStdString();
    curve["xUnit"] = result.curve.xUnit.toStdString();
    curve["yName"] = result.curve.yName.toStdString();
    curve["yUnit"] = result.curve.yUnit.toStdString();
    curve["sampleCount"] = result.curve.x.size();
    curve["x"] = result.curve.x;
    curve["y"] = result.curve.y;
    j["curve"] = curve;

    ordered_json art;
    art["audio"] = result.artifacts.audioPath;
    art["audioSha256"] = result.artifacts.audioSha256;
    j["artifacts"] = art;

    j["integrityVerified"] = result.integrityVerified;

    return j.dump(indent);
}

bool MeasurementSerialization::deserializeResult(const std::string& jsonStr, 
                                                 MeasurementResult& outResult, 
                                                 std::string& outError)
{
    try
    {
        auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_object())
        {
            outError = "MeasurementResult root is not a JSON object";
            return false;
        }

        std::string sVer = j.value("schemaVersion", "");
        std::string sUri = j.value("schemaUri", "");
        if (!validateSchema(sVer, sUri, outError))
            return false;

        outResult.schemaVersion = sVer;
        outResult.schemaUri = sUri;
        outResult.measurementId = j.value("measurementId", "");
        outResult.measurementType = j.value("measurementType", "");
        outResult.status = measurementStatusFromString(j.value("status", "failed"));
        outResult.reason = j.value("reason", "");

        if (j.contains("dut") && j["dut"].is_object())
        {
            const auto& d = j["dut"];
            outResult.dut.name = d.value("name", "");
            outResult.dut.format = d.value("format", "");
            outResult.dut.version = d.value("version", "");
            outResult.dut.type = d.value("type", "");
        }

        if (j.contains("execution") && j["execution"].is_object())
        {
            const auto& exec = j["execution"];
            outResult.execution.sampleRateHz = exec.value("sampleRateHz", 48000.0);
            outResult.execution.blockSize = exec.value("blockSize", 512);
            outResult.execution.numChannels = exec.value("numChannels", 2);
            outResult.execution.numSamples = exec.value("numSamples", int64_t(0));
            outResult.execution.latencySamples = exec.value("latencySamples", 0);
        }

        if (j.contains("stimulus") && j["stimulus"].is_object())
        {
            if (!jsonToStimulus(j["stimulus"], outResult.stimulus, outError))
                return false;
        }

        if (j.contains("analyzer") && j["analyzer"].is_object())
        {
            const auto& an = j["analyzer"];
            outResult.analyzer.name = an.value("name", "");
            outResult.analyzer.version = an.value("version", "");
        }

        if (j.contains("observability") && j["observability"].is_object())
        {
            const auto& obs = j["observability"];
            outResult.observability.status = obs.value("status", "observed");
            if (obs.contains("reason") && !obs["reason"].is_null())
                outResult.observability.reason = obs["reason"].get<std::string>();
            else
                outResult.observability.reason = std::nullopt;
        }

        outResult.metrics.clear();
        if (j.contains("metrics") && j["metrics"].is_array())
        {
            for (const auto& m : j["metrics"])
            {
                MeasurementMetric met;
                met.name = juce::String(m.value("name", ""));
                met.value = m.value("value", 0.0);
                met.unit = juce::String(m.value("unit", ""));
                met.status = juce::String(m.value("status", "observed"));
                outResult.metrics.push_back(met);
            }
        }

        if (j.contains("curve") && j["curve"].is_object())
        {
            const auto& c = j["curve"];
            outResult.curve.xName = juce::String(c.value("xName", ""));
            outResult.curve.xUnit = juce::String(c.value("xUnit", ""));
            outResult.curve.yName = juce::String(c.value("yName", ""));
            outResult.curve.yUnit = juce::String(c.value("yUnit", ""));
            outResult.curve.x.clear();
            outResult.curve.y.clear();

            if (c.contains("x") && c["x"].is_array())
            {
                outResult.curve.x = c["x"].get<std::vector<double>>();
            }
            if (c.contains("y") && c["y"].is_array())
            {
                outResult.curve.y = c["y"].get<std::vector<double>>();
            }
        }

        // Validate curve integrity
        if (!validateCurve(outResult.curve, outResult.status, outError))
            return false;

        if (j.contains("artifacts") && j["artifacts"].is_object())
        {
            const auto& art = j["artifacts"];
            outResult.artifacts.audioPath = art.value("audio", "");
            outResult.artifacts.audioSha256 = art.value("audioSha256", "");
        }

        outResult.integrityVerified = j.value("integrityVerified", false);

        return true;
    }
    catch (const std::exception& e)
    {
        outError = "JSON parse error in MeasurementResult: " + std::string(e.what());
        return false;
    }
}

} // namespace abdaudiolab::measurement
