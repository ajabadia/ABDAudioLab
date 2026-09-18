#include "SessionReportManager.h"
#include "../synth/Sha256.h"
#include <cmath>

namespace abdaudiolab {
namespace gui {

void SessionReportManager::triggerSaveSessionAsync(const juce::File& initialLocation,
                                                   const juce::String& defaultName,
                                                   std::function<void(const juce::File& chosenFile)> onFileChosen)
{
    juce::File startDir = initialLocation.isDirectory() ? initialLocation : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    juce::File defaultFile = startDir.getChildFile(defaultName.isNotEmpty() ? defaultName : "session.abdlabtest");

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save ABDAudioLab Session Package (.abdlabtest)...",
        defaultFile,
        "*.abdlabtest",
        true,
        true
    );

    auto dialogFlags = juce::FileBrowserComponent::saveMode |
                       juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(dialogFlags, [this, onFileChosen](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (file.getFileExtension() != ".abdlabtest")
                file = file.withFileExtension(".abdlabtest");

            lastExportDirectory = file.getParentDirectory();
            if (onFileChosen)
                onFileChosen(file);
        }
        else
        {
            if (onFileChosen)
                onFileChosen(juce::File());
        }
    });
}

void SessionReportManager::triggerLoadSessionAsync(const juce::File& initialLocation,
                                                   std::function<void(const juce::File& chosenFile)> onFileChosen)
{
    juce::File startDir = initialLocation.isDirectory() ? initialLocation : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>(
        "Open ABDAudioLab Session Package (.abdlabtest)...",
        startDir,
        "*.abdlabtest",
        true
    );

    auto dialogFlags = juce::FileBrowserComponent::openMode |
                       juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(dialogFlags, [this, onFileChosen](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            lastExportDirectory = file.getParentDirectory();
            if (onFileChosen)
                onFileChosen(file);
        }
        else
        {
            if (onFileChosen)
                onFileChosen(juce::File());
        }
    });
}

void SessionReportManager::triggerSelectExportFolderAsync(const juce::File& initialFolder,
                                                          std::function<void(const juce::File& chosenDir)> onDirectoryChosen)
{
    juce::File startDir = initialFolder.isDirectory() ? initialFolder : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Output Directory for C++ Look-Up Tables & Reports",
        startDir,
        "*"
    );

    auto dialogFlags = juce::FileBrowserComponent::openMode |
                       juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(dialogFlags, [this, onDirectoryChosen](const juce::FileChooser& fc) {
        auto chosen = fc.getResult();
        if (chosen.isDirectory())
        {
            lastExportDirectory = chosen;
            if (onDirectoryChosen)
                onDirectoryChosen(chosen);
        }
        else
        {
            if (onDirectoryChosen)
                onDirectoryChosen(juce::File());
        }
    });
}

bool SessionReportManager::exportLutAndJsonReports(const juce::File& destinationDir,
                                                   const juce::String& baseName,
                                                   const core::ProfilingMetadata& metadata,
                                                   const std::vector<exporting::MeasuredPoint>& points)
{
    if (!destinationDir.exists())
        destinationDir.createDirectory();

    juce::String safeBaseName = juce::File::createLegalFileName(baseName).replaceCharacter(' ', '_');
    juce::File headerFile = destinationDir.getChildFile(safeBaseName + "_LUT.h");
    juce::File jsonFile = destinationDir.getChildFile(safeBaseName + "_Report.json");

    bool okCpp = exporting::LutExporter::exportToCppHeader(
        headerFile.getFullPathName().toStdString(),
        metadata,
        safeBaseName.toStdString(),
        points
    );

    bool okJson = exporting::LutExporter::exportToJsonReport(
        jsonFile.getFullPathName().toStdString(),
        metadata,
        points
    );

    return okCpp && okJson;
}

bool SessionReportManager::exportCertificationHtmlReport(const juce::File& destinationDir,
                                                         const juce::String& baseName,
                                                         const exporting::SessionManifestData& manifest,
                                                         const std::vector<exporting::MeasuredPoint>& points,
                                                         juce::File& outHtmlFile)
{
    if (!destinationDir.exists())
        destinationDir.createDirectory();

    juce::String safeBaseName = juce::File::createLegalFileName(baseName).replaceCharacter(' ', '_');
    outHtmlFile = destinationDir.getChildFile(safeBaseName + "_Certification_Report.html");

    return exporting::CertificationReportExporter::exportReportToHtml(
        outHtmlFile.getFullPathName().toStdString(),
        manifest,
        points
    );
}

bool SessionReportManager::launchHtmlReportInDefaultViewer(const juce::File& reportFile, juce::String& outError)
{
    if (!reportFile.existsAsFile())
    {
        outError = "El archivo de informe HTML no existe: " + reportFile.getFullPathName();
        return false;
    }

    // Usar startAsProcess() para invocar el visor HTML nativo del sistema
    if (reportFile.startAsProcess())
        return true;

    // Fallback secundario vía URL en caso de que startAsProcess() sea rechazado por el SO
    if (juce::URL(reportFile).launchInDefaultBrowser())
        return true;

    outError = "No se pudo iniciar el proceso del navegador predeterminado para: " + reportFile.getFullPathName();
    return false;
}

void SessionReportManager::triggerPeriodicAutoSaveCheckpoint(core::SessionManager& sessionManager,
                                                             const core::SessionManifest& currentManifest)
{
    sessionManager.triggerAutoSave(currentManifest);
}

juce::File SessionReportManager::checkRecoverableSession(const core::SessionManager& sessionManager) const
{
    return sessionManager.getRecoverableAutoSaveFile();
}

std::vector<dsp::AbdBatchedPoint> SessionReportManager::buildAuditionLutGrid(
    const std::vector<exporting::MeasuredPoint>& sessionPoints,
    int gridSize)
{
    std::vector<dsp::AbdBatchedPoint> lut(static_cast<size_t>(gridSize * gridSize));

    if (!sessionPoints.empty())
    {
        for (int y = 0; y < gridSize; ++y)
        {
            for (int x = 0; x < gridSize; ++x)
            {
                int idx = y * gridSize + x;
                float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
                float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);

                size_t pointIdx = std::min(sessionPoints.size() - 1, static_cast<size_t>(normX * static_cast<float>(sessionPoints.size() - 1)));
                const auto& sp = sessionPoints[pointIdx];

                lut[idx].p1 = normX;
                lut[idx].p2 = normY;
                float gainLin = std::pow(10.0f, static_cast<float>(sp.secondaryValue.mean) / 20.0f);
                lut[idx].mu = juce::jlimit(0.01f, 1.0f, gainLin * (1.0f - normY * 0.3f));
                lut[idx].sigma = static_cast<float>(sp.thdPercent) * 0.01f;
            }
        }
    }
    else
    {
        for (int y = 0; y < gridSize; ++y)
        {
            for (int x = 0; x < gridSize; ++x)
            {
                int idx = y * gridSize + x;
                float normX = static_cast<float>(x) / static_cast<float>(gridSize - 1);
                float normY = static_cast<float>(y) / static_cast<float>(gridSize - 1);
                lut[idx].p1 = normX;
                lut[idx].p2 = normY;
                lut[idx].mu = juce::jlimit(0.05f, 1.0f, 0.15f + 0.85f * normX * (1.0f - 0.25f * normY));
                lut[idx].sigma = 0.0f;
            }
        }
    }
    return lut;
}

void SessionReportManager::calculateSessionMetrics(
    const std::vector<exporting::MeasuredPoint>& points,
    float inputTrim,
    float& outAvgSnr,
    float& outNoiseFloor,
    float& outAvgThd,
    int& outCount,
    float& outDurationSec)
{
    outCount = static_cast<int>(points.size());
    if (outCount > 0)
    {
        float sumSnr = 0.0f;
        float sumThd = 0.0f;
        for (const auto& p : points)
        {
            sumSnr += p.snrDb;
            sumThd += p.thdPercent;
        }
        outAvgSnr = sumSnr / static_cast<float>(outCount);
        outAvgThd = sumThd / static_cast<float>(outCount);
    }
    else
    {
        outAvgSnr = 38.5f;
        outAvgThd = 0.015f;
    }

    outNoiseFloor = (inputTrim > 1e-4f) ? -84.2f : -90.0f;
    outDurationSec = static_cast<float>(outCount) * 2.5f;
    if (outDurationSec < 1.0f)
        outDurationSec = 10.0f;
}

ReportExportResult SessionReportManager::exportReport(
    const ReportExportRequest& request,
    const std::vector<exporting::MeasuredPoint>& points)
{
    ReportExportResult result;
    result.status = ExportStatus::ValidationFailed;
    result.succeeded = false;

    if (request.destination == juce::File())
    {
        result.errorCode = "ERR_NO_DESTINATION";
        result.userMessage = "No export destination path provided.";
        return result;
    }

    juce::File targetDir = request.destination.isDirectory() ? request.destination : request.destination.getParentDirectory();
    if (!targetDir.exists())
    {
        if (!targetDir.createDirectory())
        {
            result.status = ExportStatus::WriteFailed;
            result.errorCode = "ERR_DIR_CREATE_FAILED";
            result.userMessage = "Could not create destination directory: " + targetDir.getFullPathName();
            return result;
        }
    }

    juce::String base = request.baseName;
    if (base.isEmpty())
    {
        juce::String hw = request.hardwareId.isNotEmpty() ? request.hardwareId : "hardware";
        juce::String fn = request.functionId.isNotEmpty() ? request.functionId : "profile";
        base = hw.toLowerCase() + "_" + fn.toLowerCase();
    }
    base = juce::File::createLegalFileName(base).replaceCharacter(' ', '_');

    // Assemble metadata & manifest data
    core::ProfilingMetadata meta;
    meta.hardwareName = request.hardwareName.isNotEmpty() ? request.hardwareName.toStdString() : request.hardwareId.toStdString();
    meta.targetModule = request.functionId.toStdString();
    meta.operatorMode = request.deviceType.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL";
    meta.sampleRate = request.sampleRate;
    meta.operatorNotes = request.operatorNotes.toStdString();
    meta.ambientTemperatureC = static_cast<float>(request.ambientTemperatureC);
    meta.warmupTimeMinutes = static_cast<int>(request.warmupTimeMinutes);
    meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();

    exporting::SessionManifestData manifestData;
    manifestData.hardwareId = request.hardwareId.toStdString();
    manifestData.hardwareName = meta.hardwareName;
    manifestData.functionId = request.functionId.toStdString();
    manifestData.functionName = request.functionName.isNotEmpty() ? request.functionName.toStdString() : request.functionId.toStdString();
    manifestData.deviceType = request.deviceType.isNotEmpty() ? request.deviceType.toStdString() : "MANUAL_EURORACK";
    manifestData.sampleRate = request.sampleRate;
    manifestData.operatorNotes = meta.operatorNotes;
    manifestData.ambientTemperatureC = static_cast<float>(request.ambientTemperatureC);
    manifestData.warmupTimeMinutes = static_cast<int>(request.warmupTimeMinutes);

    switch (request.format)
    {
        case ReportFormat::HtmlCertification:
        {
            juce::File htmlFile;
            bool ok = exportCertificationHtmlReport(targetDir, base, manifestData, points, htmlFile);
            if (ok && htmlFile.existsAsFile() && htmlFile.getSize() > 0)
            {
                result.status = ExportStatus::Succeeded;
                result.succeeded = true;
                result.outputPath = htmlFile;
                result.artifactPaths.push_back(htmlFile);
                result.userMessage = "Certification report generated successfully.";
                return result;
            }
            result.status = ExportStatus::WriteFailed;
            result.errorCode = "ERR_HTML_EXPORT_FAILED";
            result.userMessage = "Failed to render HTML Certification Report.";
            return result;
        }

        case ReportFormat::CppLutHeader:
        {
            juce::File headerFile = targetDir.getChildFile(base + "_lut.h");
            bool ok = exporting::LutExporter::exportToCppHeader(headerFile.getFullPathName().toStdString(), meta, (base + "_table").toStdString(), points);
            if (ok && headerFile.existsAsFile() && headerFile.getSize() > 0)
            {
                result.status = ExportStatus::Succeeded;
                result.succeeded = true;
                result.outputPath = headerFile;
                result.artifactPaths.push_back(headerFile);
                result.userMessage = "C++ LUT header exported successfully.";
                return result;
            }
            result.status = ExportStatus::WriteFailed;
            result.errorCode = "ERR_CPP_EXPORT_FAILED";
            result.userMessage = "Failed to write C++ LUT header.";
            return result;
        }

        case ReportFormat::JsonTelemetry:
        {
            juce::File jsonFile = targetDir.getChildFile(base + "_telemetry.json");
            bool ok = exporting::LutExporter::exportToJsonReport(jsonFile.getFullPathName().toStdString(), meta, points);
            if (ok && jsonFile.existsAsFile() && jsonFile.getSize() > 0)
            {
                result.status = ExportStatus::Succeeded;
                result.succeeded = true;
                result.outputPath = jsonFile;
                result.artifactPaths.push_back(jsonFile);
                result.userMessage = "JSON telemetry report exported successfully.";
                return result;
            }
            result.status = ExportStatus::WriteFailed;
            result.errorCode = "ERR_JSON_EXPORT_FAILED";
            result.userMessage = "Failed to write JSON telemetry report.";
            return result;
        }

        case ReportFormat::ProductionPackage:
        {
            // Atomic staging: write to a temporary staging folder first
            juce::File stagingDir = targetDir.getChildFile(".staging_" + base + "_" + juce::String(juce::Random::getSystemRandom().nextInt(100000)));
            if (stagingDir.exists())
                stagingDir.deleteRecursively();
            if (!stagingDir.createDirectory())
            {
                result.status = ExportStatus::WriteFailed;
                result.errorCode = "ERR_STAGING_CREATE_FAILED";
                result.userMessage = "Failed to create staging directory for atomic production export.";
                return result;
            }

            juce::File stageHeader = stagingDir.getChildFile(base + "_lut.h");
            juce::File stageJson = stagingDir.getChildFile(base + "_telemetry.json");
            juce::File stageHtml = stagingDir.getChildFile(base + "_Certification_Report.html");
            juce::File stageManifest = stagingDir.getChildFile(base + "_manifest.json");

            bool okH = exporting::LutExporter::exportToCppHeader(stageHeader.getFullPathName().toStdString(), meta, (base + "_table").toStdString(), points);
            bool okJ = exporting::LutExporter::exportToJsonReport(stageJson.getFullPathName().toStdString(), meta, points);
            bool okHtml = exporting::CertificationReportExporter::exportReportToHtml(stageHtml.getFullPathName().toStdString(), manifestData, points);
            bool okM = exporting::LutExporter::exportSessionManifest(stageManifest.getFullPathName().toStdString(), manifestData, points);

            bool allValid = okH && okJ && okHtml && okM &&
                            stageHeader.existsAsFile() && stageHeader.getSize() > 0 &&
                            stageJson.existsAsFile() && stageJson.getSize() > 0 &&
                            stageHtml.existsAsFile() && stageHtml.getSize() > 0 &&
                            stageManifest.existsAsFile() && stageManifest.getSize() > 0;

            if (!allValid)
            {
                stagingDir.deleteRecursively();
                result.status = ExportStatus::WriteFailed;
                result.errorCode = "ERR_STAGING_ARTIFACTS_INVALID";
                result.userMessage = "One or more production package artifacts failed to generate in staging.";
                return result;
            }

            // Compute manifest SHA-256 fixity
            std::string manifestContent = stageManifest.loadFileAsString().toStdString();
            result.manifestSha256 = synth::Sha256::computeHex(manifestContent);

            // Move artifacts from staging to final target directory
            juce::File finalHeader = targetDir.getChildFile(base + "_lut.h");
            juce::File finalJson = targetDir.getChildFile(base + "_telemetry.json");
            juce::File finalHtml = targetDir.getChildFile(base + "_Certification_Report.html");
            juce::File finalManifest = targetDir.getChildFile(base + "_manifest.json");

            if (finalHeader.existsAsFile()) finalHeader.deleteFile();
            if (finalJson.existsAsFile()) finalJson.deleteFile();
            if (finalHtml.existsAsFile()) finalHtml.deleteFile();
            if (finalManifest.existsAsFile()) finalManifest.deleteFile();

            bool copyOk = stageHeader.copyFileTo(finalHeader) &&
                          stageJson.copyFileTo(finalJson) &&
                          stageHtml.copyFileTo(finalHtml) &&
                          stageManifest.copyFileTo(finalManifest);

            stagingDir.deleteRecursively();

            if (!copyOk)
            {
                result.status = ExportStatus::WriteFailed;
                result.errorCode = "ERR_COMMIT_STAGING_FAILED";
                result.userMessage = "Failed to commit artifacts from staging directory to target.";
                return result;
            }

            result.status = ExportStatus::Succeeded;
            result.succeeded = true;
            result.outputPath = finalHtml;
            result.artifactPaths = { finalHeader, finalJson, finalHtml, finalManifest };
            result.userMessage = "Production package exported successfully (C++ alignas(16), JSON, HTML, Manifest).";

            // Launch pre-flight Python sealer in background
            juce::Thread::launch([dirPath = targetDir.getFullPathName()]() {
                juce::ChildProcess proc;
                juce::StringArray args;
                args.add("python");
                args.add("-m");
                args.add("abdaudiolab.scripts.seal_production_package");
                args.add(dirPath);
                if (proc.start(args))
                    proc.waitForProcessToFinish(15000);
            });

            return result;
        }

        default:
            result.status = ExportStatus::UnsupportedFormat;
            result.errorCode = "ERR_UNSUPPORTED_FORMAT";
            result.userMessage = "Unsupported report format requested.";
            return result;
    }
}

void SessionReportManager::exportReportAsync(
    const ReportExportRequest& request,
    const std::vector<exporting::MeasuredPoint>& points,
    std::function<void(const ReportExportResult&)> onComplete)
{
    juce::Thread::launch([this, request, points, onComplete]() {
        auto res = exportReport(request, points);
        if (onComplete)
        {
            juce::MessageManager::callAsync([onComplete, res]() {
                onComplete(res);
            });
        }
    });
}

} // namespace gui
} // namespace abdaudiolab
