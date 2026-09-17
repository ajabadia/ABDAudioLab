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

static ordered_json curveToJson(const MeasurementCurve& curve)
{
    ordered_json c;
    c["xName"] = curve.xName.toStdString();
    c["xUnit"] = curve.xUnit.toStdString();
    c["yName"] = curve.yName.toStdString();
    c["yUnit"] = curve.yUnit.toStdString();
    c["sampleCount"] = curve.x.size();
    c["x"] = curve.x;
    c["y"] = curve.y;
    return c;
}

static void jsonToCurve(const nlohmann::json& c, MeasurementCurve& curve)
{
    curve.xName = juce::String(c.value("xName", ""));
    curve.xUnit = juce::String(c.value("xUnit", ""));
    curve.yName = juce::String(c.value("yName", ""));
    curve.yUnit = juce::String(c.value("yUnit", ""));
    curve.x.clear();
    curve.y.clear();
    if (c.contains("x") && c["x"].is_array())
        curve.x = c["x"].get<std::vector<double>>();
    if (c.contains("y") && c["y"].is_array())
        curve.y = c["y"].get<std::vector<double>>();
}

static ordered_json curveFitToJson(const CurveFitMetadata& fit)
{
    ordered_json j;
    j["model"] = fit.model;
    j["rSquared"] = fit.rSquared;
    j["x"] = fit.xVariable;
    j["y"] = fit.yVariable;
    return j;
}

static CurveFitMetadata jsonToCurveFit(const nlohmann::json& j)
{
    CurveFitMetadata fit;
    fit.model = j.value("model", "none");
    fit.rSquared = j.value("rSquared", 0.0);
    fit.xVariable = j.value("x", "");
    fit.yVariable = j.value("y", "");
    return fit;
}

static ordered_json discontinuityToJson(const DiscontinuityObservation& d)
{
    ordered_json j;
    j["detected"] = d.detected;
    j["lowerVelocity"] = d.lowerVelocity;
    j["upperVelocity"] = d.upperVelocity;
    j["jumpDb"] = d.jumpDb;
    j["confidence"] = d.confidence;
    j["reason"] = d.reason.toStdString();
    return j;
}

static DiscontinuityObservation jsonToDiscontinuity(const nlohmann::json& j)
{
    DiscontinuityObservation d;
    d.detected = j.value("detected", false);
    d.lowerVelocity = j.value("lowerVelocity", 0);
    d.upperVelocity = j.value("upperVelocity", 0);
    d.jumpDb = j.value("jumpDb", 0.0);
    d.confidence = j.value("confidence", 0.0);
    d.reason = juce::String(j.value("reason", ""));
    return d;
}

static ordered_json spectralAnalysisToJson(const SpectralAnalysisMetadata& s)
{
    ordered_json j;
    j["fftSize"] = s.fftSize;
    j["hopSize"] = s.hopSize;
    j["window"] = s.window.toStdString();
    j["frequencyResolutionHz"] = s.frequencyResolutionHz;
    j["averagingCount"] = s.averagingCount;
    return j;
}

static SpectralAnalysisMetadata jsonToSpectralAnalysis(const nlohmann::json& j)
{
    SpectralAnalysisMetadata s;
    s.fftSize = j.value("fftSize", 0);
    s.hopSize = j.value("hopSize", 0);
    s.window = juce::String(j.value("window", "hann"));
    s.frequencyResolutionHz = j.value("frequencyResolutionHz", 0.0);
    s.averagingCount = j.value("averagingCount", 1);
    return s;
}

static ordered_json dynamicPointToJson(const DynamicPoint& pt)
{
    ordered_json j;
    j["velocity"] = pt.velocity;
    j["peakDbfs"] = pt.peakDbfs;
    j["rmsDbfs"] = pt.rmsDbfs;
    j["spectralCentroidHz"] = pt.spectralCentroidHz;
    j["spectralRolloffHz"] = pt.spectralRolloffHz;
    j["attackTimeMs"] = pt.attackTimeMs;
    j["status"] = pt.status;
    if (!pt.reason.empty())
        j["reason"] = pt.reason;
    j["measurementWindowStartMs"] = pt.measurementWindowStartMs;
    j["measurementWindowEndMs"] = pt.measurementWindowEndMs;
    if (!pt.presetStateHash.empty())
        j["presetStateHash"] = pt.presetStateHash;
    if (!pt.audioArtifactHash.empty())
        j["audioArtifactHash"] = pt.audioArtifactHash;
    if (!pt.metrics.empty())
    {
        ordered_json mets = ordered_json::array();
        for (const auto& m : pt.metrics)
        {
            ordered_json mj;
            mj["name"] = m.name.toStdString();
            mj["value"] = m.value;
            mj["unit"] = m.unit.toStdString();
            mj["status"] = m.status.toStdString();
            if (m.reason.isNotEmpty())
                mj["reason"] = m.reason.toStdString();
            mets.push_back(mj);
        }
        j["metrics"] = mets;
    }
    return j;
}

static DynamicPoint jsonToDynamicPoint(const nlohmann::json& j)
{
    DynamicPoint pt;
    pt.velocity = j.value("velocity", 0);
    pt.peakDbfs = j.value("peakDbfs", -96.0);
    pt.rmsDbfs = j.value("rmsDbfs", -96.0);
    pt.spectralCentroidHz = j.value("spectralCentroidHz", 0.0);
    pt.spectralRolloffHz = j.value("spectralRolloffHz", 0.0);
    pt.attackTimeMs = j.value("attackTimeMs", 0.0);
    pt.status = j.value("status", "observed");
    pt.reason = j.value("reason", "");
    pt.measurementWindowStartMs = j.value("measurementWindowStartMs", 0.0);
    pt.measurementWindowEndMs = j.value("measurementWindowEndMs", 0.0);
    pt.presetStateHash = j.value("presetStateHash", "");
    pt.audioArtifactHash = j.value("audioArtifactHash", "");
    if (j.contains("metrics") && j["metrics"].is_array())
    {
        for (const auto& m : j["metrics"])
        {
            MeasurementMetric met;
            met.name = juce::String(m.value("name", ""));
            met.value = m.value("value", 0.0);
            met.unit = juce::String(m.value("unit", ""));
            met.status = juce::String(m.value("status", "observed"));
            met.reason = juce::String(m.value("reason", ""));
            pt.metrics.push_back(met);
        }
    }
    return pt;
}

static ordered_json dynamicResultToJson(const DynamicResponseResult& dyn)
{
    ordered_json j;
    ordered_json pts = ordered_json::array();
    for (const auto& pt : dyn.points)
        pts.push_back(dynamicPointToJson(pt));
    j["points"] = pts;
    j["amplitudeCurve"] = curveToJson(dyn.amplitudeCurve);
    j["brightnessCurve"] = curveToJson(dyn.brightnessCurve);
    if (dyn.amplitudeFit.has_value())
        j["amplitudeFit"] = curveFitToJson(*dyn.amplitudeFit);
    if (dyn.brightnessFit.has_value())
        j["brightnessFit"] = curveFitToJson(*dyn.brightnessFit);
    if (dyn.spectralMetadata.has_value())
        j["spectralMetadata"] = spectralAnalysisToJson(*dyn.spectralMetadata);
    j["dynamicRangeDb"] = dyn.dynamicRangeDb;
    j["discontinuity"] = discontinuityToJson(dyn.discontinuity);
    return j;
}

static DynamicResponseResult jsonToDynamicResult(const nlohmann::json& j)
{
    DynamicResponseResult dyn;
    if (j.contains("points") && j["points"].is_array())
    {
        for (const auto& pj : j["points"])
            dyn.points.push_back(jsonToDynamicPoint(pj));
    }
    if (j.contains("amplitudeCurve") && j["amplitudeCurve"].is_object())
        jsonToCurve(j["amplitudeCurve"], dyn.amplitudeCurve);
    if (j.contains("brightnessCurve") && j["brightnessCurve"].is_object())
        jsonToCurve(j["brightnessCurve"], dyn.brightnessCurve);
    if (j.contains("amplitudeFit") && j["amplitudeFit"].is_object())
        dyn.amplitudeFit = jsonToCurveFit(j["amplitudeFit"]);
    if (j.contains("brightnessFit") && j["brightnessFit"].is_object())
        dyn.brightnessFit = jsonToCurveFit(j["brightnessFit"]);
    if (j.contains("spectralMetadata") && j["spectralMetadata"].is_object())
        dyn.spectralMetadata = jsonToSpectralAnalysis(j["spectralMetadata"]);
    dyn.dynamicRangeDb = j.value("dynamicRangeDb", 0.0);
    if (j.contains("discontinuity") && j["discontinuity"].is_object())
        dyn.discontinuity = jsonToDiscontinuity(j["discontinuity"]);
    return dyn;
}

static ordered_json modulationSidebandToJson(const ModulationSideband& sb)
{
    ordered_json j;
    j["carrierFrequencyHz"] = sb.carrierFrequencyHz;
    j["sidebandFrequencyHz"] = sb.sidebandFrequencyHz;
    j["order"] = sb.order;
    j["levelRelativeToCarrierDb"] = sb.levelRelativeToCarrierDb;
    return j;
}

static ModulationSideband jsonToModulationSideband(const nlohmann::json& j)
{
    ModulationSideband sb;
    sb.carrierFrequencyHz = j.value("carrierFrequencyHz", 0.0);
    sb.sidebandFrequencyHz = j.value("sidebandFrequencyHz", 0.0);
    sb.order = j.value("order", 1);
    sb.levelRelativeToCarrierDb = j.value("levelRelativeToCarrierDb", 0.0);
    return sb;
}

static ordered_json modulationResultToJson(const ModulationResultData& mod)
{
    ordered_json j;
    j["targetDestination"] = mod.targetDestination;
    ordered_json r;
    r["name"] = mod.rateHz.name.toStdString();
    r["value"] = mod.rateHz.value;
    r["unit"] = mod.rateHz.unit.toStdString();
    r["status"] = mod.rateHz.status.toStdString();
    if (mod.rateHz.reason.isNotEmpty())
        r["reason"] = mod.rateHz.reason.toStdString();
    j["rateHz"] = r;
    j["rateMethod"] = mod.rateMethod;

    ordered_json d;
    d["name"] = mod.depth.name.toStdString();
    d["value"] = mod.depth.value;
    d["unit"] = mod.depth.unit.toStdString();
    d["status"] = mod.depth.status.toStdString();
    if (mod.depth.reason.isNotEmpty())
        d["reason"] = mod.depth.reason.toStdString();
    j["depth"] = d;

    ordered_json w;
    w["waveform"] = mod.waveform.waveform;
    w["status"] = mod.waveform.status;
    w["confidence"] = mod.waveform.confidence;
    j["waveform"] = w;

    ordered_json sbs = ordered_json::array();
    for (const auto& sb : mod.sidebands)
        sbs.push_back(modulationSidebandToJson(sb));
    j["sidebands"] = sbs;

    j["spectralMetadata"] = spectralAnalysisToJson(mod.spectralMetadata);
    j["timeCurve"] = curveToJson(mod.timeCurve);
    j["spectrumCurve"] = curveToJson(mod.spectrumCurve);
    return j;
}

static ModulationResultData jsonToModulationResult(const nlohmann::json& j)
{
    ModulationResultData mod;
    mod.targetDestination = j.value("targetDestination", "unknown");
    if (j.contains("rateHz") && j["rateHz"].is_object())
    {
        const auto& r = j["rateHz"];
        mod.rateHz.name = juce::String(r.value("name", "rate"));
        mod.rateHz.value = r.value("value", 0.0);
        mod.rateHz.unit = juce::String(r.value("unit", "Hz"));
        mod.rateHz.status = juce::String(r.value("status", "observed"));
        mod.rateHz.reason = juce::String(r.value("reason", ""));
    }
    mod.rateMethod = j.value("rateMethod", "spectral_peak");

    if (j.contains("depth") && j["depth"].is_object())
    {
        const auto& d = j["depth"];
        mod.depth.name = juce::String(d.value("name", "depth"));
        mod.depth.value = d.value("value", 0.0);
        mod.depth.unit = juce::String(d.value("unit", "cents"));
        mod.depth.status = juce::String(d.value("status", "observed"));
        mod.depth.reason = juce::String(d.value("reason", ""));
    }

    if (j.contains("waveform") && j["waveform"].is_object())
    {
        const auto& w = j["waveform"];
        mod.waveform.waveform = w.value("waveform", "none");
        mod.waveform.status = w.value("status", "not_observable");
        mod.waveform.confidence = w.value("confidence", 0.0);
    }

    if (j.contains("spectralMetadata") && j["spectralMetadata"].is_object())
        mod.spectralMetadata = jsonToSpectralAnalysis(j["spectralMetadata"]);

    if (j.contains("sidebands") && j["sidebands"].is_array())
    {
        for (const auto& sb : j["sidebands"])
            mod.sidebands.push_back(jsonToModulationSideband(sb));
    }

    if (j.contains("timeCurve") && j["timeCurve"].is_object())
        jsonToCurve(j["timeCurve"], mod.timeCurve);
    if (j.contains("spectrumCurve") && j["spectrumCurve"].is_object())
        jsonToCurve(j["spectrumCurve"], mod.spectrumCurve);

    return mod;
}

std::string MeasurementSerialization::serializeSpec(const MeasurementSpec& spec, int indent)
{
    ordered_json j;
    j["schemaVersion"] = spec.schemaVersion;
    j["schemaUri"] = spec.schemaUri;
    j["measurementId"] = spec.measurementId;
    j["measurementType"] = spec.measurementType;
    j["measurementDomain"] = spec.measurementDomain;
    j["filterTopology"] = spec.filterTopology;
    j["modulationDestination"] = spec.modulationDestination;
    j["dutType"] = deviceUnderTestToString(spec.dutType);
    j["parameterId"] = spec.parameterId;
    j["parameterName"] = spec.parameterName;

    // Reproducibility
    j["presetStateHash"] = spec.presetStateHash;
    if (!spec.velocityGrid.empty())
        j["velocityGrid"] = spec.velocityGrid;
    j["measurementWindowStartMs"] = spec.measurementWindowStartMs;
    j["measurementWindowEndMs"] = spec.measurementWindowEndMs;

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
        outSpec.measurementDomain = j.value("measurementDomain", "directTransferFunction");
        outSpec.filterTopology = j.value("filterTopology", "unknown");
        outSpec.modulationDestination = j.value("modulationDestination", "unknown");
        outSpec.dutType = deviceUnderTestFromString(j.value("dutType", "unknown"));
        outSpec.parameterId = j.value("parameterId", "");
        outSpec.parameterName = j.value("parameterName", "");

        outSpec.presetStateHash = j.value("presetStateHash", "");
        outSpec.velocityGrid.clear();
        if (j.contains("velocityGrid") && j["velocityGrid"].is_array())
            outSpec.velocityGrid = j["velocityGrid"].get<std::vector<int>>();
        outSpec.measurementWindowStartMs = j.value("measurementWindowStartMs", 0.0);
        outSpec.measurementWindowEndMs = j.value("measurementWindowEndMs", 0.0);

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
    j["measurementDomain"] = result.measurementDomain;
    j["filterTopology"] = result.filterTopology;
    j["modulationDestination"] = result.modulationDestination;
    j["status"] = measurementStatusToString(result.status);
    j["reason"] = result.reason;

    // Reproducibility
    j["presetStateHash"] = result.presetStateHash;
    j["measurementWindowStartMs"] = result.measurementWindowStartMs;
    j["measurementWindowEndMs"] = result.measurementWindowEndMs;

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
        if (m.reason.isNotEmpty())
            mj["reason"] = m.reason.toStdString();
        metrics.push_back(mj);
    }
    j["metrics"] = metrics;

    if (result.slopeFit.has_value())
    {
        ordered_json sf;
        sf["frequencyStartHz"] = result.slopeFit->frequencyStartHz;
        sf["frequencyEndHz"] = result.slopeFit->frequencyEndHz;
        sf["rSquared"] = result.slopeFit->rSquared;
        sf["sampleCount"] = result.slopeFit->sampleCount;
        sf["selectionReason"] = result.slopeFit->selectionReason;
        j["slopeFit"] = sf;
    }

    ordered_json curve;
    curve["xName"] = result.curve.xName.toStdString();
    curve["xUnit"] = result.curve.xUnit.toStdString();
    curve["yName"] = result.curve.yName.toStdString();
    curve["yUnit"] = result.curve.yUnit.toStdString();
    curve["sampleCount"] = result.curve.x.size();
    curve["x"] = result.curve.x;
    curve["y"] = result.curve.y;
    j["curve"] = curve;

    if (result.dynamicResult.has_value())
        j["dynamicResult"] = dynamicResultToJson(*result.dynamicResult);

    if (result.modulationResult.has_value())
        j["modulationResult"] = modulationResultToJson(*result.modulationResult);

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
        outResult.measurementDomain = j.value("measurementDomain", "directTransferFunction");
        outResult.filterTopology = j.value("filterTopology", "unknown");
        outResult.modulationDestination = j.value("modulationDestination", "unknown");
        outResult.status = measurementStatusFromString(j.value("status", "failed"));
        outResult.reason = j.value("reason", "");

        outResult.presetStateHash = j.value("presetStateHash", "");
        outResult.measurementWindowStartMs = j.value("measurementWindowStartMs", 0.0);
        outResult.measurementWindowEndMs = j.value("measurementWindowEndMs", 0.0);

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
                met.reason = juce::String(m.value("reason", ""));
                outResult.metrics.push_back(met);
            }
        }

        if (j.contains("slopeFit") && j["slopeFit"].is_object())
        {
            const auto& sf = j["slopeFit"];
            SlopeFitMetadata s;
            s.frequencyStartHz = sf.value("frequencyStartHz", 0.0);
            s.frequencyEndHz = sf.value("frequencyEndHz", 0.0);
            s.rSquared = sf.value("rSquared", 0.0);
            s.sampleCount = sf.value("sampleCount", 0);
            s.selectionReason = sf.value("selectionReason", "");
            outResult.slopeFit = s;
        }
        else
        {
            outResult.slopeFit = std::nullopt;
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

        if (j.contains("dynamicResult") && j["dynamicResult"].is_object())
            outResult.dynamicResult = jsonToDynamicResult(j["dynamicResult"]);
        else
            outResult.dynamicResult = std::nullopt;

        if (j.contains("modulationResult") && j["modulationResult"].is_object())
            outResult.modulationResult = jsonToModulationResult(j["modulationResult"]);
        else
            outResult.modulationResult = std::nullopt;

        // Validate curve integrity if general curve is present or if no specialized payload curves exist
        bool hasPayloadCurve = (outResult.dynamicResult.has_value() && !outResult.dynamicResult->amplitudeCurve.x.empty()) ||
                               (outResult.modulationResult.has_value() && (!outResult.modulationResult->timeCurve.x.empty() || !outResult.modulationResult->spectrumCurve.x.empty()));
        if (!outResult.curve.x.empty() || !hasPayloadCurve)
        {
            if (!validateCurve(outResult.curve, outResult.status, outError))
                return false;
        }

        if (outResult.dynamicResult.has_value())
        {
            if (!outResult.dynamicResult->amplitudeCurve.x.empty())
            {
                if (!validateCurve(outResult.dynamicResult->amplitudeCurve, outResult.status, outError))
                    return false;
            }
            if (!outResult.dynamicResult->brightnessCurve.x.empty())
            {
                if (!validateCurve(outResult.dynamicResult->brightnessCurve, outResult.status, outError))
                    return false;
            }
        }

        if (outResult.modulationResult.has_value())
        {
            if (!outResult.modulationResult->timeCurve.x.empty())
            {
                if (!validateCurve(outResult.modulationResult->timeCurve, outResult.status, outError))
                    return false;
            }
            if (!outResult.modulationResult->spectrumCurve.x.empty())
            {
                if (!validateCurve(outResult.modulationResult->spectrumCurve, outResult.status, outError))
                    return false;
            }
        }

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

std::string MeasurementSerialization::serializeDynamicResult(const DynamicResponseResult& res, int indent)
{
    return dynamicResultToJson(res).dump(indent);
}

bool MeasurementSerialization::deserializeDynamicResult(const std::string& jsonStr, 
                                                         DynamicResponseResult& outRes, 
                                                         std::string& outError)
{
    try
    {
        auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_object())
        {
            outError = "DynamicResponseResult root is not a JSON object";
            return false;
        }
        outRes = jsonToDynamicResult(j);
        return true;
    }
    catch (const std::exception& e)
    {
        outError = "JSON parse error in DynamicResponseResult: " + std::string(e.what());
        return false;
    }
}

std::string MeasurementSerialization::serializeModulationResult(const ModulationResultData& res, int indent)
{
    return modulationResultToJson(res).dump(indent);
}

bool MeasurementSerialization::deserializeModulationResult(const std::string& jsonStr, 
                                                           ModulationResultData& outRes, 
                                                           std::string& outError)
{
    try
    {
        auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_object())
        {
            outError = "ModulationResultData root is not a JSON object";
            return false;
        }
        outRes = jsonToModulationResult(j);
        return true;
    }
    catch (const std::exception& e)
    {
        outError = "JSON parse error in ModulationResultData: " + std::string(e.what());
        return false;
    }
}

} // namespace abdaudiolab::measurement
