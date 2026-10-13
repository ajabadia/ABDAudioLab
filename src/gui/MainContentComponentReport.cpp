/**
 * @file MainContentComponentReport.cpp
 * @brief Report generation, dataset export, and the loaded-session / report-export hosts.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"

namespace abdaudiolab
{

// ==============================================================================
// POR QUE ESTO ESTA EN UN FICHERO PROPIO Y NO EN MainContentComponent.cpp.
//
// Porque el fichero eran 3925 lineas con 80 metodos. En un fichero asi el numero
// de un metodo no dice nada: hay que recorrerlo entero para saber si esta en esta
// seccion o en la siguiente. Leer no es el problema, modificar si: cambiar diez
// lineas de un metodo obliga a recorrer 4000 lineas, y el metodo que toca acaba
// en el sitio menos probable de donde estabas mirando.
//
// No se ha movido ni una linea de codigo: los mismos cuerpos de funcion, en la
// misma clase, enlazados igual. Lo unico que cambia es el fichero donde viven, y
// eso se comprueba compilando.
// ==============================================================================

// ==============================================================================
// SECTION 9: REPORT GENERATION & DATASET EXPORT DELEGATION
// Owns UI export action triggers, report format selection modals, and export progress display.
// Report compilation delegated to SessionReportManager; certification rendering to CertificationReportExporter.
// ==============================================================================
core::SessionManifest MainContentComponent::buildCurrentSessionManifest()
{
    core::SessionManifest sm;
    sm.appVersion = version::kAppVersion;
    sm.buildNumber = version::kBuildNumber;
    sm.formatVersion = "1.0";
    sm.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    sm.hardwareId = drawer.getSelectedHardwareId().toStdString();
    sm.hardwareDisplayName = drawer.getActiveHardwareDisplayName().toStdString();
    sm.activeFunctionId = drawer.getSelectedFunctionId().toStdString();
    sm.activeFunctionName = drawer.getActiveFunctionDisplayName().toStdString();
    sm.sampleRate = audioEngine.getSampleRate();
    sm.gainPlan = sequencer.getGainPlan();
    sm.lineCalibrationGainDb = sm.gainPlan.effectiveTrimDb;
    const bool isBypassed = workflowNavController.isCalibrationSkipped() || canonicalCalibrationState.isSkipped;
    sm.calibrationMode = isBypassed ? "Bypass" : "ValidatedPhysicalLoopback";
    if (isBypassed)
    {
        sm.calibrationSnapshot = std::nullopt;
        sm.hasPhysicalNoiseBaseline = false;
        sm.snrMeasurementMethod = math::SnrMeasurementMethod::NotAvailable;
    }
    else if (auto ctx = sequencer.getActiveCalibrationContext())
    {
        const auto& snapOpt = ctx->getSnapshot();
        if (snapOpt.has_value())
        {
            sm.calibrationSnapshot = *snapOpt;
            const auto& baseline = snapOpt->noiseBaseline;
            sm.noiseBaselineStatus = baseline.status;
            if (baseline.status == calibration::NoiseBaselineStatus::Valid
                || baseline.status == calibration::NoiseBaselineStatus::BelowMeasurementFloor)
            {
                sm.measuredNoiseFloorRmsDbfs = baseline.rmsDbfs;
                sm.measuredNoiseFloorPeakDbfs = baseline.peakDbfs;
                sm.hasPhysicalNoiseBaseline = true;
                sm.snrMeasurementMethod = math::SnrMeasurementMethod::PhysicalNoiseBaseline;
            }
            else
            {
                sm.hasPhysicalNoiseBaseline = false;
                sm.snrMeasurementMethod = math::SnrMeasurementMethod::NotAvailable;
            }
        }
    }
    // Acceptance policy threshold remains strictly decoupled from observed physical measurement
    sm.noiseFloorThresholdDb = -85.0f;
    sm.totalMeasuredPoints = totalPointsMeasured;
    sm.operatorNotes = drawer.getOperatorNotes().toStdString();
    sm.ambientTemperatureC = drawer.getAmbientTemperature();
    sm.warmupTimeMinutes = drawer.getWarmupTimeMinutes();

    for (const auto& item : suiteList.getQueue())
    {
        gui::TestConfiguration tc;
        tc.testName = item.title;
        tc.stimulusType = item.stimulusType;
        tc.burstDurationSec = item.burstDurationSec;
        tc.captureMode = item.captureMode;
        tc.controls = item.controls;
        sm.tests.push_back(tc);
    }
    return sm;
}

void MainContentComponent::applyLoadedSession(const core::SessionManifest& manifest,
                                              const std::vector<exporting::MeasuredPoint>& points)
{
    const bool hasContract = (hardwareManager.findContractById(manifest.hardwareId) != nullptr);
    auto result = gui::LoadedSessionApplier::apply(manifest, points, hasContract, *this);

    if (!result.succeeded())
    {
        manualPromptLabel.setText("Failed to load session: " + result.message, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
        return;
    }

    resized();
    manualPromptLabel.setText(result.message, juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
}

// =============================================================================
// ILoadedSessionTarget implementation
// =============================================================================

void MainContentComponent::setSessionData(const core::SessionManifest& manifest,
                                           const std::vector<exporting::MeasuredPoint>& points)
{
    totalPointsMeasured = 0;
    sessionManager.setManifest(manifest);
    sessionManager.setMeasuredPoints(points);
    sessionManager.setDirty(false);
}

void MainContentComponent::clearPlotterAndAddPoints(const std::vector<exporting::MeasuredPoint>& points)
{
    curvePlotter.clear();
    for (const auto& pt : points)
    {
        curvePlotter.addMeasuredPoint(pt);
        ++totalPointsMeasured;
    }
}

void MainContentComponent::updateDrawerAndEnvironment(const gui::SessionUiPresentationData& data)
{
    drawer.setOperatorNotes(data.operatorNotes);
    drawer.setAmbientTemperature(data.ambientTemperatureC);
    drawer.setWarmupTimeMinutes(data.warmupTimeMinutes);

    // Synchronize Session Summary card
    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName       = data.hardwareDisplayName;
    summary.hardwareCategory   = data.targetModule;
    summary.loopbackCalibrated = true;
    sidebarStepper.setSessionSummary(summary);
}

void MainContentComponent::updateHardwarePanels(const gui::SessionUiPresentationData& data)
{
    drawer.setSelectedHardwareId(data.hardwareId);
    drawer.setHardwareLocked(true);
    catalogSelector.setSelectedHardware(data.hardwareId, data.activeFunctionId);
    catalogSelector.setHardwareLocked(true);

    if (data.hasValidHardwareContract)
    {
        onHardwareSelected(data.hardwareId, data.activeFunctionId);
    }
    else
    {
        mainHeader.setHardwareInfo(data.hardwareDisplayName,
                                   data.activeFunctionId,
                                   juce::Image(),
                                   gui::HardwareConnectionStatus::NotApplicable);
    }
}

void MainContentComponent::rebuildTestSuiteQueue(const std::vector<core::SessionManifest>& /*manifests*/,
                                                  const std::vector<gui::QueueItem>& items)
{
    suiteList.clearQueue();
    for (const auto& item : items)
        suiteList.addTestToQueue(item);
}

void MainContentComponent::updateWorkflowAndNavigation(const gui::WorkflowStepState& workflowState)
{
    // Unlock prerequisites so loaded sessions can re-enter from step 1
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::SystemInfo,        false);
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::HardwareRouting,   false);
    sidebarStepper.setStepLocked(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, false);

    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::SystemInfo,        gui::SoundIdSidebarStepper::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::HardwareRouting,   gui::SoundIdSidebarStepper::StepStatus::Completed);
    sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::CalibrateLoopback, gui::SoundIdSidebarStepper::StepStatus::Completed);

    sidebarStepper.setCurrentStep(workflowState.targetSidebarStep);

    sidebarStepper.setStepStatus(
        gui::SoundIdSidebarStepper::Step::RunSession,
        workflowState.runSessionStatus == gui::CanonicalStepStatus::Completed
            ? gui::SoundIdSidebarStepper::StepStatus::Completed
            : gui::SoundIdSidebarStepper::StepStatus::Current);

    if (workflowState.isSessionComplete)
    {
        sidebarStepper.setStepStatus(gui::SoundIdSidebarStepper::Step::ExportReport, gui::SoundIdSidebarStepper::StepStatus::Current);
    }

    workflowNavController.setStep(workflowState.targetSidebarStep);
}

void MainContentComponent::handleSaveSession()
{
    sessionIoController.handleSaveSession();
}

void MainContentComponent::handleSaveSessionAs()
{
    sessionIoController.handleSaveSessionAs();
}

void MainContentComponent::saveSessionToFile(const juce::File& file)
{
    sessionIoController.saveSessionToFile(file);
}

void MainContentComponent::exportCertificationReport()
{
    reportExportController.requestExportCertificationReport();
}

void MainContentComponent::updateExportReportMetrics()
{
    reportExportController.updateMetricsPreview();
}

void MainContentComponent::exportProductionPackage()
{
    reportExportController.requestExportProductionPackage();
}

void MainContentComponent::openCertificationReportHtml()
{
    reportExportController.openCertificationReportHtml();
}

// ==============================================================================
// IReportExportHost Implementation
// ==============================================================================
gui::ReportExportSnapshot MainContentComponent::createReportExportSnapshot() const
{
    gui::ReportExportSnapshot snapshot;

    const auto canonicalTarget = resolveCanonicalTarget();
    juce::String hwId;
    juce::String hwName;
    juce::String targetModule = "MANUAL_EURORACK";

    if (canonicalTarget.has_value())
    {
        hwId = juce::String(canonicalTarget->targetId);
        hwName = juce::String(canonicalTarget->targetName);
        if (canonicalTarget->kind == gui::session::TargetKind::PluginVST3)
        {
            targetModule = "PLUGIN_VIRTUAL";
        }
        else if (hwId.containsIgnoreCase("AIRA"))
        {
            targetModule = "AUTOMATED_SYSEX";
        }
    }

    juce::String funcId = drawer.getSelectedFunctionId();
    if (funcId.isEmpty())
        funcId = catalogSelector.getSelectedFunctionId();

    if (funcId.isEmpty() && hwId.isNotEmpty())
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr && !contract->functions.empty())
            funcId = contract->functions[0].id;
    }

    if (hwName.isEmpty() && hwId.isNotEmpty())
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
            hwName = contract->displayName;
        else
            hwName = hwId;
    }

    snapshot.manifest.hardwareId = hwId.toStdString();
    snapshot.manifest.hardwareDisplayName = hwName.toStdString();
    snapshot.manifest.hardwareName = hwName.toStdString();

    snapshot.manifest.activeFunctionId = funcId.toStdString();
    juce::String funcName = drawer.getActiveFunctionDisplayName();
    if (funcName.isEmpty() && hwId.isNotEmpty())
    {
        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
        {
            for (const auto& fn : contract->functions)
            {
                if (fn.id == funcId.toStdString())
                {
                    funcName = fn.name;
                    break;
                }
            }
        }
    }
    if (funcName.isEmpty()) funcName = funcId;
    snapshot.manifest.activeFunctionName = funcName.toStdString();

    snapshot.manifest.targetModule = targetModule.toStdString();

    snapshot.measuredPoints = sessionManager.getMeasuredPoints();
    snapshot.exportDirectory = sessionIoController.getExportDirectory().getFullPathName().toStdString();

    juce::String base = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();
    snapshot.baseFileName = base.toStdString();

    snapshot.sampleRate = audioEngine.getCurrentSampleRate();
    snapshot.inputTrimDb = audioEngine.getInputAutoTrim();
    snapshot.operatorNotes = drawer.getOperatorNotes().toStdString();
    snapshot.ambientTemperatureC = drawer.getAmbientTemperature();
    snapshot.warmupMinutes = drawer.getWarmupTimeMinutes();

    return snapshot;
}

void MainContentComponent::showStatusBanner(const juce::String& message, bool /*isError*/)
{
    manualPromptLabel.setText(message, juce::dontSendNotification);
    manualPromptLabel.setVisible(true);
    hidePromptAfterDelay(4000);
}

void MainContentComponent::showMessageBox(const juce::String& title, const juce::String& message, bool isError)
{
    juce::AlertWindow::showMessageBoxAsync(
        isError ? juce::AlertWindow::WarningIcon : juce::AlertWindow::InfoIcon,
        title,
        message,
        "OK"
    );
}

void MainContentComponent::updateExportReportMetrics(const exporting::CalculatedSessionMetrics& metrics)
{
    exportReportPanel.updateMetrics(
        metrics.avgSnrDb,
        metrics.noiseFloorDb,
        metrics.avgThdPercent,
        std::max(1, metrics.validPointCount),
        metrics.totalDurationSec
    );
}

void MainContentComponent::notifyExportSuccess(const juce::File& destinationDir, const juce::String& baseName)
{
    exportReportPanel.showExportSuccess(destinationDir.getFullPathName(), baseName);
    workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::ExportReport,
                                        gui::SoundIdSidebarStepper::StepStatus::Completed);
}

void MainContentComponent::showPanelStatus(const juce::String& statusMessage, bool isWarning)
{
    exportReportPanel.showStatusMessage(statusMessage, isWarning);
}

void MainContentComponent::launchProcess(const juce::File& file)
{
    file.startAsProcess();
}

void MainContentComponent::revealInFolder(const juce::File& folder)
{
    if (!folder.startAsProcess())
        folder.revealToUser();
}

void MainContentComponent::prepareAuditionLut()
{
    const int gridSize = 8;
    auto lut = gui::SessionReportManager::buildAuditionLutGrid(sessionManager.getMeasuredPoints(), gridSize);
    audioEngine.loadAuditionLut(lut, gridSize);
}

void MainContentComponent::publishCertificationToCloud()
{
    exportProductionPackage();
    sessionIoController.publishCertificationToCloud();
}

} // namespace abdaudiolab
