#pragma once

#include "SoundIdTheme.h"
#include "TestConfigModal.h"
#include "TestEditorPanel.h"
#include "ControlIcon.h"
#include "InfoDrawer.h"
#include "../audio/LabStimulusGenerator.h"
#include "../core/HardwareContractRegistry.h"
#include "drawers/DrawerDataModels.h"
#include "drawers/DrawerFileSessionTab.h"
#include "drawers/DrawerHardwareTab.h"
#include "drawers/DrawerSetupTab.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace abdaudiolab::gui
{

enum class DrawerViewMode
{
    FileSessionAndStorage,
    HardwareAndRouting,
    TestAndParametersEditor,
    EngineCalibrationAndInfo
};

/**
 * @brief Unified Contextual Sliding Drawer hosting modular tabs:
 * DrawerFileSessionTab, DrawerHardwareTab, TestEditorPanel, and DrawerSetupTab.
 */
class SlideInDrawer : public juce::Component,
                      public juce::Timer
{
public:
    SlideInDrawer();
    ~SlideInDrawer() override = default;

    void openDrawer();
    void openFileDrawer(const juce::String& currentExportPath);
    void openHardwareDrawer();
    void openTestEditorDrawer(const TestConfiguration& initialConfig, int editingIndex = -1);
    void openSetupDrawer(const TelemetryInfo& info);
    void closeDrawer();
    [[nodiscard]] bool isDrawerOpen() const noexcept { return isOpen; }
    [[nodiscard]] DrawerViewMode getCurrentViewMode() const noexcept { return currentViewMode; }

    void setTelemetryInfo(const TelemetryInfo& info);
    void setHardwareList(const std::vector<HardwareItem>& list);
    void setContracts(std::vector<core::HardwareContract> contractsList);
    void setSelectedHardwareId(const juce::String& id);
    void clearSelectedHardware();
    void setHardwareLocked(bool locked);
    void triggerAutoDetect();
    [[nodiscard]] bool getHardwareLocked() const;
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getActiveHardwareDisplayName() const;
    [[nodiscard]] juce::String getActiveFunctionDisplayName() const;
    [[nodiscard]] const juce::Image& getActiveModelRasterImage() const noexcept;
    [[nodiscard]] int getPrimaryControlSteps() const;
    [[nodiscard]] int getSecondaryControlSteps() const;
    [[nodiscard]] float getBurstDurationSeconds() const;
    [[nodiscard]] bool isAdaptiveEnvelopeMode() const;
    [[nodiscard]] int getSelectedHardwareModeIndex() const;

    [[nodiscard]] audio::StimulusType getSelectedStimulusType() const;
    [[nodiscard]] const TestConfiguration& getCustomConfiguration() const noexcept { return testEditorConfig; }

    // Laboratory Conditions & Notes (1.7.12)
    [[nodiscard]] juce::String getOperatorNotes() const { return tabFileSession.getOperatorNotes(); }
    void setOperatorNotes(const juce::String& notes) { tabFileSession.setOperatorNotes(notes); }
    [[nodiscard]] float getAmbientTemperature() const { return tabFileSession.getAmbientTemperature(); }
    void setAmbientTemperature(float degC) { tabFileSession.setAmbientTemperature(degC); }
    [[nodiscard]] int getWarmupTimeMinutes() const { return tabFileSession.getWarmupTimeMinutes(); }
    void setWarmupTimeMinutes(int minutes) { tabFileSession.setWarmupTimeMinutes(minutes); }

    // File Actions Callbacks
    std::function<void()> onNewSessionClicked;
    std::function<void()> onOpenSessionClicked;
    std::function<void()> onSaveSessionClicked;
    std::function<void()> onSaveSessionAsClicked;
    std::function<void()> onReanalyzeSessionClicked;
    std::function<void()> onRevealExportFolderClicked;
    std::function<void()> onExportReportClicked;
    std::function<void()> onExitAppClicked;

    // Standard Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onHardwareSelected;
    std::function<void(const juce::String& displayName)> onDeviceDetected;
    std::function<void()> onNewFlowRequested;
    std::function<void(const TestConfiguration& conf, int editingIndex)> onTestConfigConfirmed;
    std::function<void()> onChangeExportFolderClicked;
    std::function<void()> onOpenAudioSettingsClicked;
    std::function<void()> onAboutClicked;
    std::function<void()> onCheckUpdatesClicked;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void timerCallback() override;
    void updateTheme();
    void preWarmHardwareDetector() { tabHardware.preWarmHardwarePicker(); }

private:
    void switchViewMode(DrawerViewMode mode);
    void layoutDrawerContent();
    [[nodiscard]] float getResponsivePanelWidth() const;

    DrawerViewMode currentViewMode { DrawerViewMode::HardwareAndRouting };
    int currentEditingTestIndex { -1 };

    bool isOpen { false };
    float currentAnimationPos { 0.0f }; // 0.0 = closed (hidden left), 1.0 = fully open

    TestConfiguration testEditorConfig;
    TelemetryInfo telemetryInfo;

    juce::Component panel;
    juce::Viewport viewport;
    juce::Component contentComp;
    juce::TextButton btnClose { "X" };

    // Modular Drawer Tabs
    DrawerFileSessionTab tabFileSession;
    DrawerHardwareTab tabHardware;
    TestEditorPanel testEditorPanel;
    DrawerSetupTab tabSetup;

    // Bottom Fixed Action Bar
    juce::Component bottomBar;
    juce::TextButton btnCancel { "Cancel" };
    juce::TextButton btnConfirm { "Accept" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlideInDrawer)
};

} // namespace abdaudiolab::gui
