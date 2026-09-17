/**
 * @file ComplexEnvelopeContracts.h
 * @brief Canonical data contracts, validators and RFC 8785 serialization for multi-domain complex envelopes.
 * @author ABDSynths
 * @date 2026
 *
 * Implements DUT-agnostic contracts for temporal envelope characterization
 * across pitch (DCO), timbre / phase distortion (DCW), and amplitude (DCA),
 * separating observed physical trajectories from inferred stages and native parameters.
 */

#pragma once

#include "MeasurementContracts.h"
#include "DexedParametricCampaignContracts.h"
#include "../synth/Sha256.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <cmath>

namespace abdaudiolab::measurement
{

/**
 * @brief Domain of envelope observable trajectory.
 */
enum class EnvelopeDomain
{
    Pitch,       // DCO / Pitch EG (Hz, cents, semitones)
    Timbre,      // DCW / Phase Distortion / Spectral Centroid (spectralCentroidHz, rolloffHz, phaseDistortionProxy)
    Amplitude,   // DCA / Amplitude / RMS (dBFS, normalized)
    Unknown
};

[[nodiscard]] inline std::string envelopeDomainToString(EnvelopeDomain d) noexcept
{
    switch (d)
    {
        case EnvelopeDomain::Pitch:     return "Pitch";
        case EnvelopeDomain::Timbre:    return "Timbre";
        case EnvelopeDomain::Amplitude: return "Amplitude";
        case EnvelopeDomain::Unknown:   return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] inline EnvelopeDomain envelopeDomainFromString(const std::string& s) noexcept
{
    if (s == "Pitch")     return EnvelopeDomain::Pitch;
    if (s == "Timbre")    return EnvelopeDomain::Timbre;
    if (s == "Amplitude") return EnvelopeDomain::Amplitude;
    return EnvelopeDomain::Unknown;
}

/**
 * @brief Strict validation of allowable units per physical domain.
 */
[[nodiscard]] inline bool isUnitCompatibleWithDomain(EnvelopeDomain domain, const std::string& unit) noexcept
{
    if (domain == EnvelopeDomain::Pitch)
    {
        return unit == "Hz" || unit == "cents" || unit == "semitones";
    }
    if (domain == EnvelopeDomain::Timbre)
    {
        return unit == "spectralCentroidHz" || unit == "rolloffHz" || unit == "phaseDistortionProxy";
    }
    if (domain == EnvelopeDomain::Amplitude)
    {
        return unit == "dBFS" || unit == "normalized";
    }
    return false; // Unknown domain is never compatible with physical measurement units
}

/**
 * @brief Explicit declared temporal grid for alignment between multiple observables.
 */
struct TemporalGrid
{
    std::string gridId;
    double originMs { 0.0 };
    double hopMs { 0.0 };
    int frameCount { 0 };
    std::string alignmentMethod { "stft_hop_synchronous" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["alignmentMethod"] = alignmentMethod;
        j["frameCount"] = frameCount;
        j["gridId"] = gridId;
        j["hopMs"] = hopMs;
        j["originMs"] = originMs;
        return j;
    }

    static TemporalGrid fromJson(const nlohmann::json& j)
    {
        TemporalGrid g;
        g.alignmentMethod = j.value("alignmentMethod", "stft_hop_synchronous");
        g.frameCount = j.value("frameCount", 0);
        g.gridId = j.value("gridId", "");
        g.hopMs = j.value("hopMs", 0.0);
        g.originMs = j.value("originMs", 0.0);
        return g;
    }
};

/**
 * @brief Discrete observation point in a temporal trajectory.
 */
struct EnvelopeObservationPoint
{
    int frameIndex { 0 };
    double timeMs { 0.0 };
    std::optional<double> value;
    std::string unit;
    std::string status { "valid" };     // "valid", "below_noise_floor", "unreliable", "silence", "not_observable"
    double resolution { 0.01 };
    std::string uncertaintyStatus { "not_estimated" }; // "not_estimated", "evaluated"
    std::optional<double> uncertaintyValue;
    std::string uncertaintyMethod;

    // Metrology & observability extensions (T20.11.7-2)
    double confidence { 1.0 };
    std::string reason;                 // e.g. "ambiguous_f0", "silence", "noise_floor"
    std::optional<int> peakBin;
    std::optional<double> interpolatedBin;
    std::string voicedStatus;           // "voiced", "unvoiced", "silence", "not_evaluated"
    std::string normalizationReference; // "peak_observed", "full_scale", "calibration_reference"
    std::optional<double> rmsDbfs;
    std::optional<double> amplitudeNormalized;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        if (amplitudeNormalized.has_value())
            j["amplitudeNormalized"] = *amplitudeNormalized;
        else
            j["amplitudeNormalized"] = nullptr;
        j["confidence"] = confidence;
        j["frameIndex"] = frameIndex;
        if (interpolatedBin.has_value())
            j["interpolatedBin"] = *interpolatedBin;
        else
            j["interpolatedBin"] = nullptr;
        j["normalizationReference"] = normalizationReference;
        if (peakBin.has_value())
            j["peakBin"] = *peakBin;
        else
            j["peakBin"] = nullptr;
        j["reason"] = reason;
        j["resolution"] = resolution;
        if (rmsDbfs.has_value())
            j["rmsDbfs"] = *rmsDbfs;
        else
            j["rmsDbfs"] = nullptr;
        j["status"] = status;
        j["timeMs"] = timeMs;
        j["uncertaintyMethod"] = uncertaintyMethod;
        j["uncertaintyStatus"] = uncertaintyStatus;
        if (uncertaintyValue.has_value())
            j["uncertaintyValue"] = *uncertaintyValue;
        else
            j["uncertaintyValue"] = nullptr;
        j["unit"] = unit;
        if (value.has_value())
            j["value"] = *value;
        else
            j["value"] = nullptr;
        j["voicedStatus"] = voicedStatus;
        return j;
    }

    static EnvelopeObservationPoint fromJson(const nlohmann::json& j)
    {
        EnvelopeObservationPoint p;
        p.frameIndex = j.value("frameIndex", 0);
        p.resolution = j.value("resolution", 0.01);
        p.status = j.value("status", "valid");
        p.timeMs = j.value("timeMs", 0.0);
        p.uncertaintyMethod = j.value("uncertaintyMethod", "");
        p.uncertaintyStatus = j.value("uncertaintyStatus", "not_estimated");
        if (j.contains("uncertaintyValue") && !j["uncertaintyValue"].is_null())
            p.uncertaintyValue = j["uncertaintyValue"].get<double>();
        p.unit = j.value("unit", "");
        if (j.contains("value") && !j["value"].is_null())
            p.value = j["value"].get<double>();

        p.confidence = j.value("confidence", 1.0);
        p.reason = j.value("reason", "");
        if (j.contains("peakBin") && !j["peakBin"].is_null())
            p.peakBin = j["peakBin"].get<int>();
        if (j.contains("interpolatedBin") && !j["interpolatedBin"].is_null())
            p.interpolatedBin = j["interpolatedBin"].get<double>();
        p.voicedStatus = j.value("voicedStatus", "");
        p.normalizationReference = j.value("normalizationReference", "");
        if (j.contains("rmsDbfs") && !j["rmsDbfs"].is_null())
            p.rmsDbfs = j["rmsDbfs"].get<double>();
        if (j.contains("amplitudeNormalized") && !j["amplitudeNormalized"].is_null())
            p.amplitudeNormalized = j["amplitudeNormalized"].get<double>();
        return p;
    }
};

/**
 * @brief Segment/stage descriptor with explicit parameterization semantics (ADSR, CZ 8-stage, DX7 6-stage).
 */
struct EnvelopeStageDescriptor
{
    int stageIndex { 1 };               // 1..8
    std::string parameterization { "rate_level" }; // "rate_level", "time_target", "adsr", "unknown"
    double durationMs { 0.0 };
    std::optional<double> targetLevel;
    std::optional<double> rateOrSlope;
    std::string levelUnit;
    bool isSustainPoint { false };
    bool isEndKeyOnPoint { false };
    std::string status { "valid" };

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["durationMs"] = durationMs;
        j["isEndKeyOnPoint"] = isEndKeyOnPoint;
        j["isSustainPoint"] = isSustainPoint;
        j["levelUnit"] = levelUnit;
        j["parameterization"] = parameterization;
        if (rateOrSlope.has_value())
            j["rateOrSlope"] = *rateOrSlope;
        else
            j["rateOrSlope"] = nullptr;
        j["stageIndex"] = stageIndex;
        j["status"] = status;
        if (targetLevel.has_value())
            j["targetLevel"] = *targetLevel;
        else
            j["targetLevel"] = nullptr;
        return j;
    }

    static EnvelopeStageDescriptor fromJson(const nlohmann::json& j)
    {
        EnvelopeStageDescriptor s;
        s.durationMs = j.value("durationMs", 0.0);
        s.isEndKeyOnPoint = j.value("isEndKeyOnPoint", false);
        s.isSustainPoint = j.value("isSustainPoint", false);
        s.levelUnit = j.value("levelUnit", "");
        s.parameterization = j.value("parameterization", "rate_level");
        if (j.contains("rateOrSlope") && !j["rateOrSlope"].is_null())
            s.rateOrSlope = j["rateOrSlope"].get<double>();
        s.stageIndex = j.value("stageIndex", 1);
        s.status = j.value("status", "valid");
        if (j.contains("targetLevel") && !j["targetLevel"].is_null())
            s.targetLevel = j["targetLevel"].get<double>();
        return s;
    }
};

/**
 * @brief Analysis metadata for spectral extraction methods.
 */
struct EnvelopeSpectralMetadata
{
    double sampleRateHz { 48000.0 };
    int hopSizeSamples { 256 };
    int fftSize { 2048 };
    int windowLengthSamples { 1024 };
    std::string windowFunction { "Hann" };
    std::optional<double> noiseFloorDbfs;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["fftSize"] = fftSize;
        j["hopSizeSamples"] = hopSizeSamples;
        if (noiseFloorDbfs.has_value())
            j["noiseFloorDbfs"] = *noiseFloorDbfs;
        else
            j["noiseFloorDbfs"] = nullptr;
        j["sampleRateHz"] = sampleRateHz;
        j["windowFunction"] = windowFunction;
        j["windowLengthSamples"] = windowLengthSamples;
        return j;
    }

    static EnvelopeSpectralMetadata fromJson(const nlohmann::json& j)
    {
        EnvelopeSpectralMetadata m;
        m.fftSize = j.value("fftSize", 2048);
        m.hopSizeSamples = j.value("hopSizeSamples", 256);
        if (j.contains("noiseFloorDbfs") && !j["noiseFloorDbfs"].is_null())
            m.noiseFloorDbfs = j["noiseFloorDbfs"].get<double>();
        m.sampleRateHz = j.value("sampleRateHz", 48000.0);
        m.windowFunction = j.value("windowFunction", "Hann");
        m.windowLengthSamples = j.value("windowLengthSamples", 1024);
        return m;
    }
};

/**
 * @brief Complete trajectory record of an observable physical envelope domain.
 */
struct EnvelopeTrajectory
{
    EnvelopeDomain domain { EnvelopeDomain::Unknown };
    std::string trajectoryLabel;
    std::string temporalGridId;
    std::string alignmentStatus { "aligned" }; // "aligned", "not_aligned"
    std::vector<EnvelopeObservationPoint> points;
    std::optional<double> attackTimeMs;
    std::optional<double> releaseTimeMs;
    std::string extractionMethod;       // "stft_instantaneous_freq", "spectral_centroid_tracking", "rms_windowed"

    // Honestidad metrológica: segregación entre observado, inferido y parámetros nativos
    std::string nativeEnvelopeReconstruction { "not_claimed" }; // "not_claimed", "inferred_approximate", "verified_exact"
    std::optional<TargetParameterBinding> nativeBinding;
    std::vector<EnvelopeStageDescriptor> inferredStages;

    // Metrología temporal y de normalización (T20.11.7-2)
    std::string normalizationReference { "peak_observed" };
    std::string noteOffMethod { "provided_midi_event" }; // "provided_midi_event", "energy_decay", "manual_marker", "not_available"
    std::string noteOnMethod { "provided_midi_event" };  // "provided_midi_event", "energy_onset", "manual_marker", "not_available"
    std::string phaseDistortionProxy { "not_claimed" };  // "not_claimed", "evaluated"

    EnvelopeSpectralMetadata spectralMetadata;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["alignmentStatus"] = alignmentStatus;
        if (attackTimeMs.has_value())
            j["attackTimeMs"] = *attackTimeMs;
        else
            j["attackTimeMs"] = nullptr;
        j["domain"] = envelopeDomainToString(domain);
        j["extractionMethod"] = extractionMethod;

        nlohmann::ordered_json stagesJson = nlohmann::ordered_json::array();
        for (const auto& s : inferredStages)
            stagesJson.push_back(s.toCanonicalJson());
        j["inferredStages"] = stagesJson;

        if (nativeBinding.has_value())
            j["nativeBinding"] = nativeBinding->toJson();
        else
            j["nativeBinding"] = nullptr;

        j["nativeEnvelopeReconstruction"] = nativeEnvelopeReconstruction;
        j["normalizationReference"] = normalizationReference;
        j["noteOffMethod"] = noteOffMethod;
        j["noteOnMethod"] = noteOnMethod;
        j["phaseDistortionProxy"] = phaseDistortionProxy;

        nlohmann::ordered_json ptsJson = nlohmann::ordered_json::array();
        for (const auto& p : points)
            ptsJson.push_back(p.toCanonicalJson());
        j["points"] = ptsJson;

        if (releaseTimeMs.has_value())
            j["releaseTimeMs"] = *releaseTimeMs;
        else
            j["releaseTimeMs"] = nullptr;

        j["spectralMetadata"] = spectralMetadata.toCanonicalJson();
        j["temporalGridId"] = temporalGridId;
        j["trajectoryLabel"] = trajectoryLabel;
        return j;
    }

    [[nodiscard]] std::string serializeCanonicalJson() const
    {
        return toCanonicalJson().dump();
    }

    [[nodiscard]] std::string computeCanonicalSha256() const
    {
        return abdaudiolab::synth::Sha256::computeHex(serializeCanonicalJson());
    }

    static EnvelopeTrajectory fromJson(const nlohmann::json& j)
    {
        EnvelopeTrajectory t;
        t.alignmentStatus = j.value("alignmentStatus", "aligned");
        if (j.contains("attackTimeMs") && !j["attackTimeMs"].is_null())
            t.attackTimeMs = j["attackTimeMs"].get<double>();
        t.domain = envelopeDomainFromString(j.value("domain", "Unknown"));
        t.extractionMethod = j.value("extractionMethod", "");
        if (j.contains("inferredStages") && j["inferredStages"].is_array())
        {
            for (const auto& sj : j["inferredStages"])
                t.inferredStages.push_back(EnvelopeStageDescriptor::fromJson(sj));
        }
        if (j.contains("nativeBinding") && j["nativeBinding"].is_object())
        {
            TargetParameterBinding b;
            b.logicalName = j["nativeBinding"].value("logicalName", "");
            b.nativeId = j["nativeBinding"].value("nativeId", "");
            b.unit = j["nativeBinding"].value("unit", "");
            b.mappingVersion = j["nativeBinding"].value("mappingVersion", "1.0");
            t.nativeBinding = b;
        }
        t.nativeEnvelopeReconstruction = j.value("nativeEnvelopeReconstruction", "not_claimed");
        t.normalizationReference = j.value("normalizationReference", "peak_observed");
        t.noteOffMethod = j.value("noteOffMethod", "provided_midi_event");
        t.noteOnMethod = j.value("noteOnMethod", "provided_midi_event");
        t.phaseDistortionProxy = j.value("phaseDistortionProxy", "not_claimed");

        if (j.contains("points") && j["points"].is_array())
        {
            for (const auto& pj : j["points"])
                t.points.push_back(EnvelopeObservationPoint::fromJson(pj));
        }
        if (j.contains("releaseTimeMs") && !j["releaseTimeMs"].is_null())
            t.releaseTimeMs = j["releaseTimeMs"].get<double>();
        if (j.contains("spectralMetadata"))
            t.spectralMetadata = EnvelopeSpectralMetadata::fromJson(j["spectralMetadata"]);
        t.temporalGridId = j.value("temporalGridId", "");
        t.trajectoryLabel = j.value("trajectoryLabel", "");
        return t;
    }
};

/**
 * @brief Ground truth descriptor for synthetic control validation and calibration.
 */
struct SyntheticEnvelopeGroundTruth
{
    std::vector<EnvelopeStageDescriptor> pitchStages;
    std::vector<EnvelopeStageDescriptor> timbreStages;
    std::vector<EnvelopeStageDescriptor> amplitudeStages;
    double expectedF0ToleranceHz { 5.0 };
    double expectedCentroidToleranceHz { 50.0 };
    double expectedRmsToleranceDb { 2.0 };
};

/**
 * @brief Multi-domain envelope capture synchronizing pitch, timbre and amplitude.
 */
struct MultiDomainEnvelopeCaptureRecord
{
    std::string captureId;
    DutIdentity dut;
    TemporalGrid temporalGrid;
    double excitationFrequencyHz { 261.6256 }; // C4 default
    int midiVelocity { 100 };
    double noteDurationMs { 1000.0 };
    double totalDurationMs { 2000.0 };

    EnvelopeTrajectory pitchTrajectory;      // DCO
    EnvelopeTrajectory timbreTrajectory;     // DCW
    EnvelopeTrajectory amplitudeTrajectory;  // DCA

    std::string audioSha256;
    std::string stimulusSha256;
    std::string nativePatchStateSha256;

    [[nodiscard]] nlohmann::ordered_json toCanonicalJson() const
    {
        nlohmann::ordered_json j;
        j["amplitudeTrajectory"] = amplitudeTrajectory.toCanonicalJson();
        j["audioSha256"] = audioSha256;
        j["captureId"] = captureId;
        j["dut"] = dutIdentityToJson(dut);
        j["excitationFrequencyHz"] = excitationFrequencyHz;
        j["midiVelocity"] = midiVelocity;
        j["nativePatchStateSha256"] = nativePatchStateSha256;
        j["noteDurationMs"] = noteDurationMs;
        j["pitchTrajectory"] = pitchTrajectory.toCanonicalJson();
        j["stimulusSha256"] = stimulusSha256;
        j["temporalGrid"] = temporalGrid.toCanonicalJson();
        j["timbreTrajectory"] = timbreTrajectory.toCanonicalJson();
        j["totalDurationMs"] = totalDurationMs;
        return j;
    }

    [[nodiscard]] std::string serializeCanonicalJson() const
    {
        return toCanonicalJson().dump();
    }

    [[nodiscard]] std::string computeCanonicalSha256() const
    {
        return abdaudiolab::synth::Sha256::computeHex(serializeCanonicalJson());
    }

    static MultiDomainEnvelopeCaptureRecord fromJson(const nlohmann::json& j)
    {
        MultiDomainEnvelopeCaptureRecord r;
        if (j.contains("amplitudeTrajectory"))
            r.amplitudeTrajectory = EnvelopeTrajectory::fromJson(j["amplitudeTrajectory"]);
        r.audioSha256 = j.value("audioSha256", "");
        r.captureId = j.value("captureId", "");
        if (j.contains("dut"))
        {
            const auto& dj = j["dut"];
            r.dut.name = dj.value("name", "");
            r.dut.format = dj.value("format", "");
            r.dut.version = dj.value("version", "");
            r.dut.type = dj.value("type", "");
            r.dut.dutType = dj.value("dutType", "vst3");
            r.dut.vendor = dj.value("vendor", "");
            r.dut.model = dj.value("model", "");
            r.dut.instanceId = dj.value("instanceId", "");
            r.dut.binarySha256 = dj.value("binarySha256", "");
            r.dut.firmwareSha256 = dj.value("firmwareSha256", "");
            r.dut.stateSha256 = dj.value("stateSha256", "");
            r.dut.interfaceId = dj.value("interfaceId", "");
        }
        r.excitationFrequencyHz = j.value("excitationFrequencyHz", 261.6256);
        r.midiVelocity = j.value("midiVelocity", 100);
        r.nativePatchStateSha256 = j.value("nativePatchStateSha256", "");
        r.noteDurationMs = j.value("noteDurationMs", 1000.0);
        if (j.contains("pitchTrajectory"))
            r.pitchTrajectory = EnvelopeTrajectory::fromJson(j["pitchTrajectory"]);
        r.stimulusSha256 = j.value("stimulusSha256", "");
        if (j.contains("temporalGrid"))
            r.temporalGrid = TemporalGrid::fromJson(j["temporalGrid"]);
        if (j.contains("timbreTrajectory"))
            r.timbreTrajectory = EnvelopeTrajectory::fromJson(j["timbreTrajectory"]);
        r.totalDurationMs = j.value("totalDurationMs", 2000.0);
        return r;
    }
};

/**
 * @brief Structured validation report for envelope trajectories and capture records.
 */
struct EnvelopeValidationResult
{
    bool valid { true };
    std::vector<std::string> errors;

    void addError(const std::string& err)
    {
        valid = false;
        errors.push_back(err);
    }
};

/**
 * @brief Strict validation of envelope trajectory properties, unit compatibility, monotonicity, and stages.
 */
[[nodiscard]] inline EnvelopeValidationResult validateEnvelopeTrajectory(const EnvelopeTrajectory& traj)
{
    EnvelopeValidationResult res;

    if (traj.domain == EnvelopeDomain::Unknown)
        res.addError("domain_unknown: trajectory cannot have Unknown domain");

    if (traj.trajectoryLabel.empty())
        res.addError("empty_label: trajectoryLabel cannot be empty");

    // Spectral metadata validation
    if (traj.spectralMetadata.sampleRateHz <= 0.0)
        res.addError("invalid_sample_rate: sampleRateHz must be > 0");
    if (traj.spectralMetadata.hopSizeSamples <= 0)
        res.addError("invalid_hop_size: hopSizeSamples must be > 0");
    if (traj.spectralMetadata.windowLengthSamples <= 0)
        res.addError("invalid_window_length: windowLengthSamples must be > 0");
    if (traj.spectralMetadata.fftSize < traj.spectralMetadata.windowLengthSamples)
        res.addError("fft_smaller_than_window: fftSize must be >= windowLengthSamples");

    // Points validation
    double prevTimeMs = -1.0;
    int prevFrame = -1;

    for (size_t i = 0; i < traj.points.size(); ++i)
    {
        const auto& pt = traj.points[i];

        if (i > 0)
        {
            if (pt.timeMs <= prevTimeMs)
            {
                res.addError("time_not_strictly_increasing: point[" + std::to_string(i) +
                             "] timeMs " + std::to_string(pt.timeMs) + " <= previous " + std::to_string(prevTimeMs));
            }
            if (pt.frameIndex <= prevFrame)
            {
                res.addError("duplicate_or_decreasing_frame: point[" + std::to_string(i) +
                             "] frameIndex " + std::to_string(pt.frameIndex) + " <= previous " + std::to_string(prevFrame));
            }
        }
        prevTimeMs = pt.timeMs;
        prevFrame = pt.frameIndex;

        // Domain & unit compatibility
        if (!isUnitCompatibleWithDomain(traj.domain, pt.unit))
        {
            res.addError("incompatible_unit: unit '" + pt.unit + "' is incompatible with domain '" +
                         envelopeDomainToString(traj.domain) + "' at point[" + std::to_string(i) + "]");
        }

        // Value & status consistency
        if (pt.status == "valid")
        {
            if (!pt.value.has_value())
                res.addError("null_value_in_valid_status: point[" + std::to_string(i) + "] has status 'valid' but null value");
        }
        else
        {
            if (pt.value.has_value() && pt.status == "not_observable")
                res.addError("value_present_in_not_observable: point[" + std::to_string(i) + "] has status 'not_observable' but value is present");
        }
    }

    // Inferred stages validation (if present)
    if (traj.inferredStages.size() > 8)
    {
        res.addError("too_many_stages: inferredStages count exceeds 8 (" + std::to_string(traj.inferredStages.size()) + ")");
    }

    for (size_t i = 0; i < traj.inferredStages.size(); ++i)
    {
        const auto& st = traj.inferredStages[i];
        const int expectedIndex = static_cast<int>(i + 1);
        if (st.stageIndex != expectedIndex)
        {
            res.addError("non_consecutive_stage_index: stage[" + std::to_string(i) +
                         "] has stageIndex " + std::to_string(st.stageIndex) + ", expected " + std::to_string(expectedIndex));
        }
        if (st.durationMs < 0.0)
        {
            res.addError("negative_stage_duration: stage[" + std::to_string(i) + "] durationMs < 0");
        }
    }

    return res;
}

/**
 * @brief Strict validation of multi-domain capture record.
 */
[[nodiscard]] inline EnvelopeValidationResult validateMultiDomainEnvelopeCaptureRecord(const MultiDomainEnvelopeCaptureRecord& rec)
{
    EnvelopeValidationResult res;

    if (rec.captureId.empty())
        res.addError("empty_capture_id: captureId cannot be empty");

    if (rec.dut.name.empty() && rec.dut.model.empty())
        res.addError("empty_dut_identity: dut name and model cannot both be empty");

    if (rec.temporalGrid.originMs < 0.0)
        res.addError("negative_grid_origin: temporalGrid.originMs must be >= 0");
    if (rec.temporalGrid.hopMs <= 0.0)
        res.addError("invalid_grid_hop: temporalGrid.hopMs must be > 0");
    if (rec.temporalGrid.frameCount < 0)
        res.addError("negative_grid_frame_count: temporalGrid.frameCount must be >= 0");

    // Trajectory domain verification
    if (rec.pitchTrajectory.domain != EnvelopeDomain::Pitch)
        res.addError("pitch_trajectory_domain_mismatch: pitchTrajectory must have domain Pitch");
    if (rec.timbreTrajectory.domain != EnvelopeDomain::Timbre)
        res.addError("timbre_trajectory_domain_mismatch: timbreTrajectory must have domain Timbre");
    if (rec.amplitudeTrajectory.domain != EnvelopeDomain::Amplitude)
        res.addError("amplitude_trajectory_domain_mismatch: amplitudeTrajectory must have domain Amplitude");

    // Individual trajectory validation
    auto pitchRes = validateEnvelopeTrajectory(rec.pitchTrajectory);
    for (const auto& e : pitchRes.errors) res.addError("pitchTrajectory." + e);

    auto timbreRes = validateEnvelopeTrajectory(rec.timbreTrajectory);
    for (const auto& e : timbreRes.errors) res.addError("timbreTrajectory." + e);

    auto ampRes = validateEnvelopeTrajectory(rec.amplitudeTrajectory);
    for (const auto& e : ampRes.errors) res.addError("amplitudeTrajectory." + e);

    // Grid alignment validation
    auto checkGridAlignment = [&](const EnvelopeTrajectory& traj, const std::string& name) {
        if (traj.alignmentStatus == "aligned")
        {
            if (traj.temporalGridId.empty() || traj.temporalGridId != rec.temporalGrid.gridId)
            {
                res.addError(name + "_grid_id_mismatch: trajectory declared 'aligned' but temporalGridId '" +
                             traj.temporalGridId + "' does not match container grid '" + rec.temporalGrid.gridId + "'");
            }
        }
    };
    checkGridAlignment(rec.pitchTrajectory, "pitchTrajectory");
    checkGridAlignment(rec.timbreTrajectory, "timbreTrajectory");
    checkGridAlignment(rec.amplitudeTrajectory, "amplitudeTrajectory");

    // Hash sanity (if present, must be 64 hex characters)
    auto checkShaHex = [&](const std::string& hash, const std::string& fieldName) {
        if (!hash.empty())
        {
            if (hash.size() != 64)
                res.addError("invalid_" + fieldName + "_length: expected 64 hex characters");
            for (char c : hash)
            {
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                {
                    res.addError("invalid_" + fieldName + "_hex: contains non-hex character");
                    break;
                }
            }
        }
    };

    checkShaHex(rec.audioSha256, "audioSha256");
    checkShaHex(rec.stimulusSha256, "stimulusSha256");
    checkShaHex(rec.nativePatchStateSha256, "nativePatchStateSha256");

    return res;
}

} // namespace abdaudiolab::measurement
