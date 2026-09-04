#pragma once

#include "SoundIdTheme.h"
#include "TestConfigModal.h"
#include "TestEditorPanel.h"
#include "ControlIcon.h"
#include "InfoDrawer.h"
#include "../audio/LabStimulusGenerator.h"
#include "../core/HardwareContractRegistry.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "HardwareMidiDetect/MidiHardwareBackend.h"

namespace abdaudiolab::gui
{

enum class DrawerViewMode
{
    FileSessionAndStorage,
    HardwareAndRouting,
    TestAndParametersEditor,
    EngineCalibrationAndInfo
};

struct ControlItem
{
    juce::String name;
    juce::String type;
};

struct FunctionItem
{
    juce::String id;
    juce::String name;
    juce::String blockType;
    juce::String stimulusOutput;
    juce::String responseInput;
    juce::String notes;
    juce::String captureMode { "FIXED_TIME" };
    float defaultBurstDurationSec { 1.0f };
    std::vector<ControlItem> controls;
};

struct HardwareItem
{
    juce::String id;
    juce::String displayName;
    juce::String description;
    juce::String category;
    juce::String brand;
    juce::String brandLogo;
    juce::String modelImage;
    juce::String protocol; // "AIRA_SYSEX", "MIDI_CC", "MANUAL_ANALOGUE", "MOCK_DSP"
    std::vector<FunctionItem> functions;
};

/**
 * @brief Unified Contextual Sliding Drawer supporting File/Session, Hardware Inspector,
 * Test & Parameter Editor, and Audio Setup modes.
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
    void setContracts(std::vector<core::HardwareContract> contractsList) { availableContracts = std::move(contractsList); }
    void setSelectedHardwareId(const juce::String& id);
    void clearSelectedHardware();
    void setHardwareLocked(bool locked);
    [[nodiscard]] bool getHardwareLocked() const { return isHardwareLocked; }
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getActiveHardwareDisplayName() const;
    [[nodiscard]] juce::String getActiveFunctionDisplayName() const;
    [[nodiscard]] const juce::Image& getActiveModelRasterImage() const noexcept { return modelRasterImage; }
    [[nodiscard]] int getPrimaryControlSteps() const;
    [[nodiscard]] int getSecondaryControlSteps() const;
    [[nodiscard]] float getBurstDurationSeconds() const;
    [[nodiscard]] bool isAdaptiveEnvelopeMode() const;
    [[nodiscard]] int getSelectedHardwareModeIndex() const { return hwModeCombo.getSelectedId(); }

    [[nodiscard]] audio::StimulusType getSelectedStimulusType() const;
    [[nodiscard]] const TestConfiguration& getCustomConfiguration() const noexcept { return testEditorConfig; }

    // File Actions Callbacks
    std::function<void()> onNewSessionClicked;
    std::function<void()> onOpenSessionClicked;
    std::function<void()> onSaveSessionClicked;
    std::function<void()> onSaveSessionAsClicked;
    std::function<void()> onRevealExportFolderClicked;
    std::function<void()> onExportReportClicked;
    std::function<void()> onExitAppClicked;

    // Standard Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onHardwareSelected;
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

private:
    void switchViewMode(DrawerViewMode mode);
    void layoutDrawerContent();
    void updateBrandAndModelGraphics();
    void updateFunctionSelectionUI(const HardwareItem& item);
    void rebuildTestEditorControls();
    void updateTestEditorEstimatedTime();
    [[nodiscard]] float getResponsivePanelWidth() const;

    DrawerViewMode currentViewMode { DrawerViewMode::HardwareAndRouting };
    int currentEditingTestIndex { -1 };

    bool isOpen { false };
    float currentAnimationPos { 0.0f }; // 0.0 = closed (hidden left), 1.0 = fully open

    std::vector<HardwareItem> hardwareList;
    std::vector<core::HardwareContract> availableContracts;
    TestConfiguration testEditorConfig;
    TelemetryInfo telemetryInfo;
    juce::String currentExportDirStr;

    juce::Component panel;
    juce::Viewport viewport;
    juce::Component contentComp;
    juce::TextButton btnClose { "X" };

    // ==========================================
    // VISTA 0: File & Session Management
    // ==========================================
    juce::Label lblFileSection;
    juce::TextButton btnFileNew { "New Session (Reset Plan)" };
    juce::TextButton btnFileOpen { "Open Session Manifest (.json)..." };
    juce::TextButton btnFileSave { "Save Session" };
    juce::TextButton btnFileSaveAs { "Save Session As..." };
    juce::Label lblFileExportSection;
    juce::Label lblFileExportPathVal;
    juce::TextButton btnFileChangeExport { "Change Target Folder..." };
    juce::TextButton btnFileRevealExport { "Show Target Folder in Explorer" };
    juce::TextButton btnFileExportReport { "Generate / Export Certification Report (HTML/PDF)" };
    juce::TextButton btnCheckUpdates { "Check for Updates..." };
    juce::TextButton btnFileExit { "Exit ABDAudioLab" };

    // File Previewer Widgets
    juce::Label lblFilePreviewSection;
    juce::ComboBox comboPreviewFiles;
    juce::TextEditor txtCodePreview;
    juce::TextButton btnCopyPreview { "Copy Code" };
    juce::TextButton btnOpenFileInEditor { "Open in Editor" };
    juce::String currentExportFolderPath;
    void refreshFilePreviewList();

    // ==========================================
    // VISTA 1: Hardware & Routing Widgets
    // ==========================================
    std::unique_ptr<juce::Drawable> brandLogoDrawable;
    std::unique_ptr<juce::Drawable> modelSvgDrawable;
    juce::Image modelRasterImage;
    juce::String currentHwBrand;

    class ImageDisplayComponent : public juce::Component
    {
    public:
        ImageDisplayComponent(SlideInDrawer& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
    private:
        SlideInDrawer& owner;
    };
    ImageDisplayComponent imgDisplay { *this };

    bool isHardwareLocked { false };
    juce::Label lblHardwareLockedBanner;
    juce::Label lblHwTitle;
    juce::Label lblSelectHw;
    juce::ComboBox hwModeCombo;
    juce::Label lblSelectFunc;
    juce::ComboBox hwFunctionCombo;

    class WiringGuideCard : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        juce::String stimulusText;
        juce::String responseText;
    };
    WiringGuideCard cardWiring;
    juce::Label lblAutoDetectSection;
    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::Label lblOrSeparator;

    // ==========================================
    // VISTA 2: Test & Parameter Editor Widgets
    // ==========================================
    TestEditorPanel testEditorPanel;

    // ==========================================
    // VISTA 3: Audio Setup & Telemetry Widgets
    // ==========================================
    juce::Label lblSetupTargetSection;
    juce::Label lblSetupTargetHwName;
    juce::Label lblSetupTargetSubmodule;
    juce::Label lblSetupTargetRouting;
    ImageDisplayComponent setupImgDisplay { *this };

    juce::Label lblSetupAudioSection;
    juce::Label lblSetupAudioDevice;
    juce::Label lblSetupAudioDeviceVal;
    juce::Label lblSetupSampleRate;
    juce::Label lblSetupSampleRateVal;
    juce::Label lblSetupLatency;
    juce::Label lblSetupLatencyVal;
    juce::Label lblSetupMidiInput;
    juce::Label lblSetupMidiInputVal;
    juce::Label lblSetupMidiOutput;
    juce::Label lblSetupMidiOutputVal;

    juce::TextButton btnSetupAudioMidi { "Configure Audio & MIDI Settings..." };
    juce::TextButton btnSetupAbout { "About ABDAudioLab & Research Architecture" };

    // Inline About & Research Architecture Card (BUG-04)
    bool aboutSectionExpanded { false };
    juce::Label lblAboutVersion;
    juce::Label lblAboutTagline;
    juce::Label lblAboutArchitecture;
    juce::Label lblAboutCredits;

    // ==========================================
    // Bottom Fixed Action Bar
    // ==========================================
    juce::Component bottomBar;
    juce::TextButton btnCancel { "Cancel" };
    juce::TextButton btnConfirm { "Accept" };

    // ==========================================
    // Hardware MIDI Detection (ABDSharedCode WebUI)
    // ==========================================
    std::unique_ptr<abd::hwid::MidiHardwareBackend> midiBackend;
    std::unique_ptr<juce::DocumentWindow> pickerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlideInDrawer)
};

} // namespace abdaudiolab::gui
