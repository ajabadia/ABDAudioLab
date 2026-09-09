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
#include "gui/AudioABVerificationModal.h"
#include "gui/ScopeWebFloatingWindow.h"
#include "export/CertificationReportExporter.h"
#include "export/NamDatasetExporter.h"
#include "config/AutoUpdaterConfig.h"
#include <AutoUpdater/AutoUpdater.h>

namespace abdaudiolab
{

class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::KeyListener,
                             public juce::ChangeListener
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
    void preWarmScopeWindow();
    void preWarmHardwareDetector();
    void performOfflineReanalysis();
    void showInfoDrawer();
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
    gui::OperatorStepModalDialog operatorStepModal;
    gui::ConfirmationModalDialog confirmationModal;
    gui::AudioABVerificationModal abVerificationModal;

    juce::Label manualPromptLabel;
    juce::TextButton btnStepBack;
    juce::TextButton btnRepeatStep;
    juce::TextButton confirmManualButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
} // namespace abdaudiolab


