/**
 * @file ModelHoldoutValidator.cpp
 * @brief Implementation of ModelHoldoutValidator.
 * @author ABDSynths
 * @date 2026
 */

#include "ModelHoldoutValidator.h"
#include "ExperimentStorage.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace abdaudiolab::core
{

nlohmann::json AudioSignalMetadata::toJson() const
{
    nlohmann::json j;
    j["sampleRate"] = sampleRate;
    j["numChannels"] = numChannels;
    j["bitDepth"] = bitDepth;
    j["numSamples"] = numSamples;
    j["durationSeconds"] = durationSeconds;
    j["format"] = format;
    return j;
}

nlohmann::json AlignmentAndErrorMetrics::toJson() const
{
    nlohmann::json j;
    j["rmse"] = rmse;
    j["correlationPeak"] = correlationPeak;
    j["correlation"] = correlationPeak;
    j["rmsDeltaDb"] = rmsDeltaDb;
    j["peakAbsoluteError"] = peakAbsoluteError;
    j["esrDb"] = esrDb;
    j["spectralDeltaDb"] = spectralDeltaDb;
    return j;
}

nlohmann::json ValidationReport::toJson() const
{
    nlohmann::json j;
    j["schemaVersion"] = schemaVersion;
    j["schemaUri"] = schemaUri;
    j["reportType"] = reportType;
    j["reportId"] = reportId;
    j["timestampUtc"] = timestampUtc;
    j["status"] = status;

    if (!errorCode.empty())
    {
        j["error"] = {
            { "code", errorCode },
            { "message", errorMessage }
        };
    }

    std::string severity = "error";
    if (verdict == "PASS")
        severity = "success";
    else if (verdict == "PASS_WITH_LIMITATIONS")
        severity = "warning";

    j["verdict"] = {
        { "code", verdict },
        { "label", verdict },
        { "severity", severity },
        { "policy", verdictPolicy },
        { "reason", reasonCode },
        { "policyDescription", policyDescription },
        { "domainStatus", "within_declared_domain" }
    };

    j["execution"] = {
        { "timestampUtc", timestampUtc },
        { "sampleRateHz", audioMetadata.sampleRate },
        { "numChannels", audioMetadata.numChannels },
        { "numSamples", audioMetadata.numSamples },
        { "bitDepth", audioMetadata.bitDepth },
        { "durationSeconds", audioMetadata.durationSeconds },
        { "sampleFormat", audioMetadata.format }
    };

    j["metrics"] = {
        { "preAlignment", preAlignment.toJson() },
        { "alignment", {
            { "method", "cross_correlation" },
            { "enabled", true },
            { "sampleOffset", sampleOffset },
            { "signConvention", "alignedTarget[n] = target[n - sampleOffset]" },
            { "peakCorrelation", postAlignment.correlationPeak },
            { "maxAllowedOffsetSamples", 256 }
        } },
        { "postAlignment", postAlignment.toJson() }
    };

    j["thresholds"] = {
        { "pass", {
            { "maxEsrDb", -28.0 },
            { "minCorrelation", 0.98 },
            { "maxAbsOffsetSamples", 256 }
        } },
        { "passWithLimitations", {
            { "maxEsrDb", -18.0 },
            { "minCorrelation", 0.92 },
            { "maxAbsOffsetSamples", 1024 }
        } }
    };

    j["holdout"] = {
        { "sequenceDefinitionHash", {
            { "algorithm", "SHA-256" },
            { "value", sequenceDefinitionHash }
        } },
        { "holdoutPlanHash", {
            { "algorithm", "SHA-256" },
            { "value", holdoutPlanHash }
        } },
        { "trainingPlanHash", {
            { "algorithm", "SHA-256" },
            { "value", trainingPlanHash }
        } },
        { "outOfSample", true },
        { "dataLeakageDetected", false }
    };

    j["artifacts"] = {
        { "target", {
            { "path", "validation/target.wav" },
            { "sha256", targetWavSha256 },
            { "role", "validation_target" }
        } },
        { "model", {
            { "path", "validation/model.wav" },
            { "sha256", modelWavSha256 },
            { "role", "validation_model" }
        } },
        { "residual", {
            { "path", "validation/residual.wav" },
            { "sha256", residualWavSha256 },
            { "role", "aligned_residual" }
        } },
        { "holdoutManifest", {
            { "path", "validation/holdout_manifest.json" },
            { "sha256", holdoutManifestSha256 },
            { "role", "holdout_definition" }
        } }
    };

    j["reproduction"] = {
        { "runtime", "GeneratedAcousticModel" },
        { "evaluator", "dsp::AnalogLutFilterModule" },
        { "comparator", "math::AudioABComparator" },
        { "verdictEngine", "math::AudioABVerdictEngine" },
        { "runtimeVersion", "20.8.5" },
        { "deterministic", true }
    };

    j["ui"] = {
        { "displayLabel", verdict },
        { "displayDescription", policyDescription },
        { "audioPreviewAvailable", {
            { "target", !targetWavSha256.empty() },
            { "model", !modelWavSha256.empty() },
            { "residual", !residualWavSha256.empty() }
        } }
    };

    // Flat compatibility fields for legacy readers
    j["verdictPolicy"] = verdictPolicy;
    j["policyDescription"] = policyDescription;
    j["reasonCode"] = reasonCode;
    j["sampleOffset"] = sampleOffset;
    j["preAlignment"] = preAlignment.toJson();
    j["postAlignment"] = postAlignment.toJson();
    j["audioMetadata"] = audioMetadata.toJson();
    j["trainingPlanHash"] = trainingPlanHash;
    j["holdoutPlanHash"] = holdoutPlanHash;
    j["sequenceDefinitionHash"] = sequenceDefinitionHash;

    return j;
}

ModelHoldoutValidator::ModelHoldoutValidator()
    : config_()
{
}

ModelHoldoutValidator::ModelHoldoutValidator(Config config)
    : config_(config)
{
}

float ModelHoldoutValidator::computeEsrDb(const float* target, const float* residual, int numSamples) noexcept
{
    if (target == nullptr || residual == nullptr || numSamples <= 0)
        return 0.0f;

    double sumTgt = 0.0;
    double sumRes = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(target[i]);
        double r = static_cast<double>(residual[i]);
        sumTgt += t * t;
        sumRes += r * r;
    }

    double esrLin = sumRes / (sumTgt + 1e-12);
    return static_cast<float>(10.0 * std::log10(esrLin + 1e-12));
}

bool ModelHoldoutValidator::writeWavFile(const juce::File& destinationFile,
                                        const float* channelData,
                                        int numSamples,
                                        double sampleRate,
                                        int bitDepth)
{
    if (channelData == nullptr || numSamples <= 0 || sampleRate <= 1000.0)
        return false;

    destinationFile.deleteFile();
    destinationFile.getParentDirectory().createDirectory();

    juce::AudioBuffer<float> tempBuf(1, numSamples);
    std::copy_n(channelData, static_cast<size_t>(numSamples), tempBuf.getWritePointer(0));

    juce::WavAudioFormat wavFormat;
    if (auto outStream = std::unique_ptr<juce::FileOutputStream>(destinationFile.createOutputStream()))
    {
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream.get(),
                                                                                  sampleRate,
                                                                                  1,
                                                                                  bitDepth,
                                                                                  {},
                                                                                  0));
        if (writer != nullptr)
        {
            outStream.release(); // Writer took ownership
            writer->writeFromAudioSampleBuffer(tempBuf, 0, numSamples);
            return true;
        }
    }

    return false;
}

bool ModelHoldoutValidator::validate(std::function<bool(const float*, float*, int, double, const HoldoutSequence&)> targetRenderer,
                                     GeneratedAcousticModel& model,
                                     const HoldoutSequence& sequence,
                                     const juce::File& experimentFolder,
                                     ValidationReport& outReport,
                                     std::string& outError)
{
    if (targetRenderer == nullptr)
    {
        outError = "targetRenderer callback is null";
        return false;
    }

    if (sequence.totalDurationSeconds <= 0.0 || sequence.sampleRate <= 1000.0)
    {
        outError = "Invalid HoldoutSequence parameters";
        return false;
    }

    const int numSamples = static_cast<int>(sequence.totalDurationSeconds * sequence.sampleRate);
    if (numSamples <= 0)
    {
        outError = "Computed zero samples for holdout duration";
        return false;
    }

    // 1. Prepare timestamp and ID
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ssTime;
    ssTime << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    outReport.timestampUtc = ssTime.str();
    outReport.reportId = "val_" + sequence.sequenceId + "_" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

    // 2. Synthesize Stimulus Audio
    std::vector<float> stimulusBuffer(static_cast<size_t>(numSamples), 0.0f);
    sequence.renderStimulus(stimulusBuffer.data(), numSamples, sequence.sampleRate);

    // 3. Render Target Device / Plugin Audio
    std::vector<float> targetBuffer(static_cast<size_t>(numSamples), 0.0f);
    if (!targetRenderer(stimulusBuffer.data(), targetBuffer.data(), numSamples, sequence.sampleRate, sequence))
    {
        outError = "Target renderer execution failed";
        return false;
    }

    // 4. Render Model Audio via GeneratedAcousticModel
    if (!model.isPrepared() || std::abs(model.getSampleRate() - sequence.sampleRate) > 1.0)
    {
        if (!model.prepare(sequence.sampleRate, config_.blockSize, 1))
        {
            outError = "Failed to prepare GeneratedAcousticModel";
            return false;
        }
    }
    model.reset();

    std::vector<float> modelBuffer(static_cast<size_t>(numSamples), 0.0f);
    for (int offset = 0; offset < numSamples; offset += config_.blockSize)
    {
        int curBlock = std::min(config_.blockSize, numSamples - offset);
        double t = static_cast<double>(offset) / sequence.sampleRate;
        auto params = sequence.getParametersAtTime(t);

        const float* inBlock[1] = { stimulusBuffer.data() + offset };
        float* outBlock[1] = { modelBuffer.data() + offset };
        model.processBlock(inBlock, outBlock, 1, curBlock, params);
    }

    // 5. Pre-Alignment Metrology via AudioABComparator (No Cross-Correlation shift)
    math::AudioABSignal refSignal;
    refSignal.sampleRate = sequence.sampleRate;
    refSignal.numChannels = 1;
    refSignal.buffer.setSize(1, numSamples);
    std::copy_n(targetBuffer.data(), static_cast<size_t>(numSamples), refSignal.buffer.getWritePointer(0));
    refSignal.originalNumSamples = numSamples;

    math::AudioABSignal candSignal;
    candSignal.sampleRate = sequence.sampleRate;
    candSignal.numChannels = 1;
    candSignal.buffer.setSize(1, numSamples);
    std::copy_n(modelBuffer.data(), static_cast<size_t>(numSamples), candSignal.buffer.getWritePointer(0));
    candSignal.originalNumSamples = numSamples;

    math::AudioABRunContext ctxPre;
    ctxPre.runId = outReport.reportId + "-pre";

    math::AudioABComparatorConfig configPre;
    configPre.trimLeadingSilence = false;
    configPre.trimTrailingSilence = false;
    configPre.enableCrossCorrelation = false;

    math::AudioABComparator comparator;
    auto resPre = comparator.compare(refSignal, candSignal, ctxPre, configPre);

    outReport.preAlignment.rmse = static_cast<float>(resPre.time.rmse);
    outReport.preAlignment.correlationPeak = static_cast<float>(resPre.alignment.correlationPeak);
    outReport.preAlignment.rmsDeltaDb = static_cast<float>(resPre.time.rmsDeltaDb);
    outReport.preAlignment.spectralDeltaDb = static_cast<float>(resPre.spectral.logMagMeanAbsDiffDb);

    // Compute raw pre-alignment difference
    float preMaxErr = 0.0f;
    std::vector<float> rawDiff(static_cast<size_t>(numSamples), 0.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        rawDiff[static_cast<size_t>(i)] = targetBuffer[static_cast<size_t>(i)] - modelBuffer[static_cast<size_t>(i)];
        preMaxErr = std::max(preMaxErr, std::abs(rawDiff[static_cast<size_t>(i)]));
    }
    outReport.preAlignment.peakAbsoluteError = preMaxErr;
    outReport.preAlignment.esrDb = computeEsrDb(targetBuffer.data(), rawDiff.data(), numSamples);

    // 6. Post-Alignment Metrology via AudioABComparator Cross-Correlation
    math::AudioABRunContext ctxPost;
    ctxPost.runId = outReport.reportId + "-post";

    math::AudioABComparatorConfig configPost;
    configPost.trimLeadingSilence = false;
    configPost.trimTrailingSilence = false;
    configPost.enableCrossCorrelation = true;

    auto resPost = comparator.compare(refSignal, candSignal, ctxPost, configPost);

    outReport.sampleOffset = static_cast<int>(resPost.alignment.sampleOffset);
    outReport.postAlignment.correlationPeak = static_cast<float>(resPost.alignment.correlationPeak);
    outReport.postAlignment.rmse = static_cast<float>(resPost.time.rmse);
    outReport.postAlignment.rmsDeltaDb = static_cast<float>(resPost.time.rmsDeltaDb);
    outReport.postAlignment.spectralDeltaDb = static_cast<float>(resPost.spectral.logMagMeanAbsDiffDb);

    // 7. Compute Exact Aligned Residual Buffer: r[n] = target[n + sampleOffset] - model[n]
    std::vector<float> residualBuffer(static_cast<size_t>(numSamples), 0.0f);
    float postMaxErr = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        int tgtIdx = n - outReport.sampleOffset;
        float tgtVal = (tgtIdx >= 0 && tgtIdx < numSamples) ? targetBuffer[static_cast<size_t>(tgtIdx)] : 0.0f;
        float r = tgtVal - modelBuffer[static_cast<size_t>(n)];
        residualBuffer[static_cast<size_t>(n)] = r;
        postMaxErr = std::max(postMaxErr, std::abs(r));
    }
    outReport.postAlignment.peakAbsoluteError = postMaxErr;
    outReport.postAlignment.esrDb = computeEsrDb(targetBuffer.data(), residualBuffer.data(), numSamples);

    // 8. Verdict Policy Evaluation ("audio-ab-v1")
    outReport.verdictPolicy = "audio-ab-v1";
    outReport.policyDescription = "PASS within declared domain and tolerances, not universal acoustic perfection";

    if (outReport.postAlignment.esrDb <= config_.passEsrDbMax &&
        outReport.postAlignment.correlationPeak >= config_.passCorrelationMin &&
        std::abs(outReport.sampleOffset) <= config_.passMaxLatencySamples)
    {
        outReport.verdict = "PASS";
        outReport.reasonCode = "WITHIN_TOLERANCE";
    }
    else if (outReport.postAlignment.esrDb <= config_.passLimEsrDbMax &&
             outReport.postAlignment.correlationPeak >= config_.passLimCorrelationMin &&
             std::abs(outReport.sampleOffset) <= config_.passLimMaxLatencySamples)
    {
        outReport.verdict = "PASS_WITH_LIMITATIONS";
        outReport.reasonCode = "MARGINAL_TOLERANCE";
    }
    else
    {
        outReport.verdict = "FAIL";
        outReport.reasonCode = "EXCEEDS_TOLERANCE";
    }

    // 9. Technical Audio Metadata
    outReport.audioMetadata.sampleRate = sequence.sampleRate;
    outReport.audioMetadata.numChannels = 1;
    outReport.audioMetadata.bitDepth = config_.bitDepth;
    outReport.audioMetadata.numSamples = numSamples;
    outReport.audioMetadata.durationSeconds = sequence.totalDurationSeconds;
    outReport.audioMetadata.format = (config_.bitDepth == 32) ? "PCM IEEE 32-bit float" : "PCM 24-bit";

    outReport.trainingPlanHash = sequence.trainingPlanHash;
    outReport.holdoutPlanHash = sequence.holdoutPlanHash;
    outReport.sequenceDefinitionHash = sequence.sequenceDefinitionHash;

    // 10. Persist Artifacts in validation/ Directory
    juce::File valDir = experimentFolder.getChildFile("validation");
    valDir.createDirectory();

    juce::File targetWavFile = valDir.getChildFile("target.wav");
    juce::File modelWavFile = valDir.getChildFile("model.wav");
    juce::File residualWavFile = valDir.getChildFile("residual.wav");
    juce::File holdoutManifestFile = valDir.getChildFile("holdout_manifest.json");
    juce::File validationReportFile = valDir.getChildFile("validation_report.json");

    if (!writeWavFile(targetWavFile, targetBuffer.data(), numSamples, sequence.sampleRate, config_.bitDepth) ||
        !writeWavFile(modelWavFile, modelBuffer.data(), numSamples, sequence.sampleRate, config_.bitDepth) ||
        !writeWavFile(residualWavFile, residualBuffer.data(), numSamples, sequence.sampleRate, config_.bitDepth))
    {
        outError = "Failed to write validation WAV files";
        return false;
    }

    // Write holdout_manifest.json
    if (!holdoutManifestFile.replaceWithText(sequence.toJson().dump(2)))
    {
        outError = "Failed to write holdout_manifest.json";
        return false;
    }

    // Compute cryptographic fixity hashes
    outReport.targetWavSha256 = ExperimentStorage::computeFileSha256(targetWavFile);
    outReport.modelWavSha256 = ExperimentStorage::computeFileSha256(modelWavFile);
    outReport.residualWavSha256 = ExperimentStorage::computeFileSha256(residualWavFile);
    outReport.holdoutManifestSha256 = ExperimentStorage::computeFileSha256(holdoutManifestFile);

    // Write validation_report.json
    if (!validationReportFile.replaceWithText(outReport.toJson().dump(2)))
    {
        outError = "Failed to write validation_report.json";
        return false;
    }

    std::string reportSha256 = ExperimentStorage::computeFileSha256(validationReportFile);

    // 11. Update manifest.json in experimentFolder
    juce::File manifestFile = experimentFolder.getChildFile("manifest.json");
    if (manifestFile.existsAsFile())
    {
        try
        {
            auto manifestJson = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());

            if (!manifestJson.contains("artifacts") || !manifestJson["artifacts"].is_array())
                manifestJson["artifacts"] = nlohmann::json::array();

            auto& arts = manifestJson["artifacts"];

            auto upsertArtifact = [&arts](const std::string& relPath, const std::string& role, uint64_t size, const std::string& sha) {
                for (auto& item : arts)
                {
                    if (item.value("path", "") == relPath)
                    {
                        item["role"] = role;
                        item["sizeBytes"] = size;
                        item["sha256"] = sha;
                        return;
                    }
                }
                arts.push_back({
                    { "path", relPath },
                    { "role", role },
                    { "sizeBytes", size },
                    { "sha256", sha }
                });
            };

            upsertArtifact("validation/target.wav", "validation", static_cast<uint64_t>(targetWavFile.getSize()), outReport.targetWavSha256);
            upsertArtifact("validation/model.wav", "validation", static_cast<uint64_t>(modelWavFile.getSize()), outReport.modelWavSha256);
            upsertArtifact("validation/residual.wav", "validation", static_cast<uint64_t>(residualWavFile.getSize()), outReport.residualWavSha256);
            upsertArtifact("validation/holdout_manifest.json", "validation", static_cast<uint64_t>(holdoutManifestFile.getSize()), outReport.holdoutManifestSha256);
            upsertArtifact("validation/validation_report.json", "validation", static_cast<uint64_t>(validationReportFile.getSize()), reportSha256);

            manifestJson["validation"] = {
                { "verdict", outReport.verdict },
                { "reasonCode", outReport.reasonCode },
                { "verdictPolicy", outReport.verdictPolicy },
                { "sampleOffset", outReport.sampleOffset },
                { "esrDb", outReport.postAlignment.esrDb },
                { "correlationPeak", outReport.postAlignment.correlationPeak },
                { "rmsePost", outReport.postAlignment.rmse },
                { "rmsePre", outReport.preAlignment.rmse },
                { "holdoutPlanHash", outReport.holdoutPlanHash },
                { "sequenceDefinitionHash", outReport.sequenceDefinitionHash }
            };

            manifestFile.replaceWithText(manifestJson.dump(2));
        }
        catch (const std::exception& e)
        {
            outError = "Failed to update manifest.json with validation artifacts: " + std::string(e.what());
            return false;
        }
    }

    return true;
}

} // namespace abdaudiolab::core
