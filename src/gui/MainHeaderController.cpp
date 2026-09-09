/**
 * @file MainHeaderController.cpp
 * @brief Implementation of autonomous top header controller.
 * @author ABDSynths
 * @date 2026
 */

#include "MainHeaderController.h"

namespace abdaudiolab {
namespace gui {

MainHeaderController::MainHeaderController(audio::LabAudioEngine& engine)
    : audioEngine(engine)
{
    // Autonomous subscription to audio device manager to auto-refresh drivers and sample rate
    audioEngine.getDeviceManager().addChangeListener(this);

    // 1. File Menu Button
    btnFileMenu.setButtonText(juce::String::fromUTF8(u8"File \u25BE"));
    btnFileMenu.setTooltip("Session Management & File Operations (New, Open, Save, Export, Exit)");
    btnFileMenu.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnFileMenu.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
    btnFileMenu.onClick = [this] { showFileMenu(); };
    addAndMakeVisible(btnFileMenu);

    // 2. Scope Button
    btnScope.setButtonText("Scope");
    btnScope.setTooltip("ABDScope Visualizer - Open floating multi-lane oscilloscope, real-time FFT spectrum, waterfall, freeze and snapshot inspector.");
    btnScope.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnScope.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
    btnScope.onClick = [this] { if (onScopeToggle) onScopeToggle(); };
    addAndMakeVisible(btnScope);

    // 3. Audio & MIDI Status Pill
    audioMidiStatusPill = std::make_unique<AudioMidiStatusPill>();
    audioMidiStatusPill->onConfigureClicked = [this] { if (onConfigureAudioMidi) onConfigureAudioMidi(); };
    audioMidiStatusPill->updateStatus(audioEngine);
    addAndMakeVisible(audioMidiStatusPill.get());

    // 4. Calibration Pill
    btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CALIBRATION: -3.0 dBFS"));
    btnCalibratePill.setTooltip("Sound Card Line Loopback Calibration - Calibrate DAC->ADC loopback latency and flat frequency compensation.");
    btnCalibratePill.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
    btnCalibratePill.onClick = [this] { if (onCalibrateClicked) onCalibrateClicked(); };
    addAndMakeVisible(btnCalibratePill);

    // 5. Hardware Selector Pill
    btnHardwareSelector = std::make_unique<HardwareSelectorPill>();
    btnHardwareSelector->clearHardware();
    btnHardwareSelector->setTooltip("Target Hardware & Submodule - Select device under test (Roland, Moog, Minilogue, etc.) and active circuit/submodule.");
    btnHardwareSelector->onClick = [this] { if (onHardwareSelectorClicked) onHardwareSelectorClicked(); };
    addAndMakeVisible(btnHardwareSelector.get());

    // 6. Theme Toggle Button
    btnThemeToggle = std::make_unique<ThemeToggleButton>();
    btnThemeToggle->onClick = [this] { if (onThemeToggled) onThemeToggled(); };
    addAndMakeVisible(btnThemeToggle.get());

    // 7. Info Button
    btnInfo = std::make_unique<MonochromeInfoButton>();
    btnInfo->onClick = [this] { if (onInfoClicked) onInfoClicked(); };
    addAndMakeVisible(btnInfo.get());
}

MainHeaderController::~MainHeaderController()
{
    audioEngine.getDeviceManager().removeChangeListener(this);
    stopTimer();
}

void MainHeaderController::changeListenerCallback(juce::ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &audioEngine.getDeviceManager())
    {
        updateAudioMidiStatus();
    }
}

void MainHeaderController::updateAudioMidiStatus()
{
    if (audioMidiStatusPill != nullptr)
        audioMidiStatusPill->updateStatus(audioEngine);
}

void MainHeaderController::setHardwareInfo(const juce::String& displayName,
                                           const juce::String& functionName,
                                           const juce::Image& thumbnail,
                                           HardwareConnectionStatus status)
{
    if (btnHardwareSelector != nullptr)
        btnHardwareSelector->setHardwareInfo(displayName, functionName, thumbnail, status);
}

void MainHeaderController::clearHardware()
{
    if (btnHardwareSelector != nullptr)
        btnHardwareSelector->clearHardware();
}

void MainHeaderController::updateCalibrationStatus(bool isCalibrated, double calSampleRate, bool isSkipped)
{
    if (isCalibrated)
    {
        if (std::abs(calSampleRate - audioEngine.getCurrentSampleRate()) < 1.0)
        {
            isFlashing = false;
            stopTimer();
            calibrationStatusMode = 1;
            btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CAL: VALID (-3.0 dBFS)"));
            btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentGreen);
        }
        else
        {
            calibrationStatusMode = 2;
            if (!isFlashing)
            {
                isFlashing = true;
                startTimer(500); // 2 Hz flashing alert for SR change
            }
        }
    }
    else if (isSkipped)
    {
        isFlashing = false;
        stopTimer();
        calibrationStatusMode = 3;
        btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CAL: OMITIDA (NOMINAL 0 dB)"));
        btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentAmber);
    }
    else
    {
        isFlashing = false;
        stopTimer();
        calibrationStatusMode = 0;
        btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CALIBRATION: -3.0 dBFS"));
        btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
    }
    repaint();
}

void MainHeaderController::timerCallback()
{
    flashState = !flashState;
    if (calibrationStatusMode == 2)
    {
        btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"⚠ CAL: RECALIBRATE (SR CHANGED)"));
        btnCalibratePill.setColour(juce::TextButton::textColourOffId,
                                   flashState ? gui::SoundIdTheme::accentAmber : gui::SoundIdTheme::textMuted);
    }
}

void MainHeaderController::updateTheme()
{
    btnCalibratePill.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    if (calibrationStatusMode == 0)
        btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

    btnFileMenu.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnFileMenu.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

    btnScope.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
    btnScope.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

    if (btnThemeToggle != nullptr) btnThemeToggle->repaint();
    if (btnHardwareSelector != nullptr) btnHardwareSelector->repaint();
    if (audioMidiStatusPill != nullptr) audioMidiStatusPill->repaint();
    if (btnInfo != nullptr) btnInfo->repaint();
    repaint();
}

void MainHeaderController::showFileMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Session\tCtrl+N");
    menu.addItem(2, "Open Session...\tCtrl+O");
    menu.addItem(3, "Save Session\tCtrl+S");
    menu.addItem(4, "Save Session As...\tCtrl+Shift+S");
    menu.addItem(5, "Re-Analyze Session (Offline)...");
    menu.addSeparator();
    menu.addItem(6, "Export Certification Report (PDF/HTML)...");
    menu.addItem(7, "Open Export Folder");
    menu.addSeparator();
    menu.addItem(8, "Exit ABDAudioLab");

    juce::Component::SafePointer<MainHeaderController> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnFileMenu),
                       [safeThis](int result) {
        if (safeThis == nullptr || result == 0) return;
        switch (result)
        {
            case 1: if (safeThis->onNewSession) safeThis->onNewSession(); break;
            case 2: if (safeThis->onOpenSession) safeThis->onOpenSession(); break;
            case 3: if (safeThis->onSaveSession) safeThis->onSaveSession(); break;
            case 4: if (safeThis->onSaveSessionAs) safeThis->onSaveSessionAs(); break;
            case 5: if (safeThis->onReanalyzeOffline) safeThis->onReanalyzeOffline(); break;
            case 6: if (safeThis->onExportCertificationReport) safeThis->onExportCertificationReport(); break;
            case 7: if (safeThis->onOpenExportFolder) safeThis->onOpenExportFolder(); break;
            case 8: if (safeThis->onExitApp) safeThis->onExitApp(); break;
            default: break;
        }
    });
}

void MainHeaderController::resized()
{
    auto topArea = getLocalBounds();

    btnFileMenu.setBounds(topArea.removeFromLeft(68).withHeight(32));
    topArea.removeFromLeft(8);
    btnScope.setBounds(topArea.removeFromLeft(74).withHeight(32));
    topArea.removeFromLeft(8);

    if (audioMidiStatusPill != nullptr)
        audioMidiStatusPill->setBounds(topArea.removeFromLeft(240).withHeight(32));

    if (btnInfo != nullptr)
        btnInfo->setBounds(topArea.removeFromRight(32).withSizeKeepingCentre(28, 28));
    topArea.removeFromRight(8);

    if (btnThemeToggle != nullptr)
        btnThemeToggle->setBounds(topArea.removeFromRight(36).withHeight(32));
    topArea.removeFromRight(8);

    if (btnHardwareSelector != nullptr)
        btnHardwareSelector->setBounds(topArea.removeFromRight(290).withHeight(32));
    topArea.removeFromRight(8);

    btnCalibratePill.setBounds(topArea.removeFromRight(185).withHeight(32));
}

void MainHeaderController::paint(juce::Graphics& /*g*/)
{
    // Transparent or theme background rendered by parent
}

} // namespace gui
} // namespace abdaudiolab
