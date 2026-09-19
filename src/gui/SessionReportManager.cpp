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

    bool okCpp = exporting::ReportExportService::writeCppLutHeader(
        headerFile.getFullPathName().toStdString(),
        metadata,
        safeBaseName.toStdString(),
        points
    );

    bool okJson = exporting::ReportExportService::writeTelemetryJson(
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

    return exporting::ReportExportService::writeCertificationHtml(
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
    return exporting::ReportExportService::buildAuditionGrid(sessionPoints, gridSize);
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
    auto m = exporting::ReportExportService::calculateMetrics(points, inputTrim);
    outAvgSnr = m.avgSnrDb;
    outNoiseFloor = m.noiseFloorDb;
    outAvgThd = m.avgThdPercent;
    outCount = m.validPointCount;
    outDurationSec = m.totalDurationSec;
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

    // Build ReportExportRequest for domain/infrastructure service
    exporting::ReportExportRequest srvReq;
    srvReq.manifest.hardwareId = request.hardwareId.toStdString();
    srvReq.manifest.hardwareDisplayName = request.hardwareName.isNotEmpty() ? request.hardwareName.toStdString() : request.hardwareId.toStdString();
    srvReq.manifest.activeFunctionId = request.functionId.toStdString();
    srvReq.manifest.activeFunctionName = request.functionName.isNotEmpty() ? request.functionName.toStdString() : request.functionId.toStdString();
    srvReq.manifest.targetModule = srvReq.manifest.activeFunctionId;
    srvReq.destinationDirectory = targetDir.getFullPathName().toStdString();
    srvReq.baseFileName = base.toStdString();
    srvReq.context.sampleRate = request.sampleRate;
    srvReq.context.operatorNotes = request.operatorNotes.toStdString();
    srvReq.context.ambientTemperatureC = request.ambientTemperatureC;
    srvReq.context.warmupTimeMinutes = request.warmupTimeMinutes;
    srvReq.measuredPoints = points;

    // Reset default options
    srvReq.options.includeHtmlCertification = false;
    srvReq.options.includeCppLutHeader = false;
    srvReq.options.includeTelemetryJson = false;
    srvReq.options.includeProductionPackage = false;
    srvReq.options.includeAuditionData = false;

    switch (request.format)
    {
        case ReportFormat::HtmlCertification:
            srvReq.options.includeHtmlCertification = true;
            break;
        case ReportFormat::CppLutHeader:
            srvReq.options.includeCppLutHeader = true;
            break;
        case ReportFormat::JsonTelemetry:
            srvReq.options.includeTelemetryJson = true;
            break;
        case ReportFormat::ProductionPackage:
            srvReq.options.includeProductionPackage = true;
            break;
        case ReportFormat::AuditionLutGrid:
            srvReq.options.includeAuditionData = true;
            break;
        default:
            result.status = ExportStatus::UnsupportedFormat;
            result.errorCode = "ERR_UNSUPPORTED_FORMAT";
            result.userMessage = "Unsupported report format requested.";
            return result;
    }

    auto srvRes = exporting::ReportExportService::exportReport(srvReq);

    result.succeeded = srvRes.succeeded();
    result.status = srvRes.succeeded() ? ExportStatus::Succeeded : ExportStatus::WriteFailed;
    result.errorCode = srvRes.errorCode;
    result.userMessage = srvRes.userMessage;
    result.manifestSha256 = srvRes.manifestSha256;
    if (!srvRes.primaryOutputPath.empty())
    {
        result.outputPath = juce::File(srvRes.primaryOutputPath.string());
    }
    for (const auto& art : srvRes.artifacts)
    {
        result.artifactPaths.push_back(juce::File(art.publishedPath.string()));
    }

    if (result.succeeded && request.format == ReportFormat::ProductionPackage)
    {
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
    }

    return result;
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
