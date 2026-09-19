/**
 * @file MainContentComponent.h
 * @brief Main application content component: hardware selection, session plan, measurement
 *        sequencing, live visualization, and session management UI orchestration.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "BuildVersion.h"
#include "audio/LabAudioEngine.h"
#include "core/HardwareContractRegistry.h"
#include "core/ProfilingSession.h"
#include "core/ProfilingSequencer.h"
#include "core/SessionSerializer.h"
#include "core/SessionManager.h"
#include "core/HardwareManager.h"

#include "gui/SoundIdTheme.h"
#include "gui/TopHeaderWidgets.h"
#include "gui/CenterSplitterBar.h"
#include "gui/WorkflowStepperBar.h"
#include "gui/soundid/SoundIdSidebarStepper.h"
#include "gui/soundid/SoundIdHardwareCatalogSelector.h"
#include "gui/ExportReportPanel.h"
#include "gui/SoundIdCurvePlotter.h"
#include "gui/SoundIdMeterStrip.h"
#include "gui/SoundIdSuiteList.h"
#include "gui/SlideInDrawer.h"
#include "gui/InfoDrawer.h"
#include "gui/AboutModalDialog.h"
#include "gui/LoopbackCalibrationModal.h"
#include "gui/HardwareRoutingPanel.h"
#include "gui/NativeCalibrationPanel.h"
#include "gui/HardwareSelectorPill.h"
#include "gui/AudioMidiStatusPill.h"
#include "gui/SoundIdSplashScreen.h"
#include "gui/MeasurementHealthPanel.h"
#include "gui/OperatorStepModalDialog.h"
#include "gui/ConfirmationModalDialog.h"
#include "gui/SessionReportManager.h"
#include "gui/MainHeaderController.h"
#include "gui/SessionExecutionCoordinator.h"
#include "gui/presentation/SessionStatusPresenter.h"
#include "gui/controllers/SessionIoController.h"
#include "gui/controllers/WorkflowNavigationController.h"
#include "gui/controllers/LoadedSessionApplier.h"
#include "gui/AudioABVerificationModal.h"
#include "gui/ScopeWebFloatingWindow.h"
#include <StudioTopology/StudioTopologyController.h>
#include "export/CertificationReportExporter.h"
#include "export/NamDatasetExporter.h"
#include "config/AutoUpdaterConfig.h"
#include <AutoUpdater/AutoUpdater.h>
#include "core/plugins/PluginHostManager.h"
#include "core/plugins/PluginHardwareContractAdapter.h"
#include "gui/plugins/PluginWindowController.h"
#include "gui/plugins/PluginScanDirectoriesModal.h"
#include <MidiKeyboard/MidiKeyboardFloatingWindow.h>
#include "gui/session/ProfilingSessionController.h"
#include "gui/session/UiStrings.h"
#include "gui/soundid/SoundIdGuidedWorkflowContainer.h"
#include "gui/soundid/SoundIdProfilingRunView.h"

namespace abdaudiolab
{

class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::KeyListener,
                             public juce::ChangeListener,
                             public gui::ILoadedSessionTarget
{
public:
    using StartupProgressCallback = std::function<void(const juce::String& statusText, float progress)>;
    explicit MainContentComponent(StartupProgressCallback onProgress = nullptr);
    ~MainContentComponent() override;

    enum class CenterSplitMode
    {
        QueueMaximized,  // Graph minimized to header (~32px), Queue takes the rest
        Balanced,        // Standard split (Queue ~220px resizable, Graph takes the rest)
        GraphMaximized   // Queue minimized to header (~36px), Graph takes the rest
    };

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void updateSplitLayout();
    void toggleScopeWebWindow();
    void toggleStudioTopologyWindow();
    void toggleVirtualKeyboardWindow();
    void openMeasurementViewerWindow();
    void openMeasurementComparisonWindow();
    void preWarmScopeWindow();
    void preWarmHardwareDetector();
    void performOfflineReanalysis();
    void showInfoDrawer();
    void updateSetupDrawerInfo();
    void chooseExportFolder();
    void onHardwareSelected(const juce::String& hwId, const juce::String& funcId);
    void hidePromptAfterDelay(int delayMs = 4000);
    void handleClearPoint(int queueIdx, int pointIdx);
    void startProfilingSession(bool resumeFromExisting = false);
    void stopProfilingSession();
    void confirmManualStep();
    void openAudioMidiSettings();
    void showAboutDialog();

    core::ProfilingSession buildProfilingSessionFromQueue(const std::string& hwName, const std::string& modeStr);
    void startTargetedPatchSession(const std::vector<std::pair<int, int>>& pointsToPatch);
    core::ProfilingSession buildPatchProfilingSession(const std::vector<std::pair<int, int>>& pointsToPatch, const std::string& hwName, const std::string& modeStr);
    core::SessionManifest buildCurrentSessionManifest();
    void applyLoadedSession(const core::SessionManifest& manifest, const std::vector<exporting::MeasuredPoint>& points);

    // ILoadedSessionTarget interface
    void setSessionData(const core::SessionManifest& manifest,
                        const std::vector<exporting::MeasuredPoint>& points) override;
    void clearPlotterAndAddPoints(const std::vector<exporting::MeasuredPoint>& points) override;
    void updateDrawerAndEnvironment(const gui::SessionUiPresentationData& data) override;
    void updateHardwarePanels(const gui::SessionUiPresentationData& data) override;
    void rebuildTestSuiteQueue(const std::vector<core::SessionManifest>& manifests,
                               const std::vector<gui::QueueItem>& items) override;
    void updateWorkflowAndNavigation(const gui::WorkflowStepState& workflowState) override;

    void handleSaveSession();
    void handleSaveSessionAs();
    void saveSessionToFile(const juce::File& file);
    void exportCertificationReport();
    void updateExportReportMetrics();
    void exportProductionPackage();
    void openCertificationReportHtml();
    void prepareAuditionLut();
    void publishCertificationToCloud();
    void promptNewSession();
    void performNewSessionReset();
    void handleOpenSession();
    void performOpenSessionFileChooser();
    void promptDeleteTest(int index, const gui::QueueItem& item);
    void confirmAndExit();
    void openAudioABVerificationModal();
    void initializeAutoUpdater();
    void checkForAppUpdates(bool isManual);

    CenterSplitMode centerSplitMode { CenterSplitMode::Balanced };
    float currentBottomH { 220.0f };
    float targetBottomH { 220.0f };
    float balancedBottomH { 220.0f };

private:
    // Engine & Controllers
    audio::LabAudioEngine audioEngine;
    core::HardwareManager hardwareManager;
    core::PluginHostManager pluginHostManager;
    gui::PluginWindowController pluginWindowController;
    gui::PluginScanDirectoriesModal pluginScanModal;
    juce::AudioPluginInstance* activePluginInstance { nullptr };
    juce::PluginDescription activePluginDescription;
    void loadPluginInstance(const juce::PluginDescription& desc, std::function<void(bool success)> onLoaded = nullptr);
    void applyActivePluginRoutingAndUi(const juce::PluginDescription& desc, double sr, int bs);
    std::unique_ptr<ABDShared::AutoUpdater> autoUpdater;
    core::ProfilingSequencer sequencer;
    core::SessionManager sessionManager;

    gui::SoundIdTheme soundIdTheme;
    juce::TooltipWindow tooltipWindow { this, 400 };

    // Files, Directories & Report Manager
    juce::File settingsFile;
    juce::File exportDirectory;
    gui::SessionReportManager sessionReportManager;
    int totalPointsMeasured { 0 };
    bool isPatchingSession { false };

    // UI Widgets & Visualizers
    gui::MainHeaderController mainHeader;
    int statusUpdateCounter { 0 };

    gui::WorkflowStepperBar stepperBar;
    gui::SoundIdSidebarStepper sidebarStepper;
    gui::SoundIdHardwareCatalogSelector catalogSelector;
    gui::ExportReportPanel exportReportPanel;

    std::unique_ptr<gui::ScopeWebFloatingWindow> scopeWebWindow;
    std::unique_ptr<abd::keyboard::MidiKeyboardFloatingWindow> virtualKeyboardWindow;
    std::unique_ptr<juce::DocumentWindow> measurementViewerWindow;
    std::unique_ptr<juce::DocumentWindow> measurementComparisonWindow;
    abd::topology::StudioTopologyController topologyController;
    std::unique_ptr<gui::SoundIdSplashWindow> aboutSplashWindow;

    gui::SoundIdCurvePlotter curvePlotter;
    gui::SessionExecutionCoordinator sessionCoordinator { sequencer, sessionManager, curvePlotter };
    gui::CenterSplitterBar centerSplitterBar;
    gui::MeasurementHealthPanel healthPanel;
    gui::SoundIdMeterStrip meterStrip;
    gui::SoundIdSuiteList suiteList;
    gui::SlideInDrawer drawer;
    gui::AboutModalDialog aboutModal;
    gui::LoopbackCalibrationModal loopbackModal { audioEngine };
    gui::HardwareRoutingPanel hardwareRoutingPanel;
    gui::NativeCalibrationPanel nativeCalibrationPanel { audioEngine };
    gui::DrawerSetupTab setupInfoTab;
    gui::OperatorStepModalDialog operatorStepModal;
    gui::ConfirmationModalDialog confirmationModal;
    gui::AudioABVerificationModal abVerificationModal;

    // Sub-controllers
    gui::SessionIoController sessionIoController { sessionManager, sessionReportManager, exportReportPanel, confirmationModal };
    gui::WorkflowNavigationController workflowNavController { sidebarStepper, setupInfoTab, catalogSelector, nativeCalibrationPanel, exportReportPanel, curvePlotter, healthPanel, suiteList, operatorStepModal, centerSplitterBar };

    juce::Label manualPromptLabel;
    juce::TextButton btnStepBack;
    juce::TextButton btnRepeatStep;
    juce::TextButton confirmManualButton;

    // Measurement Workspace Governance & Dual Mode UI
    juce::TextButton btnModeToggle;
    juce::Label lblHeaderStatusBadge;
    juce::Label lblActionReasonBanner;
    juce::TextButton btnFreeCapture;
    juce::TextButton btnFreeStop;
    juce::TextButton btnPrimaryAction;
    juce::TextButton btnCancelAction;
    void updateGovernanceUi();

    // Guided Workflow Architecture (Phase 16 & 20.7)
    gui::session::ProfilingSessionController profilingSessionController;
    std::unique_ptr<gui::soundid::SoundIdGuidedWorkflowContainer> guidedWorkflowContainer;
    std::unique_ptr<gui::soundid::SoundIdProfilingRunView> profilingRunView;
    gui::session::UiWorkflowMode currentWorkflowMode { gui::session::UiWorkflowMode::Classic };
    juce::TextButton btnWorkflowModeToggle;
    void setWorkflowMode(gui::session::UiWorkflowMode mode);
    void setupGuidedWorkflowInitialData();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
} // namespace abdaudiolab


