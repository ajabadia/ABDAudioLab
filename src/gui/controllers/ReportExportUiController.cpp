#include "ReportExportUiController.h"

namespace abdaudiolab::gui {

ReportExportUiController::ReportExportUiController(IReportExportHost& hostRef)
    : host(hostRef),
      sharedState(std::make_shared<SharedWorkerState>())
{
}

ReportExportUiController::~ReportExportUiController()
{
    sharedState->cancelled.store(true);
    sharedState->currentRunId.fetch_add(1);

    if (workerThread.joinable())
    {
        workerThread.join();
    }

    cancelPendingUpdate();
}

bool ReportExportUiController::isExportInProgress() const noexcept
{
    return inProgress.load();
}

void ReportExportUiController::updateMetricsPreview()
{
    auto snapshot = host.createReportExportSnapshot();
    auto metrics = exporting::ReportExportService::calculateMetrics(
        snapshot.measuredPoints,
        snapshot.inputTrimDb
    );
    host.updateExportReportMetrics(metrics);
}

void ReportExportUiController::flushAsyncUpdates()
{
    handleUpdateNowIfNeeded();
}

bool ReportExportUiController::requestExportProductionPackage()
{
    exporting::ReportExportOptions opt;
    opt.includeProductionPackage = true;
    opt.includeHtmlCertification = false;
    return launchExportAsync(opt);
}

bool ReportExportUiController::requestExportCertificationReport()
{
    exporting::ReportExportOptions opt;
    opt.includeHtmlCertification = true;
    opt.includeProductionPackage = false;
    return launchExportAsync(opt);
}

bool ReportExportUiController::launchExportAsync(exporting::ReportExportOptions options)
{
    bool expected = false;
    if (!inProgress.compare_exchange_strong(expected, true))
    {
        // Concurrency guard: export already in progress
        return false;
    }

    // 1. Snapshot captured strictly on the message thread
    auto snapshot = host.createReportExportSnapshot();
    if (snapshot.exportDirectory.empty() || snapshot.measuredPoints.empty())
    {
        inProgress.store(false);
        host.showMessageBox("Export Warning", "Cannot export report: destination directory is invalid or session contains no measured points.", true);
        return false;
    }

    // 2. Generate unique execution token
    uint64_t nextRunId = sharedState->currentRunId.fetch_add(1) + 1;
    ExportExecutionToken token{ nextRunId };

    // 3. Clean up previously finished worker thread if any
    if (workerThread.joinable())
    {
        workerThread.join();
    }

    // 4. Launch background worker thread passing snapshot and shared state by value
    workerThread = std::thread([state = sharedState, snapshot = std::move(snapshot), options, token, this]() {
        exporting::ReportExportRequest req;
        req.manifest = snapshot.manifest;
        req.measuredPoints = snapshot.measuredPoints;
        req.destinationDirectory = snapshot.exportDirectory;
        req.baseFileName = snapshot.baseFileName;
        req.context.sampleRate = snapshot.sampleRate;
        req.context.inputTrimDb = snapshot.inputTrimDb;
        req.context.operatorNotes = snapshot.operatorNotes;
        req.context.ambientTemperatureC = snapshot.ambientTemperatureC;
        req.context.warmupTimeMinutes = snapshot.warmupMinutes;
        req.options = options;

        auto result = exporting::ReportExportService::exportReport(req);

        if (state->cancelled.load())
            return;

        CompletedResult comp;
        comp.token = token;
        comp.result = std::move(result);
        comp.options = options;
        comp.baseFileName = juce::String(req.baseFileName);
        comp.destinationDirectory = juce::File(snapshot.exportDirectory.string());

        {
            std::lock_guard<std::mutex> lock(state->queueMutex);
            state->completedResults.push_back(std::move(comp));
        }

        this->triggerAsyncUpdate();
    });

    return true;
}

void ReportExportUiController::handleAsyncUpdate()
{
    if (sharedState->cancelled.load())
        return;

    std::vector<CompletedResult> batch;
    {
        std::lock_guard<std::mutex> lock(sharedState->queueMutex);
        batch.swap(sharedState->completedResults);
    }

    for (const auto& item : batch)
    {
        if (sharedState->cancelled.load())
            break;

        if (item.token.runId != sharedState->currentRunId.load())
        {
            // Discard stale result
            continue;
        }

        if (item.result.succeeded())
        {
            host.updateExportReportMetrics(item.result.metrics);
            host.notifyExportSuccess(item.destinationDirectory, item.baseFileName);

            if (item.options.includeProductionPackage)
            {
                host.showStatusBanner(juce::String::fromUTF8(u8"⚡ Paquete de producción exportado con éxito (C++ alignas(16), JSON, HTML)"), false);
            }
            else if (item.options.includeHtmlCertification)
            {
                host.showStatusBanner("Certification Report exported: " + juce::File(item.result.primaryOutputPath.string()).getFileName(), false);
                host.launchProcess(juce::File(item.result.primaryOutputPath.string()));
            }
        }
        else
        {
            juce::String errorMsg = item.result.userMessage.empty() ? "Export operation failed." : juce::String(item.result.userMessage);
            host.showMessageBox("Export Failed", errorMsg, true);
        }
    }

    inProgress.store(false);
}

void ReportExportUiController::openExportFolderInExplorer()
{
    auto snapshot = host.createReportExportSnapshot();
    juce::File folder(snapshot.exportDirectory.string());
    if (!folder.exists())
        folder.createDirectory();

    host.revealInFolder(folder);
    host.showPanelStatus(juce::String::fromUTF8(u8"✓ Carpeta de exportación abierta en el Explorador."));
}

void ReportExportUiController::openCertificationReportHtml()
{
    auto snapshot = host.createReportExportSnapshot();
    juce::File folder(snapshot.exportDirectory.string());

    juce::String base = juce::String(snapshot.baseFileName);
    if (base.isEmpty())
    {
        juce::String hw = juce::String(snapshot.manifest.hardwareId);
        juce::String fn = juce::String(snapshot.manifest.activeFunctionId);
        base = (hw.isNotEmpty() ? hw : "hardware").toLowerCase() + "_" + (fn.isNotEmpty() ? fn : "profile").toLowerCase();
    }

    juce::File htmlFile = folder.getChildFile(base + "_Certification_Report.html");
    if (!htmlFile.existsAsFile())
    {
        auto htmlFiles = folder.findChildFiles(juce::File::findFiles, false, "*Certification_Report.html");
        if (!htmlFiles.isEmpty())
            htmlFile = htmlFiles.getFirst();
    }

    if (htmlFile.existsAsFile())
    {
        host.launchProcess(htmlFile);
        host.showPanelStatus(juce::String::fromUTF8(u8"✓ Informe HTML abierto en el navegador."));
    }
    else
    {
        host.showPanelStatus(juce::String::fromUTF8(u8"⚠️ No hay informe HTML generado aún. Ejecute la exportación primero."), true);
    }
}

} // namespace abdaudiolab::gui
