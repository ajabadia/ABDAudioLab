/**
 * @file ExperimentStorage.cpp
 * @brief Implementation of transactional storage and verification for ExperimentRecord containers.
 * @author ABDSynths
 * @date 2026
 */

#include "ExperimentStorage.h"
#include "LabDataDirectories.h"
#include "../synth/Sha256.h"
#include <fstream>
#include <iostream>

namespace abdaudiolab::core
{

// ==============================================================================
// ExperimentFolderReader Implementation
// ==============================================================================

bool ExperimentFolderReader::canRead(const juce::File& location) const
{
    if (!location.isDirectory())
        return false;

    return location.getChildFile("experiment.json").existsAsFile() &&
           location.getChildFile("manifest.json").existsAsFile();
}

std::optional<ExperimentRecord> ExperimentFolderReader::read(const juce::File& location, juce::String& outError)
{
    if (!canRead(location))
    {
        outError = "Directory is not a valid ExperimentRecord package: " + location.getFullPathName();
        return std::nullopt;
    }

    // 1. Leer experiment.json
    auto expJsonFile = location.getChildFile("experiment.json");
    std::string expJsonStr = expJsonFile.loadFileAsString().toStdString();
    nlohmann::json expJson;
    try
    {
        expJson = nlohmann::json::parse(expJsonStr);
    }
    catch (const std::exception& e)
    {
        outError = "Failed to parse experiment.json: " + juce::String(e.what());
        return std::nullopt;
    }

    int schemaVer = expJson.value("schemaVersion", 0);
    if (schemaVer != 1)
    {
        outError = "Unsupported or invalid schemaVersion: " + juce::String(schemaVer);
        return std::nullopt;
    }

    ExperimentRecord record;
    record.schemaVersion = schemaVer;
    record.experimentId = expJson.value("experimentId", "");
    record.revision = expJson.value("revision", 1u);
    if (expJson.contains("parentExperimentId") && !expJson["parentExperimentId"].is_null())
    {
        std::string parent = expJson["parentExperimentId"].get<std::string>();
        if (!parent.empty())
            record.parentExperimentId = parent;
    }

    record.kind = experimentKindFromString(expJson.value("kind", "Measurement"));
    record.status = experimentStatusFromString(expJson.value("status", "LoadedForExploration"));

    if (expJson.contains("capture") && expJson["capture"].is_object())
    {
        const auto& cap = expJson["capture"];
        record.capture.sampleRate = cap.value("sampleRate", 48000.0);
        record.capture.hostBufferSize = cap.value("hostBufferSize", 480);
        record.capture.processingBlockSize = cap.value("processingBlockSize", 256);
        record.capture.channels = cap.value("channels", 2);
        record.capture.durationSeconds = cap.value("durationSeconds", 0.0);
        record.capture.presetStateHash = cap.value("presetStateHash", "");
        record.capture.excitationPlanHash = cap.value("excitationPlanHash", "");
        record.capture.storageProfile = storageProfileFromString(cap.value("storageProfile", "Standard"));
    }

    // 2. Leer y verificar manifest.json
    auto manifestFile = location.getChildFile("manifest.json");
    std::string manifestStr = manifestFile.loadFileAsString().toStdString();
    nlohmann::json manifestJson;
    try
    {
        manifestJson = nlohmann::json::parse(manifestStr);
    }
    catch (const std::exception& e)
    {
        record.status = ExperimentStatus::Corrupt;
        record.failureOrCorruptionReason = "Corrupt manifest.json: " + std::string(e.what());
        outError = juce::String(record.failureOrCorruptionReason);
        return record;
    }

    std::string manifestFormat = manifestJson.value("manifestFormat", "");
    if (manifestFormat != "artifact-list-v1")
    {
        record.status = ExperimentStatus::Corrupt;
        record.failureOrCorruptionReason = "Unsupported manifestFormat: " + manifestFormat;
        outError = juce::String(record.failureOrCorruptionReason);
        return record;
    }

    if (!manifestJson.contains("artifacts") || !manifestJson["artifacts"].is_array())
    {
        record.status = ExperimentStatus::Corrupt;
        record.failureOrCorruptionReason = "Manifest does not contain an artifacts array";
        outError = juce::String(record.failureOrCorruptionReason);
        return record;
    }

    // 3. Verificación exhaustiva de artefactos
    for (const auto& item : manifestJson["artifacts"])
    {
        ExperimentArtifact art;
        art.relativePath = item.value("path", "");
        art.role = item.value("role", "data");
        art.sizeBytes = item.value("sizeBytes", uint64_t(0));
        art.sha256 = item.value("sha256", "");

        // Seguridad: Control estricto anti-traversal
        if (!isSafeRelativePath(art.relativePath))
        {
            record.status = ExperimentStatus::Corrupt;
            record.failureOrCorruptionReason = "Security violation: Illegal relative path / traversal in manifest: " + art.relativePath;
            outError = juce::String(record.failureOrCorruptionReason);
            return record;
        }

        juce::File artFile = location.getChildFile(art.relativePath);
        if (!artFile.existsAsFile())
        {
            record.status = ExperimentStatus::Corrupt;
            record.failureOrCorruptionReason = "Missing required artifact on disk: " + art.relativePath;
            outError = juce::String(record.failureOrCorruptionReason);
            return record;
        }

        std::string computedSha = ExperimentStorage::computeFileSha256(artFile);
        if (computedSha != art.sha256)
        {
            record.status = ExperimentStatus::Corrupt;
            record.failureOrCorruptionReason = "Cryptographic mismatch for " + art.relativePath +
                                               " (expected: " + art.sha256 + ", computed: " + computedSha + ")";
            outError = juce::String(record.failureOrCorruptionReason);
            return record;
        }

        if (item.contains("audio") && item["audio"].is_object())
        {
            const auto& a = item["audio"];
            AudioArtifactMetadata meta;
            meta.sampleRate = a.value("sampleRate", 0.0);
            meta.channels = a.value("channels", 0);
            meta.bitsPerSample = a.value("bitsPerSample", 0);
            meta.sampleCount = a.value("sampleCount", int64_t(0));
            meta.durationSeconds = a.value("durationSeconds", 0.0);
            meta.format = a.value("format", "WAV_PCM");
            meta.isInterleaved = a.value("isInterleaved", true);
            art.audio = meta;
        }

        record.artifacts.push_back(art);
    }

    // 4. Leer metadatos complementarios si existen
    auto targetFile = location.getChildFile("target.json");
    if (targetFile.existsAsFile())
    {
        try
        {
            auto tj = nlohmann::json::parse(targetFile.loadFileAsString().toStdString());
            record.target.targetId = tj.value("targetId", "");
            record.target.targetName = tj.value("targetName", "");
            record.target.manufacturer = tj.value("manufacturer", "");
            record.target.version = tj.value("version", "");
            record.target.format = tj.value("format", "");
            record.target.binarySha256 = tj.value("binarySha256", "");
            record.target.binaryPath = tj.value("binaryPath", "");
            record.target.isDeterministic = tj.value("isDeterministic", true);
        }
        catch (...) {}
    }

    auto provFile = location.getChildFile("provenance.json");
    if (provFile.existsAsFile())
    {
        try
        {
            auto pj = nlohmann::json::parse(provFile.loadFileAsString().toStdString());
            record.provenance.appVersion = pj.value("appVersion", "");
            record.provenance.buildNumber = pj.value("buildNumber", 0);
            record.provenance.gitCommit = pj.value("gitCommit", "");
            record.provenance.executionMode = pj.value("executionMode", "");
            record.provenance.operatingSystem = pj.value("operatingSystem", "");
            record.provenance.machineName = pj.value("machineName", "");
            record.provenance.timestampUtc = pj.value("timestampUtc", "");
            record.provenance.operatorNotes = pj.value("operatorNotes", "");
            record.provenance.ambientTemperatureC = pj.value("ambientTemperatureC", 22.0f);
            record.provenance.warmupTimeMinutes = pj.value("warmupTimeMinutes", 0);
        }
        catch (...) {}
    }

    auto evalFile = location.getChildFile("evaluation.json");
    if (evalFile.existsAsFile())
    {
        try
        {
            auto ej = nlohmann::json::parse(evalFile.loadFileAsString().toStdString());
            record.evaluation.hasEvaluation = ej.value("hasEvaluation", false);
            record.evaluation.recommendedModelType = ej.value("recommendedModelType", "");
            record.evaluation.selectionStatus = ej.value("selectionStatus", "");
            record.evaluation.canonicalEvaluationHash = ej.value("canonicalEvaluationHash", "");
            record.evaluation.validationEsrDb = ej.value("validationEsrDb", 0.0);
            record.evaluation.validationCorrelation = ej.value("validationCorrelation", 0.0);
            record.evaluation.criteriaCompliancePercent = ej.value("criteriaCompliancePercent", 0.0);
            record.evaluation.validatedDomain = ej.value("validatedDomain", "");
            record.evaluation.relativeCpuCost = ej.value("relativeCpuCost", 1.0);
            record.evaluation.hashVerified = ej.value("hashVerified", false);
        }
        catch (...) {}
    }

    auto limFile = location.getChildFile("limitations.json");
    if (limFile.existsAsFile())
    {
        try
        {
            auto lj = nlohmann::json::parse(limFile.loadFileAsString().toStdString());
            record.limitations.modeledAspects = lj.value("modeledAspects", std::vector<std::string>{});
            record.limitations.unmodeledAspects = lj.value("unmodeledAspects", std::vector<std::string>{});
            record.limitations.validityDomain = lj.value("validityDomain", "");
            record.limitations.extrapolationWarnings = lj.value("extrapolationWarnings", std::vector<std::string>{});
        }
        catch (...) {}
    }

    return record;
}

// ==============================================================================
// ExperimentStorage Implementation
// ==============================================================================

juce::File ExperimentStorage::getDefaultExperimentsDirectory()
{
    return resolveLabDataDirectories().experiments;
}

std::string ExperimentStorage::computeFileSha256(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};

    juce::FileInputStream fis(file);
    if (!fis.openedOk())
        return {};

    synth::Sha256 hasher;
    constexpr size_t kBufSize = 65536;
    std::vector<uint8_t> buffer(kBufSize);

    while (!fis.isExhausted())
    {
        int bytesRead = fis.read(buffer.data(), static_cast<int>(kBufSize));
        if (bytesRead <= 0)
            break;
        hasher.update(buffer.data(), static_cast<size_t>(bytesRead));
    }
    return hasher.finalHex();
}

bool ExperimentStorage::saveExperiment(const juce::File& baseDir,
                                       const ExperimentRecord& record,
                                       const std::vector<std::pair<std::string, juce::File>>& audioFilesToInclude,
                                       juce::String& outError,
                                       const std::optional<EmbeddedModelPayload>& embeddedModel,
                                       StagingHook stagingHook)
{
    if (record.experimentId.empty())
    {
        outError = "experimentId cannot be empty";
        return false;
    }

    std::string folderName = record.experimentId;
    if (record.revision > 1)
        folderName += "_rev" + std::to_string(record.revision);

    juce::File finalDir = baseDir.getChildFile(folderName);
    if (finalDir.exists())
    {
        outError = "Invariance violation: Experiment directory already exists (" + finalDir.getFullPathName() +
                   "). Existing experiments cannot be overwritten. Create a new revision.";
        return false;
    }

    if (!baseDir.isDirectory())
        baseDir.createDirectory();

    // Crear carpeta temporal transaccional
    juce::String randSuffix = juce::String::toHexString(juce::Random::getSystemRandom().nextInt());
    juce::String tempFolderName = "." + juce::String(folderName) + ".tmp_" + randSuffix;
    juce::File tempDir = baseDir.getChildFile(tempFolderName);
    if (tempDir.exists())
    {
        tempDir.deleteRecursively();
    }

    tempDir.createDirectory();

    struct TempDirCleaner
    {
        juce::File dir;
        bool committed { false };
        ~TempDirCleaner()
        {
            if (!committed && dir.exists())
                dir.deleteRecursively();
        }
    } cleaner { tempDir };

    tempDir.getChildFile("measurements").createDirectory();
    tempDir.getChildFile("models").createDirectory();
    tempDir.getChildFile("logs").createDirectory();

    // 1. Escribir target.json
    {
        nlohmann::json tj;
        tj["schemaVersion"] = record.schemaVersion;
        tj["targetId"] = record.target.targetId;
        tj["targetName"] = record.target.targetName;
        tj["manufacturer"] = record.target.manufacturer;
        tj["version"] = record.target.version;
        tj["format"] = record.target.format;
        tj["binarySha256"] = record.target.binarySha256;
        tj["binaryPath"] = record.target.binaryPath;
        tj["isDeterministic"] = record.target.isDeterministic;
        tempDir.getChildFile("target.json").replaceWithText(tj.dump(2));
    }

    // 2. Escribir provenance.json
    {
        nlohmann::json pj;
        pj["schemaVersion"] = record.schemaVersion;
        pj["appVersion"] = record.provenance.appVersion;
        pj["buildNumber"] = record.provenance.buildNumber;
        pj["gitCommit"] = record.provenance.gitCommit;
        pj["executionMode"] = record.provenance.executionMode;
        pj["operatingSystem"] = record.provenance.operatingSystem;
        pj["machineName"] = record.provenance.machineName;
        pj["timestampUtc"] = record.provenance.timestampUtc;
        pj["operatorNotes"] = record.provenance.operatorNotes;
        pj["ambientTemperatureC"] = record.provenance.ambientTemperatureC;
        pj["warmupTimeMinutes"] = record.provenance.warmupTimeMinutes;
        tempDir.getChildFile("provenance.json").replaceWithText(pj.dump(2));
    }

    // 3. Escribir evaluation.json
    {
        nlohmann::json ej;
        ej["schemaVersion"] = record.schemaVersion;
        ej["hasEvaluation"] = record.evaluation.hasEvaluation;
        ej["recommendedModelType"] = record.evaluation.recommendedModelType;
        ej["selectionStatus"] = record.evaluation.selectionStatus;
        ej["canonicalEvaluationHash"] = record.evaluation.canonicalEvaluationHash;
        ej["validationEsrDb"] = record.evaluation.validationEsrDb;
        ej["validationCorrelation"] = record.evaluation.validationCorrelation;
        ej["criteriaCompliancePercent"] = record.evaluation.criteriaCompliancePercent;
        ej["validatedDomain"] = record.evaluation.validatedDomain;
        ej["relativeCpuCost"] = record.evaluation.relativeCpuCost;
        ej["hashVerified"] = record.evaluation.hashVerified;
        tempDir.getChildFile("evaluation.json").replaceWithText(ej.dump(2));
    }

    // 4. Escribir limitations.json
    {
        nlohmann::json lj;
        lj["schemaVersion"] = record.schemaVersion;
        lj["modeledAspects"] = record.limitations.modeledAspects;
        lj["unmodeledAspects"] = record.limitations.unmodeledAspects;
        lj["validityDomain"] = record.limitations.validityDomain;
        lj["extrapolationWarnings"] = record.limitations.extrapolationWarnings;
        tempDir.getChildFile("limitations.json").replaceWithText(lj.dump(2));
    }

    // 5. Escribir experiment.json
    {
        nlohmann::json exp;
        exp["schemaVersion"] = record.schemaVersion;
        exp["experimentId"] = record.experimentId;
        exp["revision"] = record.revision;
        if (record.parentExperimentId.has_value())
            exp["parentExperimentId"] = *record.parentExperimentId;
        else
            exp["parentExperimentId"] = nullptr;

        exp["kind"] = experimentKindToString(record.kind);
        exp["status"] = experimentStatusToString(record.status);

        nlohmann::json cap;
        cap["sampleRate"] = record.capture.sampleRate;
        cap["hostBufferSize"] = record.capture.hostBufferSize;
        cap["processingBlockSize"] = record.capture.processingBlockSize;
        cap["channels"] = record.capture.channels;
        cap["durationSeconds"] = record.capture.durationSeconds;
        cap["presetStateHash"] = record.capture.presetStateHash;
        cap["excitationPlanHash"] = record.capture.excitationPlanHash;
        cap["storageProfile"] = storageProfileToString(record.capture.storageProfile);
        exp["capture"] = cap;

        tempDir.getChildFile("experiment.json").replaceWithText(exp.dump(2));
    }

    // 6. Copiar archivos de audio
    for (const auto& item : audioFilesToInclude)
    {
        const std::string& relPath = item.first;
        const juce::File& srcFile = item.second;

        if (!isSafeRelativePath(relPath))
        {
            outError = "Security error: Unsafe relative path in audioFilesToInclude: " + juce::String(relPath);
            return false;
        }

        if (srcFile.existsAsFile())
        {
            juce::File destFile = tempDir.getChildFile(relPath);
            destFile.getParentDirectory().createDirectory();
            if (!srcFile.copyFileTo(destFile))
            {
                outError = "Failed to copy audio artifact: " + srcFile.getFullPathName() + " to " + destFile.getFullPathName();
                return false;
            }
        }
    }

    // 6.5. Escribir modelo embebido y copia a exports/ si se proporciona
    juce::File writtenConvenienceFile;
    if (embeddedModel.has_value())
    {
        if (!isSafeRelativePath(embeddedModel->relativePathInsideExperiment))
        {
            outError = "Security error: Unsafe relative path in embeddedModel: " + juce::String(embeddedModel->relativePathInsideExperiment);
            return false;
        }

        juce::File embeddedFile = tempDir.getChildFile(embeddedModel->relativePathInsideExperiment);
        embeddedFile.getParentDirectory().createDirectory();
        if (!embeddedFile.replaceWithText(embeddedModel->modelSourceCode))
        {
            outError = "Failed to write embedded model code to: " + embeddedFile.getFullPathName();
            return false;
        }

        std::string embeddedSha = computeFileSha256(embeddedFile);

        // Si se especificó archivo de conveniencia en exports/
        if (embeddedModel->convenienceExportFile != juce::File())
        {
            writtenConvenienceFile = embeddedModel->convenienceExportFile;
            writtenConvenienceFile.getParentDirectory().createDirectory();

            if (!writtenConvenienceFile.replaceWithText(embeddedModel->modelSourceCode))
            {
                outError = "Failed to write convenience export file to: " + writtenConvenienceFile.getFullPathName();
                return false;
            }

            std::string exportSha = computeFileSha256(writtenConvenienceFile);
            if (exportSha != embeddedSha)
            {
                writtenConvenienceFile.deleteFile();
                outError = "Cryptographic mismatch between embedded model (" + embeddedSha + ") and export (" + exportSha + ")";
                return false;
            }
        }
    }

    // 6.75. Ejecutar stagingHook opcional para poblar artefactos antes del cálculo del manifest
    if (stagingHook != nullptr)
    {
        juce::String hookErr;
        if (!stagingHook(tempDir, hookErr))
        {
            if (writtenConvenienceFile.existsAsFile())
                writtenConvenienceFile.deleteFile();

            outError = "Staging hook failed: " + hookErr;
            return false;
        }
    }

    // 7. Enumerate all files, compute SHA-256 and build manifest.json
    juce::Array<juce::File> allFiles;
    tempDir.findChildFiles(allFiles, juce::File::findFiles, true);

    nlohmann::json manifest;
    manifest["manifestFormat"] = "artifact-list-v1";
    manifest["selfHashExcluded"] = true;
    nlohmann::json artList = nlohmann::json::array();

    for (const auto& file : allFiles)
    {
        if (file.getFileName() == "manifest.json")
            continue;

        std::string relPath = file.getRelativePathFrom(tempDir).replaceCharacter('\\', '/').toStdString();
        std::string sha = computeFileSha256(file);
        uint64_t sizeBytes = static_cast<uint64_t>(file.getSize());

        std::string role = "data";
        if (relPath == "validation/target.wav")
            role = "validation_target";
        else if (relPath == "validation/model.wav")
            role = "validation_model";
        else if (relPath == "validation/residual.wav")
            role = "aligned_residual";
        else if (relPath == "validation/holdout_manifest.json")
            role = "holdout_definition";
        else if (relPath == "validation/validation_report.json")
            role = "validation_report";
        else if (relPath == "evidence/guided/baseline.wav")
            role = "guided_baseline_audio";
        else if (relPath == "evidence/guided/modified.wav")
            role = "guided_modified_audio";
        else if (relPath == "evidence/guided/difference.wav")
            role = "guided_differential_audio";
        else if (relPath == "evidence/guided/parameter-test-cutoff.json" || (relPath.rfind("evidence/guided/", 0) == 0 && file.hasFileExtension(".json")))
            role = "guided_parameter_differential_report";
        else if (relPath == "reports/certification_report.html" || file.hasFileExtension(".html"))
            role = "audit_report_html";
        else if (file.hasFileExtension(".wav"))
            role = "target_audio";
        else if (file.getFileName() == "experiment.json")
            role = "experiment_metadata";
        else if (file.getFileName() == "target.json")
            role = "target_metadata";
        else if (file.getFileName() == "provenance.json")
            role = "provenance_metadata";
        else if (file.getFileName() == "evaluation.json")
            role = "evaluation_metadata";
        else if (file.getFileName() == "limitations.json")
            role = "limitations_metadata";
        else if (file.hasFileExtension(".h") || relPath.rfind("models/", 0) == 0)
            role = "embedded_model";

        nlohmann::json artItem;
        artItem["path"] = relPath;
        artItem["role"] = role;
        artItem["sizeBytes"] = sizeBytes;
        artItem["sha256"] = sha;

        // Comprobar si coincide con algún metadato de audio provisto en record.artifacts
        for (const auto& ra : record.artifacts)
        {
            if (ra.relativePath == relPath && ra.audio.has_value())
            {
                const auto& a = *ra.audio;
                nlohmann::json aj;
                aj["sampleRate"] = a.sampleRate;
                aj["channels"] = a.channels;
                aj["bitsPerSample"] = a.bitsPerSample;
                aj["sampleCount"] = a.sampleCount;
                aj["durationSeconds"] = a.durationSeconds;
                aj["format"] = a.format;
                aj["isInterleaved"] = a.isInterleaved;
                artItem["audio"] = aj;
                break;
            }
        }

        artList.push_back(artItem);
    }

    manifest["artifacts"] = artList;
    tempDir.getChildFile("manifest.json").replaceWithText(manifest.dump(2));

    // 8. Verificación previa de lectura en el directorio temporal
    ExperimentFolderReader reader;
    juce::String verifyErr;
    auto readBack = reader.read(tempDir, verifyErr);
    if (!readBack.has_value() || readBack->isCorrupt())
    {
        if (writtenConvenienceFile.existsAsFile())
            writtenConvenienceFile.deleteFile();

        outError = "Self-verification failed before committing experiment: " + verifyErr;
        return false;
    }

    // 9. Renombrado atómico a destino final
    if (!tempDir.moveFileTo(finalDir))
    {
        if (writtenConvenienceFile.existsAsFile())
            writtenConvenienceFile.deleteFile();

        outError = "Failed to move temporary directory to final destination: " + finalDir.getFullPathName();
        return false;
    }

    cleaner.committed = true;
    return true;
}

std::optional<ExperimentRecord> ExperimentStorage::loadExperiment(const juce::File& experimentDir, juce::String& outError)
{
    ExperimentFolderReader reader;
    return reader.read(experimentDir, outError);
}

} // namespace abdaudiolab::core
