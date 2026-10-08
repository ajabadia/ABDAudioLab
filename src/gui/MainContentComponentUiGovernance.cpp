/**
 * @file MainContentComponentUiGovernance.cpp
 * @brief UI governance, interaction modes, and component sizing.
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
// SECTION 5: UI GOVERNANCE & INTERACTION MODES (GUIDED / LAB BENCH)
// Owns studio step state machine UI representation, view mode toggling (Guided vs Lab Bench), and action guards.
// Session execution state delegated to SessionExecutionCoordinator; hardware capability validation to ContractRegistry.
// ==============================================================================
void MainContentComponent::updateGovernanceUi()
{
    bool isRunSessionStep = (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession);
    if (!isRunSessionStep)
    {
        btnModeToggle.setVisible(false);
        lblHeaderStatusBadge.setVisible(false);
        btnFreeCapture.setVisible(false);
        btnFreeStop.setVisible(false);
        btnPromoteToRecipe.setVisible(false);
        btnPrimaryAction.setVisible(false);
        btnCancelAction.setVisible(false);
        lblActionReasonBanner.setVisible(false);
        return;
    }

    btnModeToggle.setVisible(true);
    lblHeaderStatusBadge.setVisible(true);

    // Hide suiteList run button to avoid duplicate conflicting execution buttons
    suiteList.setRunButtonVisible(false);

    auto mode = sessionCoordinator.getWorkspaceInteractionMode();
    auto state = sessionCoordinator.getCoordinatorState();
    int currentPt = sessionCoordinator.getTotalPointsMeasured();
    int totalPts = suiteList.getTotalPointCount();
    if (totalPts <= 0) totalPts = suiteList.getQueueSize();

    auto sessState = sessionCoordinator.getSessionState();
    unsigned progressPct = totalPts > 0 ? static_cast<unsigned>(currentPt * 100 / totalPts) : 0;

    juce::String errMessage;
    if (sessState == gui::SessionState::Failed || state == measurement::CoordinatorState::Error)
    {
        const auto& hist = sessionCoordinator.getTransitionHistory();
        errMessage = hist.empty() ? juce::String("Error en el flujo de medicion") : juce::String(hist.back().reason);
    }
    const auto sessionStatus = gui::presentation::SessionStatusPresenter::present(sessState, progressPct, errMessage);

    // 1. Mode Text
    juce::String modeStr = (mode == measurement::WorkspaceInteractionMode::Guided) ? gui::strings::MODE_GUIDED : gui::strings::MODE_LAB;
    btnModeToggle.setButtonText(modeStr);
    btnModeToggle.setEnabled(sessionCoordinator.isModeChangeAllowed());
    if (!sessionCoordinator.isModeChangeAllowed())
        btnModeToggle.setTooltip(sessionCoordinator.getRejectionReasonForAction("change_mode"));
    else
        btnModeToggle.setTooltip(gui::strings::TOOLTIP_MODE_TOGGLE);

    // 2. Lifecycle State Text and Colors (from pure SessionStatusPresenter)
    juce::String stateStr = sessionStatus.statusText;
    juce::Colour stateCol = sessionStatus.badgeColour;

    const bool isDark = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark);
    const bool hasRealTarget = resolveCanonicalTarget().has_value() ||
                               gui::session::hasRealTargetInSnapshot(profilingSessionController.getCurrentSnapshot());

    if (mode == measurement::WorkspaceInteractionMode::Guided && !hasRealTarget)
    {
        stateStr = "No Target";
        stateCol = isDark ? juce::Colour(0xffff5252) : juce::Colour(0xffb91c1c);
    }

    // 3. Point Progress & Live Telemetry Text
    juce::String ptStr = (mode == measurement::WorkspaceInteractionMode::Guided)
                             ? (!hasRealTarget ? juce::String("Target & Routing Required")
                                               : ("Point: " + juce::String(currentPt) + " of " + juce::String(std::max(currentPt, totalPts))))
                             : ("Recorded takes: " + juce::String(currentPt));

    float liveRms = audioEngine.getLastPluginOutputRms();
    int lastNote = audioEngine.getLastNoteOnNumber();
    juce::String telemetrySuffix;
    if (liveRms > 0.00001f)
    {
        float rmsDb = juce::Decibels::gainToDecibels(liveRms);
        telemetrySuffix = "  |  RMS: " + juce::String(rmsDb, 1) + " dBFS";
        if (lastNote >= 0)
            telemetrySuffix += " (" + juce::MidiMessage::getMidiNoteName(lastNote, true, true, 3) + ")";
    }

    const auto badgeBg = isDark ? juce::Colour(0xff1e2329) : juce::Colour(0xfff1f3f5);
    lblHeaderStatusBadge.setText("  " + modeStr + "  |  " + stateStr + "  |  " + ptStr + telemetrySuffix + "  ", juce::dontSendNotification);
    lblHeaderStatusBadge.setColour(juce::Label::backgroundColourId, badgeBg);
    lblHeaderStatusBadge.setColour(juce::Label::textColourId, stateCol);
    lblHeaderStatusBadge.setColour(juce::Label::outlineColourId, stateCol.withAlpha(0.6f));

    // 4. Button Enablement & Explanatory Tooltips
    bool canConfirm = sessionCoordinator.isManualConfirmationAllowed();
    confirmManualButton.setEnabled(canConfirm);
    if (!canConfirm)
        confirmManualButton.setTooltip(sessionCoordinator.getRejectionReasonForAction("confirm"));
    else
        confirmManualButton.setTooltip(gui::strings::TOOLTIP_CONFIRM_MANUAL);

    bool isFreeMode = (mode == measurement::WorkspaceInteractionMode::Free);

    if (isFreeMode)
    {
        btnPrimaryAction.setVisible(false);
        btnCancelAction.setVisible(false);

        btnFreeCapture.setButtonText(gui::strings::FREE_CAPTURE);
        btnFreeCapture.setVisible(true);
        bool canCapture = sessionCoordinator.isDirectCaptureAllowed();
        btnFreeCapture.setEnabled(canCapture);
        if (!canCapture)
            btnFreeCapture.setTooltip(sessionCoordinator.getRejectionReasonForAction("capture"));
        else
            btnFreeCapture.setTooltip(gui::strings::TOOLTIP_FREE_CAPTURE);

        btnFreeStop.setButtonText(gui::strings::STOP);
        btnFreeStop.setVisible(true);
        bool canCancel = sessionCoordinator.isCancellationAllowed();
        btnFreeStop.setEnabled(canCancel);
        if (!canCancel)
            btnFreeStop.setTooltip(sessionCoordinator.getRejectionReasonForAction("cancel"));
        else
            btnFreeStop.setTooltip(gui::strings::TOOLTIP_FREE_STOP);

        btnPromoteToRecipe.setButtonText(gui::strings::PROMOTE_TO_RECIPE);
        btnPromoteToRecipe.setVisible(true);
        bool hasContext = (sessionCoordinator.getActiveMeasurementSession() != nullptr || resolveCanonicalTarget().has_value());
        btnPromoteToRecipe.setEnabled(hasContext);
        btnPromoteToRecipe.setTooltip(gui::strings::TOOLTIP_PROMOTE_TO_RECIPE);
    }
    else
    {
        btnFreeCapture.setVisible(false);
        btnFreeStop.setVisible(false);
        btnPromoteToRecipe.setVisible(false);

        // En modo guiado, la barra superior no duplica los comandos de sesión.
        // SoundIdProfilingRunView contiene de forma exclusiva START, PAUSE/RESUME y CANCEL.
        btnPrimaryAction.setVisible(false);
        btnCancelAction.setVisible(false);
    }

    // 6. Persistent Operator Instructions & Error Banners
    if (sessionStatus.bannerVisible)
    {
        lblActionReasonBanner.setText(sessionStatus.bannerText, juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, sessionStatus.bannerColour);
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::AwaitingManualConfirmation)
    {
        lblActionReasonBanner.setText("Paso de alineacion manual pendiente. Ajuste el control fisico y pulse Confirmar o la barra espaciadora.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::Capturing)
    {
        lblActionReasonBanner.setText("Capturando audio inmutable: Cambio de perfil y controles bloqueados durante la grabacion.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, juce::Colour(0xff3498db));
        lblActionReasonBanner.setVisible(true);
    }
    else if (state == measurement::CoordinatorState::Aborted)
    {
        lblActionReasonBanner.setText("Sesion cancelada. Las tomas previas selladas se han preservado.", juce::dontSendNotification);
        lblActionReasonBanner.setColour(juce::Label::textColourId, juce::Colours::grey);
        lblActionReasonBanner.setVisible(true);
    }
    else
    {
        lblActionReasonBanner.setVisible(false);
    }

    resized();
    repaint();
}

// ==============================================================================
// SECTION 6: COMPONENT SIZING, SPLITTER & STEP LAYOUT
// Owns component bounds calculation, responsive splitter positioning, paint dispatch, and UI refresh timer.
// Child element internal layout delegated to individual view components.
// ==============================================================================
void MainContentComponent::updateSplitLayout()
{
    switch (centerSplitMode)
    {
        case CenterSplitMode::Balanced:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (pointing down to expand downwards)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (pointing up to expand upwards)
            curvePlotter.setCollapsed(false);
            suiteList.setCollapsed(false);
            targetBottomH = balancedBottomH;
            break;

        case CenterSplitMode::GraphMaximized:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
            targetBottomH = 36.0f;
            break;

        case CenterSplitMode::QueueMaximized:
            curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
            suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
            {
                auto totalArea = getLocalBounds().reduced(20);
                // Reserve at least 180px graph + 32px health + 12px splitter + 48px top header
                int minTopAndGraphH = 48 + 180 + 32 + 12;
                int maxH = totalArea.getHeight() - minTopAndGraphH;
                targetBottomH = static_cast<float>(std::max(140, maxH));
            }
            break;
    }
}

void MainContentComponent::paint(juce::Graphics& g)
{
    static bool firstPaintLogged = false;
    if (!firstPaintLogged)
    {
        firstPaintLogged = true;
        juce::Logger::writeToLog("[MainComponent] First paint() executed.");
    }
    g.fillAll(gui::SoundIdTheme::bgLight);
}

void MainContentComponent::resized()
{
    static bool firstResizeLogged = false;
    if (!firstResizeLogged)
    {
        firstResizeLogged = true;
        juce::Logger::writeToLog("[MainComponent] First resized() executed. Bounds: " + getLocalBounds().toString());
    }
    auto bounds = getLocalBounds().reduced(20);

    // 1. Top Header Area (Single Coordinated Component)
    auto headerRow = bounds.removeFromTop(36);
    mainHeader.setBounds(headerRow);

    // El panel de avisos va POR ENCIMA de la cabecera, y no debajo de ella ni
    // superpuesto. Encima porque lo que dice no es informacion del flujo que se
    // esta siguiendo: es que falta hardware y por que. Y en una barra propia
    // porque su alto depende de cuantos avisos hay, y metido en el mismo
    // troceado que la cabecera haria que al crecer uno empujase el resto de la
    // pantalla sin que se note por donde.
    //
    // Sin avisos no ocupa nada. `getPreferredHeight` devuelve cero y el panel se
    // esconde solo, para no dejar un hueco arriba de todo en cada arranque
    // limpio, que son los que mas se miran.
    const int avisosAlto = colocarPanelDeAvisos(bounds);
    bounds.removeFromTop(avisosAlto + (avisosAlto > 0 ? 8 : 0));

    bounds.removeFromTop(10);


    // 2. Left Collapsible Sidebar Stepper (SoundID Vertical Workflow Rail)
    int sidebarW = sidebarStepper.getDesiredWidth();
    sidebarStepper.setBounds(bounds.removeFromLeft(sidebarW));
    bounds.removeFromLeft(12);

    // 3. Right Meter Strip
    auto rightArea = bounds.removeFromRight(120);
    meterStrip.setBounds(rightArea);
    bounds.removeFromRight(12);

    // 4. In Step::RunSession, show Governance Bar at the top of the central measurement area
    if (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession)
    {
        auto govRow = bounds.removeFromTop(32);
        btnModeToggle.setBounds(govRow.removeFromLeft(110));
        govRow.removeFromLeft(10);
        lblHeaderStatusBadge.setBounds(govRow.removeFromLeft(380));
        govRow.removeFromLeft(10);

        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Free)
        {
            btnFreeCapture.setBounds(govRow.removeFromLeft(160));
            govRow.removeFromLeft(8);
            btnFreeStop.setBounds(govRow.removeFromLeft(80));
            govRow.removeFromLeft(8);
            btnPromoteToRecipe.setBounds(govRow.removeFromLeft(160));
            govRow.removeFromLeft(10);
        }
        else
        {
            if (btnPrimaryAction.isVisible())
            {
                int btnW = (sessionCoordinator.getCoordinatorState() == measurement::CoordinatorState::SessionCompleted) ? 240 : 180;
                btnPrimaryAction.setBounds(govRow.removeFromLeft(btnW));
                govRow.removeFromLeft(8);
            }
            if (btnCancelAction.isVisible())
            {
                btnCancelAction.setBounds(govRow.removeFromLeft(110));
                govRow.removeFromLeft(10);
            }
        }
        if (lblActionReasonBanner.isVisible())
        {
            lblActionReasonBanner.setBounds(govRow);
        }
        bounds.removeFromTop(10);
    }

    // Manual Prompt Banner & Error Correction Controls
    if (confirmManualButton.isVisible() && !operatorStepModal.isVisible())
    {
        auto manualRow = bounds.removeFromBottom(36);
        confirmManualButton.setBounds(manualRow.removeFromRight(150));
        manualRow.removeFromRight(8);
        btnRepeatStep.setBounds(manualRow.removeFromRight(100));
        manualRow.removeFromRight(8);
        btnStepBack.setBounds(manualRow.removeFromRight(100));
        manualRow.removeFromRight(8);
        manualPromptLabel.setBounds(manualRow);
        bounds.removeFromBottom(8);
    }

    // Delegate SoundID canvas switching and geometry to WorkflowNavigationController
    bool isSplittingBalanced = (centerSplitMode == CenterSplitMode::Balanced) &&
                               (std::abs(targetBottomH - currentBottomH) < 2.0f);
    workflowNavController.layoutStepViews(bounds, currentBottomH, isSplittingBalanced);

    if (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::RunSession)
    {
        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided)
        {
            if (profilingRunView != nullptr)
            {
                profilingRunView->setVisible(true);
                profilingRunView->setBounds(bounds);
            }
            if (resultsSummaryView != nullptr)
                resultsSummaryView->setVisible(false);
            curvePlotter.setVisible(false);
            suiteList.setVisible(false);
            centerSplitterBar.setVisible(false);
            healthPanel.setVisible(false);
        }
        else
        {
            if (profilingRunView != nullptr)
                profilingRunView->setVisible(false);
            if (resultsSummaryView != nullptr)
                resultsSummaryView->setVisible(false);
        }
    }
    else if (workflowNavController.getCurrentStep() == gui::WorkflowNavigationController::Step::ExportReport)
    {
        if (profilingRunView != nullptr)
            profilingRunView->setVisible(false);

        if (sessionCoordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Guided)
        {
            if (resultsSummaryView != nullptr)
            {
                resultsSummaryView->setVisible(true);
                resultsSummaryView->setBounds(bounds);
            }
            exportReportPanel.setVisible(false);
        }
        else
        {
            if (resultsSummaryView != nullptr)
                resultsSummaryView->setVisible(false);
            exportReportPanel.setVisible(true);
            exportReportPanel.setBounds(bounds);
        }
    }
    else
    {
        if (profilingRunView != nullptr)
            profilingRunView->setVisible(false);
        if (resultsSummaryView != nullptr)
            resultsSummaryView->setVisible(false);
    }

    // 6. Slide-in Drawer & Modals fill full window bounds
    drawer.setBounds(getLocalBounds());
    aboutModal.setBounds(getLocalBounds());
    confirmationModal.setBounds(getLocalBounds());
    abVerificationModal.setBounds(getLocalBounds());
    startupWarningsPanel.setBounds(getLocalBounds());
}

void MainContentComponent::timerCallback()
{
    diagnosticsTelemetryPoller.pollNow();
    animateSplitter();
}

void MainContentComponent::animateSplitter()
{
    // Slower, smooth and relaxed chevron/split animation
    if (std::abs(targetBottomH - currentBottomH) > 0.5f)
    {
        float diff = targetBottomH - currentBottomH;
        float step = diff * 0.07f;
        if (std::abs(step) < 0.35f)
            step = (diff > 0.0f ? 0.35f : -0.35f);

        if (std::abs(diff) <= std::abs(step))
            currentBottomH = targetBottomH;
        else
            currentBottomH += step;

        resized();
    }
    else if (currentBottomH != targetBottomH)
    {
        currentBottomH = targetBottomH;
        if (centerSplitMode == CenterSplitMode::GraphMaximized)
            suiteList.setCollapsed(true);
        else if (centerSplitMode == CenterSplitMode::QueueMaximized)
            curvePlotter.setCollapsed(true);
        resized();
    }
}

void MainContentComponent::applyTelemetrySnapshot(const gui::TelemetrySnapshot& snap)
{
    meterStrip.setLevels(snap.inputPeakL, snap.inputPeakR, snap.inputRmsL,
                         snap.outputPeakL, snap.outputPeakR, snap.outputRmsL);

    if (snap.spectrumReady)
    {
        curvePlotter.getSpectrumAnalyzer().pushSpectrumData(snap.fftMagnitudes, snap.sampleRate);
    }

    if (snap.calibrationTickDue)
    {
        mainHeader.updateCalibrationStatus(snap.isCalibrated, snap.calibrationSampleRate, snap.isCalibrationSkipped);
    }

    if (profilingRunView != nullptr && profilingRunView->isVisible())
    {
        auto profileSnap = profilingSessionController.getCurrentSnapshot();
        profileSnap.progress.currentTrial = snap.currentTrial;
        profileSnap.progress.totalTrials = snap.totalTrials;
        profileSnap.progress.progressPercent = snap.progressPercent;
        profileSnap.observation.lastRmsDb = snap.lastPluginOutputRmsDb;
        profileSnap.progress.currentStimulusDescription = snap.stimulusDescription;

        switch (snap.sessionStateCode)
        {
            case 1:
                profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Profiling;
                profileSnap.progress.trialStage = gui::session::TrialLifecycleStage::Capturing;
                break;
            case 2:
                profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Paused;
                break;
            case 3:
                profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Completed;
                profileSnap.progress.trialStage = gui::session::TrialLifecycleStage::Finished;
                break;
            case 4:
                profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::Cancelled;
                break;
            default:
                profileSnap.sessionStatus = gui::session::ProfilingSessionStatus::ReadyToProfile;
                profileSnap.progress.trialStage = gui::session::TrialLifecycleStage::Armed;
                break;
        }

        profilingRunView->updateFromSnapshot(profileSnap);
    }


}

void MainContentComponent::onSessionSnapshotUpdated(const gui::session::ProfilingSessionSnapshot& snapshot)
{
    targetView.updateFromSnapshot(snapshot);
    excitationConfigPanel.updateFromSnapshot(snapshot);
    nativeCalibrationPanel.updateFromSnapshot(snapshot);
    if (profilingRunView != nullptr)
        profilingRunView->updateFromSnapshot(snapshot);
    if (resultsSummaryView != nullptr)
        resultsSummaryView->updateFromSnapshot(snapshot);
}

void MainContentComponent::onAlertRaised(const gui::session::UiAlert& alert)
{
    juce::ignoreUnused(alert);
}

void MainContentComponent::onWorkflowStageChanged(gui::session::ProfilingWorkflowStage newStage)
{
    if (newStage == gui::session::ProfilingWorkflowStage::ConfigureAndStart ||
        newStage == gui::session::ProfilingWorkflowStage::ProfilingActive)
    {
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::HardwareRouting,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::RunSession);
    }
    else if (newStage == gui::session::ProfilingWorkflowStage::ReviewResults)
    {
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::RunSession,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
        workflowNavController.setStepLocked(gui::WorkflowNavigationController::Step::ExportReport, false);
        if (workflowNavController.getStepStatus(gui::WorkflowNavigationController::Step::ExportReport) !=
            gui::SoundIdSidebarStepper::StepStatus::Completed)
        {
            workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::ExportReport,
                                                gui::SoundIdSidebarStepper::StepStatus::Pending);
        }
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::ExportReport);
    }
    else if (newStage == gui::session::ProfilingWorkflowStage::TargetSelection)
    {
        workflowNavController.setStep(gui::WorkflowNavigationController::Step::HardwareRouting);
    }
}

void MainContentComponent::onSessionStatusChanged(gui::session::ProfilingSessionStatus newStatus)
{
    if (newStatus == gui::session::ProfilingSessionStatus::Exported)
    {
        workflowNavController.setStepStatus(gui::WorkflowNavigationController::Step::ExportReport,
                                            gui::SoundIdSidebarStepper::StepStatus::Completed);
    }
}

} // namespace abdaudiolab
