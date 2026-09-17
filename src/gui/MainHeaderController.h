/**
 * @file MainHeaderController.h
 * @brief Autonomous top header controller orchestrating session menu, scope toggle,
 *        Audio/MIDI status pill with driver auto-refresh, calibration pill,
 *        hardware selector, theme switcher, and system info buttons.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "SoundIdTheme.h"
#include "TopHeaderWidgets.h"
#include "AudioMidiStatusPill.h"
#include "HardwareSelectorPill.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab {
namespace gui {

/**
 * @class MainHeaderController
 * @brief Encapsulates the entire top navigation and status bar of ABDAudioLab.
 */
class MainHeaderController : public juce::Component,
                             public juce::ChangeListener,
                             private juce::Timer
{
public:
    explicit MainHeaderController(audio::LabAudioEngine& engine);
    ~MainHeaderController() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* broadcaster) override;

    // State update API
    void setHardwareInfo(const juce::String& displayName,
                         const juce::String& functionName,
                         const juce::Image& thumbnail,
                         HardwareConnectionStatus status);
    void clearHardware();
    [[nodiscard]] bool hasHardwareSelected() const noexcept
    {
        return btnHardwareSelector != nullptr && btnHardwareSelector->hasHardwareSelected();
    }
    void updateAudioMidiStatus();
    void updateCalibrationStatus(bool isCalibrated, double calSampleRate, bool isSkipped);
    void updateTheme();

    // Popup file menu
    void showFileMenu();

    // Event callbacks towards MainContentComponent
    std::function<void()> onNewSession;
    std::function<void()> onOpenSession;
    std::function<void()> onSaveSession;
    std::function<void()> onSaveSessionAs;
    std::function<void()> onReanalyzeOffline;
    std::function<void()> onOpenMeasurementViewer;
    std::function<void()> onOpenMeasurementComparison;
    std::function<void()> onExportCertificationReport;
    std::function<void()> onOpenExportFolder;
    std::function<void()> onOpenExperimentsFolder;
    std::function<void()> onExitApp;
    std::function<void()> onScanPluginDirectories;

    std::function<void()> onScopeToggle;
    std::function<void()> onVirtualKeyboardToggle;
    std::function<void()> onConfigureAudioMidi;
    std::function<void()> onCalibrateClicked;
    std::function<void()> onHardwareSelectorClicked;
    std::function<void()> onThemeToggled;

private:
    audio::LabAudioEngine& audioEngine;

    // Header buttons
    juce::TextButton btnFileMenu;
    juce::TextButton btnScope;
    juce::TextButton btnVirtualKeyboard;
    juce::TextButton btnCalibratePill;

    // Custom UI Pills
    std::unique_ptr<AudioMidiStatusPill> audioMidiStatusPill;
    std::unique_ptr<HardwareSelectorPill> btnHardwareSelector;
    std::unique_ptr<ThemeToggleButton> btnThemeToggle;

    // Calibration flashing state
    bool isFlashing { false };
    bool flashState { false };
    int calibrationStatusMode { 0 }; // 0: invalid/uncalibrated, 1: valid, 2: recalibrate needed, 3: skipped
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainHeaderController)
};

} // namespace gui
} // namespace abdaudiolab
