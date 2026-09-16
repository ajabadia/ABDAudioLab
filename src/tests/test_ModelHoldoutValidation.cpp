/**
 * @file test_ModelHoldoutValidation.cpp
 * @brief Unit and integration tests for ModelHoldoutValidator and FAIR persistence (T3 / T4).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/ModelHoldoutValidator.h"
#include "core/ExperimentStorage.h"
#include "core/LabDataDirectories.h"
#include <cmath>

namespace
{

std::vector<abdaudiolab::dsp::AbdBatchedPoint> createHoldoutTestLut(int gridSize)
{
    std::vector<abdaudiolab::dsp::AbdBatchedPoint> lut(static_cast<size_t>(gridSize * gridSize));
    for (int y = 0; y < gridSize; ++y)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            size_t idx = static_cast<size_t>(y * gridSize + x);
            lut[idx].p1 = static_cast<float>(x) / static_cast<float>(gridSize - 1);
            lut[idx].p2 = static_cast<float>(y) / static_cast<float>(gridSize - 1);
            lut[idx].mu = 0.5f * (0.3f + 0.7f * lut[idx].p1);
            lut[idx].sigma = 0.01f;
            lut[idx].sec_mu = 1.0f;
            lut[idx].sec_sigma = 0.0f;
            lut[idx].thd_percent = 0.5f;
            lut[idx].reserved = 0.0f;
        }
    }
    return lut;
}

abdaudiolab::core::ExperimentRecord createMinimalExperimentRecord(const std::string& expId)
{
    abdaudiolab::core::ExperimentRecord rec;
    rec.schemaVersion = 1;
    rec.experimentId = expId;
    rec.status = abdaudiolab::core::ExperimentStatus::AuditedApproved;

    rec.target.targetId = "ReferenceSynth";
    rec.target.targetName = "Reference Ground Truth Synth VST3";
    rec.target.manufacturer = "ABDSynths";
    rec.target.version = "1.0.0";
    rec.target.format = "VST3";
    rec.target.isDeterministic = true;

    rec.capture.sampleRate = 48000.0;
    rec.capture.hostBufferSize = 480;
    rec.capture.processingBlockSize = 256;
    rec.capture.channels = 1;
    rec.capture.durationSeconds = 1.5;

    rec.provenance.timestampUtc = "2026-09-15T20:00:00Z";
    rec.provenance.executionMode = "InProcess";

    rec.evaluation.hasEvaluation = true;
    rec.evaluation.recommendedModelType = "LUT_SIMD_2D";
    rec.evaluation.selectionStatus = "Accepted";
    return rec;
}

} // namespace

TEST_CASE("ModelHoldoutValidator - Nominal Validation and FAIR Artifact Persistence", "[validation][holdout][fair]")
{
    using namespace abdaudiolab::core;
    using namespace abdaudiolab::dsp;

    juce::File tempDir = juce::File::createTempFile("abdaudiolab_val_test_");
    tempDir.deleteFile();
    tempDir.createDirectory();

    auto cleanup = [&]() { tempDir.deleteRecursively(); };

    // 1. Create a minimal valid experiment package via ExperimentStorage
    auto record = createMinimalExperimentRecord("EXP_VAL_001");
    EmbeddedModelPayload modelPayload;
    modelPayload.relativePathInsideExperiment = "models/ModelPackage.h";
    modelPayload.modelSourceCode = "// Minimal Model Package\n#pragma once\n";

    juce::String expErr;
    REQUIRE(ExperimentStorage::saveExperiment(tempDir, record, {}, expErr, modelPayload));

    juce::File expFolder = tempDir.getChildFile("EXP_VAL_001");
    REQUIRE(expFolder.isDirectory());

    // 2. Setup empirical model and holdout sequence
    const int gridSize = 4;
    auto lut = createHoldoutTestLut(gridSize);
    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(48000.0, 64, 1));

    auto sequence = createCanonicalHoldoutSequence(48000.0, "mock_train_hash_abc123");

    // 3. Setup Target renderer that simulates a high-fidelity physical device
    auto idealTargetRenderer = [&](const float* inStim, float* outTarget, int numSamples, double sr, const HoldoutSequence& seq) -> bool {
        GeneratedAcousticModel groundTruth(lut.data(), gridSize);
        groundTruth.prepare(sr, 64, 1);
        groundTruth.reset();

        for (int offset = 0; offset < numSamples; offset += 64)
        {
            int curBlock = std::min(64, numSamples - offset);
            double t = static_cast<double>(offset) / sr;
            auto params = seq.getParametersAtTime(t);
            const float* inB[1] = { inStim + offset };
            float* outB[1] = { outTarget + offset };
            groundTruth.processBlock(inB, outB, 1, curBlock, params);
        }
        return true;
    };

    // 4. Run Validator
    ModelHoldoutValidator validator;
    ValidationReport report;
    std::string valErr;
    REQUIRE(validator.validate(idealTargetRenderer, model, sequence, expFolder, report, valErr));

    // Verify metrology
    CHECK(report.verdict == "PASS");
    CHECK(report.reasonCode == "WITHIN_TOLERANCE");
    CHECK(report.verdictPolicy == "audio-ab-v1");
    CHECK(report.sampleOffset == 0);
    CHECK(report.postAlignment.correlationPeak > 0.99f);
    CHECK(report.postAlignment.esrDb < -35.0f); // Ground truth match: extremely low ESR
    CHECK(report.audioMetadata.sampleRate == 48000.0);
    CHECK(report.audioMetadata.bitDepth == 32);

    // Verify dual metrics: pre and post alignment are captured
    CHECK(report.preAlignment.correlationPeak > 0.99f);
    CHECK(report.postAlignment.rmse >= 0.0f);

    // 5. Verify validation/ files on disk
    juce::File valDir = expFolder.getChildFile("validation");
    REQUIRE(valDir.isDirectory());

    juce::File targetWav = valDir.getChildFile("target.wav");
    juce::File modelWav = valDir.getChildFile("model.wav");
    juce::File residualWav = valDir.getChildFile("residual.wav");
    juce::File holdoutJson = valDir.getChildFile("holdout_manifest.json");
    juce::File reportJson = valDir.getChildFile("validation_report.json");

    CHECK(targetWav.existsAsFile());
    CHECK(modelWav.existsAsFile());
    CHECK(residualWav.existsAsFile());
    CHECK(holdoutJson.existsAsFile());
    CHECK(reportJson.existsAsFile());

    CHECK(targetWav.getSize() > 1000);
    CHECK(modelWav.getSize() > 1000);
    CHECK(residualWav.getSize() > 1000);

    // 6. Verify manifest.json indexing and SHA-256 fixity
    juce::String reopenErr;
    auto verifiedRec = ExperimentStorage::loadExperiment(expFolder, reopenErr);
    REQUIRE(verifiedRec.has_value());
    CHECK(verifiedRec->status == ExperimentStatus::AuditedApproved);
    CHECK_FALSE(verifiedRec->isCorrupt());

    // Check that manifest contains validation artifacts
    bool hasTarget = false, hasModel = false, hasResidual = false, hasHoldout = false, hasReport = false;
    for (const auto& art : verifiedRec->artifacts)
    {
        if (art.relativePath == "validation/target.wav") hasTarget = true;
        if (art.relativePath == "validation/model.wav") hasModel = true;
        if (art.relativePath == "validation/residual.wav") hasResidual = true;
        if (art.relativePath == "validation/holdout_manifest.json") hasHoldout = true;
        if (art.relativePath == "validation/validation_report.json") hasReport = true;
    }
    CHECK(hasTarget);
    CHECK(hasModel);
    CHECK(hasResidual);
    CHECK(hasHoldout);
    CHECK(hasReport);

    cleanup();
}

TEST_CASE("ModelHoldoutValidator - Latency Recovery and Offset Alignment", "[validation][latency]")
{
    using namespace abdaudiolab::core;

    juce::File tempDir = juce::File::createTempFile("abdaudiolab_lat_test_");
    tempDir.deleteFile();
    tempDir.createDirectory();
    auto cleanup = [&]() { tempDir.deleteRecursively(); };

    auto record = createMinimalExperimentRecord("EXP_VAL_LAT");
    EmbeddedModelPayload modelPayload;
    modelPayload.relativePathInsideExperiment = "models/ModelPackage.h";
    modelPayload.modelSourceCode = "// Minimal Model Package\n#pragma once\n";

    juce::String expErr;
    REQUIRE(ExperimentStorage::saveExperiment(tempDir, record, {}, expErr, modelPayload));
    juce::File expFolder = tempDir.getChildFile("EXP_VAL_LAT");

    const int gridSize = 4;
    auto lut = createHoldoutTestLut(gridSize);
    GeneratedAcousticModel model(lut.data(), gridSize);
    auto sequence = createCanonicalHoldoutSequence(48000.0, "train_hash");

    // Target introduces an artificial delay of 25 samples
    const int artificialDelay = 25;
    auto delayedTargetRenderer = [&](const float* inStim, float* outTarget, int numSamples, double sr, const HoldoutSequence& seq) -> bool {
        GeneratedAcousticModel groundTruth(lut.data(), gridSize);
        groundTruth.prepare(sr, 64, 1);
        groundTruth.reset();

        std::vector<float> temp(numSamples, 0.0f);
        for (int offset = 0; offset < numSamples; offset += 64)
        {
            int curBlock = std::min(64, numSamples - offset);
            double t = static_cast<double>(offset) / sr;
            auto params = seq.getParametersAtTime(t);
            const float* inB[1] = { inStim + offset };
            float* outB[1] = { temp.data() + offset };
            groundTruth.processBlock(inB, outB, 1, curBlock, params);
        }

        // Shift by artificialDelay
        std::fill_n(outTarget, numSamples, 0.0f);
        for (int i = 0; i < numSamples - artificialDelay; ++i)
        {
            outTarget[i + artificialDelay] = temp[i];
        }
        return true;
    };

    ModelHoldoutValidator validator;
    ValidationReport report;
    std::string valErr;
    REQUIRE(validator.validate(delayedTargetRenderer, model, sequence, expFolder, report, valErr));

    // Must correctly detect sample offset magnitude equal to artificialDelay
    CHECK(std::abs(std::abs(report.sampleOffset) - artificialDelay) <= 1);
    CHECK(report.postAlignment.correlationPeak > 0.98f);
    CHECK(report.verdict == "PASS");

    cleanup();
}

TEST_CASE("ModelHoldoutValidator - Rejection Policy for Mismatched Target", "[validation][rejection]")
{
    using namespace abdaudiolab::core;

    juce::File tempDir = juce::File::createTempFile("abdaudiolab_fail_test_");
    tempDir.deleteFile();
    tempDir.createDirectory();
    auto cleanup = [&]() { tempDir.deleteRecursively(); };

    auto record = createMinimalExperimentRecord("EXP_VAL_FAIL");
    EmbeddedModelPayload modelPayload;
    modelPayload.relativePathInsideExperiment = "models/ModelPackage.h";
    modelPayload.modelSourceCode = "// Minimal Model Package\n#pragma once\n";

    juce::String expErr;
    REQUIRE(ExperimentStorage::saveExperiment(tempDir, record, {}, expErr, modelPayload));
    juce::File expFolder = tempDir.getChildFile("EXP_VAL_FAIL");

    const int gridSize = 4;
    auto lut = createHoldoutTestLut(gridSize);
    GeneratedAcousticModel model(lut.data(), gridSize);
    auto sequence = createCanonicalHoldoutSequence(48000.0, "train_hash");

    // Heavily distorted / inverted target to force failure
    auto failedTargetRenderer = [](const float* inStim, float* outTarget, int numSamples, double, const HoldoutSequence&) -> bool {
        for (int i = 0; i < numSamples; ++i)
        {
            outTarget[i] = -inStim[i] * 2.0f; // Inverted phase and 2x amplitude
        }
        return true;
    };

    ModelHoldoutValidator validator;
    ValidationReport report;
    std::string valErr;
    REQUIRE(validator.validate(failedTargetRenderer, model, sequence, expFolder, report, valErr));

    // High error, must fail verdict policy
    CHECK(report.verdict == "FAIL");
    CHECK(report.reasonCode == "EXCEEDS_TOLERANCE");
    CHECK(report.postAlignment.esrDb > -10.0f);

    cleanup();
}

TEST_CASE("ModelHoldoutValidator - Generate Persistent Experiment For Manual Audio A/B Inspection", "[validation][manual_artifact]")
{
    using namespace abdaudiolab::core;
    using namespace abdaudiolab::dsp;

    auto labDirs = resolveLabDataDirectories();
    REQUIRE(labDirs.experiments.isDirectory());

    std::string expId = "20260916T070000Z_ReferenceSynth_holdout_eval";
    juce::File expFolder = labDirs.experiments.getChildFile(expId);
    if (expFolder.isDirectory())
        expFolder.deleteRecursively();

    auto record = createMinimalExperimentRecord(expId);
    record.target.targetId = "ReferenceSynth";
    record.target.targetName = "Reference Ground Truth Synth VST3";

    EmbeddedModelPayload modelPayload;
    modelPayload.relativePathInsideExperiment = "models/ModelPackage.h";
    modelPayload.modelSourceCode = "// ABDAudioLab Executable Model Package\n#pragma once\n";

    juce::String expErr;
    REQUIRE(ExperimentStorage::saveExperiment(labDirs.experiments, record, {}, expErr, modelPayload));
    REQUIRE(expFolder.isDirectory());

    const int gridSize = 8;
    auto lut = createHoldoutTestLut(gridSize);
    GeneratedAcousticModel model(lut.data(), gridSize);
    REQUIRE(model.prepare(48000.0, 64, 1));

    auto sequence = createCanonicalHoldoutSequence(48000.0, "training_plan_sha256_refsynth_canonical");

    auto targetRenderer = [&](const float* inStim, float* outTarget, int numSamples, double sr, const HoldoutSequence& seq) -> bool {
        GeneratedAcousticModel targetSynth(lut.data(), gridSize);
        targetSynth.prepare(sr, 64, 1);
        targetSynth.reset();

        for (int offset = 0; offset < numSamples; offset += 64)
        {
            int curBlock = std::min(64, numSamples - offset);
            double t = static_cast<double>(offset) / sr;
            auto params = seq.getParametersAtTime(t);
            const float* inB[1] = { inStim + offset };
            float* outB[1] = { outTarget + offset };
            targetSynth.processBlock(inB, outB, 1, curBlock, params);
        }
        return true;
    };

    ModelHoldoutValidator validator;
    ValidationReport report;
    std::string valErr;
    REQUIRE(validator.validate(targetRenderer, model, sequence, expFolder, report, valErr));
    CHECK(report.verdict == "PASS");
    CHECK(expFolder.getChildFile("validation/target.wav").existsAsFile());
    CHECK(expFolder.getChildFile("validation/model.wav").existsAsFile());
    CHECK(expFolder.getChildFile("validation/residual.wav").existsAsFile());
    CHECK(expFolder.getChildFile("validation/holdout_manifest.json").existsAsFile());
    CHECK(expFolder.getChildFile("validation/validation_report.json").existsAsFile());
}

TEST_CASE("ModelHoldoutValidator - Tamper Detection and Rejection on Corrupted Artifacts", "[validation][corruption_audit]")
{
    using namespace abdaudiolab::core;

    auto labDirs = resolveLabDataDirectories();
    REQUIRE(labDirs.experiments.isDirectory());

    juce::File validExpFolder = labDirs.experiments.getChildFile("20260916T070000Z_ReferenceSynth_holdout_eval");
    REQUIRE(validExpFolder.isDirectory());

    // 1. Verify that the untouched experiment is NOT corrupt and is AuditedApproved
    juce::String err;
    auto initialLoad = ExperimentStorage::loadExperiment(validExpFolder, err);
    REQUIRE(initialLoad.has_value());
    CHECK_FALSE(initialLoad->isCorrupt());
    CHECK(initialLoad->status == ExperimentStatus::AuditedApproved);
    CHECK(initialLoad->isExportable());

    // 2. Clone to a temporary tampered experiment folder
    juce::File tamperedFolder = labDirs.experiments.getChildFile("20260916T070000Z_ReferenceSynth_tampered_test");
    if (tamperedFolder.isDirectory())
        tamperedFolder.deleteRecursively();

    REQUIRE(validExpFolder.copyDirectoryTo(tamperedFolder));

    // 3. Alter one single byte in validation/target.wav
    juce::File targetWav = tamperedFolder.getChildFile("validation/target.wav");
    REQUIRE(targetWav.existsAsFile());

    juce::MemoryBlock mb;
    targetWav.loadFileAsData(mb);
    REQUIRE(mb.getSize() > 100);
    char* data = static_cast<char*>(mb.getData());
    data[80] = static_cast<char>(data[80] ^ 0xFF); // Flip byte at offset 80
    targetWav.replaceWithData(mb.getData(), mb.getSize());

    // 4. Attempt to load the tampered experiment
    juce::String tamperErr;
    auto tamperedLoad = ExperimentStorage::loadExperiment(tamperedFolder, tamperErr);
    REQUIRE(tamperedLoad.has_value());

    // Cryptographic audit verification
    CHECK(tamperedLoad->isCorrupt());
    CHECK(tamperedLoad->status == ExperimentStatus::Corrupt);
    CHECK_FALSE(tamperedLoad->isExportable());
    CHECK(tamperedLoad->failureOrCorruptionReason.find("Cryptographic mismatch for validation/target.wav") != std::string::npos);
    CHECK(tamperErr.toStdString().find("Cryptographic mismatch for validation/target.wav") != std::string::npos);

    // 5. Clean up tampered folder
    tamperedFolder.deleteRecursively();
}
